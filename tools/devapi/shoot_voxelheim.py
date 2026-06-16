#!/usr/bin/env python3
"""Launch game_seed (voxelheim), request an engine-side screenshot via DevAPI,
wait for it, then run pixel_health. Bypasses OS window-grab (which returns
black on this headless/RDP session) by using the game's own glReadPixels
capture (frame.screenshot endpoint).

Usage: shoot_voxelheim.py [out_png] [port]
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
    line = buf.split(b"\n", 1)[0].decode("utf-8")
    return json.loads(line)


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else "build/captures/voxelheim_p1.png"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 9123
    out_abs = out if os.path.isabs(out) else os.path.join(ROOT, out)
    os.makedirs(os.path.dirname(out_abs), exist_ok=True)
    if os.path.exists(out_abs):
        os.remove(out_abs)

    proc = subprocess.Popen([EXE, "--devapi", str(port)], cwd=ROOT)
    try:
        # Wait for devapi + atlas to come up.
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
            print("FAIL: game/atlas did not become ready", file=sys.stderr)
            return 1

        call(port, "frame.screenshot", {"path": out})
        # Poll until the frame loop writes the file.
        deadline = time.time() + 10
        done = False
        while time.time() < deadline:
            r = call(port, "frame.screenshot")
            res = r.get("result", {})
            if res.get("done"):
                done = True
                ok = res.get("ok")
                break
            time.sleep(0.2)
        if not done or not ok:
            print(f"FAIL: screenshot not completed ok (done={done})", file=sys.stderr)
            return 1
        print(out_abs)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    # Pixel health.
    ph = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "devapi", "pixel_health.py"), out_abs], cwd=ROOT)
    return ph.returncode


if __name__ == "__main__":
    raise SystemExit(main())
