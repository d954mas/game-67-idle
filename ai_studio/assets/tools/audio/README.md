# Audio Tools

## SFX tagging

`tag_audio.py` proposes searchable tags for the audio records of one Pack
Manifest pack. It scores every clip against `sfx_vocabulary.json` with a local
CLAP model (`laion/larger_clap_general`, zero-shot, nothing leaves the machine)
and writes a proposal JSON. It never edits `assets.jsonl`; applying the
proposal is a storage step, and applied rows should carry an `auto-tagged` tag
until someone listens to them.

```powershell
python ai_studio/assets/tools/audio/tag_audio.py --pack <pack-dir> --out tmp/ai_studio/assets/audio_tags/<pack>.json --calibrate --scope ui=ui --scope weapons-combat=combat,human
```

- `--scope TAG=GROUP[,GROUP]`: records carrying `TAG` are ranked only against
  those vocabulary groups. Use it when the pack already sorts sounds into
  categories; unscoped zero-shot over the whole vocabulary confuses classes
  (a UI blip scores as a beep or a gun).
- `--calibrate`: subtracts each label's mean logit over the run, so one label
  that scores high on every clip stops winning by default.
- `--top-k` / `--min-ratio`: how many runner-up labels join the best one.

Each proposal row holds `labels` with scores, `tags` (label slugs, their
groups, and a duration bucket), a short `summary`, and `duration_s`.

## SFX descriptions

`describe_audio.py` listens to each clip with Qwen2-Audio-7B-Instruct (4-bit,
local GPU) and writes `<pack>.qwen.json` in the same proposal format plus a
one-sentence `description`. The Asset Viewer review panel shows every
`<pack>*.json` run next to each other.

```powershell
python ai_studio/assets/tools/audio/describe_audio.py --pack <pack-dir> --out tmp/ai_studio/assets/audio_tags/<pack>.qwen.json
```

- `--context TAG=TEXT` adds a hint for records carrying `TAG`. Keep it off by
  default: with a category list the model repeats the list instead of
  describing the clip.
- `--stride N` / `--limit N` sample a pack before a full run.

The two tools disagree often, so neither is ground truth: applied tags stay
`auto-tagged` until a review in the Asset Viewer saves them.

## Importing audio packs

`import_audio_pack.py` turns a plan JSON (pack fields plus a list of source
files with their asset id, title, and tags) into Pack Manifest packs. It
converts each file with ffmpeg to at most 48 kHz, 16-bit, stereo (soxr
resampling, triangular dither; lower rates and mono stay as they are), copies
the pack's license fields into every row, and writes `pack.json` and
`assets.jsonl`. Converted files that already exist are reused, so a stopped
run resumes; a file that fails to convert is reported and left out.

```powershell
python ai_studio/assets/tools/audio/import_audio_pack.py --plan <plan.json> --jobs 8
```

The module docstring holds the plan shape. Building the plan (which libraries
to keep, how to title them) is per-source work and stays outside the tool.

Requirements: `ffmpeg` and `ffprobe` on `PATH` for imports; `torch`, `transformers`, `soundfile`, `scipy` for tagging; descriptions also
need a CUDA build of `torch`, `bitsandbytes`, and `accelerate`. The Qwen weights
take about 16 GB in the Hugging Face cache. Models download into that cache on
first run. CLAP is fine on CPU (500 short clips in a few minutes); Qwen needs
the GPU and takes about 4 seconds per clip.

Tests: `python -I ai_studio/assets/tools/audio/test_tag_audio.py`,
`python -I ai_studio/assets/tools/audio/test_describe_audio.py`, and
`python -I ai_studio/assets/tools/audio/test_import_audio_pack.py`.
