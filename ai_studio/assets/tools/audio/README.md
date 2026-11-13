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

## How to compress game audio

`export_mp3.py` turns a source sound into the file a game ships. A game imports
it (`decode`, `shape_effect` or `shape_music`, then `choose_rate` or
`export_at`) and keeps only its own spec and provenance rows; Dig a Million's
`tools/audio/build_audio_exports.py` is the worked example. One file by hand:

```powershell
python ai_studio/assets/tools/audio/export_mp3.py <source> <out.mp3> [--music] [--rate HZ]
```

- **MP3 only.** Every browser decodes it, and the engine's native and wasm
  builds already carry dr_mp3. Opus or Vorbis would add a decoder to the build
  to save a few KB per sound; do not add one.
- **Mono.** The mixer places effects and loops; their stereo image is not
  heard, and mono halves the data.
- **Shape first, resample second.** Effects are trimmed (`trim_db` under the
  peak), faded out and peak-normalised; music is set to an integrated LUFS.
  This happens once at 44.1 kHz; only then is the shaped PCM resampled with
  soxr. Peak-normalising after the resample moved loudness by up to +2.7 dB,
  because the resampler changes the peak and not the body. Shape-then-resample
  keeps the level within about 0.3 dB of the 44.1 kHz encode.
- **Sample rate by rule.** `choose_rate` encodes the sound at 44.1 kHz and at
  each candidate rate (24 kHz, then 32 kHz), decodes both at 44.1 kHz, and keeps
  the lowest candidate where
  - the RMS level moved less than `MAX_LEVEL_CHANGE_DB` (1.0 dB), and
  - the spectral balance moved less than `MAX_BALANCE_SHIFT_DB` (1.0 dB): from
    the long-term power spectrum (2048-sample Hann frames, hop 512), each 1 kHz
    band from 1 to 7 kHz as dB of the 100 Hz-7.5 kHz total, RMS over the bands
    of candidate minus reference. A band under -60 dB counts as -60 dB.

  Otherwise the sound stays at 44.1 kHz. The measured numbers go into the
  export's provenance. `export_at` fixes one sound's rate when a game needs to.
- **Why it works.** LAME at 24 kHz cuts above about 7.5 kHz. A sound whose
  energy sits at 5-8 kHz (a coin pour) loses its top, the rest becomes
  relatively louder, and that is heard as hiss. A sound dominated by lower
  energy loses its top with no change anyone hears.
- **Calibration.** 25 level-matched listening verdicts by the lead on Dig a
  Million's effects and music loop: only the coin pour at 24 kHz was audible,
  at a 1.3 dB balance shift; every inaudible one measured 0.92 dB or less. That
  is one audible point, so the threshold is a named constant to move with new
  evidence, not a proven limit. A UI click failed on level instead (-2.2 dB at
  24 kHz).
- **Quality.** `-q:a 9` (the lowest VBR setting) for effects: short transients
  mask its artefacts. `-q:a 7` for music: the lead heard no difference at
  24 kHz `-q:a 7` nor at 32 kHz `-q:a 8`.
- **Gapless.** `encode` writes to a file so the muxer can seek back and fill
  the Xing/LAME header with the encoder delay and padding, and it refuses an
  output without that header. A loop then decodes to exactly its length; music
  is also resampled as a periodic signal, so the seam stays continuous.
- **Listen to new kinds of sound.** The rule is calibrated on one game's
  effects and one music loop. For a new type of sound (voice, ambience, a
  bright synth lead), compare the chosen file with the 44.1 kHz encode by ear
  before shipping it.

Requirements: `ffmpeg` and `ffprobe` on `PATH` for imports and exports (exports need libmp3lame and soxr); `torch`, `transformers`, `soundfile`, `scipy` for tagging; descriptions also
need a CUDA build of `torch`, `bitsandbytes`, and `accelerate`. The Qwen weights
take about 16 GB in the Hugging Face cache. Models download into that cache on
first run. CLAP is fine on CPU (500 short clips in a few minutes); Qwen needs
the GPU and takes about 4 seconds per clip.

Tests: `python -I ai_studio/assets/tools/audio/test_tag_audio.py`,
`python -I ai_studio/assets/tools/audio/test_describe_audio.py`,
`python -I ai_studio/assets/tools/audio/test_import_audio_pack.py`, and
`python -I ai_studio/assets/tools/audio/test_export_mp3.py`.
