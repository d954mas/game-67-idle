#!/usr/bin/env python3
"""Advisory, CI-optional check for build_spec_a1_a3_2026-07-06.md A2.4 item 2:
"does a real localStorage save survive a browser restart, with the APP_ID-scoped
key the new game_storage.c web backend builds?"

This is NOT part of `ctest` -- headless-localStorage automation is genuinely
capricious across Chrome/driver versions (per the spec and the repo's own
`web-wasm-headless-verify` recipe), so a failure here does not fail A2's
acceptance. It exists so a human (or CI) can run one command and get a real
signal instead of "trust me, the EM_JS mirrors rb-dark-rpg".

What this actually checks (deep-review correction: an earlier draft "saved"
whatever the default state already was and never set a distinguishing value,
so a bug that discarded the save silently could still "pass"; it also used
Page.reload, which localStorage survives almost by construction and so barely
exercises real persistence). This version:
  1. emcmake-configure + builds the template's `game` target for wasm, with
     GAME_DEVAPI_ENABLED=ON (DevAPI-web transport is the write/read/set/get
     trigger -- no URL-hash hook exists in the template, and this script must
     not add one).
  2. Serves build/<preset>/bin/ over Python's http.server.
  3. Launches Chrome `--headless=new` with a PERSISTENT --user-data-dir and
     --remote-debugging-port, talks to it over the Chrome DevTools Protocol
     (raw websocket, stdlib only -- no selenium/pychrome dependency).
  4. `game.state.set {path: STATE_PATH, value: STATE_VALUE}` -- writes a known,
     non-default value via DevAPI (STATE_VALUE != the field's schema default,
     so a no-op save could not accidentally "pass").
  5. `game.state.save` -- persists it to localStorage under the APP_ID-scoped
     key game_storage.c builds for the FIXED autosave slot (the DevAPI
     save/load handlers ignore any `key` param; there is no per-slot
     selection -- see game_save_devapi.c ep_state_save/ep_state_load).
  6. Fully QUITS Chrome (not Page.reload -- a full process exit + restart is
     the actual "does it survive" moment; a reload's in-memory localStorage
     object typically never even round-trips through disk).
  7. Relaunches Chrome with the SAME --user-data-dir (same profile -> same
     localStorage), reconnects DevTools.
  8. `game.state.load` then `game.state.get {path: STATE_PATH}`
     and compares the result against STATE_VALUE.

If `game.state.set`/`game.state.get` are ever removed from the generated
DevAPI, this script has nothing honest left to check and must be rewritten
(not patched to fall back on silently comparing default state).

Usage:
    python web_persistence_check.py [--build-dir build/wasm-devapi-debug]
                                     [--chrome PATH] [--port 8934] [--cdp-port 9333]
                                     [--keep-profile]

Exit codes: 0 = the value round-tripped through a full browser restart (PASS).
            1 = built and ran, but the value did NOT round-trip (FAIL).
            2 = prerequisite missing or a build/automation step could not run
                at all (SKIPPED -- advisory, not a test failure).
"""

import argparse
import base64
import http.client
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATE_DIR = os.path.dirname(TESTS_DIR)
# game.state.set routes the FIRST path segment to the save-fragment id
# (GAME_STATE_FRAGMENT_ID="game"); the rest is the field within it. hero.gold is
# an int field, schema default 0, range [0, 999999] (see
# templates/template/state/game_state.schema.json) -- 424242 is unambiguous proof
# the SAVED value (not the default) survived the restart.
STATE_PATH = "game.hero.gold"
STATE_VALUE = 424242


# ---- prerequisite discovery -------------------------------------------------

def find_emscripten_toolchain_file():
    emsdk = os.environ.get("EMSDK")
    if not emsdk:
        return None
    path = os.path.join(emsdk, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake")
    return path if os.path.isfile(path) else None


def find_chrome(explicit):
    candidates = [
        explicit,
        os.environ.get("CHROME_PATH"),
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
        "/usr/bin/google-chrome",
        "/usr/bin/google-chrome-stable",
        "/usr/bin/chromium-browser",
        "/usr/bin/chromium",
    ]
    for c in candidates:
        if c and os.path.isfile(c):
            return c
    return None


class Skip(Exception):
    """Prereq/build/automation step could not run at all -- exit(2), not a failure."""


# ---- minimal RFC6455 websocket client (text frames only; enough for CDP) ---
# Stdlib-only by design (memory: web-wasm-headless-verify prefers no puppeteer/
# selenium deps): a handshake + masked-send + unmasked-recv is ~40 lines.

class MiniWebSocket:
    def __init__(self, ws_url):
        assert ws_url.startswith("ws://"), f"unsupported ws url: {ws_url}"
        rest = ws_url[len("ws://"):]
        host_port, _, path = rest.partition("/")
        host, _, port = host_port.partition(":")
        self.host = host
        self.port = int(port) if port else 80
        self.path = "/" + path
        self.sock = socket.create_connection((self.host, self.port), timeout=10)
        self._buf = b""
        self._handshake()

    def _handshake(self):
        key = base64.b64encode(os.urandom(16)).decode()
        req = (
            f"GET {self.path} HTTP/1.1\r\n"
            f"Host: {self.host}:{self.port}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        )
        self.sock.sendall(req.encode())
        resp = b""
        while b"\r\n\r\n" not in resp:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("CDP websocket handshake closed early")
            resp += chunk
        status_line = resp.split(b"\r\n", 1)[0]
        if b"101" not in status_line:
            raise ConnectionError(f"CDP websocket handshake rejected: {status_line!r}")

    def send_text(self, text):
        payload = text.encode("utf-8")
        mask = os.urandom(4)
        masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        header = bytearray([0x81])  # FIN + text opcode
        length = len(payload)
        if length < 126:
            header.append(0x80 | length)
        elif length < 65536:
            header.append(0x80 | 126)
            header += struct.pack(">H", length)
        else:
            header.append(0x80 | 127)
            header += struct.pack(">Q", length)
        header += mask
        self.sock.sendall(bytes(header) + masked)

    def _read_exact(self, n):
        while len(self._buf) < n:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("CDP websocket closed mid-frame")
            self._buf += chunk
        data, self._buf = self._buf[:n], self._buf[n:]
        return data

    def recv_text(self, timeout=20.0):
        self.sock.settimeout(timeout)
        while True:
            hdr = self._read_exact(2)
            opcode = hdr[0] & 0x0F
            length = hdr[1] & 0x7F
            if length == 126:
                length = struct.unpack(">H", self._read_exact(2))[0]
            elif length == 127:
                length = struct.unpack(">Q", self._read_exact(8))[0]
            payload = self._read_exact(length)
            if opcode == 0x1:  # text frame; ignore ping/pong/close (0x9/0xA/0x8)
                return payload.decode("utf-8")

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


# ---- Chrome DevTools Protocol helpers --------------------------------------

def cdp_page_ws_url(cdp_port, timeout=15.0):
    deadline = time.time() + timeout
    last_err = None
    while time.time() < deadline:
        try:
            conn = http.client.HTTPConnection("127.0.0.1", cdp_port, timeout=2)
            conn.request("GET", "/json/list")
            targets = json.loads(conn.getresponse().read())
            conn.close()
            page = next((t for t in targets if t.get("type") == "page"), None)
            if page and page.get("webSocketDebuggerUrl"):
                return page["webSocketDebuggerUrl"]
        except OSError as e:
            last_err = e
        time.sleep(0.5)
    raise Skip(f"chrome devtools not reachable on port {cdp_port}: {last_err}")


class Cdp:
    def __init__(self, ws_url):
        self.ws = MiniWebSocket(ws_url)
        self._next_id = 1
        self._next_devapi_request_id = 1

    def call(self, method, params=None, timeout=20.0):
        msg_id = self._next_id
        self._next_id += 1
        self.ws.send_text(json.dumps({"id": msg_id, "method": method, "params": params or {}}))
        deadline = time.time() + timeout
        while time.time() < deadline:
            msg = json.loads(self.ws.recv_text(timeout=max(0.1, deadline - time.time())))
            if msg.get("id") == msg_id:
                return msg
        raise Skip(f"CDP call {method} timed out")

    def eval_js(self, expression, timeout=20.0):
        msg = self.call("Runtime.evaluate", {
            "expression": expression, "returnByValue": True, "awaitPromise": False,
        }, timeout=timeout)
        result = msg.get("result", {})
        if "exceptionDetails" in result:
            raise Skip(f"JS exception evaluating {expression!r}: {result['exceptionDetails']}")
        return result.get("result", {}).get("value")

    def wait_for_devapi(self, timeout=90.0):
        # Debug wasm carries ASan and boots slowly -> generous default (MED-5).
        deadline = time.time() + timeout
        while time.time() < deadline:
            ready = self.eval_js("(typeof window.__devapi !== 'undefined' && window.__devapi.ready) ? 1 : 0")
            if ready:
                return
            time.sleep(0.25)
        raise Skip("window.__devapi never became ready (wasm module did not finish booting)")

    def devapi_submit(self, method, params):
        request_id = self._next_devapi_request_id
        self._next_devapi_request_id += 1
        line = json.dumps({"method": method, "request_id": request_id, "params": params})
        raw = self.eval_js(f"window.__devapi.submit({json.dumps(line)})")
        if not raw:
            raise Skip(f"devapi submit({method}) returned no response")
        return json.loads(raw)

    def close(self):
        self.ws.close()


# ---- build + serve ----------------------------------------------------------

def ensure_web_pack(wasm_bin_dir):
    """Copy the native-built pack flat next to index.html. The pack builder is
    native-only, so the wasm build never produces game.ntpack; the engine streams
    it over HTTP relative to the page URL (GAME_ASSET_PACK_PATH="assets/game.ntpack"
    on web). Mirrors tools/build_web.sh step 3."""
    native_dir = os.path.join(TEMPLATE_DIR, "build", "native-debug")
    if not os.path.isfile(os.path.join(native_dir, "CMakeCache.txt")):
        configure = [
            "cmake", "-S", TEMPLATE_DIR, "-B", native_dir, "-G", "Ninja",
            "-DCMAKE_C_COMPILER=clang", "-DCMAKE_BUILD_TYPE=Debug",
        ]
        print("+ " + " ".join(configure))
        if subprocess.run(configure, cwd=TEMPLATE_DIR).returncode != 0:
            raise Skip("native configure (for pack) failed")
    build = ["cmake", "--build", native_dir, "--target", "game_asset_packs"]
    print("+ " + " ".join(build))
    if subprocess.run(build, cwd=TEMPLATE_DIR).returncode != 0:
        raise Skip("native pack build failed")
    src_pack = os.path.join(native_dir, "bin", "assets", "game.ntpack")
    if not os.path.isfile(src_pack):
        raise Skip(f"native pack not found at {src_pack}")
    dst_assets = os.path.join(wasm_bin_dir, "assets")
    os.makedirs(dst_assets, exist_ok=True)
    shutil.copyfile(src_pack, os.path.join(dst_assets, "game.ntpack"))


def build_wasm_devapi(build_dir="build/wasm-devapi-debug"):
    """Canonical wasm-devapi build helper (shared with web_devapi_check.py).

    Debug+DevAPI -> preset wasm-devapi-debug -> its OWN build/engine dir, so it
    never clobbers the clean human build/engine/wasm-release (Release+DevAPI
    likewise resolves to its own preset name, wasm-devapi-release, so no
    combination can collide). Debug wasm links thanks to the shared sanitizer
    flags on the game target. Returns the bin dir with the pack copied in."""
    toolchain = find_emscripten_toolchain_file()
    if not toolchain:
        raise Skip("EMSDK env var not set or Emscripten.cmake toolchain file not found")
    abs_build_dir = os.path.join(TEMPLATE_DIR, build_dir)
    configure = [
        "cmake", "-S", TEMPLATE_DIR, "-B", abs_build_dir, "-G", "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        "-DCMAKE_BUILD_TYPE=Debug",   # preset wasm-devapi-debug (own engine dir; no clobber)
        "-DGAME_DEVAPI_ENABLED=ON",
    ]
    print("+ " + " ".join(configure))
    r = subprocess.run(configure, cwd=TEMPLATE_DIR)
    if r.returncode != 0:
        raise Skip(f"cmake configure failed (exit {r.returncode})")
    build = ["cmake", "--build", abs_build_dir, "--target", "game"]
    print("+ " + " ".join(build))
    r = subprocess.run(build, cwd=TEMPLATE_DIR)
    if r.returncode != 0:
        raise Skip(f"wasm build failed (exit {r.returncode}) -- see build log above")
    bin_dir = os.path.join(abs_build_dir, "bin")
    if not os.path.isfile(os.path.join(bin_dir, "game.js")):
        raise Skip(f"build reported success but no game.js under {bin_dir}")
    ensure_web_pack(bin_dir)
    return bin_dir


def serve_dir(directory, port):
    handler = lambda *a, **kw: SimpleHTTPRequestHandler(*a, directory=directory, **kw)
    httpd = ThreadingHTTPServer(("127.0.0.1", port), handler)
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    return httpd


def launch_chrome(chrome_path, url, cdp_port, user_data_dir):
    args = [
        chrome_path,
        "--headless=new",
        f"--remote-debugging-port={cdp_port}",
        f"--user-data-dir={user_data_dir}",
        "--no-first-run", "--no-default-browser-check",
        "--disable-gpu",
        "--use-gl=swiftshader",
        url,
    ]
    print("+ " + " ".join(args))
    return subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def quit_chrome(chrome_proc, cdp=None, timeout=10.0):
    """Full process exit (not Page.reload/close-tab): the whole point is to prove
    the value is on disk in the profile dir, not merely alive in this process's
    in-memory localStorage object.

    MUST ask Chrome to close itself via CDP `Browser.close` first (verified by
    hand against a plain localStorage page during A2 deep-review): on Windows,
    subprocess.terminate() is TerminateProcess -- an unconditional kill with no
    chance for Chrome's storage backend to flush -- and silently drops a
    setItem() made moments earlier, which would make this script report FAIL
    for a persistence layer that is actually fine. `Browser.close` triggers
    Chrome's own shutdown path (flushes then exits on its own); the hard
    terminate()/kill() below is only a fallback for a hung process.
    """
    if not chrome_proc:
        return
    if cdp:
        try:
            cdp.call("Browser.close", timeout=5.0)
        except Exception:
            pass  # fall through to hard-terminate below
        deadline = time.time() + timeout
        while chrome_proc.poll() is None and time.time() < deadline:
            time.sleep(0.1)
        if chrome_proc.poll() is not None:
            return
    chrome_proc.terminate()
    try:
        chrome_proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        chrome_proc.kill()
        chrome_proc.wait(timeout=timeout)


# ---- main flow ---------------------------------------------------------------

def run(build_dir, chrome_path, http_port, cdp_port, keep_profile):
    bin_dir = build_wasm_devapi(build_dir)

    html_name = "index.html"
    if not os.path.isfile(os.path.join(bin_dir, html_name)):
        raise Skip(f"{bin_dir} has no index.html shell (configure_file should have delivered it)")

    resolved_chrome = find_chrome(chrome_path)
    if not resolved_chrome:
        raise Skip("no Chrome/Chromium binary found (pass --chrome or set CHROME_PATH)")

    httpd = serve_dir(bin_dir, http_port)
    profile_dir = tempfile.mkdtemp(prefix="game67_web_persistence_")
    url = f"http://127.0.0.1:{http_port}/{html_name}"
    chrome_proc = None
    cdp = None
    try:
        # ---- session 1: set a known value, save it, then fully quit ----
        chrome_proc = launch_chrome(resolved_chrome, url, cdp_port, profile_dir)
        cdp = Cdp(cdp_page_ws_url(cdp_port))
        cdp.wait_for_devapi()

        set_resp = cdp.devapi_submit("game.state.set", {"path": STATE_PATH, "value": STATE_VALUE})
        if not set_resp.get("ok"):
            raise Skip(f"game.state.set failed before we could test persistence: {set_resp}")

        save_resp = cdp.devapi_submit("game.state.save", {})
        if not save_resp.get("ok"):
            raise Skip(f"game.state.save failed before we could test persistence: {save_resp}")

        print(f"Set {STATE_PATH}={STATE_VALUE} and saved (fixed autosave slot); quitting Chrome entirely...")
        quit_chrome(chrome_proc, cdp=cdp)
        cdp = None
        chrome_proc = None

        # ---- session 2: brand-new Chrome process, SAME profile dir ----
        chrome_proc = launch_chrome(resolved_chrome, url, cdp_port, profile_dir)
        cdp = Cdp(cdp_page_ws_url(cdp_port))
        cdp.wait_for_devapi()

        load_resp = cdp.devapi_submit("game.state.load", {})
        if not load_resp.get("ok"):
            print(f"FAIL: game.state.load after restart returned: {load_resp}")
            return 1

        get_resp = cdp.devapi_submit("game.state.get", {"path": STATE_PATH})
        if not get_resp.get("ok"):
            print(f"FAIL: game.state.get after restart returned: {get_resp}")
            return 1

        got_value = get_resp.get("result", {}).get("value")
        if got_value is None or float(got_value) != float(STATE_VALUE):
            print(f"FAIL: expected {STATE_PATH}=={STATE_VALUE} after restart, got {got_value!r}")
            return 1

        print(f"PASS: {STATE_PATH}=={got_value} survived a full Chrome quit+restart under the same profile dir.")
        return 0
    finally:
        if cdp:
            cdp.close()
        quit_chrome(chrome_proc)
        httpd.shutdown()
        if not keep_profile:
            shutil.rmtree(profile_dir, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default="build/wasm-devapi-debug",
                         help="CMake binary dir, relative to templates/template/ (default: %(default)s)")
    parser.add_argument("--chrome", default=None, help="path to chrome/chromium binary")
    parser.add_argument("--port", type=int, default=8934, help="http.server port for bin/ (default: %(default)s)")
    parser.add_argument("--cdp-port", type=int, default=9333, help="Chrome --remote-debugging-port (default: %(default)s)")
    parser.add_argument("--keep-profile", action="store_true", help="keep the temporary Chrome --user-data-dir for inspection")
    args = parser.parse_args()

    try:
        code = run(args.build_dir, args.chrome, args.port, args.cdp_port, args.keep_profile)
    except Skip as e:
        print(f"SKIP (advisory, not a failure): {e}")
        code = 2
    sys.exit(code)


if __name__ == "__main__":
    main()
