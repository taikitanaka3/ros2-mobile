from __future__ import annotations

import argparse
import importlib
import logging
import os
from typing import Any, Mapping, Sequence

from .node import BridgeNode, ROS_MESSAGE_TYPES, StreamBinding

LOGGER = logging.getLogger(__name__)
SENSOR_MESSAGE_CLASS_NAMES = {
    "sensor_msgs/msg/Imu": "Imu",
    "sensor_msgs/msg/NavSatFix": "NavSatFix",
    "sensor_msgs/msg/BatteryState": "BatteryState",
    "sensor_msgs/msg/Image": "Image",
    "sensor_msgs/msg/Joy": "Joy",
    "sensor_msgs/msg/MagneticField": "MagneticField",
    "sensor_msgs/msg/FluidPressure": "FluidPressure",
    "sensor_msgs/msg/Range": "Range",
}


class RuntimeDependencyError(RuntimeError):
    """Raised when required runtime dependencies are not installed."""


def parse_binding(value: str) -> StreamBinding:
    parts = [part.strip() for part in value.split(":", 2)]
    if len(parts) != 3 or any(not part for part in parts):
        raise argparse.ArgumentTypeError(
            "Binding format must be STREAM:ZENOH_KEY:ROS_TOPIC "
            "(example: imu:android/device1/imu:/android/device1/imu)"
        )

    stream, zenoh_topic, ros_topic = parts
    if stream not in ROS_MESSAGE_TYPES:
        known = ", ".join(sorted(ROS_MESSAGE_TYPES))
        raise argparse.ArgumentTypeError(
            f"Unsupported stream {stream!r}. Known streams: {known}"
        )
    return StreamBinding(stream=stream, zenoh_topic=zenoh_topic, ros_topic=ros_topic)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="bridge-cli",
        description="Android sensor bridge: Zenoh JSON -> ROS 2 messages.",
    )
    parser.add_argument(
        "--binding",
        action="append",
        type=parse_binding,
        required=True,
        metavar="STREAM:ZENOH_KEY:ROS_TOPIC",
        help=(
            "Route mapping for one stream. Repeat this option for multiple streams. "
            "Valid STREAM values: imu, gps, battery, camera, front_camera, "
            "joy, magnetometer, barometer, infrared."
        ),
    )
    parser.add_argument(
        "--zenoh-endpoint",
        default="tcp/127.0.0.1:7447",
        help="Zenoh endpoint used to open a session.",
    )
    parser.add_argument(
        "--ros-node-name",
        default="android_bridge_node",
        help="ROS2 node name used by the bridge.",
    )
    parser.add_argument(
        "--queue-size",
        default=10,
        type=int,
        help="ROS publisher queue depth.",
    )
    parser.add_argument(
        "--default-qos",
        default="best_effort",
        choices=("best_effort", "reliable"),
        help="Default ROS publisher reliability profile when payload does not override qos_profile.",
    )
    parser.add_argument(
        "--log-level",
        default="INFO",
        choices=("DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"),
        help="Log level.",
    )
    parser.add_argument(
        "--ros-domain-id",
        type=_parse_domain_id,
        default=None,
        help="Override ROS_DOMAIN_ID (0-255) before starting the bridge node.",
    )
    return parser


def _parse_domain_id(value: str) -> int:
    stripped = value.strip()
    try:
        parsed = int(stripped, 10)
    except ValueError:
        raise argparse.ArgumentTypeError("ROS domain id must be an integer")
    if parsed < 0 or parsed > 255:
        raise argparse.ArgumentTypeError("ROS domain id must be between 0 and 255")
    return parsed


def _load_runtime_dependencies() -> tuple[Any, Any, Any]:
    errors: list[str] = []
    modules: dict[str, Any] = {}
    # install_hint is used only for pip-installable packages;
    # ROS packages (rclpy, sensor_msgs) have None as hint.
    dependencies = [
        ("zenoh", "zenoh"),
        ("rclpy", None),
        ("sensor_msgs.msg", None),
    ]

    for module_name, install_name in dependencies:
        try:
            modules[module_name] = importlib.import_module(module_name)
        except ImportError as exc:
            if install_name is not None:
                errors.append(
                    f"- missing dependency {module_name!r}: "
                    f"`python3 -m pip install {install_name}` (import error: {exc})"
                )
            else:
                errors.append(
                    f"- missing dependency {module_name!r}: "
                    f"install your ROS 2 distro and source setup.bash (import error: {exc})"
                )

    if errors:
        errors.append(
            "For ROS2 packages (`rclpy`, `sensor_msgs`), install your ROS2 distro and source setup.bash."
        )
        raise RuntimeDependencyError("\n".join(errors))

    return modules["zenoh"], modules["rclpy"], modules["sensor_msgs.msg"]


def _build_message_type_map(sensor_msgs_msg: Any) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for ros_message_type, class_name in SENSOR_MESSAGE_CLASS_NAMES.items():
        message_class = getattr(sensor_msgs_msg, class_name, None)
        if message_class is None:
            raise RuntimeDependencyError(
                f"sensor_msgs.msg.{class_name} is missing. Verify ROS2 sensor message packages."
            )
        result[ros_message_type] = message_class
    return result


def _set_ros_time_from_ms(stamp: Any, timestamp_ms: int) -> None:
    stamp.sec = int(timestamp_ms // 1000)
    stamp.nanosec = int(timestamp_ms % 1000) * 1_000_000


def _assign_ros_fields(
    target: Any, values: Mapping[str, Any], *, path: str = "$"
) -> None:
    for key, value in values.items():
        if key == "stamp_ms":
            if not isinstance(value, int):
                raise TypeError(
                    f"{path}.stamp_ms must be int, got {type(value).__name__}"
                )
            if hasattr(target, "stamp"):
                _set_ros_time_from_ms(target.stamp, value)
                continue
            if hasattr(target, "sec") and hasattr(target, "nanosec"):
                _set_ros_time_from_ms(target, value)
                continue
            raise AttributeError(
                f"{path}: cannot map stamp_ms to {target.__class__.__name__}"
            )

        if not hasattr(target, key):
            raise AttributeError(
                f"{path}: field {key!r} not found in {target.__class__.__name__}"
            )

        current = getattr(target, key)
        if isinstance(value, Mapping):
            _assign_ros_fields(current, value, path=f"{path}.{key}")
            continue
        setattr(target, key, value)


def _extract_zenoh_payload(sample: Any) -> Any:
    payload = getattr(sample, "payload", sample)
    if isinstance(payload, memoryview):
        return payload.tobytes()
    for method_name in ("to_bytes", "to_byte_array"):
        method = getattr(payload, method_name, None)
        if callable(method):
            return method()
    return payload


def _open_zenoh_session(zenoh: Any, endpoint: str) -> Any:
    config_cls = getattr(zenoh, "Config", None)
    if callable(config_cls):
        config = config_cls()
        insert_json5 = getattr(config, "insert_json5", None)
        if callable(insert_json5):
            insert_json5("connect/endpoints", f'["{endpoint}"]')
        return zenoh.open(config)

    try:
        return zenoh.open(endpoint)
    except TypeError:
        return zenoh.open()


class _RosPublisherAdapter:
    def __init__(self, publisher: Any, message_class: Any) -> None:
        self._publisher = publisher
        self._message_class = message_class

    def publish(self, message: Mapping[str, Any]) -> None:
        ros_message = self._message_class()
        _assign_ros_fields(ros_message, message)
        self._publisher.publish(ros_message)


class _RosPublisherFactoryAdapter:
    def __init__(
        self,
        node: Any,
        message_type_map: Mapping[str, Any],
        queue_size: int,
        default_qos: str,
        rclpy_qos: Any | None,
    ) -> None:
        self._node = node
        self._message_type_map = dict(message_type_map)
        self._queue_size = queue_size
        self._default_qos = default_qos
        self._rclpy_qos = rclpy_qos

    def create_publisher(
        self,
        topic: str,
        message_type: str,
        *,
        qos_profile: str | None = None,
        queue_size: int | None = None,
    ) -> _RosPublisherAdapter:
        message_class = self._message_type_map.get(message_type)
        if message_class is None:
            raise ValueError(f"Unsupported ROS message type: {message_type}")
        requested_qos = (qos_profile or self._default_qos).strip().lower()
        requested_queue_size = queue_size or self._queue_size
        if requested_queue_size <= 0:
            raise ValueError("queue_size must be > 0")
        qos_obj = _build_ros_qos_profile(
            self._rclpy_qos, requested_qos, requested_queue_size
        )
        publisher = self._node.create_publisher(message_class, topic, qos_obj)
        return _RosPublisherAdapter(publisher, message_class)


class _ZenohSubscriberAdapter:
    def __init__(self, session: Any) -> None:
        self._session = session
        self._handles: list[Any] = []

    def subscribe(self, topic: str, callback: Any) -> None:
        handle = self._session.declare_subscriber(
            topic, lambda sample: callback(_extract_zenoh_payload(sample))
        )
        self._handles.append(handle)

    def close(self) -> None:
        for handle in self._handles:
            undeclare = getattr(handle, "undeclare", None)
            if callable(undeclare):
                undeclare()
        self._handles.clear()


def _configure_logging(level: str) -> None:
    logging.basicConfig(
        level=getattr(logging, level, logging.INFO),
        format="%(asctime)s %(levelname)s %(name)s - %(message)s",
    )


def _load_rclpy_qos_module() -> Any | None:
    try:
        return importlib.import_module("rclpy.qos")
    except ImportError:
        LOGGER.warning(
            "rclpy.qos is not available; falling back to integer queue depth publishers."
        )
        return None


def _build_ros_qos_profile(
    rclpy_qos: Any | None, qos_profile: str, queue_size: int
) -> Any:
    if rclpy_qos is None:
        return queue_size

    qos_profile_cls = getattr(rclpy_qos, "QoSProfile", None)
    reliability_policy = getattr(rclpy_qos, "ReliabilityPolicy", None)
    if qos_profile_cls is None or reliability_policy is None:
        return queue_size

    profile = qos_profile_cls(depth=queue_size)
    if qos_profile == "reliable":
        profile.reliability = reliability_policy.RELIABLE
    else:
        profile.reliability = reliability_policy.BEST_EFFORT
    return profile


def run(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.queue_size <= 0:
        parser.error("--queue-size must be > 0")
    _configure_logging(args.log_level)

    if args.ros_domain_id is not None:
        os.environ["ROS_DOMAIN_ID"] = str(args.ros_domain_id)
        LOGGER.info("Using ROS_DOMAIN_ID=%d", args.ros_domain_id)

    try:
        zenoh, rclpy, sensor_msgs_msg = _load_runtime_dependencies()
        message_type_map = _build_message_type_map(sensor_msgs_msg)
        rclpy_qos = _load_rclpy_qos_module()
    except RuntimeDependencyError as exc:
        LOGGER.error("%s", exc)
        return 2

    ros_node: Any | None = None
    session: Any | None = None
    subscriber: _ZenohSubscriberAdapter | None = None
    external_shutdown_exception: type[BaseException] | None = None

    try:
        rclpy.init(args=None)
        try:
            rclpy_executors = importlib.import_module("rclpy.executors")
            candidate = getattr(rclpy_executors, "ExternalShutdownException", None)
            if isinstance(candidate, type) and issubclass(candidate, BaseException):
                external_shutdown_exception = candidate
        except ImportError:
            external_shutdown_exception = None
        ros_node = rclpy.create_node(args.ros_node_name)
        session = _open_zenoh_session(zenoh, args.zenoh_endpoint)
        subscriber = _ZenohSubscriberAdapter(session)
        publisher_factory = _RosPublisherFactoryAdapter(
            ros_node,
            message_type_map,
            args.queue_size,
            args.default_qos,
            rclpy_qos,
        )

        bridge_node = BridgeNode(
            subscriber,
            publisher_factory,
            args.binding,
            on_error=lambda topic, exc, raw: LOGGER.error(
                "Bridge callback error topic=%s payload_type=%s error=%s",
                topic,
                type(raw).__name__,
                exc,
            ),
            logger=LOGGER,
            default_qos_profile=args.default_qos,
            default_queue_size=args.queue_size,
        )
        bridge_node.start()
        LOGGER.info("Bridge started with %d binding(s)", len(args.binding))
        rclpy.spin(ros_node)
        return 0
    except KeyboardInterrupt:
        LOGGER.info("Bridge interrupted by user")
        return 0
    except Exception as exc:
        if external_shutdown_exception is not None and isinstance(
            exc, external_shutdown_exception
        ):
            LOGGER.info("Bridge stopped by external shutdown")
            return 0
        LOGGER.exception("Bridge runtime failed")
        return 1
    finally:
        if subscriber is not None:
            subscriber.close()
        if session is not None:
            close = getattr(session, "close", None)
            if callable(close):
                close()
        if ros_node is not None:
            destroy_node = getattr(ros_node, "destroy_node", None)
            if callable(destroy_node):
                destroy_node()
        shutdown = getattr(rclpy, "shutdown", None)
        ok = getattr(rclpy, "ok", None)
        if callable(shutdown):
            should_shutdown = True
            if callable(ok):
                try:
                    should_shutdown = bool(ok())
                except Exception:
                    should_shutdown = True
            if should_shutdown:
                shutdown()


def main() -> None:
    raise SystemExit(run())


if __name__ == "__main__":
    main()
