from __future__ import annotations

import math
import re
from pathlib import Path


MARKER_COLOR_ARGB = 0xFFFF00FF

_FRAME_HEADER = re.compile(
    r"^frame:(?P<frame>\d+)\s+pts:(?P<pts>-?\d+)\s+pts_time:(?P<seconds>-?(?:\d+(?:\.\d*)?|\.\d+))\s*$"
)
_MARKER_EVENT = re.compile(r"^lavfi\.black_(?P<kind>start|end)=(?P<seconds>-?(?:\d+(?:\.\d*)?|\.\d+))\s*$")


def build_marker_probe_command(ffmpeg: Path, video: Path) -> list[str]:
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(video),
        "-an",
        "-vf",
        "format=rgba,colorkey=0xff00ff:0.1:0,alphaextract,blackdetect=d=0:pix_th=0.1:pic_th=0.99,metadata=print:file=-",
        "-f",
        "null",
        "-",
    ]


def parse_marker_receipt(stdout: str) -> dict:
    events: list[dict] = []
    frame: int | None = None
    seconds: float | None = None
    for raw_line in stdout.splitlines():
        line = raw_line.strip()
        header = _FRAME_HEADER.fullmatch(line)
        if header is not None:
            candidate_seconds = float(header["seconds"])
            if not math.isfinite(candidate_seconds) or candidate_seconds < 0:
                raise RuntimeError("OBS marker probe reported an invalid frame PTS")
            frame = int(header["frame"])
            seconds = candidate_seconds
            continue
        event = _MARKER_EVENT.fullmatch(line)
        if event is None:
            continue
        if frame is None or seconds is None:
            raise RuntimeError("OBS marker probe event has no decoded frame PTS")
        event_seconds = float(event["seconds"])
        if not math.isfinite(event_seconds) or event_seconds < 0:
            raise RuntimeError("OBS marker probe reported an invalid marker PTS")
        if not math.isclose(event_seconds, seconds, rel_tol=0.0, abs_tol=1e-6):
            raise RuntimeError("OBS marker probe event does not match its decoded frame PTS")
        events.append({"kind": event["kind"], "frame": frame, "seconds": seconds})

    if len(events) != 2 or [event["kind"] for event in events] != ["start", "end"]:
        raise RuntimeError("OBS marker probe requires one complete contiguous marker run")
    start, end = events
    if end["frame"] <= start["frame"] or end["seconds"] <= start["seconds"]:
        raise RuntimeError("OBS marker probe reported an empty marker run")
    return {
        "startSeconds": start["seconds"],
        "endSeconds": end["seconds"],
        "startFrame": start["frame"],
        "endFrame": end["frame"],
        "markerFrames": end["frame"] - start["frame"],
    }
