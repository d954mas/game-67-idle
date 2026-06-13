#!/usr/bin/env python3
"""Simulate ordinary 67 World progression against the one-hour release target."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


PASSIVE_INTERVAL_SECONDS = 5.0
BOARD_SLOTS = 12
FASTER_SPAWN_COST = 25
BETTER_CRATE_MAX_LEVEL = 28


def better_crate_cost(variants: list[dict], next_level: int) -> int:
    if next_level <= 0 or next_level > BETTER_CRATE_MAX_LEVEL:
        return 0
    passive = int(variants[next_level]["passive_coins_per_5_seconds"])
    if next_level <= 3:
        return passive * 216 + 270
    if next_level <= 10:
        return passive * 378 + next_level * 216
    if next_level <= 20:
        return passive * 560 + next_level * 700
    return passive * 800 + next_level * 1500


def total_on_board(counts: list[int]) -> int:
    return sum(counts)


def passive_income(variants: list[dict], counts: list[int]) -> int:
    return sum(int(variant["passive_coins_per_5_seconds"]) * count for variant, count in zip(variants, counts))


def first_pair(counts: list[int]) -> int | None:
    for index, count in enumerate(counts[:-1]):
        if count >= 2:
            return index
    return None


def run_sim(balance_path: Path, min_minutes: float, target_minutes: float, verbose: bool) -> tuple[bool, dict]:
    data = json.loads(balance_path.read_text(encoding="utf-8"))
    variants = data["variants"]
    counts = [0 for _ in variants]
    discovered = 1
    highest_order = 1
    wallet = int(data.get("currency", {}).get("starting_amount", 0))
    faster_spawn = False
    better_crate_level = 0
    time_seconds = 0.0
    spawn_interval = float(data.get("spawn", {}).get("base_cooldown_seconds", 1.5))
    upgraded_spawn_interval = float(data.get("spawn", {}).get("upgraded_cooldown_seconds", 1.0))
    next_spawn_at = 0.0
    next_passive_at = PASSIVE_INTERVAL_SECONDS
    unlock_times: dict[int, float] = {1: 0.0}
    events: list[str] = []

    def log(text: str) -> None:
        events.append(f"{time_seconds / 60.0:05.2f}m {text}")

    while time_seconds <= target_minutes * 60.0 and discovered < len(variants):
        changed = True
        while changed:
            changed = False
            if not faster_spawn and discovered >= 2 and wallet >= FASTER_SPAWN_COST:
                wallet -= FASTER_SPAWN_COST
                faster_spawn = True
                spawn_interval = upgraded_spawn_interval
                log("buy faster_spawn")
                changed = True
                continue

            next_level = better_crate_level + 1
            cost = better_crate_cost(variants, next_level)
            if faster_spawn and next_level <= BETTER_CRATE_MAX_LEVEL and discovered > next_level and wallet >= cost:
                wallet -= cost
                better_crate_level = next_level
                log(f"buy better_crate_level={better_crate_level} cost={cost}")
                changed = True
                continue

            pair = first_pair(counts)
            if pair is not None:
                counts[pair] -= 2
                counts[pair + 1] += 1
                wallet += 1
                if highest_order < pair + 2:
                    highest_order = pair + 2
                    discovered = max(discovered, highest_order)
                    wallet += int(variants[pair + 1]["first_discovery_bonus"])
                    unlock_times[highest_order] = time_seconds / 60.0
                    log(f"discover {highest_order:02d} {variants[pair + 1]['display_name']}")
                changed = True

            if total_on_board(counts) >= BOARD_SLOTS and first_pair(counts) is None:
                for index, count in enumerate(counts):
                    if count > 0:
                        counts[index] -= 1
                        wallet += int(variants[index]["passive_coins_per_5_seconds"]) * 2
                        log(f"recycle {variants[index]['display_name']}")
                        changed = True
                        break

        if total_on_board(counts) < BOARD_SLOTS and time_seconds >= next_spawn_at:
            spawn_index = min(max(better_crate_level, 0), discovered - 1, BETTER_CRATE_MAX_LEVEL)
            counts[spawn_index] += 1
            next_spawn_at = time_seconds + spawn_interval
            continue

        next_time = min(next_spawn_at, next_passive_at)
        if next_time <= time_seconds:
            next_time = time_seconds + 0.25
        time_seconds = next_time

        if time_seconds >= next_passive_at:
            income = passive_income(variants, counts)
            if income > 0:
                wallet += income
            next_passive_at += PASSIVE_INTERVAL_SECONDS

    minutes = time_seconds / 60.0
    result = {
        "ok": discovered >= len(variants) and min_minutes <= minutes <= target_minutes,
        "minutes": round(minutes, 2),
        "target_window_minutes": [min_minutes, target_minutes],
        "discovered": discovered,
        "wallet": wallet,
        "better_crate_level": better_crate_level,
        "unlock_times": unlock_times,
        "events": events if verbose else events[-40:],
    }
    return bool(result["ok"]), result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--balance", default="gamedesign/meme-evolution/data/balance.json")
    parser.add_argument("--min-minutes", type=float, default=55.0)
    parser.add_argument("--target-minutes", type=float, default=60.0)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    ok, result = run_sim(Path(args.balance), args.min_minutes, args.target_minutes, args.verbose)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
