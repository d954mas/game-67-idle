"""Deterministic safe-area mask normalization and policy derivation."""

from __future__ import annotations

import hashlib
import math
from dataclasses import dataclass
from typing import Any, Iterable, Mapping, Sequence

from capture_contracts import CaptureContractError, canonical_hash, validate_document


class SafeAreaError(CaptureContractError):
    def __init__(self, message: str, *, details: Mapping[str, Any] | None = None):
        super().__init__(
            "CONTRACT_MISMATCH",
            message,
            remediation="Normalize every mask to one declared frame domain and polarity.",
            details=details,
        )


def _pixel_bounds(
    width: int, height: int, rect: Sequence[float]
) -> tuple[int, int, int, int]:
    if len(rect) != 4:
        raise SafeAreaError("safe-area rectangle must contain four coordinates")
    x0, y0, x1, y1 = rect
    if not (
        all(isinstance(value, (int, float)) and math.isfinite(value) for value in rect)
        and 0 <= x0 < x1 <= 1
        and 0 <= y0 < y1 <= 1
    ):
        raise SafeAreaError(
            "safe-area rectangle is outside normalized half-open output space",
            details={"rectangle": list(rect)},
        )
    return (
        math.floor(x0 * width),
        math.floor(y0 * height),
        math.ceil(x1 * width),
        math.ceil(y1 * height),
    )


@dataclass(frozen=True)
class UnsafeMask:
    width: int
    height: int
    pixels: bytes

    def __post_init__(self) -> None:
        if self.width <= 0 or self.height <= 0:
            raise SafeAreaError("mask dimensions must be positive")
        if len(self.pixels) != self.width * self.height:
            raise SafeAreaError("mask byte count does not match its dimensions")
        if any(value not in (0, 1) for value in self.pixels):
            raise SafeAreaError("mask pixels must use canonical zero/one polarity")

    @classmethod
    def from_normalized_rectangles(
        cls,
        width: int,
        height: int,
        rectangles: Iterable[Sequence[float]],
    ) -> "UnsafeMask":
        pixels = bytearray(width * height)
        for rect in rectangles:
            x0, y0, x1, y1 = _pixel_bounds(width, height, rect)
            for y in range(y0, y1):
                row = y * width
                for x in range(x0, x1):
                    pixels[row + x] = 1
        return cls(width, height, bytes(pixels))

    def is_unsafe(self, x: int, y: int) -> bool:
        if not (0 <= x < self.width and 0 <= y < self.height):
            raise SafeAreaError("pixel coordinate is outside the mask")
        return bool(self.pixels[y * self.width + x])

    def union(self, other: "UnsafeMask") -> "UnsafeMask":
        if (self.width, self.height) != (other.width, other.height):
            raise SafeAreaError(
                "cannot combine safe-area masks with different domains",
                details={
                    "left": [self.width, self.height],
                    "right": [other.width, other.height],
                },
            )
        return UnsafeMask(
            self.width,
            self.height,
            bytes(left | right for left, right in zip(self.pixels, other.pixels)),
        )

    def contains_critical_rect(self, rect: Sequence[float]) -> bool:
        x0, y0, x1, y1 = _pixel_bounds(self.width, self.height, rect)
        for y in range(y0, y1):
            row = y * self.width
            if any(self.pixels[row + x] for x in range(x0, x1)):
                return False
        return True

    def sha256(self) -> str:
        prefix = f"unsafe-mask-v1:{self.width}x{self.height}:".encode("ascii")
        return hashlib.sha256(prefix + self.pixels).hexdigest()


def _geometry_mask(record: Mapping[str, Any]) -> UnsafeMask:
    dimensions = record.get("normalized_dimensions", {})
    width = dimensions.get("width")
    height = dimensions.get("height")
    if not isinstance(width, int) or not isinstance(height, int):
        raise SafeAreaError("source record has invalid normalized dimensions")
    geometry = record.get("geometry", {})
    if geometry.get("polarity") != "unsafe":
        raise SafeAreaError("source record polarity must be unsafe")
    if geometry.get("kind") != "rectangles":
        raise SafeAreaError(
            "this WP0 implementation accepts normalized rectangle geometry only",
            details={"kind": geometry.get("kind")},
        )
    return UnsafeMask.from_normalized_rectangles(
        width, height, geometry.get("rectangles", [])
    )


def source_record_payload(record: Mapping[str, Any]) -> dict[str, Any]:
    validate_document(dict(record), "safe-area-source.v1.schema.json")
    payload = dict(record)
    payload.pop("source_record_hash", None)
    payload["normalized_geometry_sha256"] = _geometry_mask(record).sha256()
    return payload


def source_record_hash(record: Mapping[str, Any]) -> str:
    computed = canonical_hash(source_record_payload(record))
    supplied = record.get("source_record_hash")
    if supplied is not None and supplied != computed:
        raise SafeAreaError(
            "stored source-record hash does not match its canonical payload",
            details={"stored": supplied, "computed": computed},
        )
    return computed


def _variant_key(record: Mapping[str, Any]) -> tuple[str, ...]:
    return (
        str(record.get("platform", "")),
        str(record.get("surface", "")),
        str(record.get("ui_variant_id", "")),
        str(record.get("caption_variant_id", "")),
        str(record.get("direction", "")),
    )


def _required_key(record: Mapping[str, Any]) -> tuple[str, ...]:
    return (
        str(record["platform"]),
        str(record["surface"]),
        str(record["ui_variant_id"]),
        str(record["caption_variant_id"]),
        str(record["direction"]),
    )


def _evidence_class(record: Mapping[str, Any]) -> str | None:
    source = record.get("source", {})
    rectangles = record.get("geometry", {}).get("rectangles", [])
    caption = record.get("caption_bound", {})
    if (
        source.get("license_review") != "reviewed"
        or not source.get("sha256")
        or not rectangles
        or caption.get("max_visible_lines", 0) <= 0
        or not caption.get("obstruction_geometry_sha256")
    ):
        return None

    placement = record.get("placement_class")
    authority = source.get("authority")
    if placement == "organic_standard" and authority == "platform_official":
        return "official"
    if placement == "measured_organic" and authority == "first_party_measurement":
        return "measured"
    return None


def derive_policy(
    policy_id: str,
    records: Iterable[Mapping[str, Any]],
    required_variants: Sequence[Mapping[str, Any]],
) -> dict[str, Any]:
    if not required_variants:
        raise SafeAreaError("safe-area policy requires at least one variant")
    required_keys = [_required_key(value) for value in required_variants]
    if len(set(required_keys)) != len(required_keys):
        raise SafeAreaError("safe-area policy required variants must be unique")

    by_hash: dict[str, dict[str, Any]] = {}
    for raw in records:
        record = dict(raw)
        identity = source_record_hash(record)
        record["source_record_hash"] = identity
        by_hash.setdefault(identity, record)

    ordered = sorted(
        by_hash.values(),
        key=lambda record: (
            str(record.get("platform", "")),
            str(record.get("surface", "")),
            str(record.get("placement_class", "")),
            str(record.get("ui_variant_id", "")),
            str(record.get("caption_variant_id", "")),
            str(record.get("direction", "")),
            str(record.get("locale", "")),
            str(record["source_record_hash"]),
        ),
    )

    eligible = [record for record in ordered if _evidence_class(record) is not None]
    available_keys = {_variant_key(record) for record in eligible}
    missing = [
        dict(variant)
        for variant, key in zip(required_variants, required_keys)
        if key not in available_keys
    ]

    masks: list[UnsafeMask] = []
    selected: list[dict[str, Any]] = []
    for record in eligible:
        if _variant_key(record) in required_keys:
            masks.append(_geometry_mask(record))
            selected.append(record)

    if masks:
        combined = masks[0]
        for mask in masks[1:]:
            combined = combined.union(mask)
        width, height = combined.width, combined.height
    else:
        width = height = 0
        combined = None

    if missing:
        status = "incomplete"
    elif any(_evidence_class(record) == "measured" for record in selected):
        status = "measured"
    else:
        status = "official"

    payload: dict[str, Any] = {
        "schema": "ai_studio.capture.safe_area_policy.v1",
        "id": policy_id,
        "version": 1,
        "status": status,
        "normalized_dimensions": {"width": width, "height": height},
        "source_record_hashes": [
            record["source_record_hash"] for record in selected
        ],
        "required_variants": [dict(value) for value in required_variants],
        "missing_variants": missing,
        "derived_safe_mask_sha256": combined.sha256() if combined else None,
    }
    payload["policy_hash"] = canonical_hash(payload)
    return payload


def evaluate_critical_regions(
    mask: UnsafeMask,
    regions: Sequence[Mapping[str, Any]],
    *,
    measured_ticks: Iterable[int],
) -> dict[str, Any]:
    """Evaluate half-open authored critical regions against one unsafe mask.

    A result can pass only when evidence covers every tick where a critical
    region exists.  A known unsafe intersection wins over incomplete evidence;
    otherwise incomplete evidence is reported as ``not_measured``.
    """

    required_ticks: set[int] = set()
    violations: list[dict[str, Any]] = []
    for raw in regions:
        region_id = raw.get("id")
        start = raw.get("start_tick")
        end = raw.get("end_tick_exclusive")
        rectangle = raw.get("rectangle")
        if (
            not isinstance(region_id, str)
            or not region_id
            or not isinstance(start, int)
            or isinstance(start, bool)
            or not isinstance(end, int)
            or isinstance(end, bool)
            or start < 0
            or end <= start
            or end - start > 1_000_000
            or not isinstance(rectangle, Sequence)
        ):
            raise SafeAreaError(
                "critical region has an invalid half-open tick interval",
                details={"region_id": region_id},
            )
        # Reuse the exact rectangle validation and conservative rasterization.
        safe = mask.contains_critical_rect(rectangle)
        required_ticks.update(range(start, end))
        if not safe:
            violations.append(
                {
                    "id": region_id,
                    "start_tick": start,
                    "end_tick_exclusive": end,
                }
            )

    measured: set[int] = set()
    for tick in measured_ticks:
        if not isinstance(tick, int) or isinstance(tick, bool) or tick < 0:
            raise SafeAreaError("measured critical-region ticks must be non-negative integers")
        measured.add(tick)
    missing = sorted(required_ticks - measured)
    status = (
        "fail"
        if violations
        else ("not_measured" if not regions or missing else "pass")
    )
    return {
        "schema": "ai_studio.capture.critical_region_result.v1",
        "status": status,
        "required_ticks": sorted(required_ticks),
        "measured_ticks": sorted(required_ticks & measured),
        "missing_ticks": missing,
        "violations": violations,
    }
