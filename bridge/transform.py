from __future__ import annotations

import base64
import math
import re
import binascii
from typing import Any, Mapping

from .schema_validation import SchemaValidationError, validate_payload

BATTERY_STATUS_TO_ROS = {
    "unknown": 0,
    "charging": 1,
    "discharging": 2,
    "not_charging": 3,
    "full": 4,
}

MAX_TIMESTAMP_MS = 253_402_300_799_000
FRAME_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_./-]{0,127}$")


def validate_and_transform(stream: str, payload: Mapping[str, Any]) -> dict[str, Any]:
    if stream == "imu":
        return _transform_imu(payload)
    if stream == "gps":
        return _transform_gps(payload)
    if stream == "battery":
        return _transform_battery(payload)
    if stream in ("camera", "front_camera"):
        return _transform_camera(payload)
    if stream == "joy":
        return _transform_joy(payload)
    if stream == "magnetometer":
        return _transform_magnetometer(payload)
    if stream == "barometer":
        return _transform_barometer(payload)
    if stream == "infrared":
        return _transform_infrared(payload)
    if stream == "thermal":
        return _transform_thermal(payload)
    raise ValueError(f"Unsupported stream type: {stream}")


def _transform_imu(payload: Mapping[str, Any]) -> dict[str, Any]:
    validate_payload("imu", payload)
    return {
        "header": _header(payload),
        "orientation": _vector4(payload["orientation"]),
        "angular_velocity": _vector3(payload["angular_velocity"]),
        "linear_acceleration": _vector3(payload["linear_acceleration"]),
    }


def _transform_gps(payload: Mapping[str, Any]) -> dict[str, Any]:
    validate_payload("gps", payload)
    covariance = payload.get("covariance_m2")
    has_covariance = covariance is not None

    return {
        "header": _header(payload),
        "latitude": float(payload["latitude"]),
        "longitude": float(payload["longitude"]),
        "altitude": float(payload["altitude_m"]),
        "position_covariance": (
            [float(v) for v in covariance] if has_covariance else [0.0] * 9
        ),
        "position_covariance_type": 2 if has_covariance else 0,
    }


def _transform_battery(payload: Mapping[str, Any]) -> dict[str, Any]:
    validate_payload("battery", payload)
    status = payload["status"]
    return {
        "header": _header(payload),
        "percentage": float(payload["percentage"]),
        "voltage": float(payload["voltage_v"]),
        "current": float(payload["current_a"]),
        "power_supply_status": BATTERY_STATUS_TO_ROS[status],
    }


def _transform_camera(payload: Mapping[str, Any]) -> dict[str, Any]:
    validate_payload("camera", payload)

    try:
        image_data = base64.b64decode(payload["data_base64"], validate=True)
    except (binascii.Error, TypeError) as exc:
        raise SchemaValidationError(
            "$.data_base64: invalid base64-encoded image data"
        ) from exc

    encoding = str(payload["encoding"])
    width = int(payload.get("width", 0))
    height = int(payload.get("height", 0))
    # NV21: 1.5 bytes per pixel (Y plane + interleaved VU)
    if encoding == "nv21":
        step = width
    elif encoding in ("bgr8", "rgb8"):
        step = width * 3
    elif encoding in ("bgra8", "rgba8"):
        step = width * 4
    elif encoding == "mono8":
        step = width
    else:
        # Fallback: estimate from data size
        step = width * 3 if width > 0 and height > 0 else 0

    return {
        "header": _header(payload),
        "encoding": encoding,
        "width": width,
        "height": height,
        "step": step,
        "is_bigendian": 0,
        "data": list(image_data),
    }


def _transform_joy(payload: Mapping[str, Any]) -> dict[str, Any]:
    validate_payload("joy", payload)
    raw_buttons = payload.get("buttons", [])
    if raw_buttons is None:
        buttons: list[int] = []
    else:
        buttons = [int(button) for button in raw_buttons]

    return {
        "header": _header(payload),
        "axes": [float(value) for value in payload["axes"]],
        "buttons": buttons,
    }


def _transform_magnetometer(payload: Mapping[str, Any]) -> dict[str, Any]:
    """Convert Android magnetometer payload to ROS MagneticField."""
    validate_payload("magnetometer", payload)
    field = payload["magnetic_field_ut"]
    return {
        "header": _header(payload),
        "magnetic_field": {
            "x": float(field["x"]) * 1e-6,  # microtesla -> tesla
            "y": float(field["y"]) * 1e-6,
            "z": float(field["z"]) * 1e-6,
        },
        "magnetic_field_covariance": [0.0] * 9,
    }


def _transform_barometer(payload: Mapping[str, Any]) -> dict[str, Any]:
    """Convert Android barometer payload to ROS FluidPressure."""
    validate_payload("barometer", payload)
    return {
        "header": _header(payload),
        "fluid_pressure": float(payload["pressure_pa"]),
        "variance": 0.0,
    }


def _transform_infrared(payload: Mapping[str, Any]) -> dict[str, Any]:
    """Convert Android infrared/proximity payload to ROS Range."""
    validate_payload("infrared", payload)
    return {
        "header": _header(payload),
        "radiation_type": 1,  # INFRARED
        "field_of_view": math.pi,
        "min_range": 0.0,
        "max_range": 0.05,
        "range": float(payload["distance_cm"]) / 100.0,
    }


def _transform_thermal(payload: Mapping[str, Any]) -> dict[str, Any]:
    """Convert Android device-temperature payload to ROS Float32.

    Android reports CPU/battery temperature as a simple scalar — not a
    thermal-camera reading — so we use std_msgs/Float32 (just ``data``)
    instead of sensor_msgs/Temperature.
    """
    validate_payload("thermal", payload)
    return {
        "data": float(payload["temperature_celsius"]),
    }


def _header(payload: Mapping[str, Any]) -> dict[str, Any]:
    timestamp_ms = payload["timestamp_ms"]
    if not isinstance(timestamp_ms, int) or isinstance(timestamp_ms, bool):
        raise SchemaValidationError(
            "$.timestamp_ms: expected integer timestamp in milliseconds"
        )
    if timestamp_ms < 0 or timestamp_ms > MAX_TIMESTAMP_MS:
        raise SchemaValidationError(
            f"$.timestamp_ms: {timestamp_ms} is outside supported range [0, {MAX_TIMESTAMP_MS}]"
        )

    timestamp_offset_ms = payload.get("timestamp_offset_ms", 0)
    if not isinstance(timestamp_offset_ms, int) or isinstance(
        timestamp_offset_ms, bool
    ):
        raise SchemaValidationError(
            "$.timestamp_offset_ms: expected integer milliseconds offset"
        )
    adjusted_timestamp_ms = timestamp_ms + timestamp_offset_ms
    if adjusted_timestamp_ms < 0 or adjusted_timestamp_ms > MAX_TIMESTAMP_MS:
        raise SchemaValidationError(
            "$.timestamp_offset_ms: adjusted timestamp "
            f"{adjusted_timestamp_ms} is outside supported range [0, {MAX_TIMESTAMP_MS}]"
        )

    frame_id = payload["frame_id"]
    if not isinstance(frame_id, str):
        raise SchemaValidationError("$.frame_id: expected string")
    if frame_id != frame_id.strip():
        raise SchemaValidationError(
            "$.frame_id: must not include leading/trailing whitespace"
        )
    if not FRAME_ID_PATTERN.fullmatch(frame_id):
        raise SchemaValidationError(
            "$.frame_id: must match ^[A-Za-z0-9][A-Za-z0-9_./-]{0,127}$"
        )

    return {
        "stamp_ms": adjusted_timestamp_ms,
        "frame_id": frame_id,
    }


def _vector3(source: Mapping[str, Any]) -> dict[str, float]:
    return {
        "x": float(source["x"]),
        "y": float(source["y"]),
        "z": float(source["z"]),
    }


def _vector4(source: Mapping[str, Any]) -> dict[str, float]:
    return {
        "x": float(source["x"]),
        "y": float(source["y"]),
        "z": float(source["z"]),
        "w": float(source["w"]),
    }
