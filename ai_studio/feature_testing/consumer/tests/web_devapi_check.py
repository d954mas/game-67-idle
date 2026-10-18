#!/usr/bin/env python3
"""Advisory, CI-optional headless probe of the web DevAPI shim round-trip
(closes the T0328 tail: "command.describe answers over window.__devapi").

NOT part of `ctest` -- headless Chrome is capricious across driver versions (see
the repo's web-wasm-headless-verify recipe), so a SKIP here is not a test
failure. It exists so one command yields a real signal that the wasm-devapi-debug
build actually installs the JS transport shim and answers the discovery contract.

It REUSES the scaffolding of web_persistence_check.py (same dir) rather than
duplicating it: the canonical build helper, the stdlib CDP client, the Chrome
launcher, and the static server all come from that module.

Flow:
  1. build_wasm_devapi() -> build/wasm-devapi-debug (Debug+DevAPI; its own
     build/engine dir, no clobber) with the pack copied flat next to index.html.
     configure_file bakes `Module.arguments=['--devapi','17890']` into the shell,
     so main() sees `--devapi` in argv and installs window.__devapi (main.c: the
     web shim installs ONLY when --devapi was parsed).
  2. Serve bin/ and load index.html headless; wait for window.__devapi.ready
     (set SYNCHRONOUSLY at shim install -- proves the transport is live, not that
     the pack loaded; the pack-over-HTTP render is the human G1's job).
  3. `endpoints {}` -> result.commands is non-empty (the command list).
  4. `command.describe {"method":"endpoints"}` -> the 7-field descriptor. Both go
     through window.__devapi.submit (the same transport poll drains) -> the shim
     round-trip is proven. This is literally the card's "command.describe answers".
  5. Cheap secondary signal: scan the browser log for a 404/file:// on the pack
     URL (a regression of the relative-path/copy would show here). Does not
     replace the human G1 render check.

Usage:
    python web_devapi_check.py [--chrome PATH] [--port 8935] [--cdp-port 9334]

Exit codes: 0 = the discovery round-trip answered over the shim (PASS).
            1 = built and booted, but the round-trip did not answer (FAIL).
            2 = a host prerequisite or launch boundary is unavailable (SKIP -- advisory).
"""

import argparse
import json
import os
import shutil
import sys
import tempfile
import time

from web_persistence_check import (
    Cdp,
    CheckFailure,
    Skip,
    build_wasm_devapi,
    cdp_page_ws_url,
    find_chrome,
    launch_chrome,
    quit_chrome,
    serve_dir,
)

BOOT_TIMEOUT_S = 90.0  # wasm-devapi-debug carries ASan -> slow boot (MED-5)

# 7-field self-describing contract of command.describe (nt_devapi_discovery.c).
DESCRIBE_FIELDS = ("method", "group", "summary", "params_shape",
                   "result_shape", "frame_behavior", "side_effects")


def detect_pack_fetch_error(cdp, seconds=2.5):
    """Cheap secondary signal (MED-4): Log.enable replays Chrome's stored log
    entries, so a boot-time pack fetch failure (network-source error) is caught
    even though it fired before we connected. Returns the offending text/url, or
    None. Advisory: a CDP hiccup returns None (never a false positive)."""
    try:
        cdp.call("Log.enable", timeout=10.0)
    except (Skip, CheckFailure):
        return None
    deadline = time.time() + seconds
    while time.time() < deadline:
        try:
            raw = cdp.ws.recv_text(timeout=max(0.1, deadline - time.time()))
        except Exception:
            break
        try:
            msg = json.loads(raw)
        except Exception:
            continue
        if msg.get("method") != "Log.entryAdded":
            continue
        entry = msg.get("params", {}).get("entry", {})
        url = entry.get("url") or ""
        text = entry.get("text") or ""
        if "game.ntpack" not in (url + " " + text):
            continue
        if entry.get("source") == "network" and entry.get("level") == "error":
            return text or url
        if url.startswith("file://"):
            return url
    return None


def run(chrome_path, http_port, cdp_port):
    bin_dir = build_wasm_devapi()  # build/wasm-devapi-debug, pack copied in

    if not os.path.isfile(os.path.join(bin_dir, "index.html")):
        raise CheckFailure(f"{bin_dir} has no index.html shell (configure_file should have delivered it)")

    resolved_chrome = find_chrome(chrome_path)
    if not resolved_chrome:
        raise Skip("no Chrome/Chromium binary found (pass --chrome or set CHROME_PATH)")

    httpd = serve_dir(bin_dir, http_port)
    profile_dir = tempfile.mkdtemp(prefix="game67_web_devapi_")
    url = f"http://127.0.0.1:{http_port}/index.html"
    chrome_proc = None
    cdp = None
    try:
        chrome_proc = launch_chrome(resolved_chrome, url, cdp_port, profile_dir)
        cdp = Cdp(cdp_page_ws_url(cdp_port))
        cdp.wait_for_devapi(timeout=BOOT_TIMEOUT_S)

        # (1) endpoints -> non-empty command list.
        r1 = cdp.devapi_submit("endpoints", {})
        if r1.get("ok") is not True:
            print(f"FAIL: endpoints did not return ok=true: {r1}")
            return 1
        commands = (r1.get("result") or {}).get("commands")
        if not commands:
            print(f"FAIL: endpoints returned no commands: {r1}")
            return 1

        # (2) command.describe {method: endpoints} -> full 7-field descriptor.
        r2 = cdp.devapi_submit("command.describe", {"method": "endpoints"})
        if r2.get("ok") is not True:
            print(f"FAIL: command.describe did not return ok=true: {r2}")
            return 1
        desc = r2.get("result") or {}
        missing = [f for f in DESCRIBE_FIELDS if f not in desc]
        if missing:
            print(f"FAIL: command.describe descriptor missing fields {missing}: {r2}")
            return 1
        if desc.get("method") != "endpoints":
            print(f"FAIL: command.describe returned wrong method {desc.get('method')!r}: {r2}")
            return 1

        # (3) cheap secondary signal: no pack 404/file:// (relative path/copy).
        pack_error = detect_pack_fetch_error(cdp)
        if pack_error:
            print(f"FAIL: pack fetch error in browser log (relative path/copy broke): {pack_error}")
            return 1

        print(f"PASS: endpoints -> {len(commands)} commands; "
              f"command.describe{{method:endpoints}} -> 7-field descriptor, "
              f"both over window.__devapi.submit (shim round-trip proven).")
        return 0
    finally:
        if cdp:
            cdp.close()
        quit_chrome(chrome_proc)
        httpd.shutdown()
        shutil.rmtree(profile_dir, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--chrome", default=None, help="path to chrome/chromium binary")
    parser.add_argument("--port", type=int, default=8935, help="http.server port for bin/ (default: %(default)s)")
    parser.add_argument("--cdp-port", type=int, default=9334, help="Chrome --remote-debugging-port (default: %(default)s)")
    args = parser.parse_args()

    try:
        code = run(args.chrome, args.port, args.cdp_port)
    except CheckFailure as e:
        print(f"FAIL: {e}")
        code = 1
    except Skip as e:
        print(f"SKIP (advisory, not a failure): {e}")
        code = 2
    sys.exit(code)


if __name__ == "__main__":
    main()
