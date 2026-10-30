"""Free-text SFX descriptions and tags with a local audio-language model.

Listens to every audio record of one Pack Manifest pack with Qwen2-Audio and
writes the same proposal JSON as `tag_audio.py`, plus a one-sentence
description. It never edits `assets.jsonl`.

    python ai_studio/assets/tools/audio/describe_audio.py --pack <pack-dir> --out tmp/ai_studio/assets/audio_tags/<pack>.qwen.json --context ui="menu clicks, hover ticks, error buzzers"
"""

import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tag_audio import audio_records, duration_tag, read_mono, slug

DEFAULT_MODEL = "Qwen/Qwen2-Audio-7B-Instruct"
SAMPLE_RATE = 16000
# Words the model echoes from the prompt or uses as filler; they never help search.
GENERIC_TAGS = {"sound", "sound-effect", "sound-effects", "sfx", "audio", "noise", "game", "video-game", "video-game-asset", "asset", "use", "using", "operating"}
PROMPT = (
    "This is a sound effect from a video game asset pack.{context} "
    "Describe only what you actually hear in this clip: the sound source and the action. "
    "Do not list categories or possibilities. Answer in English, with JSON only: "
    '{{"description": "<one short sentence saying what makes this sound>", '
    '"tags": ["<2 to 5 short lowercase tags naming the sound source and action>"]}}'
)


def parse_contexts(values):
    """`--context ui="menu clicks"` -> {"ui": "menu clicks"}: a hint for records carrying that tag."""
    contexts = {}
    for value in values or []:
        tag, _, text = value.partition("=")
        if not tag or not text:
            raise ValueError(f"--context expects tag=text, got {value!r}")
        contexts[tag] = text.strip().strip('"')
    return contexts


def context_for(record_tags, contexts):
    hints = [contexts[tag] for tag in record_tags if tag in contexts]
    return (" The pack files it under: " + "; ".join(hints) + ".") if hints else ""


def parse_answer(text):
    """The model is asked for JSON but may wrap it in prose or fences; fall back to the raw sentence."""
    match = re.search(r"\{.*\}", text, re.S)
    for candidate in ([match.group(0), match.group(0).replace('\\"', '"')] if match else []):
        try:
            answer = json.loads(candidate)
            tags = answer.get("tags") or []
            if isinstance(tags, str):
                tags = tags.split(",")
            return str(answer.get("description") or "").strip(), [t for t in (slug(str(tag)) for tag in tags if str(tag).isascii()) if t and t not in GENERIC_TAGS]
        except (json.JSONDecodeError, AttributeError):
            continue
    return text.strip(), []


def proposal(asset_id, description, tags, seconds):
    unique = []
    for tag in tags + [duration_tag(seconds)]:
        if tag and tag not in unique:
            unique.append(tag)
    return {
        "asset_id": asset_id,
        "description": description,
        "summary": ", ".join(tag.replace("-", " ") for tag in tags[:3]),
        "tags": unique,
        "duration_s": round(seconds, 3),
    }


def load_partial(path):
    """Rows already written by an interrupted run, keyed by asset id."""
    done = {}
    if os.path.exists(path):
        with open(path, encoding="utf-8") as handle:
            for line in handle:
                try:
                    row = json.loads(line)
                except json.JSONDecodeError:
                    continue  # a line cut short by the interruption
                done[row["asset_id"]] = row
    return done


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pack", required=True, help="Pack folder that holds assets.jsonl.")
    parser.add_argument("--out", required=True, help="Proposal JSON path, normally under tmp/.")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--context", action="append", default=[], metavar="TAG=TEXT",
                        help="What the pack says records carrying TAG contain. Repeatable.")
    parser.add_argument("--quant", choices=["4bit", "8bit", "none"], default="4bit")
    parser.add_argument("--max-seconds", type=float, default=10.0)
    parser.add_argument("--max-new-tokens", type=int, default=96)
    parser.add_argument("--limit", type=int, default=0, help="Describe only the first N records.")
    parser.add_argument("--stride", type=int, default=1, help="Take every Nth record, for a spread-out sample.")
    args = parser.parse_args(argv)

    import torch
    from transformers import AutoProcessor, BitsAndBytesConfig, Qwen2AudioForConditionalGeneration

    contexts = parse_contexts(args.context)
    quant = None
    if args.quant == "4bit":
        quant = BitsAndBytesConfig(load_in_4bit=True, bnb_4bit_quant_type="nf4", bnb_4bit_compute_dtype=torch.float16)
    elif args.quant == "8bit":
        quant = BitsAndBytesConfig(load_in_8bit=True)
    processor = AutoProcessor.from_pretrained(args.model)
    model = Qwen2AudioForConditionalGeneration.from_pretrained(
        args.model, quantization_config=quant, dtype=torch.float16, device_map="auto"
    ).eval()

    records = list(audio_records(args.pack))[:: max(1, args.stride)]
    if args.limit:
        records = records[: args.limit]
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    partial_path = args.out + ".partial.jsonl"
    done = load_partial(partial_path)
    if done:
        print(f"resuming: {len(done)} already described", file=sys.stderr, flush=True)
    partial = open(partial_path, "a", encoding="utf-8")
    for index, (asset_id, path, tags) in enumerate(records, 1):
        if asset_id in done:
            continue
        clip, seconds = read_mono(path, args.max_seconds, SAMPLE_RATE)
        conversation = [{"role": "user", "content": [
            {"type": "audio", "audio_url": path},
            {"type": "text", "text": PROMPT.format(context=context_for(tags, contexts))},
        ]}]
        text = processor.apply_chat_template(conversation, add_generation_prompt=True, tokenize=False)
        inputs = processor(text=text, audio=[clip], sampling_rate=SAMPLE_RATE, return_tensors="pt", padding=True).to(model.device)
        with torch.no_grad():
            output = model.generate(**inputs, max_new_tokens=args.max_new_tokens, do_sample=False)
        answer = processor.batch_decode(output[:, inputs["input_ids"].shape[1]:], skip_special_tokens=True)[0]
        description, answer_tags = parse_answer(answer)
        done[asset_id] = proposal(asset_id, description, answer_tags, seconds)
        partial.write(json.dumps(done[asset_id], ensure_ascii=False) + "\n")
        partial.flush()
        print(f"described {index}/{len(records)}: {description[:80]}", file=sys.stderr, flush=True)

    partial.close()
    results = [done[asset_id] for asset_id, _, _ in records if asset_id in done]
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump({"model": args.model, "contexts": args.context, "pack": args.pack, "assets": results}, handle, indent=2, ensure_ascii=False)
    os.remove(partial_path)
    print(args.out)


if __name__ == "__main__":
    main(sys.argv[1:])
