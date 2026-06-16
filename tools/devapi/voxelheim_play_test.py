#!/usr/bin/env python3
"""End-to-end verification of the Voxelheim casual-RPG core loop.

Launches game_seed (voxelheim) with DevAPI, then drives the loop headlessly via
deterministic debug endpoints and asserts the progression transitions:

  - tap-to-move advances FTUE (ftue_step >= 1)
  - walking the hero into goblins kills them (enemies_defeated increases)
  - enough kills trigger a LEVEL UP (level increases)
  - clearing all 3 enemies + reaching the keep sets keep_reached / won

Captures screenshots at key beats (start, combat, level-up, victory) via the
game's own glReadPixels endpoint (frame.screenshot) and runs pixel_health on
each, so the run proves the loop works AND looks non-blank.

Usage: voxelheim_play_test.py [port]
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
PIXEL_HEALTH = os.path.join(ROOT, "tools", "devapi", "pixel_health.py")
CAP_DIR = os.path.join(ROOT, "build", "captures")

# Enemy spawn points in DESIGN units (mirror sim_reset in voxelheim_main.c).
DESIGN_W, DESIGN_H = 960.0, 540.0
ENEMIES = [
    (DESIGN_W * 0.40, DESIGN_H * 0.36),
    (DESIGN_W * 0.60, DESIGN_H * 0.46),
    (DESIGN_W * 0.50, DESIGN_H * 0.56),
]
KEEP = (DESIGN_W * 0.50, DESIGN_H * 0.66)


class Probe:
    def __init__(self, port: int):
        self.port = port

    def call(self, method: str, params: dict | None = None, timeout: float = 6.0) -> dict:
        req = {"id": "1", "method": method}
        if params is not None:
            req["params"] = params
        with socket.create_connection(("127.0.0.1", self.port), timeout=timeout) as sock:
            sock.sendall((json.dumps(req) + "\n").encode("utf-8"))
            sock.settimeout(timeout)
            buf = b""
            while b"\n" not in buf:
                chunk = sock.recv(8192)
                if not chunk:
                    break
                buf += chunk
        line = buf.split(b"\n", 1)[0].decode("utf-8")
        resp = json.loads(line)
        if not resp.get("ok", False):
            raise RuntimeError(f"{method} failed: {resp.get('error', resp)}")
        return resp.get("result", {})

    def state(self) -> dict:
        return self.call("game.state")

    def click(self, x: float, y: float) -> dict:
        return self.call("game.debug.click", {"x": x, "y": y})

    def tick(self, seconds: float) -> dict:
        return self.call("game.debug.tick", {"seconds": seconds})

    def screenshot(self, path: str) -> bool:
        self.call("frame.screenshot", {"path": path})
        deadline = time.time() + 10
        while time.time() < deadline:
            r = self.call("frame.screenshot")
            if r.get("done"):
                return bool(r.get("ok"))
            time.sleep(0.15)
        return False


PASS = []
FAIL = []


def check(name: str, cond: bool, detail: object = None) -> bool:
    if cond:
        print(f"PASS {name}")
        PASS.append(name)
    else:
        print(f"FAIL {name}: {detail!r}", file=sys.stderr)
        FAIL.append(name)
    return cond


def pixel_health(path: str) -> bool:
    r = subprocess.run([sys.executable, PIXEL_HEALTH, path], cwd=ROOT)
    return r.returncode == 0


def shoot(p: Probe, name: str) -> str:
    abs_path = os.path.join(CAP_DIR, name)
    if os.path.exists(abs_path):
        os.remove(abs_path)
    ok = p.screenshot(f"build/captures/{name}")
    check(f"screenshot {name} written", ok and os.path.exists(abs_path), abs_path)
    if ok:
        check(f"pixel_health {name}", pixel_health(abs_path), abs_path)
    return abs_path


def walk_to(p: Probe, x: float, y: float, budget: float = 6.0, step: float = 0.25) -> dict:
    """Issue a move order and tick until arrival or the time budget elapses."""
    p.click(x, y)
    elapsed = 0.0
    st = p.state()
    while elapsed < budget:
        st = p.tick(step)
        elapsed += step
        dx = st.get("hero_x", 0.0) - x
        dy = st.get("hero_y", 0.0) - y
        if (dx * dx + dy * dy) < 16.0 * 16.0 and not st.get("downed"):
            break
    return st


def engage(p: Probe, ex: float, ey: float, budget: float = 8.0) -> dict:
    """Walk next to an enemy and tick until a kill registers or budget elapses."""
    start_killed = p.state().get("enemies_defeated", 0)
    p.click(ex, ey)
    elapsed = 0.0
    st = p.state()
    while elapsed < budget:
        # keep nudging toward the enemy in case it drifted
        p.click(ex, ey)
        st = p.tick(0.3)
        elapsed += 0.3
        if st.get("enemies_defeated", 0) > start_killed:
            break
        if st.get("downed"):
            # wait out the respawn, then re-engage
            st = p.tick(1.5)
            elapsed += 1.5
    return st


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9132
    os.makedirs(CAP_DIR, exist_ok=True)
    if not os.path.exists(EXE):
        print(f"FAIL: build native debug first: {EXE}", file=sys.stderr)
        return 1

    proc = subprocess.Popen([EXE, "--devapi", str(port), "--fresh-state", "--disable-autosave"], cwd=ROOT)
    p = Probe(port)
    try:
        # Wait for devapi + atlas ready.
        deadline = time.time() + 25
        ready = False
        while time.time() < deadline:
            try:
                r = p.state()
                if r.get("atlas_ready"):
                    ready = True
                    break
            except OSError:
                pass
            time.sleep(0.3)
        if not check("game/atlas ready", ready):
            return 1

        # Clean start: reset the run so the test is deterministic.
        st = p.call("game.reset_playtest")
        check(
            "fresh run state",
            st.get("level") == 1
            and st.get("hero_hp") == 100
            and st.get("enemies_defeated") == 0
            and st.get("enemies_alive") == 3
            and not st.get("keep_reached"),
            st,
        )
        shoot(p, "voxelheim_start.png")

        # 1) Tap to move advances FTUE step 0 -> 1.
        st = walk_to(p, DESIGN_W * 0.5, DESIGN_H * 0.30, budget=3.0)
        check("tap-to-move advances FTUE", st.get("ftue_step", 0) >= 1, st)

        # 2) Engage enemy #1 -> a kill registers.
        st = engage(p, *ENEMIES[0])
        check("first kill registers", st.get("enemies_defeated", 0) >= 1, st)
        check("FTUE advanced to fight phase", st.get("ftue_step", 0) >= 2, st)
        shoot(p, "voxelheim_combat.png")

        level_before = st.get("level", 1)

        # 3) Engage enemy #2 -> more kills; XP should drive a level up
        #    (20 XP/kill, xp_to_next starts at 60 -> level up on the 3rd kill).
        st = engage(p, *ENEMIES[1])
        check("second kill registers", st.get("enemies_defeated", 0) >= 2, st)

        # 4) Engage enemy #3 -> clears the path and (with 60 XP) levels up.
        st = engage(p, *ENEMIES[2])
        check("all enemies cleared", st.get("enemies_alive", 99) == 0, st)
        check(
            "level up occurred",
            st.get("level", 1) > level_before,
            {"before": level_before, "after": st.get("level")},
        )
        shoot(p, "voxelheim_levelup.png")

        # 5) Walk to the keep portal -> victory. Issue ONE move order toward the
        #    keep center, then tick only (clicking after "won" would replay).
        p.click(KEEP[0], KEEP[1])
        for _ in range(60):
            st = p.tick(0.25)
            if st.get("keep_reached") and st.get("won"):
                break
            # if not yet there and not won, nudge again (but never after a win)
            if not st.get("won"):
                p.click(KEEP[0], KEEP[1])
        check("keep reached", bool(st.get("keep_reached")), st)
        check("victory state", bool(st.get("won")), st)
        shoot(p, "voxelheim_victory.png")

        # 6) Replay via reset clears the win.
        st = p.call("game.reset_playtest")
        check("replay resets run", not st.get("won") and st.get("enemies_alive") == 3, st)

    finally:
        try:
            print("\nfinal state:", json.dumps(p.state(), indent=2))
        except Exception:
            pass
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    print(f"\n=== {len(PASS)} passed, {len(FAIL)} failed ===")
    return 0 if not FAIL else 1


if __name__ == "__main__":
    raise SystemExit(main())
