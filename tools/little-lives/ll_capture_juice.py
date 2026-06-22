"""Capture juice in action: enter build, place an object, capture during the
placement pop (dust + squash-stretch + shake). Usage:
    python tools/little-lives/ll_capture_juice.py <out.png> [delay_frames]
"""
import sys, os
sys.path.insert(0, os.path.join(os.getcwd(), "tools", "devapi"))
from devapi_client import DevApiClient

out = sys.argv[1] if len(sys.argv) > 1 else "tmp/ll_juice.png"
delay = int(sys.argv[2]) if len(sys.argv) > 2 else 5

with DevApiClient(port=9123) as c:
    c.step("game.debug.set_time", {"minutes": 720.0, "pause": False}, wait_frames=2)
    c.key_tap("B", wait_frames=4)      # enter build mode
    # move cursor toward open floor so the pop is clearly visible
    for _ in range(3):
        c.key_tap("K", wait_frames=1)  # nudge +z
    c.key_tap("ENTER", wait_frames=delay)  # place -> dust + pop + shake
    path = c.capture_framebuffer(out)      # capture ASAP (short pop window)
    ok = os.path.exists(path)
    print("screenshot:", path, "exists:", ok,
          "bytes:", os.path.getsize(path) if ok else 0)
