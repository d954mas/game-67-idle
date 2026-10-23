#!/usr/bin/env python3
"""Capture a Chrome trace of the GPU process and summarize CrGpuMain load.

  chrome_gpu_trace.py --port 9222 --seconds 5 --out trace.json
  chrome_gpu_trace.py --summarize trace.json

Attaches to a Chrome started with --remote-debugging-port; it never launches one.
"""

from __future__ import annotations

import argparse
import base64
import collections
import http.client
import json
import os
import socket
import struct
import sys
import time

GPU_THREAD = "CrGpuMain"
# One DXGI Present per frame; SwapBuffers wraps it, so counting both doubles the frames.
PRESENT_EVENT = "DXGISwapChainImageBacking::Present"
PRESENT_FALLBACK_EVENT = "SkiaOutputSurfaceImplOnGpu::SwapBuffers"
TRACE_CATEGORIES = ["toplevel", "gpu", "viz"]


class MiniWebSocket:
    """Client-only text websocket; enough for CDP on localhost."""

    def __init__(self, ws_url: str):
        if not ws_url.startswith("ws://"):
            raise ValueError(f"unsupported ws url: {ws_url}")
        host_port, _, path = ws_url[len("ws://"):].partition("/")
        host, _, port = host_port.partition(":")
        self.sock = socket.create_connection((host, int(port) if port else 80), timeout=10)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall((
            f"GET /{path} HTTP/1.1\r\nHost: {host_port}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        ).encode())
        resp = b""
        while b"\r\n\r\n" not in resp:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("CDP websocket handshake closed early")
            resp += chunk
        status = resp.split(b"\r\n", 1)[0]
        if b"101" not in status:
            raise ConnectionError(f"CDP websocket handshake rejected: {status!r}")
        # Bytes after the header already belong to the first frame.
        self._buf = resp.split(b"\r\n\r\n", 1)[1]

    def send_text(self, text: str) -> None:
        self._send_frame(0x1, text.encode("utf-8"))

    def _send_frame(self, opcode: int, payload: bytes) -> None:
        mask = os.urandom(4)
        header = bytearray([0x80 | opcode])
        n = len(payload)
        if n < 126:
            header.append(0x80 | n)
        elif n < 65536:
            header += bytes([0x80 | 126]) + struct.pack(">H", n)
        else:
            header += bytes([0x80 | 127]) + struct.pack(">Q", n)
        masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        self.sock.sendall(bytes(header) + mask + masked)

    def _read_exact(self, n: int) -> bytes:
        while len(self._buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("CDP websocket closed mid-frame")
            self._buf += chunk
        data, self._buf = self._buf[:n], self._buf[n:]
        return data

    def recv_text(self, timeout: float = 20.0) -> str:
        self.sock.settimeout(timeout)
        message = b""
        while True:
            hdr = self._read_exact(2)
            fin, opcode, length = hdr[0] & 0x80, hdr[0] & 0x0F, hdr[1] & 0x7F
            if length == 126:
                length = struct.unpack(">H", self._read_exact(2))[0]
            elif length == 127:
                length = struct.unpack(">Q", self._read_exact(8))[0]
            payload = self._read_exact(length)
            if opcode in (0x0, 0x1):
                message += payload
                if fin:
                    return message.decode("utf-8")
            elif opcode == 0x9:
                self._send_frame(0xA, payload)
            elif opcode == 0x8:
                raise ConnectionError(f"CDP websocket closed by Chrome (payload {payload!r})")

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass


class Cdp:
    def __init__(self, ws_url: str):
        self.ws = MiniWebSocket(ws_url)
        self._next_id = 1

    def send(self, method: str, params: dict | None = None) -> int:
        msg_id = self._next_id
        self._next_id += 1
        self.ws.send_text(json.dumps({"id": msg_id, "method": method, "params": params or {}}))
        return msg_id

    def call(self, method: str, params: dict | None = None, timeout: float = 20.0) -> dict:
        msg_id = self.send(method, params)
        deadline = time.time() + timeout
        while time.time() < deadline:
            msg = json.loads(self.ws.recv_text(timeout=max(0.1, deadline - time.time())))
            if msg.get("id") == msg_id:
                if "error" in msg:
                    raise RuntimeError(f"CDP {method} failed: {msg['error']}")
                return msg.get("result", {})
        raise TimeoutError(f"CDP call {method} timed out")

    def wait_event(self, method: str, timeout: float) -> dict:
        deadline = time.time() + timeout
        while time.time() < deadline:
            msg = json.loads(self.ws.recv_text(timeout=max(0.1, deadline - time.time())))
            if msg.get("method") == method:
                return msg.get("params", {})
        raise TimeoutError(f"CDP event {method} timed out")


def browser_ws_url(port: int) -> str:
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    try:
        conn.request("GET", "/json/version")
        return json.loads(conn.getresponse().read())["webSocketDebuggerUrl"]
    finally:
        conn.close()


def capture_trace(port: int, seconds: float) -> str:
    # The browser target, not a page: GPU-process events only reach the browser tracer.
    cdp = Cdp(browser_ws_url(port))
    try:
        cdp.call("Tracing.start", {
            "transferMode": "ReturnAsStream",
            "traceConfig": {"recordMode": "recordContinuously", "includedCategories": TRACE_CATEGORIES},
        })
        time.sleep(seconds)
        # Tracing.end answers before tracingComplete, so the reply is not awaited.
        cdp.send("Tracing.end")
        handle = cdp.wait_event("Tracing.tracingComplete", timeout=60)["stream"]
        chunks = []
        while True:
            part = cdp.call("IO.read", {"handle": handle, "size": 1 << 20}, timeout=60)
            chunks.append(part["data"])
            if part.get("eof"):
                break
        return "".join(chunks)
    finally:
        cdp.ws.close()


def summarize_events(events: list[dict], top: int = 8) -> dict:
    """CrGpuMain busy share, cost per present and top inclusive events.

    Busy time is the union of the event intervals, so nested and overlapping events
    count once. Per-present figures are None when the trace has no present events.
    """
    names = {(e["pid"], e["tid"]): e["args"]["name"]
             for e in events if e.get("ph") == "M" and e.get("name") == "thread_name"}
    key = next((k for k, v in names.items() if v == GPU_THREAD), None)
    if key is None:
        raise ValueError(f"trace has no {GPU_THREAD} thread; include the gpu category")
    xs = sorted((e for e in events
                 if (e.get("pid"), e.get("tid")) == key and e.get("ph") == "X" and "dur" in e),
                key=lambda e: e["ts"])
    if not xs:
        raise ValueError(f"{GPU_THREAD} has no duration events")
    busy_us = 0
    start, end = xs[0]["ts"], xs[0]["ts"]
    for e in xs:
        if e["ts"] > end:
            busy_us += end - start
            start, end = e["ts"], e["ts"] + e["dur"]
        else:
            end = max(end, e["ts"] + e["dur"])
    busy_us += end - start
    span_s = (max(e["ts"] + e["dur"] for e in xs) - xs[0]["ts"]) / 1e6
    presents = sum(1 for e in xs if e["name"] == PRESENT_EVENT)
    if presents == 0:
        presents = sum(1 for e in xs if e["name"] == PRESENT_FALLBACK_EVENT)
    inclusive = collections.Counter()
    for e in xs:
        inclusive[e["name"]] += e["dur"]
    return {
        "busy_pct": busy_us / 1e4 / span_s,
        "ms_per_present": busy_us / 1e3 / presents if presents else None,
        "presents_per_s": presents / span_s if presents else None,
        "presents": presents,
        "top": [(n, v / 1e3 / presents if presents else v / 1e3) for n, v in inclusive.most_common(top)],
    }


def load_events(text: str) -> list[dict]:
    data = json.loads(text)
    return data["traceEvents"] if isinstance(data, dict) else data


def print_summary(summary: dict) -> None:
    if summary["presents"] == 0:
        print(f"busy {summary['busy_pct']:.0f}%  per-present metrics unavailable: no present events in trace")
        unit = "ms total"
    else:
        print(f"busy {summary['busy_pct']:.0f}%  {summary['ms_per_present']:.2f} ms/present  "
              f"{summary['presents_per_s']:.0f} presents/s")
        unit = "ms/present"
    for name, ms in summary["top"]:
        print(f"   {ms:6.2f} {unit}  {name}")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, help="CDP port of a running Chrome")
    ap.add_argument("--seconds", type=float, default=5.0)
    ap.add_argument("--out", help="where to save the captured trace")
    ap.add_argument("--summarize", metavar="TRACE", help="only summarize a saved trace")
    args = ap.parse_args(argv)
    if args.summarize:
        with open(args.summarize, encoding="utf-8") as f:
            text = f.read()
    elif args.port and args.out:
        text = capture_trace(args.port, args.seconds)
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(text)
        print(f"trace saved: {args.out}")
    else:
        ap.error("pass --port and --out, or --summarize")
    print_summary(summarize_events(load_events(text)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
