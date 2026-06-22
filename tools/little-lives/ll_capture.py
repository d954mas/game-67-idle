"""Capture one clean Little Lives frame via DevAPI. Usage:
    python tools/little-lives/ll_capture.py <out.png> [wait_frames] [mode]
mode: live (default) | build. Optionally advances a few frames first so the
scene settles. Prints the output path + byte size for verification.
"""
import sys, os
sys.path.insert(0, os.path.join(os.getcwd(), "tools", "devapi"))
from devapi_client import DevApiClient

out = sys.argv[1] if len(sys.argv) > 1 else "tmp/ll_shot.png"
wait = int(sys.argv[2]) if len(sys.argv) > 2 else 30
mode = sys.argv[3] if len(sys.argv) > 3 else "live"
minutes = float(sys.argv[4]) if len(sys.argv) > 4 else 720.0  # noon by default

with DevApiClient(port=9123) as c:
    if mode == "build":
        c.key_tap("B", wait_frames=3)
    # Pin a consistent time of day so before/after shots are comparable.
    c.step("game.debug.set_time", {"minutes": minutes, "pause": False}, wait_frames=2)
    c.wait_frames(wait)
    st = c.observe().get("state", {})
    path = c.capture_framebuffer(out)
    ok = os.path.exists(path)
    print("mode:", st.get("mode"), "sims:", len(st.get("sims", [])),
          "objects:", len(st.get("objects", [])),
          "money:", st.get("wallet", {}).get("simoleons"))
    print("screenshot:", path, "exists:", ok,
          "bytes:", os.path.getsize(path) if ok else 0)
