"""Versioned recorder contracts shared by capture CLI/backend implementations.

This module is deliberately backend-neutral.  It owns canonical JSON identity,
strict schema loading, stable error families, and the recording state machine.
It does not launch games or record media.
"""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
from typing import Any, Mapping

from jsonschema import Draft202012Validator, FormatChecker


SCHEMA_DIR = Path(__file__).with_name("schemas")

ERROR_CODES = frozenset(
    {
        "CAPABILITY_MISSING",
        "SOURCE_NOT_FOUND",
        "AUDIO_SOURCE_NOT_FOUND",
        "AUDIO_TRACK_MISSING",
        "AUDIO_ACTIVITY_MISSING",
        "AUDIO_ROUTING_INVALID",
        "TARGET_UNSUPPORTED",
        "SOURCE_SIZE_MISMATCH",
        "BACKEND_START_FAILED",
        "BACKEND_EXITED",
        "GAME_EXITED",
        "STOP_TIMEOUT",
        "AV_VALIDATION_FAILED",
        "LOW_DISK",
        "CONTRACT_MISMATCH",
        "RELEASE_SURFACE_LEAK",
    }
)

STOP_REASONS = frozenset(
    {
        "requested",
        "hotkey",
        "ctrl_c",
        "timeout",
        "game_exit",
        "backend_exit",
        "low_disk",
        "cancelled",
    }
)

ATTEMPT_TRANSITIONS: Mapping[str, frozenset[str]] = {
    "created": frozenset({"preflighted", "failed", "abandoned"}),
    "preflighted": frozenset({"countdown", "failed", "abandoned"}),
    "countdown": frozenset({"recording", "failed", "abandoned"}),
    "recording": frozenset({"stopping", "failed", "abandoned"}),
    "stopping": frozenset({"stopping", "validating", "failed", "abandoned"}),
    "validating": frozenset({"promoted", "failed", "abandoned"}),
    "promoted": frozenset(),
    "failed": frozenset(),
    "abandoned": frozenset(),
}

ENCODE_TRANSITIONS: Mapping[str, frozenset[str]] = {
    "created": frozenset({"encoding", "failed", "abandoned"}),
    "encoding": frozenset({"validating", "failed", "abandoned"}),
    "validating": frozenset({"promoted", "failed", "abandoned"}),
    "promoted": frozenset(),
    "failed": frozenset(),
    "abandoned": frozenset(),
}


class CaptureContractError(ValueError):
    """A machine-readable error with a bounded, safe diagnostic payload."""

    def __init__(
        self,
        code: str,
        message: str,
        *,
        remediation: str,
        details: Mapping[str, Any] | None = None,
    ) -> None:
        if code not in ERROR_CODES:
            raise ValueError(f"unknown capture error code: {code}")
        super().__init__(message)
        self.code = code
        self.safe_details = dict(details or {})
        self.safe_details["remediation"] = remediation


def _assert_json_value(value: Any, path: str = "$") -> None:
    if value is None or isinstance(value, (bool, str, int)):
        return
    if isinstance(value, float):
        if not math.isfinite(value):
            raise CaptureContractError(
                "CONTRACT_MISMATCH",
                f"non-finite JSON number at {path}",
                remediation="Replace NaN or infinity with a finite JSON number.",
                details={"path": path},
            )
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            _assert_json_value(item, f"{path}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            if not isinstance(key, str):
                raise CaptureContractError(
                    "CONTRACT_MISMATCH",
                    f"non-string JSON object key at {path}",
                    remediation="Use string keys in every contract object.",
                    details={"path": path},
                )
            _assert_json_value(item, f"{path}.{key}")
        return
    raise CaptureContractError(
        "CONTRACT_MISMATCH",
        f"unsupported JSON value at {path}: {type(value).__name__}",
        remediation="Convert the value to JSON null, boolean, number, string, array, or object.",
        details={"path": path},
    )


def _normalize_json_numbers(value: Any) -> Any:
    """Normalize the numeric equivalences frozen by canonical JSON v1.

    JSON has one number domain, while Python distinguishes ``int`` and
    ``float`` and preserves a negative zero spelling.  Integral finite floats
    therefore use integer tokens and both signed zero values use ``0``.
    """

    if value is None or isinstance(value, (bool, str, int)):
        return value
    if isinstance(value, float):
        if value == 0 or value.is_integer():
            return int(value)
        return value
    if isinstance(value, list):
        return [_normalize_json_numbers(item) for item in value]
    if isinstance(value, dict):
        return {key: _normalize_json_numbers(item) for key, item in value.items()}
    return value


def canonical_json_bytes(value: Any) -> bytes:
    """Return AI Studio canonical JSON v1 bytes.

    V1 uses UTF-8, sorted object keys, no insignificant whitespace, preserved
    array order, and finite JSON numbers only.
    """

    # sort_keys / compact separators / allow_nan are the documented Python 3.12
    # encoder controls: https://docs.python.org/3.12/library/json.html#json.dumps
    _assert_json_value(value)
    normalized = _normalize_json_numbers(value)
    return json.dumps(
        normalized,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")


def canonical_hash(value: Any) -> str:
    return hashlib.sha256(canonical_json_bytes(value)).hexdigest()


def load_schema(name: str) -> dict[str, Any]:
    if Path(name).name != name or not name.endswith(".schema.json"):
        raise CaptureContractError(
            "CONTRACT_MISMATCH",
            f"invalid schema name: {name}",
            remediation="Use a schema filename from runtime_automation/schemas.",
            details={"schema": name},
        )
    path = SCHEMA_DIR / name
    if not path.is_file():
        raise CaptureContractError(
            "CONTRACT_MISMATCH",
            f"unknown schema: {name}",
            remediation="Install the matching versioned capture schema.",
            details={"schema": name},
        )
    with path.open("r", encoding="utf-8") as stream:
        schema = json.load(stream)
    Draft202012Validator.check_schema(schema)
    return schema


def validate_document(value: Any, schema_name: str) -> None:
    # Draft selection and explicit FormatChecker follow jsonschema 4.26's
    # validator API: https://python-jsonschema.readthedocs.io/en/stable/validate/
    _assert_json_value(value)
    Draft202012Validator(
        load_schema(schema_name), format_checker=FormatChecker()
    ).validate(value)


def transition(
    current: str,
    requested: str,
    table: Mapping[str, frozenset[str]],
) -> str:
    if current not in table or requested not in table[current]:
        raise CaptureContractError(
            "CONTRACT_MISMATCH",
            f"illegal capture state transition: {current} -> {requested}",
            remediation="Resume from the last durable state or create a new attempt.",
            details={"from": current, "to": requested},
        )
    return requested
