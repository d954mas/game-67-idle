#!/usr/bin/env python3
"""Warm Python worker for the image-tool bridge (T0202).

A long-lived process that speaks a minimal line-delimited JSON protocol over
stdio. It pays the interpreter startup + heavy-import tax (numpy / scipy / Pillow)
ONCE and then serves every image-tool AND canvas Python entrypoint as a method:
each request names a target script and an argv, and the worker runs that script's
``__main__`` in-process (runpy) so the cold-spawn floor is paid at boot, not per op.

This is a pure TRANSPORT optimization. A served script runs byte-for-byte the same
code the cold ``python script.py --flags`` path ran (same argv, same argparse main,
same SystemExit), so tool behavior and parity are unchanged. Heavy modules
(``numpy``/``scipy``/``PIL``) stay cached in ``sys.modules`` across calls; only the
small target script is re-parsed each request (microseconds), in a fresh namespace
so no per-call state leaks.

Protocol (one JSON object per line, UTF-8; requests in, responses out):
  request : {"id": <any>, "script": <path>, "argv": [<str>, ...]}
            {"id": <any>, "method": "ping"}
  response: {"id": <same>, "ok": true,  "code": 0, "stdout": "...", "stderr": "..."}
            {"id": <same>, "ok": false, "code": <int|null>, "stdout": "...",
             "stderr": "...", "error": "<traceback | argparse usage | exit message>"}

Responses go ONLY to the real stdout captured at boot; a served script's own
stdout/stderr are redirected into the response fields, so script prints can never
corrupt the protocol channel. Errors are LOUD: a non-zero SystemExit, an argparse
error, an uncaught exception, or a missing import all return ok:false with the
captured detail (the Node manager turns that into a thrown Error — no silent
fallback to a cold spawn).
"""
from __future__ import annotations

import contextlib
import io
import json
import runpy
import sys
import traceback
from typing import Any

# The real stdout captured before any per-request redirection. Protocol responses
# ALWAYS go here, so a served script's prints cannot corrupt the channel. Force
# UTF-8 so non-ASCII paths/messages round-trip on Windows consoles.
_OUT = sys.stdout
try:  # pragma: no cover - depends on the stream type
    _OUT.reconfigure(encoding="utf-8", newline="\n")
    sys.stdin.reconfigure(encoding="utf-8")
except Exception:
    pass


def _respond(payload: dict[str, Any]) -> None:
    _OUT.write(json.dumps(payload) + "\n")
    _OUT.flush()


def _run_script(script: str, argv: list[Any]) -> dict[str, Any]:
    """Run one target script's ``__main__`` with a synthesized argv, capturing its
    stdout/stderr and its SystemExit code. Any failure is reported LOUDLY."""
    buf_out, buf_err = io.StringIO(), io.StringIO()
    saved_argv = sys.argv
    code: Any = 0
    error: str | None = None
    sys.argv = [str(script), *[str(item) for item in argv]]
    try:
        with contextlib.redirect_stdout(buf_out), contextlib.redirect_stderr(buf_err):
            try:
                runpy.run_path(str(script), run_name="__main__")
            except SystemExit as exc:  # scripts end with `raise SystemExit(main())`
                code = exc.code
    except BaseException:  # noqa: BLE001 - every script failure is surfaced, not swallowed
        error = traceback.format_exc()
        code = 1
    finally:
        sys.argv = saved_argv

    ok = error is None and (code is None or code == 0)
    if not ok and error is None:
        # A non-zero SystemExit: surface the captured stderr (argparse usage, tool
        # error text) plus a string exit message (e.g. `raise SystemExit("...")`).
        parts = [buf_err.getvalue().strip()]
        if code is not None and not isinstance(code, int):
            parts.append(str(code))
        error = "\n".join(part for part in parts if part) or f"script exited with code {code!r}"
    return {
        "ok": ok,
        "code": code if (code is None or isinstance(code, int)) else 1,
        "stdout": buf_out.getvalue(),
        "stderr": buf_err.getvalue(),
        "error": error,
    }


def main() -> int:
    while True:
        raw = sys.stdin.readline()
        if not raw:  # EOF: the parent closed stdin (clean shutdown signal)
            break
        line = raw.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except Exception as exc:  # malformed frame — report and keep serving
            _respond({"id": None, "ok": False, "error": f"bad request json: {exc}"})
            continue
        request_id = request.get("id")
        if request.get("method") == "ping":
            _respond({"id": request_id, "ok": True})
            continue
        script = request.get("script")
        if not script:
            _respond({"id": request_id, "ok": False, "error": "request missing 'script'"})
            continue
        result = _run_script(script, request.get("argv") or [])
        result["id"] = request_id
        _respond(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
