#!/usr/bin/env python3
"""Launch the game at a given --window-size, drive a few debug ticks so the
scene is populated, request an engine-side screenshot via DevAPI, wait for it.

Usage: shoot_size.py <out_png> <WxH> [port] [--ticks N] [--buys N]
"""
from __future__ import annotations

import json
import os
import socket
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
EXE = os.path.join(ROOT, "build", "game_seed", "native-debug", "game_seed.exe")


def call(port: int, method: str, params: dict | None = None, timeout: float = 5.0) -> dict:
    req = {"id": "1", "method": method}
    if params is not None:
        req["params"] = params
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as sock:
        sock.sendall((json.dumps(req) + "\n").encode("utf-8"))
        sock.settimeout(timeout)
        buf = b""
        while b"\n" not in buf:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
    return json.loads(buf.split(b"\n", 1)[0].decode("utf-8"))


def main() -> int:
    out = sys.argv[1]
    size = sys.argv[2]
    port = int(sys.argv[3]) if len(sys.argv) > 3 and not sys.argv[3].startswith("--") else 9133
    ticks = 0
    buys = 0
    for i, a in enumerate(sys.argv):
        if a == "--ticks":
            ticks = int(sys.argv[i + 1])
        if a == "--buys":
            buys = int(sys.argv[i + 1])
    out_abs = out if os.path.isabs(out) else os.path.join(ROOT, out)
    os.makedirs(os.path.dirname(out_abs), exist_ok=True)
    if os.path.exists(out_abs):
        os.remove(out_abs)

    proc = subprocess.Popen([EXE, "--devapi", str(port), "--window-size", size, "--fresh-state"], cwd=ROOT)
    try:
        deadline = time.time() + 20
        ready = False
        while time.time() < deadline:
            try:
                r = call(port, "game.state")
                if r.get("ok") and r.get("result", {}).get("atlas_ready"):
                    ready = True
                    break
            except OSError:
                pass
            time.sleep(0.3)
        if not ready:
            print("FAIL: not ready", file=sys.stderr)
            return 1

        # populate: advance stages + buy a few upgrades so the panel shows state
        for _ in range(ticks):
            call(port, "game.debug.tick", {"dt": 0.25})
        for _ in range(buys):
            for u in ("sword", "boots", "armor", "luck"):
                call(port, "game.debug.buy", {"upgrade": u})
        for _ in range(8):
            call(port, "game.debug.tick", {"dt": 0.1})

        call(port, "frame.screenshot", {"path": out})
        deadline = time.time() + 10
        ok = False
        while time.time() < deadline:
            r = call(port, "frame.screenshot")
            res = r.get("result", {})
            if res.get("done"):
                ok = res.get("ok")
                break
            time.sleep(0.2)
        print(out_abs if ok else "FAIL screenshot")
        return 0 if ok else 1
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    raise SystemExit(main())
