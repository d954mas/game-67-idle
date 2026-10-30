"""Zero-shot SFX tagging with a local CLAP model.

Reads one Pack Manifest `assets.jsonl`, scores every audio record against a
label vocabulary, and writes proposed tags to a JSON file. It never edits the
manifest: applying the proposal is a separate storage step.

    python ai_studio/assets/tools/audio/tag_audio.py --pack <pack-dir> --out tmp/ai_studio/assets/audio_tags/<pack>.json
"""

import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_MODEL = "laion/larger_clap_general"
DEFAULT_VOCAB = os.path.join(HERE, "sfx_vocabulary.json")
SAMPLE_RATE = 48000
PROMPT = "the sound of {}"


def load_vocabulary(path):
    """Flatten {group: [label, ...]} into ordered (label, group) pairs without duplicates."""
    with open(path, encoding="utf-8") as handle:
        groups = json.load(handle)
    seen = set()
    pairs = []
    for group, labels in groups.items():
        for label in labels:
            if label in seen:
                continue
            seen.add(label)
            pairs.append((label, group))
    return pairs


def parse_scopes(values):
    """`--scope ui=ui,mechanical` -> {"ui": {"ui", "mechanical"}}: records with that tag only see those groups."""
    scopes = {}
    for value in values or []:
        tag, _, names = value.partition("=")
        if not tag or not names:
            raise ValueError(f"--scope expects tag=group[,group], got {value!r}")
        scopes[tag] = {name.strip() for name in names.split(",") if name.strip()}
    return scopes


def allowed_groups(record_tags, scopes):
    """Union of the scopes the record's tags hit; None means the whole vocabulary."""
    hit = [scopes[tag] for tag in record_tags if tag in scopes]
    return set().union(*hit) if hit else None


def scoped_softmax(logits, labels, groups, allowed=None):
    """Softmax over the labels whose group is allowed, so scoped records rank only their own vocabulary."""
    keep = [i for i, label in enumerate(labels) if allowed is None or groups[label] in allowed]
    if not keep:
        keep = list(range(len(labels)))
    top = max(logits[i] for i in keep)
    weights = [math.exp(logits[i] - top) for i in keep]
    total = sum(weights)
    return [labels[i] for i in keep], [w / total for w in weights]


def calibrate(rows):
    """Subtract each label's mean logit over the batch, so a label that scores high on every clip stops winning by default."""
    if not rows:
        return rows
    means = [sum(column) / len(rows) for column in zip(*rows)]
    return [[value - mean for value, mean in zip(row, means)] for row in rows]


def pick_labels(scores, labels, top_k=3, min_ratio=0.5):
    """Best label always, then up to top_k-1 more whose probability is at least min_ratio of the best."""
    ranked = sorted(zip(labels, scores), key=lambda item: item[1], reverse=True)
    if not ranked:
        return []
    best = ranked[0][1]
    picked = [ranked[0]]
    for label, score in ranked[1:top_k]:
        if score >= best * min_ratio:
            picked.append((label, score))
    return picked


def duration_tag(seconds):
    if seconds < 0.5:
        return "very-short"
    if seconds < 2.0:
        return "short"
    if seconds < 6.0:
        return "medium"
    return "long"


def slug(label):
    return "-".join(label.lower().replace("/", " ").split())


def proposal(asset_id, picked, groups, seconds):
    tags = []
    for label, _ in picked:
        for tag in (slug(label), slug(groups[label])):
            if tag not in tags:
                tags.append(tag)
    tags.append(duration_tag(seconds))
    return {
        "asset_id": asset_id,
        "labels": [{"label": label, "score": round(float(score), 4)} for label, score in picked],
        "tags": tags,
        "summary": ", ".join(label for label, _ in picked),
        "duration_s": round(seconds, 3),
    }


def read_mono(path, max_seconds, target_rate=SAMPLE_RATE):
    import numpy as np
    import soundfile
    from scipy.signal import resample_poly

    data, rate = soundfile.read(path, dtype="float32", always_2d=True)
    seconds = data.shape[0] / rate
    mono = data.mean(axis=1)
    if rate != target_rate:
        divisor = math.gcd(rate, target_rate)
        mono = resample_poly(mono, target_rate // divisor, rate // divisor).astype(np.float32)
    return mono[: int(max_seconds * target_rate)], seconds


def unit_embeds(output):
    """transformers 5 returns a pooled output object where 4.x returned the tensor."""
    embeds = getattr(output, "pooler_output", output)
    return embeds / embeds.norm(dim=-1, keepdim=True)


def audio_records(pack_dir):
    with open(os.path.join(pack_dir, "assets.jsonl"), encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            record = json.loads(line)
            if record.get("kind") == "audio" and record.get("resource"):
                yield record["asset_id"], os.path.join(pack_dir, record["resource"]), record.get("tags") or []


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pack", required=True, help="Pack folder that holds assets.jsonl.")
    parser.add_argument("--out", required=True, help="Proposal JSON path, normally under tmp/.")
    parser.add_argument("--vocab", default=DEFAULT_VOCAB, help="JSON {group: [labels]}.")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--top-k", type=int, default=3)
    parser.add_argument("--min-ratio", type=float, default=0.5)
    parser.add_argument("--max-seconds", type=float, default=10.0)
    parser.add_argument("--batch", type=int, default=16)
    parser.add_argument("--limit", type=int, default=0, help="Tag only the first N records.")
    parser.add_argument("--scope", action="append", default=[], metavar="TAG=GROUP[,GROUP]",
                        help="Limit records carrying TAG to these vocabulary groups. Repeatable.")
    parser.add_argument("--calibrate", action="store_true", help="Remove per-label bias measured over the whole run.")
    args = parser.parse_args(argv)

    import torch
    from transformers import ClapModel, ClapProcessor

    vocabulary = load_vocabulary(args.vocab)
    labels = [label for label, _ in vocabulary]
    groups = dict(vocabulary)
    scopes = parse_scopes(args.scope)
    device = "cuda" if torch.cuda.is_available() else "cpu"
    model = ClapModel.from_pretrained(args.model).to(device).eval()
    processor = ClapProcessor.from_pretrained(args.model)

    with torch.no_grad():
        text = processor(text=[PROMPT.format(label) for label in labels], return_tensors="pt", padding=True).to(device)
        text_embeds = unit_embeds(model.get_text_features(**text))

    records = list(audio_records(args.pack))
    if args.limit:
        records = records[: args.limit]
    clips = []
    rows = []
    for start in range(0, len(records), args.batch):
        chunk = records[start : start + args.batch]
        batch = [read_mono(path, args.max_seconds) for _, path, _ in chunk]
        clips.extend(batch)
        with torch.no_grad():
            audio = processor(audio=[clip for clip, _ in batch], sampling_rate=SAMPLE_RATE, return_tensors="pt").to(device)
            audio_embeds = unit_embeds(model.get_audio_features(**audio))
            rows.extend((audio_embeds @ text_embeds.T * model.logit_scale_a.exp()).cpu().tolist())
        print(f"scored {len(rows)}/{len(records)}", file=sys.stderr, flush=True)

    if args.calibrate:
        rows = calibrate(rows)
    results = []
    for (asset_id, _, tags), (_, seconds), row in zip(records, clips, rows):
        scoped_labels, probs = scoped_softmax(row, labels, groups, allowed_groups(tags, scopes))
        results.append(proposal(asset_id, pick_labels(probs, scoped_labels, args.top_k, args.min_ratio), groups, seconds))

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump({"model": args.model, "vocabulary": args.vocab, "scopes": args.scope, "calibrated": args.calibrate, "pack": args.pack, "assets": results}, handle, indent=2, ensure_ascii=False)
    print(args.out)


if __name__ == "__main__":
    main(sys.argv[1:])
