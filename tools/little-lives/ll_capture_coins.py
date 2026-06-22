"""Capture the work-payout coin burst: send sim 0 to work, wait the shift, then
capture the instant they return (coins + shake fire on at_work true->false).
Usage: python tools/little-lives/ll_capture_coins.py <out.png>
"""
import sys, os
sys.path.insert(0, os.path.join(os.getcwd(), "tools", "devapi"))
from devapi_client import DevApiClient

out = sys.argv[1] if len(sys.argv) > 1 else "tmp/ll_coins.png"

with DevApiClient(port=9123) as c:
    c.step("game.debug.set_time", {"minutes": 720.0, "pause": False}, wait_frames=2)
    c.step("game.action.select", {"sim": 0}, wait_frames=2)
    c.step("game.action.command", {"sim": 0, "work": True}, wait_frames=2)
    # wait until at work
    for _ in range(400):
        c.wait_frames(10)
        st = c.observe().get("state", {})
        if st["sims"][0]["at_work"]:
            break
    # now wait for return; capture the moment at_work flips back to false
    captured = False
    for _ in range(400):
        c.wait_frames(2)
        st = c.observe().get("state", {})
        if not st["sims"][0]["at_work"]:
            c.wait_frames(10)  # let coins arc up to their visible apex
            path = c.capture_framebuffer(out)
            print("returned; money:", st["wallet"]["simoleons"],
                  "screenshot:", path, "bytes:",
                  os.path.getsize(path) if os.path.exists(path) else 0)
            captured = True
            break
    if not captured:
        print("sim never returned in time window")
