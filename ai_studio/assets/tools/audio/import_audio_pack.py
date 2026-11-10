"""Convert source audio into a Pack Manifest pack.

Reads a plan JSON, converts every listed file to game-ready WAV with ffmpeg
(at most 48 kHz, 16-bit, stereo; lower rates and mono stay as they are), and
writes `pack.json` plus `assets.jsonl` for each pack in the plan. Converted
files that already exist are kept, so an interrupted run resumes.

    python ai_studio/assets/tools/audio/import_audio_pack.py --plan <plan.json> --jobs 8

Plan shape:

    {"packs": [{"dir": "<source>/restricted/packs/<pack-id>",
                "pack": {<pack.json fields>},
                "licenses": ["<license file to copy into licenses/>"],
                "items": [{"src": "<source audio>", "resource": "files/<name>.wav",
                           "asset_id": "...", "title": "...", "description": "...",
                           "tags": ["..."]}]}]}

Every row copies the pack's license fields, so a row stays valid on its own.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import wave
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tag_audio import duration_tag

MAX_RATE = 48000
ROW_FIELDS = ("origin", "license", "license_url", "license_kind", "attribution_required", "notice_required",
              "credit_text", "commercial_use", "modification_allowed", "redistribution_allowed", "publish",
              "source_page", "author_vendor")


def probe(path):
    """(channels, sample_rate) of the first audio stream."""
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "a:0", "-show_entries", "stream=channels,sample_rate",
         "-of", "json", path], capture_output=True, text=True, check=True).stdout
    stream = json.loads(out)["streams"][0]
    return int(stream["channels"]), int(stream["sample_rate"])


def target_format(channels, rate):
    return min(channels, 2), min(rate, MAX_RATE)


def convert_command(src, dst, channels, rate):
    out_channels, out_rate = target_format(channels, rate)
    return ["ffmpeg", "-nostdin", "-v", "error", "-y", "-i", src, "-map", "0:a:0", "-map_metadata", "-1",
            "-ac", str(out_channels),
            "-af", f"aresample={out_rate}:resampler=soxr:precision=28:dither_method=triangular",
            "-c:a", "pcm_s16le", dst]


def convert(src, dst):
    """Convert once; a half-written file never takes the final name."""
    if not os.path.exists(dst):
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        part = dst + ".part.wav"
        subprocess.run(convert_command(src, part, *probe(src)), check=True, capture_output=True)
        os.replace(part, dst)
    with wave.open(dst, "rb") as handle:
        seconds = handle.getnframes() / handle.getframerate()
        fmt = f"{handle.getframerate() // 1000}kHz 16-bit {'mono' if handle.getnchannels() == 1 else 'stereo'}"
    digest = hashlib.sha256()
    with open(dst, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return seconds, fmt, digest.hexdigest(), os.path.getsize(dst)


def record(pack, item, seconds, fmt, sha256, size):
    tags = []
    for tag in list(pack.get("tags", [])) + list(item.get("tags", [])) + [duration_tag(seconds)]:
        if tag and tag not in tags:
            tags.append(tag)
    lead = item.get("description", "").rstrip(".")
    row = {
        "asset_id": item["asset_id"],
        "title": item["title"],
        "description": (f"{lead}. " if lead else "") + f"{fmt}, {seconds:.1f} s.",
        "kind": "audio",
        "resource": item["resource"],
        "tags": tags,
    }
    row.update({field: pack[field] for field in ROW_FIELDS if field in pack})
    row.update({"sha256": sha256, "bytes": size})
    return row


def import_pack(entry, jobs):
    pack_dir, pack = entry["dir"], entry["pack"]
    items = entry["items"]
    ids = [item["asset_id"] for item in items]
    if len(set(ids)) != len(ids):
        raise ValueError(f"{pack['pack']}: duplicate asset ids in plan")

    def one(item):
        try:
            return item, convert(item["src"], os.path.join(pack_dir, item["resource"])), None
        except (subprocess.CalledProcessError, OSError, wave.Error, KeyError, IndexError) as error:
            return item, None, error

    rows, failed = [], []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for index, (item, result, error) in enumerate(pool.map(one, items), 1):
            if error:
                failed.append((item["src"], error))
                print(f"FAILED {item['src']}: {error}", file=sys.stderr, flush=True)
                continue
            rows.append(record(pack, item, *result))
            if index % 50 == 0:
                print(f"{pack['pack']}: {index}/{len(items)}", file=sys.stderr, flush=True)

    os.makedirs(os.path.join(pack_dir, "licenses"), exist_ok=True)
    for path in entry.get("licenses", []):
        shutil.copy2(path, os.path.join(pack_dir, "licenses", os.path.basename(path)))
    with open(os.path.join(pack_dir, "pack.json"), "w", encoding="utf-8", newline="\n") as handle:
        json.dump(pack, handle, indent=2, ensure_ascii=False)
        handle.write("\n")
    with open(os.path.join(pack_dir, "assets.jsonl"), "w", encoding="utf-8", newline="\n") as handle:
        for row in rows:
            handle.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")
    return len(rows), failed


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--plan", required=True)
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    args = parser.parse_args(argv)
    with open(args.plan, encoding="utf-8") as handle:
        plan = json.load(handle)
    total_failed = 0
    for entry in plan["packs"]:
        count, failed = import_pack(entry, args.jobs)
        total_failed += len(failed)
        print(f"{entry['pack']['pack']}: {count} records, {len(failed)} failed")
    return 1 if total_failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
