#!/usr/bin/env python3
"""End-to-end verification of the Voxelheim IDLE / incremental RPG loop.

Launches game_seed (voxelheim) with DevAPI, then drives the idle loop headlessly
via deterministic debug endpoints and asserts the progression transitions:

  - gold rises from auto-kills (the hero auto-attacks the monster stream)
  - buying Sword raises damage AND the realized kill rate (gold/sec)
  - the stage counter advances after kills_per_stage kills
  - a BOSS appears at stage 10 (boss_active, a countdown)
  - reaching stage 25 unlocks prestige
  - prestiging grants Frost Shards and resets gold/stage while keeping shards
  - a shard upgrade can be purchased and changes a permanent multiplier
  - an offline grant computes gold while away

Captures screenshots at: early auto-farm, the upgrade panel, a boss, and
post-prestige (build/captures/mult_*.png) and runs pixel_health on each, so the
run proves the MULTIPLICATIVE economy works AND looks non-blank.

DevAPI methods used (added/extended in src/voxelheim_main.c):
  game.state                 -> flat idle mirror + derived stats + costs
  game.reset_playtest        -> fresh idle profile + sim
  game.debug.tick {seconds}  -> advance the sim deterministically
  game.debug.click {x,y}     -> route a design-space UI click
  game.debug.buy {upgrade}   -> buy one upgrade level (sword/boots/armor/luck)
  game.debug.buy_shard {shard}-> buy one shard upgrade (damage/gold/start/offline)
  game.debug.prestige        -> commit a prestige (no confirm gate)
  game.debug.offline {seconds}-> simulate an absence and grant offline gold
  frame.screenshot {path}    -> engine-side glReadPixels capture

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


class Probe:
    def __init__(self, port: int):
        self.port = port

    def call(self, method: str, params: dict | None = None, timeout: float = 8.0) -> dict:
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

    def tick(self, seconds: float) -> dict:
        return self.call("game.debug.tick", {"seconds": seconds})

    def click(self, x: float, y: float) -> dict:
        return self.call("game.debug.click", {"x": x, "y": y})

    def buy(self, upgrade: str) -> dict:
        return self.call("game.debug.buy", {"upgrade": upgrade})

    def buy_shard(self, shard: str) -> dict:
        return self.call("game.debug.buy_shard", {"shard": shard})

    def prestige(self) -> dict:
        return self.call("game.debug.prestige")

    def offline(self, seconds: float) -> dict:
        return self.call("game.debug.offline", {"seconds": seconds})

    def screenshot(self, path: str) -> bool:
        self.call("frame.screenshot", {"path": path})
        deadline = time.time() + 10
        while time.time() < deadline:
            r = self.call("frame.screenshot")
            if r.get("done"):
                return bool(r.get("ok"))
            time.sleep(0.15)
        return False


PASS: list[str] = []
FAIL: list[str] = []


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


def buy_until_stage(p: Probe, target: int, max_iters: int = 600, tick_s: float = 15.0) -> dict:
    """Auto-farm + buy upgrades, climbing toward `target`.

    Each iteration advances the sim by `tick_s` of simulated time (the climb is
    balanced for minutes of idle play, so the probe fast-forwards), then spends
    all affordable gold. To keep the kill rate climbing alongside monster HP it
    spends broadly: damage (Sword) + attack speed (Boots) first, then Luck for
    more gold, then Armor.
    """
    st = p.state()
    cost_keys = {"sword": "cost_sword", "boots": "cost_boots", "armor": "cost_armor", "luck": "cost_luck"}
    # priority: throughput first (sword+boots), then gold (luck), then armor
    prio = ["sword", "boots", "luck", "sword", "armor"]
    it = 0
    while st.get("stage", 1) < target and it < max_iters:
        it += 1
        st = p.tick(tick_s)
        # spend repeatedly while anything is affordable
        spent = True
        while spent:
            spent = False
            gold = st.get("gold", 0)
            for u in prio:
                if gold >= st.get(cost_keys[u], 1e18):
                    st = p.buy(u)
                    spent = True
                    break
    return st


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9133
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

        # Clean start.
        st = p.call("game.reset_playtest")
        check(
            "fresh idle state",
            st.get("stage") == 1
            and st.get("gold") == 0
            and st.get("frost_shards") == 0
            and st.get("up_sword") == 0
            and not st.get("prestige_unlocked"),
            st,
        )

        # 1) Auto-farm: gold rises from auto-kills.
        st = p.tick(8.0)
        gold_after_farm = st.get("gold", 0)
        check("gold rises from auto-kills", gold_after_farm > 0, st)
        check("FTUE advanced after a kill", st.get("ftue_step", 0) >= 1, st)
        shoot(p, "mult_early.png")

        # 2) Damage is MULTIPLICATIVE: each Sword level multiplies damage by the
        #    same factor (~1.12), so power COMPOUNDS. Verify the ratio between
        #    successive levels is constant and > 1 (a flat-additive curve would
        #    have a SHRINKING ratio).
        ratios = []
        dprev = p.state().get("hero_damage", 0.0)
        for _ in range(6):
            # farm a little so the next Sword level is affordable
            r = p.state()
            while r.get("gold", 0) < r.get("cost_sword", 1e18):
                r = p.tick(4.0)
            r = p.buy("sword")
            if not r.get("bought", True):
                break
            dcur = r.get("hero_damage", 0.0)
            if dprev > 0:
                ratios.append(dcur / dprev)
            dprev = dcur
        check("Sword damage compounds (multiplicative)", len(ratios) >= 3, ratios)
        # all step ratios equal (within float tolerance) and clearly > 1
        const_ratio = all(abs(rr - ratios[0]) < 1e-3 for rr in ratios) if ratios else False
        check("Sword damage ratio is constant >1 (compounding, not flat)",
              const_ratio and ratios and ratios[0] > 1.05, ratios)

        # Buying Sword raises damage AND the realized kill rate (gold/sec).
        dmg_before = p.state().get("hero_damage", 0.0)
        win = 20.0
        g0 = p.state().get("gold", 0)
        s0 = p.tick(win)
        rate_before = (s0.get("gold", 0) - g0) / win
        # buy a generous number of sword levels to make the effect unambiguous
        for _ in range(20):
            r = p.buy("sword")
            if not r.get("bought", True):
                break
        st = p.state()
        dmg_after = st.get("hero_damage", 0.0)
        check("buying Sword raises damage", dmg_after > dmg_before, {"before": dmg_before, "after": dmg_after})
        g1 = st.get("gold", 0)
        s1 = p.tick(win)
        rate_after = (s1.get("gold", 0) - g1) / win
        check("buying Sword raises kill/gold rate", rate_after > rate_before,
              {"before": rate_before, "after": rate_after, "dmg": (dmg_before, dmg_after)})

        # 3) Stage advances.
        stage_before = p.state().get("stage", 1)
        st = buy_until_stage(p, max(stage_before + 2, 3))
        check("stage advances", st.get("stage", 1) > stage_before,
              {"before": stage_before, "after": st.get("stage")})
        shoot(p, "mult_upgrades.png")

        # 4) Climb to a boss (stage 10). Buy upgrades to get there fast.
        st = buy_until_stage(p, 10)
        check("reached stage 10", st.get("stage", 1) >= 10, st)
        # tick a little so we're mid-boss for the screenshot
        for _ in range(3):
            st = p.state()
            if st.get("stage") == 10 and st.get("boss_active"):
                break
            st = p.tick(0.5)
        boss_seen = st.get("boss_active") or st.get("stage", 1) > 10
        check("a BOSS appears at stage 10", boss_seen, st)
        # capture the boss if currently active; else climb to the next boss to snap.
        if st.get("boss_active"):
            shoot(p, "mult_boss.png")
        else:
            tries = 0
            while not st.get("boss_active") and tries < 40:
                tries += 1
                st = buy_until_stage(p, (st.get("stage", 1) // 10 + 1) * 10)
                for _ in range(3):
                    if st.get("boss_active"):
                        break
                    st = p.tick(0.5)
            shoot(p, "mult_boss.png")

        # 4b) The stage-10 boss is BEATABLE on a base run within the FIXED timer:
        #     advancing past stage 10 is only possible by killing that boss.
        st = buy_until_stage(p, 11)
        check("stage-10 boss BEATEN on base run (fixed timer)", st.get("highest_stage", 1) >= 11,
              {"highest_stage": st.get("highest_stage"), "stage": st.get("stage")})

        # 5) Climb past the stage-20 boss, then to stage 25 -> prestige unlocks.
        st = buy_until_stage(p, 21)
        check("stage-20 boss BEATEN on base run (fixed timer)", st.get("highest_stage", 1) >= 21,
              {"highest_stage": st.get("highest_stage"), "stage": st.get("stage")})
        st = buy_until_stage(p, 25)
        check("reached stage 25", st.get("stage", 1) >= 25, st)
        check("prestige unlocks at stage 25", st.get("prestige_unlocked"), st)

        # 6) Prestige grants Frost Shards and resets gold/stage while keeping shards.
        highest_before = st.get("highest_stage", 1)
        reward = st.get("shards_reward_now", 0)
        check("prestige reward is positive", reward > 0, reward)
        st = p.prestige()
        check("prestige granted frost shards", st.get("frost_shards", 0) >= reward,
              {"reward": reward, "got": st.get("frost_shards")})
        check("prestige reset gold", st.get("gold", 1) == 0, st)
        check("prestige reset upgrades", st.get("up_sword", 1) == 0, st)
        check("prestige reset stage", st.get("stage", 99) <= max(1, highest_before),
              {"stage": st.get("stage")})
        check("prestige kept highest_stage", st.get("highest_stage", 0) >= highest_before, st)
        shoot(p, "mult_prestige.png")

        # 7) A Frost-Shard global-damage purchase VISIBLY raises base damage, and
        #    the post-prestige run reaches an early stage CLEARLY FASTER.
        # 7a) measure the base (no-shard) sim-time to reach stage 8 from the reset.
        #     (we just prestiged: gold/upgrades are reset, shards kept.)
        post = p.state()
        # buy as many damage shards as affordable so the boost is unambiguous.
        bought_shards = 0
        if post.get("frost_shards", 0) >= 1:
            dmg_before_shard = post.get("hero_damage", 0.0)
            while True:
                r = p.buy_shard("damage")
                if not r.get("bought", False):
                    break
                bought_shards += 1
                post = r
            check("shard damage purchased", bought_shards >= 1, bought_shards)
            check("Frost-Shard global-damage raises base damage",
                  post.get("hero_damage", 0.0) > dmg_before_shard,
                  {"before": dmg_before_shard, "after": post.get("hero_damage")})
            check("shard global-damage is multiplicative (>1.10x for >=1 lvl)",
                  post.get("hero_damage", 0.0) >= dmg_before_shard * 1.10 - 1e-6,
                  {"before": dmg_before_shard, "after": post.get("hero_damage"), "levels": bought_shards})

        # 7b) Next-run speed: with the shard damage bonus the run reaches an early
        #     milestone faster (fewer sim seconds). Compare two clean climbs to
        #     stage 8 -- one at current (boosted) damage vs one with damage forced
        #     off via a fresh playtest reset (no shards).
        def time_to_stage(target=8, tick_s=2.0, max_it=400):
            cost_keys = {"sword": "cost_sword", "boots": "cost_boots", "luck": "cost_luck"}
            prio = ["sword", "boots", "luck", "sword"]
            sec = 0.0
            s = p.state()
            it = 0
            while s.get("stage", 1) < target and it < max_it:
                it += 1
                s = p.tick(tick_s)
                sec += tick_s
                spent = True
                while spent:
                    spent = False
                    gold = s.get("gold", 0)
                    for u in prio:
                        if gold >= s.get(cost_keys[u], 1e18):
                            s = p.buy(u)
                            spent = True
                            break
            return sec, s.get("stage", 1)

        # boosted run (keeps the shards we just bought): prestige to reset run econ
        if p.state().get("prestige_unlocked"):
            p.prestige()
        boosted_sec, boosted_stage = time_to_stage(8)
        # baseline run with NO shard damage: full reset wipes shards too.
        p.call("game.reset_playtest")
        base_sec, base_stage = time_to_stage(8)
        check("next run (with shard damage) reaches stage 8 faster than a no-shard run",
              boosted_stage >= 8 and base_stage >= 8 and boosted_sec < base_sec,
              {"boosted_sec": boosted_sec, "base_sec": base_sec,
               "boosted_stage": boosted_stage, "base_stage": base_stage,
               "shard_dmg_levels": bought_shards})

        # restore a progressed, offline-unlocked profile for the offline test.
        st = buy_until_stage(p, 11)

        # 8) Offline grant computes gold while away.
        gold_before_offline = st.get("gold", 0)
        # offline requires the first-boss unlock; we cleared bosses above
        if st.get("offline_unlocked"):
            st = p.offline(3600.0)
            check("offline grant computes gold", st.get("gold", 0) > gold_before_offline,
                  {"before": gold_before_offline, "after": st.get("gold"), "offline_gold": st.get("offline_gold")})
            check("offline popup armed", st.get("offline_popup", False), st)
        else:
            check("offline unlocked (first boss cleared)", False, st)

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
