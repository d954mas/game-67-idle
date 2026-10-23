#!/usr/bin/env python3
"""Analyse folded stacks written by native_sampler (root;...;leaf per line).

  stack_fold.py callers <folded> <fn>
  stack_fold.py callees <folded> <fn>
  stack_fold.py paths   <folded> <substring>

Shares are of busy stacks: those whose leaf is not a wait or timer stub.
"""

from __future__ import annotations

import argparse
import collections
import sys

WAIT_MARKERS = ("Wait", "DelayExecution", "QueryPerformanceCounter", "nt_time_now")


def load_busy(path: str) -> tuple[list[list[str]], int]:
    with open(path, encoding="utf-8", errors="replace") as f:
        stacks = [line.rstrip("\n").split(";") for line in f if line.strip()]
    busy = [s for s in stacks if not any(w in s[-1] for w in WAIT_MARKERS)]
    return busy, len(stacks)


def neighbours(busy: list[list[str]], fn: str, step: int) -> collections.Counter:
    """Frame next to fn: step -1 is its caller, +1 its callee."""
    out: collections.Counter = collections.Counter()
    for st in busy:
        if fn not in st:
            continue
        j = st.index(fn) + step
        out[st[j] if 0 <= j < len(st) else ("<root>" if step < 0 else "<self>")] += 1
    return out


def paths_to(busy: list[list[str]], needle: str, depth: int = 3) -> collections.Counter:
    """Group stacks containing needle by the `depth` deepest named frames above it.

    Frames are root-first, so "above" means closer to the root. Unresolved
    addresses (leading "?") carry no name and are skipped.
    """
    out: collections.Counter = collections.Counter()
    for st in busy:
        idx = next((i for i, fr in enumerate(st) if needle in fr), None)
        if idx is None:
            continue
        named = [fr for fr in st[:idx] if not fr.startswith("?")]
        out[" > ".join(named[-depth:]) or "<root>"] += 1
    return out


def render(title: str, counts: collections.Counter, busy_n: int, total: int, limit: int = 25) -> str:
    lines = [f"busy {busy_n} of {total}; {title}"]
    for key, n in counts.most_common(limit):
        lines.append(f"{100 * n / max(busy_n, 1):6.2f}%  {key}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["callers", "callees", "paths"])
    ap.add_argument("folded")
    ap.add_argument("name")
    args = ap.parse_args(argv)
    busy, total = load_busy(args.folded)
    if args.mode == "paths":
        counts = paths_to(busy, args.name)
        share = 100 * sum(counts.values()) / max(len(busy), 1)
        title = f"paths to {args.name} ({share:.2f}% of busy stacks)"
    else:
        counts = neighbours(busy, args.name, -1 if args.mode == "callers" else 1)
        title = f"{args.mode} of {args.name}"
    print(render(title, counts, len(busy), total))
    return 0


if __name__ == "__main__":
    sys.exit(main())
