#!/usr/bin/env python3
"""Tests for the GPU trace summary; no browser involved."""

from __future__ import annotations

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from chrome_gpu_trace import MiniWebSocket, load_events, summarize_events  # noqa: E402


def _meta(pid, tid, name):
    return {"ph": "M", "name": "thread_name", "pid": pid, "tid": tid, "args": {"name": name}}


def _x(name, ts, dur, tid=1):
    return {"ph": "X", "name": name, "pid": 7, "tid": tid, "ts": ts, "dur": dur}


class _FakeSock:
    def __init__(self, incoming):
        self.incoming, self.sent = incoming, []

    def recv(self, n):
        data, self.incoming = self.incoming[:n], self.incoming[n:]
        return data

    def sendall(self, data):
        self.sent.append(data)

    def settimeout(self, t):
        pass


def _ws(incoming):
    ws = MiniWebSocket.__new__(MiniWebSocket)
    ws.sock, ws._buf = _FakeSock(incoming), b""
    return ws


class WebSocketTest(unittest.TestCase):
    def test_ping_is_answered_with_masked_pong_of_same_payload(self):
        ws = _ws(bytes([0x89, 3]) + b"abc" + bytes([0x81, 2]) + b"hi")
        self.assertEqual(ws.recv_text(), "hi")
        frame = ws.sock.sent[0]
        self.assertEqual(frame[0], 0x8A)
        self.assertEqual(frame[1], 0x80 | 3)
        mask, body = frame[2:6], frame[6:]
        self.assertEqual(bytes(b ^ mask[i % 4] for i, b in enumerate(body)), b"abc")

    def test_close_frame_raises(self):
        ws = _ws(bytes([0x88, 2, 0x03, 0xE8]))
        with self.assertRaises(ConnectionError):
            ws.recv_text()


class SummaryTest(unittest.TestCase):
    def test_nested_events_are_not_double_counted(self):
        events = [
            _meta(7, 1, "CrGpuMain"),
            _x("Outer", 0, 600_000),
            _x("Inner", 100_000, 200_000),
            _x("DXGISwapChainImageBacking::Present", 700_000, 100_000),
            _x("SkiaOutputSurfaceImplOnGpu::SwapBuffers", 900_000, 100_000),
        ]
        s = summarize_events(events)
        # busy = 600k + 100k + 100k of a 1 s span.
        self.assertAlmostEqual(s["busy_pct"], 80.0)
        # SwapBuffers is ignored while Present events exist.
        self.assertEqual(s["presents"], 1)
        self.assertAlmostEqual(s["ms_per_present"], 800.0)
        self.assertAlmostEqual(s["presents_per_s"], 1.0)
        self.assertEqual(s["top"][0][0], "Outer")

    def test_equal_start_inner_before_outer(self):
        events = [_meta(7, 1, "CrGpuMain"), _x("Inner", 0, 100), _x("Outer", 0, 500)]
        self.assertAlmostEqual(summarize_events(events)["busy_pct"], 100.0)

    def test_partial_overlap_is_a_union(self):
        events = [_meta(7, 1, "CrGpuMain"), _x("A", 0, 400), _x("B", 200, 400), _x("C", 800, 200)]
        # [0,600] + [800,1000] = 800 of a 1000 us span.
        self.assertAlmostEqual(summarize_events(events)["busy_pct"], 80.0)

    def test_swapbuffers_is_the_fallback_present(self):
        events = [_meta(7, 1, "CrGpuMain"), _x("SkiaOutputSurfaceImplOnGpu::SwapBuffers", 0, 100_000),
                  _x("SkiaOutputSurfaceImplOnGpu::SwapBuffers", 500_000, 100_000)]
        self.assertEqual(summarize_events(events)["presents"], 2)

    def test_no_presents_has_no_per_present_metrics(self):
        s = summarize_events([_meta(7, 1, "CrGpuMain"), _x("Work", 0, 100)])
        self.assertEqual(s["presents"], 0)
        self.assertIsNone(s["ms_per_present"])
        self.assertIsNone(s["presents_per_s"])

    def test_other_threads_are_ignored(self):
        events = [
            _meta(7, 1, "CrGpuMain"), _meta(7, 2, "Other"),
            _x("Work", 0, 100_000), _x("Noise", 0, 900_000, tid=2),
        ]
        s = summarize_events(events)
        self.assertEqual([n for n, _ in s["top"]], ["Work"])

    def test_missing_gpu_thread_is_an_error(self):
        with self.assertRaises(ValueError):
            summarize_events([_meta(7, 2, "Other"), _x("Work", 0, 10, tid=2)])

    def test_load_accepts_object_and_array_traces(self):
        self.assertEqual(load_events('{"traceEvents": [1]}'), [1])
        self.assertEqual(load_events("[2]"), [2])


if __name__ == "__main__":
    unittest.main()
