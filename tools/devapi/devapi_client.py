#!/usr/bin/env python3
"""Synchronous Python harness for the temporary game DevAPI."""

from __future__ import annotations

import json
import os
import shutil
import socket
import subprocess
import time
import atexit
from contextlib import contextmanager
from dataclasses import dataclass
from typing import Any, Iterable

HOST = "127.0.0.1"
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NATIVE_DEBUG_EXE = os.path.join(ROOT, "build", "game_67_idle", "native-debug", "game_67_idle.exe")
CAPTURE_SCREEN_SCRIPT = os.path.join(ROOT, "tools", "devapi", "capture_screen.ps1")
RECORD_SCREEN_SCRIPT = os.path.join(ROOT, "tools", "devapi", "record_screen_ffmpeg.ps1")
ACTIVE_RECORDINGS: list["DevApiRecording"] = []


class DevApiError(RuntimeError):
    pass


@dataclass(frozen=True)
class DevApiRequest:
    method: str
    params: dict[str, Any] | None = None


class DevApiClient:
    def __init__(self, port: int = 9123, host: str = HOST, timeout: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.file = self.sock.makefile("rwb", buffering=0)
        self.next_request_id = 1

    def close(self) -> None:
        try:
            self.file.close()
        finally:
            self.sock.close()

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
        return self.result("frame.wait", {"frames": frames})

    def observe(self) -> dict[str, Any]:
        return self.result("game.state")

    def step(self, method: str, params: dict[str, Any] | None = None, wait_frames: int = 1, observe: str = "game.state") -> dict[str, Any]:
        results = self.batch_results([
            (method, params or {}),
            ("frame.wait", {"frames": wait_frames}),
            (observe, {}),
        ])
        return results[-1]

    def key_tap(self, key: str, hold_frames: int = 1, wait_frames: int = 1) -> dict[str, Any]:
        return self.step("input.key", {"key": key, "mode": "tap", "hold_frames": hold_frames}, wait_frames=wait_frames)

    def click_ui(self, element_id: str, button: str = "left", wait_frames: int = 1) -> dict[str, Any]:
        return self.step("ui.click", {"id": element_id, "button": button}, wait_frames=wait_frames)

    def scroll_ui(self, element_id: str, dx: float = 0.0, dy: float = 0.0, wait_frames: int = 1) -> dict[str, Any]:
        return self.step("ui.scroll", {"id": element_id, "dx": dx, "dy": dy}, wait_frames=wait_frames)

    def gesture(self, kind: str, params: dict[str, Any], wait_frames: int = 1) -> dict[str, Any]:
        payload = {"type": kind}
        payload.update(params)
        return self.step("input.gesture", payload, wait_frames=wait_frames)

    def capture_screenshot(
        self,
        output: str = "build/captures/screenshot.png",
        x: int = 0,
        y: int = 0,
        width: int = 0,
        height: int = 0,
        wait_frames: int = 1,
    ) -> str:
        if wait_frames > 0:
            self.wait_frames(wait_frames)
        return run_capture_screenshot(output=output, x=x, y=y, width=width, height=height)

    def record_gameplay(
        self,
        output: str = "build/captures/gameplay.mp4",
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
        output: str = "build/captures/gameplay.mp4",
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


def run_capture_screenshot(output: str = "build/captures/screenshot.png", x: int = 0, y: int = 0, width: int = 0, height: int = 0) -> str:
    run_powershell_script(
        CAPTURE_SCREEN_SCRIPT,
        ["-Output", output, "-X", str(x), "-Y", str(y), "-Width", str(width), "-Height", str(height)],
    )
    return resolve_output_path(output)


def run_record_gameplay(
    output: str = "build/captures/gameplay.mp4",
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
    output: str = "build/captures/gameplay.mp4",
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


def connect_existing(port: int = 9123, timeout: float = 8.0) -> DevApiClient | None:
    end = time.time() + timeout
    while time.time() < end:
        try:
            return DevApiClient(port=port, timeout=1.0)
        except OSError:
            time.sleep(0.2)
    return None


@contextmanager
def running_game(port: int = 9123, exe: str = NATIVE_DEBUG_EXE, reuse_existing: bool = False):
    proc = None
    client = None
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
        proc = subprocess.Popen([exe, "--devapi", str(port)], cwd=ROOT)
        client = connect_existing(port=port)
    if client is None:
        raise DevApiError("no devapi connection")

    try:
        yield client
    finally:
        client.close()
        if proc is not None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
