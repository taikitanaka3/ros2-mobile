from __future__ import annotations

import json
import re
from functools import lru_cache
from pathlib import Path
from typing import Any, Mapping

SCHEMA_DIR = Path(__file__).resolve().parent / "schemas"


class SchemaValidationError(ValueError):
    """Raised when a payload does not satisfy its JSON schema contract."""


@lru_cache(maxsize=None)
def load_schema(schema_name: str) -> dict[str, Any]:
    path = SCHEMA_DIR / f"{schema_name}.schema.json"
    if not path.exists():
        raise FileNotFoundError(f"Schema file not found: {path}")

    with path.open("r", encoding="utf-8") as fp:
        schema = json.load(fp)

    if not isinstance(schema, dict):
        raise TypeError(f"Schema root must be object: {path}")
    return schema


def validate_payload(schema_name: str, payload: Any) -> None:
    schema = load_schema(schema_name)
    _validate_node(schema, payload, "$")


def _validate_node(schema: Mapping[str, Any], value: Any, path: str) -> None:
    expected_type = schema.get("type")
    if expected_type is not None:
        _validate_type(expected_type, value, path)

    if "enum" in schema and value not in schema["enum"]:
        allowed = ", ".join(repr(v) for v in schema["enum"])
        raise SchemaValidationError(f"{path}: value {value!r} not in enum [{allowed}]")

    if isinstance(value, str):
        min_len = schema.get("minLength")
        max_len = schema.get("maxLength")
        if isinstance(min_len, int) and len(value) < min_len:
            raise SchemaValidationError(
                f"{path}: string length {len(value)} is below minLength {min_len}"
            )
        if isinstance(max_len, int) and len(value) > max_len:
            raise SchemaValidationError(
                f"{path}: string length {len(value)} is above maxLength {max_len}"
            )
        pattern = schema.get("pattern")
        if isinstance(pattern, str):
            try:
                matched = re.search(pattern, value) is not None
            except re.error as exc:
                raise SchemaValidationError(
                    f"{path}: invalid pattern {pattern!r}: {exc}"
                ) from exc
            if not matched:
                raise SchemaValidationError(
                    f"{path}: string {value!r} does not match pattern {pattern!r}"
                )

    if _is_number(value):
        minimum = schema.get("minimum")
        maximum = schema.get("maximum")
        if isinstance(minimum, (int, float)) and value < minimum:
            raise SchemaValidationError(f"{path}: {value} is below minimum {minimum}")
        if isinstance(maximum, (int, float)) and value > maximum:
            raise SchemaValidationError(f"{path}: {value} is above maximum {maximum}")

    if _matches_type("object", value):
        required = schema.get("required", [])
        if isinstance(required, list):
            for key in required:
                if key not in value:
                    raise SchemaValidationError(
                        f"{path}: missing required property {key!r}"
                    )

        properties = schema.get("properties", {})
        if isinstance(properties, dict):
            for key, child_schema in properties.items():
                if key in value and isinstance(child_schema, Mapping):
                    _validate_node(child_schema, value[key], f"{path}.{key}")

            if schema.get("additionalProperties", True) is False:
                extras = set(value.keys()) - set(properties.keys())
                if extras:
                    extras_text = ", ".join(sorted(repr(k) for k in extras))
                    raise SchemaValidationError(
                        f"{path}: unexpected properties {extras_text}"
                    )

    if _matches_type("array", value):
        min_items = schema.get("minItems")
        max_items = schema.get("maxItems")
        if isinstance(min_items, int) and len(value) < min_items:
            raise SchemaValidationError(
                f"{path}: array length {len(value)} is below minItems {min_items}"
            )
        if isinstance(max_items, int) and len(value) > max_items:
            raise SchemaValidationError(
                f"{path}: array length {len(value)} is above maxItems {max_items}"
            )

        item_schema = schema.get("items")
        if isinstance(item_schema, Mapping):
            for index, item in enumerate(value):
                _validate_node(item_schema, item, f"{path}[{index}]")


def _validate_type(expected_type: Any, value: Any, path: str) -> None:
    if isinstance(expected_type, list):
        if any(_matches_type(type_name, value) for type_name in expected_type):
            return
        expected = " or ".join(str(type_name) for type_name in expected_type)
        raise SchemaValidationError(
            f"{path}: expected type {expected}, got {type(value).__name__}"
        )

    if not _matches_type(expected_type, value):
        raise SchemaValidationError(
            f"{path}: expected type {expected_type}, got {type(value).__name__}"
        )


def _matches_type(type_name: Any, value: Any) -> bool:
    if type_name == "object":
        return isinstance(value, Mapping)
    if type_name == "array":
        return isinstance(value, list)
    if type_name == "string":
        return isinstance(value, str)
    if type_name == "boolean":
        return isinstance(value, bool)
    if type_name == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if type_name == "number":
        return _is_number(value)
    if type_name == "null":
        return value is None
    return False


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)
