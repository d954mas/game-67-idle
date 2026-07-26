"""Machine-checkable platform delivery constraints for one media artifact."""

from __future__ import annotations

import math
from typing import Any, Mapping

from jsonschema.exceptions import ValidationError

from capture_contracts import (
    CaptureContractError,
    canonical_hash,
    canonical_json_bytes,
    validate_document,
)


def _aspect_ratio(width: int, height: int) -> str:
    divisor = math.gcd(width, height)
    return f"{width // divisor}:{height // divisor}"


def validate_descriptor(
    descriptor: Mapping[str, Any],
    constraint: Mapping[str, Any],
) -> dict[str, Any]:
    """Validate normalized ffprobe-like media facts against one constraint.

    Empty arrays in an ``official_partial`` constraint mean the platform source
    did not publish that field; they do not invent a restriction.
    """

    try:
        canonical_json_bytes(dict(descriptor))
        validate_document(
            dict(descriptor), "delivery-media-descriptor.v1.schema.json"
        )
        validate_document(dict(constraint), "delivery-constraint.v1.schema.json")
    except ValidationError as error:
        path = ".".join(str(part) for part in error.absolute_path) or "$"
        raise CaptureContractError(
            "CONTRACT_MISMATCH",
            f"invalid delivery media descriptor at {path}",
            remediation="Probe the media again and provide every normalized typed fact.",
            details={"path": path, "validator": error.validator},
        ) from error
    rules = constraint["constraints"]
    failures: list[str] = []

    if rules["containers"] and descriptor.get("container") not in rules["containers"]:
        failures.append("container")
    if rules["video_codecs"] and descriptor.get("video_codec") not in rules["video_codecs"]:
        failures.append("video_codec")
    if rules["audio_codecs"] and descriptor.get("audio_codec") not in rules["audio_codecs"]:
        failures.append("audio_codec")
    if rules["sample_rates"] and descriptor.get("sample_rate") not in rules["sample_rates"]:
        failures.append("sample_rate")

    width = descriptor["width"]
    height = descriptor["height"]
    fps = descriptor["fps"]["numerator"] / descriptor["fps"]["denominator"]
    duration = descriptor["duration_seconds"]
    byte_count = descriptor["bytes"]

    if width <= 0 or height <= 0:
        failures.append("invalid_dimensions")
    elif rules["aspect_ratios"] and _aspect_ratio(width, height) not in rules["aspect_ratios"]:
        failures.append("aspect_ratio")
    if "min_width" in rules and width < rules["min_width"]:
        failures.append("width_below_min")
    if "min_height" in rules and height < rules["min_height"]:
        failures.append("height_below_min")
    if "max_width" in rules and width > rules["max_width"]:
        failures.append("width_above_max")
    if "max_height" in rules and height > rules["max_height"]:
        failures.append("height_above_max")
    if "min_fps" in rules and fps < rules["min_fps"]:
        failures.append("fps_below_min")
    if "max_fps" in rules and fps > rules["max_fps"]:
        failures.append("fps_above_max")
    if "max_duration_seconds" in rules and duration > rules["max_duration_seconds"]:
        failures.append("duration_above_max")
    if "max_bytes" in rules and byte_count > rules["max_bytes"]:
        failures.append("bytes_above_max")

    return {
        "schema": "ai_studio.capture.delivery_constraint_result.v1",
        "constraint_id": constraint["id"],
        "platform": constraint["platform"],
        "surface": constraint["surface"],
        "constraint_hash": canonical_hash(dict(constraint)),
        "coverage": constraint["status"],
        "status": "fail" if failures else "pass",
        "failures": failures,
    }
