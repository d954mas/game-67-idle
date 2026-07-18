#!/usr/bin/env python3
"""Synchronous Python harness for a game DevAPI."""

from __future__ import annotations

import base64
import binascii
import json
import os
import shutil
import socket
import subprocess
import sys
import time
import atexit
from contextlib import contextmanager
from dataclasses import dataclass
from typing import Any, Iterable

HOST = "127.0.0.1"
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_DEVAPI_PORT = int(os.environ.get("AI_STUDIO_DEVAPI_PORT", os.environ.get("NT_DEVAPI_PORT", "17890")))
NATIVE_DEBUG_EXE = os.environ.get(
    "AI_STUDIO_GAME_EXE",
    os.path.join(ROOT, "templates", "template", "build", "bin", "game.exe"),
)
RUNTIME_AUTOMATION_DIR = os.path.join(ROOT, "ai_studio", "runtime_automation")
if RUNTIME_AUTOMATION_DIR not in sys.path:
    sys.path.insert(0, RUNTIME_AUTOMATION_DIR)
CAPTURE_SCREEN_SCRIPT = os.path.join(RUNTIME_AUTOMATION_DIR, "capture_screen.ps1")
CAPTURE_WINDOW_SCRIPT = os.path.join(RUNTIME_AUTOMATION_DIR, "capture_window.py")
RECORD_SCREEN_SCRIPT = os.path.join(RUNTIME_AUTOMATION_DIR, "record_screen_ffmpeg.ps1")
ACTIVE_RECORDINGS: list["DevApiRecording"] = []
LAUNCH_LOG_DIR = os.path.join(ROOT, "tmp", "ai_studio", "runtime_automation", "logs")


class DevApiError(RuntimeError):
    pass


@dataclass(frozen=True)
class DevApiRequest:
    method: str
    params: dict[str, Any] | None = None


class DevApiClient:
    def __init__(self, port: int = DEFAULT_DEVAPI_PORT, host: str = HOST, timeout: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.file = self.sock.makefile("rwb")
        self.next_request_id = 1
        self.process_id: int | None = None
        self.launch_log_path: str | None = None

    def __enter__(self) -> "DevApiClient":
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        self.close()

    def close(self) -> None:
        try:
            self.file.close()
        finally:
            self.sock.close()

    def launch_log_tail(self, lines: int = 160) -> str:
        return format_launch_log_tail(self.launch_log_path, lines=lines)

    def print_launch_log_tail(self, lines: int = 160) -> None:
        print_launch_log_tail(self.launch_log_path, lines=lines)

    def raw(self, payload: Any) -> Any:
        self.file.write((json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8"))
        self.file.flush()
        line = self.file.readline().decode("utf-8", "replace").strip()
        if not line:
            raise DevApiError("empty response from DevAPI")
        return json.loads(line)

    def request(self, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        request = {
            "request_id": str(self.next_request_id),
            "method": method,
            "params": params or {},
        }
        self.next_request_id += 1
        response = self.raw(request)
        if not isinstance(response, dict):
            raise DevApiError(f"expected object response for {method}, got {type(response).__name__}")
        return response

    def result(self, method: str, params: dict[str, Any] | None = None) -> Any:
        response = self.request(method, params)
        if response.get("ok") is not True:
            raise DevApiError(f"{method} failed: {response.get('error', response)}")
        return response.get("result")

    def batch(self, requests: Iterable[DevApiRequest | tuple[str, dict[str, Any] | None]]) -> list[dict[str, Any]]:
        payload = []
        for item in requests:
            if isinstance(item, DevApiRequest):
                method = item.method
                params = item.params
            else:
                method, params = item
            payload.append({
                "request_id": str(self.next_request_id),
                "method": method,
                "params": params or {},
            })
            self.next_request_id += 1
        response = self.raw(payload)
        if not isinstance(response, list):
            raise DevApiError(f"expected batch response, got {type(response).__name__}")
        return response

    def batch_results(self, requests: Iterable[DevApiRequest | tuple[str, dict[str, Any] | None]]) -> list[Any]:
        responses = self.batch(requests)
        for response in responses:
            if response.get("ok") is not True:
                raise DevApiError(f"batch request failed: {response.get('error', response)}")
        return [response.get("result") for response in responses]

    def wait_frames(self, frames: int = 1) -> dict[str, Any]:
        if frames <= 0:
            return self.result("frame.current")
        remaining = frames
        last: dict[str, Any] = {}
        while remaining > 0:
            chunk = min(remaining, 5)
            last = self.result("frame.wait", {"frames": chunk})
            remaining -= chunk
        return last

    def observe(self) -> dict[str, Any]:
        return self.result("game.state")

    def step(
        self,
        method: str,
        params: dict[str, Any] | None = None,
        wait_frames: int = 1,
        observe: str | None = "frame.current",
    ) -> Any:
        # The engine rejects deferred commands (frame.wait) inside a batch, so step
        # sequentially: act, advance frames, then observe.
        self.result(method, params or {})
        last_frame: Any = None
        if wait_frames > 0:
            last_frame = self.wait_frames(wait_frames)
        if observe is None:
            return last_frame
        return self.result(observe)

    @contextmanager
    def player_gated(self):
        """Wrap ui.click/input.* injection sequences: the player-gate ON->OFF edge clears all
        real pointer slots, so an injected click lands in the always-free slot 0 instead of a
        non-primary slot beside a live mouse."""
        self.result("input.set_player_enabled", {"enabled": False})
        try:
            yield self
        finally:
            self.result("input.set_player_enabled", {"enabled": True})

    def endpoint_methods(self) -> set[str]:
        """Engine `endpoints` returns {commands: [{method, group, ...}]}; flatten to names."""
        listing = self.result("endpoints")
        methods: set[str] = set()
        if isinstance(listing, dict) and isinstance(listing.get("commands"), list):
            for command in listing["commands"]:
                method = command.get("method") if isinstance(command, dict) else command
                if isinstance(method, str):
                    methods.add(method)
            return methods
        if isinstance(listing, list):  # tolerate a bare-list transport
            for command in listing:
                method = command.get("method") if isinstance(command, dict) else command
                if isinstance(method, str):
                    methods.add(method)
        return methods

    def key_tap(self, key: str, hold_frames: int = 1, wait_frames: int = 1, observe: str | None = "frame.current") -> Any:
        return self.step("input.key", {"key": key, "mode": "tap", "hold_frames": hold_frames}, wait_frames=wait_frames, observe=observe)

    def click_ui(self, element_id: str, button: str = "left", wait_frames: int = 1, observe: str | None = "frame.current") -> Any:
        return self.step("ui.click", {"id": element_id, "button": button}, wait_frames=wait_frames, observe=observe)

    def scroll_ui(self, element_id: str, dx: float = 0.0, dy: float = 0.0, wait_frames: int = 1, observe: str | None = "frame.current") -> Any:
        return self.step("ui.scroll", {"id": element_id, "dx": dx, "dy": dy}, wait_frames=wait_frames, observe=observe)

    def gesture(self, kind: str, params: dict[str, Any], wait_frames: int = 1, observe: str | None = "frame.current") -> Any:
        payload = {"type": kind}
        payload.update(params)
        return self.step("input.gesture", payload, wait_frames=wait_frames, observe=observe)

    def capture_screenshot(
        self,
        output: str = "tmp/captures/screenshot.png",
        x: int = 0,
        y: int = 0,
        width: int = 0,
        height: int = 0,
        wait_frames: int = 1,
        audit: bool = False,
    ) -> str:
        if wait_frames > 0:
            self.wait_frames(wait_frames)
        try:
            path = self.capture_framebuffer(output=output, x=x, y=y, width=width, height=height)
        except DevApiError:
            path = run_capture_screenshot(output=output, x=x, y=y, width=width, height=height, process_id=self.process_id)
        if audit:
            self.audit_screenshot(path)
        return path

    def audit_screenshot(self, path: str) -> Any:
        from pixel_health import PixelHealthError, assert_pixel_health
        from png_io import PngError

        try:
            return assert_pixel_health(path)
        except (PixelHealthError, PngError) as exc:
            # PngError = the screenshot did not decode (codec moved to png_io);
            # surface it as a DevApiError like every other audit failure.
            raise DevApiError(str(exc)) from exc

    def capture_framebuffer(
        self,
        output: str = "tmp/captures/screenshot.png",
        x: int = 0,
        y: int = 0,
        width: int = 0,
        height: int = 0,
    ) -> str:
        path = ensure_output_dir(output)
        method = "capture.region" if width > 0 and height > 0 else "capture.frame"
        params = {"x": x, "y": y, "w": width, "h": height} if method == "capture.region" else {}
        response = self.request(method, params)
        if response.get("ok") is True:
            return write_engine_capture_payload_png(response.get("result"), path)
        error = response.get("error") if isinstance(response.get("error"), dict) else {}
        if error.get("code") != "unknown_method":
            raise DevApiError(f"{method} failed: {response.get('error', response)}")

        # Legacy game-owned capture fallback for older templates/prototypes.
        ppm_path = path + ".ppm"
        if os.path.exists(ppm_path):
            os.remove(ppm_path)
        response = self.request("game.capture.framebuffer", {"output": ppm_path})
        if response.get("ok") is not True:
            raise DevApiError(f"game.capture.framebuffer failed: {response.get('error', response)}")
        self.wait_frames(2)
        if not os.path.exists(ppm_path) or os.path.getsize(ppm_path) <= 0:
            raise DevApiError(f"framebuffer capture was not written: {ppm_path}")
        convert_ppm_to_png(ppm_path, path)
        return path

    def record_gameplay(
        self,
        output: str = "tmp/captures/gameplay.mp4",
        seconds: int = 8,
        framerate: int = 30,
        x: int = 0,
        y: int = 0,
        width: int = 0,
        height: int = 0,
        wait_frames: int = 1,
    ) -> str:
        if wait_frames > 0:
            self.wait_frames(wait_frames)
        return run_record_gameplay(output=output, seconds=seconds, framerate=framerate, x=x, y=y, width=width, height=height)

    def start_recording(
        self,
        output: str = "tmp/captures/gameplay.mp4",
        framerate: int = 30,
        x: int = 0,
        y: int = 0,
        width: int = 0,
        height: int = 0,
        max_seconds: int = 60,
        max_megabytes: int = 512,
        wait_frames: int = 1,
    ) -> "DevApiRecording":
        if wait_frames > 0:
            self.wait_frames(wait_frames)
        return start_recording(output=output, framerate=framerate, x=x, y=y, width=width, height=height, max_seconds=max_seconds, max_megabytes=max_megabytes)


@dataclass
class DevApiRecording:
    output: str
    process: subprocess.Popen

    def __enter__(self) -> "DevApiRecording":
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        if exc_type is None:
            self.stop()
            return
        try:
            self.stop()
        except Exception:
            pass

    def stop(self, timeout: float = 5.0) -> str:
        if self.process.poll() is None:
            try:
                if self.process.stdin is not None:
                    self.process.stdin.write(b"q\n")
                    self.process.stdin.flush()
                    self.process.stdin.close()
            except OSError:
                pass
            try:
                self.process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                try:
                    self.process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=timeout)
        if self in ACTIVE_RECORDINGS:
            ACTIVE_RECORDINGS.remove(self)
        if not os.path.exists(self.output) or os.path.getsize(self.output) <= 0:
            raise DevApiError(f"recording was not written: {self.output}")
        return self.output


def stop_active_recordings() -> None:
    for recording in list(ACTIVE_RECORDINGS):
        try:
            recording.stop(timeout=2.0)
        except Exception:
            pass


atexit.register(stop_active_recordings)


def resolve_output_path(output: str) -> str:
    return output if os.path.isabs(output) else os.path.join(ROOT, output)


def ensure_output_dir(output: str) -> str:
    path = resolve_output_path(output)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return path


def make_launch_log_path(port: int) -> str:
    os.makedirs(LAUNCH_LOG_DIR, exist_ok=True)
    stamp = time.strftime("%Y%m%d_%H%M%S")
    return os.path.join(LAUNCH_LOG_DIR, f"native_devapi_{port}_{stamp}_{int((time.time() % 1) * 1000):03d}.log")


def tail_text_file(path: str, lines: int = 160, max_bytes: int = 65536) -> str:
    if not path or not os.path.exists(path):
        return ""
    with open(path, "rb") as handle:
        handle.seek(0, os.SEEK_END)
        size = handle.tell()
        handle.seek(max(0, size - max_bytes))
        data = handle.read()
    text = data.decode("utf-8", "replace")
    return "\n".join(text.splitlines()[-lines:])


def format_launch_log_tail(path: str | None, lines: int = 160) -> str:
    if not path:
        return "launch log: unavailable for reused/existing process"
    tail = tail_text_file(path, lines=lines)
    if not tail:
        return f"launch log: {path}\n<empty>"
    return f"launch log: {path}\n--- tail ---\n{tail}"


def print_launch_log_tail(path: str | None, lines: int = 160) -> None:
    print(format_launch_log_tail(path, lines=lines), file=sys.stderr)


def _ppm_token(data: bytes, offset: int) -> tuple[bytes, int]:
    n = len(data)
    while offset < n and data[offset] in b" \t\r\n":
        offset += 1
    if offset < n and data[offset] == ord("#"):
        while offset < n and data[offset] not in b"\r\n":
            offset += 1
        return _ppm_token(data, offset)
    start = offset
    while offset < n and data[offset] not in b" \t\r\n":
        offset += 1
    return data[start:offset], offset


def convert_ppm_to_png(ppm_path: str, png_path: str) -> None:
    from png_io import write_png_rgb

    with open(ppm_path, "rb") as handle:
        data = handle.read()
    magic, offset = _ppm_token(data, 0)
    if magic != b"P6":
        raise DevApiError(f"unsupported framebuffer capture format: {magic!r}")
    width_b, offset = _ppm_token(data, offset)
    height_b, offset = _ppm_token(data, offset)
    max_b, offset = _ppm_token(data, offset)
    width = int(width_b)
    height = int(height_b)
    if int(max_b) != 255:
        raise DevApiError("unsupported framebuffer capture max value")
    if offset >= len(data) or data[offset] not in b" \t\r\n":
        raise DevApiError("bad framebuffer capture header separator")
    if data[offset : offset + 2] == b"\r\n":
        offset += 2
    else:
        offset += 1
    rgb = data[offset:]
    expected = width * height * 3
    if len(rgb) != expected:
        raise DevApiError(f"bad framebuffer capture size: {len(rgb)} != {expected}")
    write_png_rgb(png_path, width, height, rgb)


def run_powershell_script(script: str, args: list[str]) -> str:
    try:
        completed = subprocess.run(
            ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, *args],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as exc:
        detail = "\n".join(part for part in (exc.stdout, exc.stderr) if part)
        raise DevApiError(f"{os.path.basename(script)} failed: {detail.strip()}") from exc
    return completed.stdout.strip()


def run_capture_screenshot(output: str = "tmp/captures/screenshot.png", x: int = 0, y: int = 0, width: int = 0, height: int = 0, process_id: int | None = None) -> str:
    path = resolve_output_path(output)
    if os.name == "nt" and os.path.exists(CAPTURE_WINDOW_SCRIPT):
        args = [sys.executable, CAPTURE_WINDOW_SCRIPT, "--output", path]
        if process_id is not None:
            args.extend(["--process-id", str(process_id)])
        else:
            args.extend(["--x", str(x), "--y", str(y), "--width", str(width), "--height", str(height)])
        try:
            subprocess.run(args, cwd=ROOT, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as exc:
            detail = "\n".join(part for part in (exc.stdout, exc.stderr) if part)
            raise DevApiError(f"{os.path.basename(CAPTURE_WINDOW_SCRIPT)} failed: {detail.strip()}") from exc
        return path

    args = ["-Output", output, "-X", str(x), "-Y", str(y), "-Width", str(width), "-Height", str(height)]
    if process_id is not None:
        args.extend(["-ProcessId", str(process_id)])
    run_powershell_script(
        CAPTURE_SCREEN_SCRIPT,
        args,
    )
    return path


def run_record_gameplay(
    output: str = "tmp/captures/gameplay.mp4",
    seconds: int = 8,
    framerate: int = 30,
    x: int = 0,
    y: int = 0,
    width: int = 0,
    height: int = 0,
) -> str:
    run_powershell_script(
        RECORD_SCREEN_SCRIPT,
        [
            "-Output",
            output,
            "-Seconds",
            str(seconds),
            "-Framerate",
            str(framerate),
            "-X",
            str(x),
            "-Y",
            str(y),
            "-Width",
            str(width),
            "-Height",
            str(height),
        ],
    )
    return resolve_output_path(output)


def start_recording(
    output: str = "tmp/captures/gameplay.mp4",
    framerate: int = 30,
    x: int = 0,
    y: int = 0,
    width: int = 0,
    height: int = 0,
    max_seconds: int = 60,
    max_megabytes: int = 512,
) -> DevApiRecording:
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        raise DevApiError("ffmpeg is not available in PATH")
    out_path = ensure_output_dir(output)
    if width <= 0 or height <= 0:
        raise DevApiError("start_recording requires explicit x/y/width/height; use record_gameplay for full-screen fixed-duration recording")
    if max_seconds <= 0:
        raise DevApiError("start_recording requires max_seconds > 0")
    if max_megabytes <= 0:
        raise DevApiError("start_recording requires max_megabytes > 0")
    max_bytes = max_megabytes * 1024 * 1024
    process = subprocess.Popen(
        [
            ffmpeg,
            "-y",
            "-f",
            "gdigrab",
            "-framerate",
            str(framerate),
            "-offset_x",
            str(x),
            "-offset_y",
            str(y),
            "-video_size",
            f"{width}x{height}",
            "-i",
            "desktop",
            "-t",
            str(max_seconds),
            "-fs",
            str(max_bytes),
            "-pix_fmt",
            "yuv420p",
            out_path,
        ],
        cwd=ROOT,
        stdin=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    recording = DevApiRecording(output=out_path, process=process)
    ACTIVE_RECORDINGS.append(recording)
    return recording


def write_engine_capture_payload_png(payload: Any, output: str) -> str:
    if not isinstance(payload, dict):
        raise DevApiError("capture payload must be an object")
    if payload.get("format") != "png":
        raise DevApiError(f"unsupported capture format: {payload.get('format')!r}")
    if not isinstance(payload.get("width"), (int, float)) or not isinstance(payload.get("height"), (int, float)):
        raise DevApiError("capture payload missing width/height")
    data = payload.get("data")
    if not isinstance(data, str) or not data:
        raise DevApiError("capture payload missing base64 data")
    try:
        png = base64.b64decode(data.encode("ascii"), validate=True)
    except (UnicodeEncodeError, binascii.Error) as exc:
        raise DevApiError("capture payload has invalid base64 data") from exc
    if not png.startswith(b"\x89PNG\r\n\x1a\n"):
        raise DevApiError("capture payload is not a PNG")
    with open(output, "wb") as handle:
        handle.write(png)
    return output


def pick_free_port(host: str = HOST) -> int:
    """Bind an OS-assigned ephemeral port and return it.

    There is an inherent TOCTOU race between closing this probe socket and the
    caller actually binding that port (another process could grab it first),
    but this is the standard "find a free port" pattern and is good enough to
    stop concurrent launches from colliding on the fixed default port.
    """
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind((host, 0))
        return probe.getsockname()[1]


def resolve_launch_port(port: int | None) -> int:
    """Resolve the DevAPI port for a single running_game() launch.

    Precedence: an explicit `port` argument wins; otherwise an explicit
    AI_STUDIO_DEVAPI_PORT / NT_DEVAPI_PORT env override wins; otherwise pick a
    free ephemeral port so concurrent launches never collide on the fixed
    17890 default (VibeJam ran 8-9 concurrent sessions on the fixed port;
    Windows SO_EXCLUSIVEADDRUSE made every instance past the first bind-fail
    and exit instantly, so probes talked to a wrong/stale instance).
    """
    if port is not None:
        return port
    env_port = os.environ.get("AI_STUDIO_DEVAPI_PORT") or os.environ.get("NT_DEVAPI_PORT")
    if env_port:
        return int(env_port)
    return pick_free_port()


def _dead_child_error(port: int, exit_code: int, launch_log_path: str | None) -> DevApiError:
    hint = (
        f"devapi launch process exited immediately (exit code {exit_code}) before a DevAPI "
        f"connection could be established on port {port}. An instant exit usually means the "
        "DevAPI port bind failed (another instance is already holding this port)."
    )
    return DevApiError(hint + "\n" + format_launch_log_tail(launch_log_path, lines=10))


def connect_existing(
    port: int = DEFAULT_DEVAPI_PORT,
    timeout: float = 8.0,
    *,
    request_timeout: float = 5.0,
    process: subprocess.Popen | None = None,
    launch_log_path: str | None = None,
) -> DevApiClient | None:
    end = time.time() + timeout
    while time.time() < end:
        if process is not None:
            exit_code = process.poll()
            if exit_code is not None:
                raise _dead_child_error(port, exit_code, launch_log_path)
        try:
            client = DevApiClient(port=port, timeout=1.0)
            try:
                client.sock.settimeout(request_timeout)
            except (OSError, TypeError, ValueError):
                client.close()
                raise
            return client
        except OSError:
            time.sleep(0.2)
    return None


def stop_game_process(proc: subprocess.Popen, graceful_timeout: float = 10.0) -> None:
    """Stop the game so main() teardown actually runs (final analytics flush,
    save flush). TerminateProcess skips teardown entirely, so first ask the
    window to close (taskkill without /F posts WM_CLOSE -> the engine exits
    its app loop cleanly), then escalate: terminate -> kill."""
    if proc.poll() is not None:
        return
    if os.name == "nt":
        try:
            subprocess.run(
                ["taskkill", "/PID", str(proc.pid)],
                capture_output=True,
                timeout=5,
                check=False,
            )
            proc.wait(timeout=graceful_timeout)
            return
        except (subprocess.TimeoutExpired, OSError):
            pass  # window gone/hung or taskkill unavailable -> escalate
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


@contextmanager
def running_game(
    port: int | None = None,
    exe: str = NATIVE_DEBUG_EXE,
    cwd: str = ROOT,
    reuse_existing: bool = False,
    fresh_state: bool = True,
    autosave_enabled: bool = False,
    window_size: str | None = None,
):
    # No explicit port: pick a fresh ephemeral port (see resolve_launch_port) so
    # concurrent launches never collide on the fixed default. Everything below
    # uses this single resolved value for both the launch args and the connect.
    port = resolve_launch_port(port)
    proc = None
    client = None
    launch_log_path = None
    launch_log = None
    if reuse_existing:
        client = connect_existing(port=port, timeout=0.5)
    else:
        existing = connect_existing(port=port, timeout=0.1)
        if existing is not None:
            existing.close()
            raise DevApiError(f"devapi port {port} is already in use; stop the old game or pass reuse_existing=True")
    if client is None:
        if not os.path.exists(exe):
            raise DevApiError(f"build native debug first: {exe}")
        args = [exe, "--devapi", str(port)]
        if window_size:
            args.extend(["--window-size", window_size])
        if fresh_state:
            args.append("--fresh-state")
        if not autosave_enabled:
            args.append("--disable-autosave")
        launch_log_path = make_launch_log_path(port)
        launch_log = open(launch_log_path, "w", encoding="utf-8", errors="replace", buffering=1)
        launch_log.write("command: " + " ".join(args) + "\n")
        launch_log.write("cwd: " + cwd + "\n\n")
        launch_log.flush()
        proc = subprocess.Popen(args, cwd=cwd, stdout=launch_log, stderr=subprocess.STDOUT)
        print(f"launch log: {launch_log_path}", file=sys.stderr)
        try:
            client = connect_existing(port=port, process=proc, launch_log_path=launch_log_path)
        except DevApiError:
            # Dead child detected mid-retry (see _dead_child_error): reap the
            # already-exited process and close the log before re-raising the
            # precise error instead of falling through to a generic timeout.
            proc.wait(timeout=3)
            launch_log.close()
            raise
        if client is not None:
            client.process_id = proc.pid
            client.launch_log_path = launch_log_path
    if client is None:
        detail = format_launch_log_tail(launch_log_path, lines=120)
        if proc is not None:
            stop_game_process(proc, graceful_timeout=3.0)
        if launch_log is not None:
            launch_log.close()
        raise DevApiError("no devapi connection\n" + detail)

    try:
        yield client
    except Exception:
        print_launch_log_tail(client.launch_log_path, lines=160)
        raise
    finally:
        client.close()
        if proc is not None:
            stop_game_process(proc)
        if launch_log is not None:
            launch_log.close()
