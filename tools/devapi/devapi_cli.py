#!/usr/bin/env python3
"""Minimal JSON client for the temporary game DevAPI TCP command bus."""

import json
import sys

from devapi_client import DevApiClient

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass


def parse_params(args):
    if not args:
        return {}
    raw = " ".join(args)
    return json.loads(raw)


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    client = DevApiClient(port)
    args = sys.argv[2:]
    if args:
        method = args[0]
        print(json.dumps(client.request(method, parse_params(args[1:])), ensure_ascii=False))
    else:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            if line in ("quit", "exit"):
                break
            payload = json.loads(line)
            print(json.dumps(client.raw(payload), ensure_ascii=False))
    client.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
