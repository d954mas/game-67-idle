#!/usr/bin/env python3
"""Small neutral pure-Python XXH64 owner for Items tooling."""

from __future__ import annotations


MASK64 = (1 << 64) - 1


def _rotl(value: int, count: int) -> int:
    return ((value << count) | (value >> (64 - count))) & MASK64


def xxh64(data: bytes, seed: int = 0) -> int:
    """Pure exact XXH64, matching the engine's seed-0 byte contract."""
    p1 = 11400714785074694791
    p2 = 14029467366897019727
    p3 = 1609587929392839161
    p4 = 9650029242287828579
    p5 = 2870177450012600261

    def round64(acc: int, lane: int) -> int:
        acc = (acc + lane * p2) & MASK64
        acc = _rotl(acc, 31)
        return (acc * p1) & MASK64

    length = len(data)
    offset = 0
    if length >= 32:
        v1 = (seed + p1 + p2) & MASK64
        v2 = (seed + p2) & MASK64
        v3 = seed & MASK64
        v4 = (seed - p1) & MASK64
        limit = length - 32
        while offset <= limit:
            v1 = round64(v1, int.from_bytes(data[offset:offset + 8], "little")); offset += 8
            v2 = round64(v2, int.from_bytes(data[offset:offset + 8], "little")); offset += 8
            v3 = round64(v3, int.from_bytes(data[offset:offset + 8], "little")); offset += 8
            v4 = round64(v4, int.from_bytes(data[offset:offset + 8], "little")); offset += 8
        result = (_rotl(v1, 1) + _rotl(v2, 7) + _rotl(v3, 12) + _rotl(v4, 18)) & MASK64
        for value in (v1, v2, v3, v4):
            result ^= round64(0, value)
            result = (result * p1 + p4) & MASK64
    else:
        result = (seed + p5) & MASK64

    result = (result + length) & MASK64
    while offset + 8 <= length:
        result ^= round64(0, int.from_bytes(data[offset:offset + 8], "little"))
        result = (_rotl(result, 27) * p1 + p4) & MASK64
        offset += 8
    if offset + 4 <= length:
        result ^= (int.from_bytes(data[offset:offset + 4], "little") * p1) & MASK64
        result = (_rotl(result, 23) * p2 + p3) & MASK64
        offset += 4
    while offset < length:
        result ^= (data[offset] * p5) & MASK64
        result = (_rotl(result, 11) * p1) & MASK64
        offset += 1
    result ^= result >> 33
    result = (result * p2) & MASK64
    result ^= result >> 29
    result = (result * p3) & MASK64
    result ^= result >> 32
    return result & MASK64
