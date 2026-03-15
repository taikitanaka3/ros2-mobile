from __future__ import annotations

import json
import logging
import time
from dataclasses import dataclass, field
from typing import Any, Callable, Iterable, Mapping

from .interfaces import (
    ErrorCallback,
    RosPublisher,
    RosPublisherFactory,
    ZenohSubscriber,
)
from .transform import validate_and_transform

ROS_MESSAGE_TYPES = {
    "imu": "sensor_msgs/msg/Imu",
    "gps": "sensor_msgs/msg/NavSatFix",
    "battery": "sensor_msgs/msg/BatteryState",
    "camera": "sensor_msgs/msg/Image",
    "front_camera": "sensor_msgs/msg/Image",
    "joy": "sensor_msgs/msg/Joy",
    "magnetometer": "sensor_msgs/msg/MagneticField",
    "barometer": "sensor_msgs/msg/FluidPressure",
    "infrared": "sensor_msgs/msg/Range",
}
QOS_PROFILES = {"best_effort", "reliable"}
DEFAULT_QOS_PROFILE = "best_effort"
DEFAULT_QUEUE_SIZE = 10
# How many seconds must pass before the same zenoh topic logs another error.
DEFAULT_ERROR_LOG_COOLDOWN_S = 5.0


@dataclass(frozen=True)
class StreamBinding:
    stream: str
    zenoh_topic: str
    ros_topic: str


@dataclass(frozen=True)
class RouteState:
    stream: str
    ros_topic: str
    message_type: str
    publisher: RosPublisher
    qos_profile: str
    queue_size: int


@dataclass
class _StreamCounters:
    """Per-stream message / error counters plus error-log rate-limit state."""

    msg_count: int = 0
    error_count: int = 0
    # monotonic timestamp of the last error that was actually logged
    last_error_log_time: float = field(default_factory=lambda: 0.0)
    # errors suppressed since the last logged error
    suppressed_errors: int = 0


def decode_json_payload(raw: Any) -> Mapping[str, Any]:
    if isinstance(raw, Mapping):
        return dict(raw)

    if isinstance(raw, (bytes, bytearray, memoryview)):
        raw_bytes = bytes(raw)
        if not raw_bytes:
            raise ValueError("Payload bytes must not be empty")
        try:
            raw = raw_bytes.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise ValueError("Payload bytes must be valid UTF-8 JSON") from exc

    if not isinstance(raw, str):
        raise TypeError(f"Unsupported payload type: {type(raw).__name__}")
    if not raw.strip():
        raise ValueError("Payload string must not be empty")

    try:
        decoded = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise ValueError(f"Invalid JSON payload: {exc.msg}") from exc
    if not isinstance(decoded, Mapping):
        raise TypeError("Decoded JSON payload must be an object")
    return dict(decoded)


class BridgeNode:
    """Pure-python bridge core for Android JSON payloads to ROS message dicts."""

    def __init__(
        self,
        subscriber: ZenohSubscriber,
        publisher_factory: RosPublisherFactory,
        bindings: Iterable[StreamBinding],
        *,
        decoder: Callable[[Any], Mapping[str, Any]] = decode_json_payload,
        on_error: ErrorCallback | None = None,
        logger: logging.Logger | None = None,
        default_qos_profile: str = DEFAULT_QOS_PROFILE,
        default_queue_size: int = DEFAULT_QUEUE_SIZE,
        error_log_cooldown_s: float = DEFAULT_ERROR_LOG_COOLDOWN_S,
    ) -> None:
        self._subscriber = subscriber
        self._publisher_factory = publisher_factory
        self._bindings = list(bindings)
        self._decoder = decoder
        self._on_error = on_error
        self._logger = logger or logging.getLogger(__name__)
        self._default_qos_profile = _parse_qos_profile(
            default_qos_profile, DEFAULT_QOS_PROFILE
        )
        self._default_queue_size = _parse_queue_size(
            default_queue_size, DEFAULT_QUEUE_SIZE
        )
        self._error_log_cooldown_s = error_log_cooldown_s
        self._routes: dict[str, RouteState] = {}
        self._counters: dict[str, _StreamCounters] = {}
        # track which bindings failed to register so stop() can report them
        self._failed_bindings: list[tuple[StreamBinding, Exception]] = []

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def start(self) -> None:
        """Register all bindings.  A failure on one binding is logged and
        skipped; remaining bindings continue to be registered."""
        for binding in self._bindings:
            message_type = ROS_MESSAGE_TYPES.get(binding.stream)
            if message_type is None:
                exc = ValueError(f"Unsupported stream in binding: {binding.stream}")
                self._failed_bindings.append((binding, exc))
                self._logger.error(
                    "Skipping binding stream=%s zenoh=%s: %s",
                    binding.stream,
                    binding.zenoh_topic,
                    exc,
                )
                continue

            try:
                publisher = self._publisher_factory.create_publisher(
                    binding.ros_topic,
                    message_type,
                    qos_profile=self._default_qos_profile,
                    queue_size=self._default_queue_size,
                )
            except Exception as exc:
                self._failed_bindings.append((binding, exc))
                self._logger.error(
                    "Publisher creation failed stream=%s zenoh=%s ros=%s: %s",
                    binding.stream,
                    binding.zenoh_topic,
                    binding.ros_topic,
                    exc,
                )
                continue

            self._routes[binding.zenoh_topic] = RouteState(
                stream=binding.stream,
                ros_topic=binding.ros_topic,
                message_type=message_type,
                publisher=publisher,
                qos_profile=self._default_qos_profile,
                queue_size=self._default_queue_size,
            )
            self._counters[binding.zenoh_topic] = _StreamCounters()
            self._logger.info(
                "Register bridge route stream=%s zenoh=%s ros=%s type=%s qos=%s depth=%s",
                binding.stream,
                binding.zenoh_topic,
                binding.ros_topic,
                message_type,
                self._default_qos_profile,
                self._default_queue_size,
            )

            try:
                self._subscriber.subscribe(
                    binding.zenoh_topic,
                    self._build_callback(binding.zenoh_topic),
                )
            except Exception as exc:
                # Route is registered but subscription failed; remove it so
                # callbacks never fire with a dangling route entry.
                del self._routes[binding.zenoh_topic]
                del self._counters[binding.zenoh_topic]
                self._failed_bindings.append((binding, exc))
                self._logger.error(
                    "Zenoh subscription failed stream=%s zenoh=%s: %s",
                    binding.stream,
                    binding.zenoh_topic,
                    exc,
                )

        if self._failed_bindings:
            self._logger.warning(
                "Bridge started with %d/%d binding(s) failed — those streams are inactive",
                len(self._failed_bindings),
                len(self._bindings),
            )

    def stop(self) -> None:
        """Log a shutdown summary with per-stream message and error counts."""
        total_msgs = 0
        total_errors = 0
        for zenoh_topic, counters in self._counters.items():
            route = self._routes.get(zenoh_topic)
            stream = route.stream if route else "unknown"
            total_msgs += counters.msg_count
            total_errors += counters.error_count
            self._logger.info(
                "Shutdown summary stream=%s zenoh=%s msg_count=%d error_count=%d",
                stream,
                zenoh_topic,
                counters.msg_count,
                counters.error_count,
            )
        self._logger.info(
            "Bridge stopped total_msg_count=%d total_error_count=%d active_routes=%d",
            total_msgs,
            total_errors,
            len(self._routes),
        )
        for binding, exc in self._failed_bindings:
            self._logger.warning(
                "Binding was inactive throughout shutdown stream=%s zenoh=%s reason=%s",
                binding.stream,
                binding.zenoh_topic,
                exc,
            )

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _build_callback(self, zenoh_topic: str) -> Callable[[Any], None]:
        def _callback(raw: Any) -> None:
            try:
                route = self._routes[zenoh_topic]
                payload = self._decoder(raw)
                requested_qos = _parse_qos_profile(
                    payload.get("qos_profile"), route.qos_profile
                )
                requested_queue = _parse_queue_size(
                    payload.get("queue_depth"), route.queue_size
                )
                if (
                    requested_qos != route.qos_profile
                    or requested_queue != route.queue_size
                ):
                    publisher = self._publisher_factory.create_publisher(
                        route.ros_topic,
                        route.message_type,
                        qos_profile=requested_qos,
                        queue_size=requested_queue,
                    )
                    route = RouteState(
                        stream=route.stream,
                        ros_topic=route.ros_topic,
                        message_type=route.message_type,
                        publisher=publisher,
                        qos_profile=requested_qos,
                        queue_size=requested_queue,
                    )
                    self._routes[zenoh_topic] = route
                    self._logger.info(
                        "Updated route qos stream=%s zenoh=%s qos=%s depth=%s",
                        route.stream,
                        zenoh_topic,
                        requested_qos,
                        requested_queue,
                    )

                ros_message = validate_and_transform(route.stream, payload)
                route.publisher.publish(ros_message)
                self._counters[zenoh_topic].msg_count += 1
                self._logger.debug(
                    "Published stream=%s from zenoh=%s payload_type=%s",
                    route.stream,
                    zenoh_topic,
                    type(raw).__name__,
                )
            except Exception as exc:
                counters = self._counters[zenoh_topic]
                counters.error_count += 1
                now = time.monotonic()
                elapsed = now - counters.last_error_log_time
                if elapsed >= self._error_log_cooldown_s:
                    suppressed = counters.suppressed_errors
                    counters.last_error_log_time = now
                    counters.suppressed_errors = 0
                    suffix = (
                        f" (suppressed {suppressed} similar errors)"
                        if suppressed
                        else ""
                    )
                    self._logger.warning(
                        "Bridge processing failed zenoh=%s payload_type=%s error=%s%s",
                        zenoh_topic,
                        type(raw).__name__,
                        exc,
                        suffix,
                    )
                else:
                    counters.suppressed_errors += 1

                if self._on_error is not None:
                    try:
                        self._on_error(zenoh_topic, exc, raw)
                    except Exception:
                        self._logger.exception(
                            "Bridge error callback failed zenoh=%s",
                            zenoh_topic,
                        )

        return _callback


def _parse_qos_profile(value: Any, default: str) -> str:
    if value is None:
        value = default
    if not isinstance(value, str):
        raise ValueError(f"qos_profile must be string, got {type(value).__name__}")
    normalized = value.strip().lower()
    if normalized in ("besteffort", "best-effort"):
        normalized = "best_effort"
    if normalized not in QOS_PROFILES:
        raise ValueError(f"Unsupported qos_profile: {value!r}")
    return normalized


def _parse_queue_size(value: Any, default: int) -> int:
    if value is None:
        return default
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"queue_depth must be integer, got {type(value).__name__}")
    if value <= 0:
        raise ValueError("queue_depth must be > 0")
    return value
