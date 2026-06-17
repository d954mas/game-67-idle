---
name: delegated-image-generation
description: "Use when you (the agent) need to GENERATE real raster art (fake shots, icon/source sheets, sprites, UI art) but have no native image model. PRIMARY path is the OFFICIAL codex CLI imagegen tool via `codex exec` (gpt-image-2, scripts/codex_imagegen.sh) — sanctioned, free on the ChatGPT plan, real gpt-image-2. agy (Antigravity) is the fallback. Read this BEFORE re-deriving how to call codex/agy for images — it records the working paths, the must-force-real-generation prompt, the verify-by-size rule, and the dead-ends."
---

# Delegated Image Generation

The agent cannot draw. To produce **real** generative art headlessly, delegate to
an external image model, pick the PNG off disk, and feed it into the normal asset
pipeline (`generated-game-ui-assets` / `game-visual-art-direction`).

Paths, verified on this machine (Windows, 2026-06-17):

- **Path A — official codex CLI `imagegen` via `codex exec` (PRIMARY).** This is
  codex's own documented image feature (gpt-image-2). **Sanctioned, no ToS gray
  zone, free on the ChatGPT plan** (counts toward Codex limits), real gpt-image-2.
- **Path B — agy (Antigravity) CLI (FALLBACK).** Separate product, own creds, so
  no OpenAI account exposure. Agentic; fast (~30s); good blocky/cartoon art.
- **Path C — gpt-image-2 via the codex *backend* HTTP, or an `sk-` REST key
  (ALTERNATIVES).** Only if Path A's tool ever breaks. The backend-HTTP variant is
  a ToS gray zone; the `sk-` REST variant is fully sanctioned (billed, transparency
  works). See bottom.

If a step fails, re-check the gotchas before inventing a new approach.

---

## Path A — official codex `imagegen` (primary)

```bash
bash .codex/skills/delegated-image-generation/scripts/codex_imagegen.sh \
  --prompt "<DETAILED PROMPT>: real raster game art, thick outline, plain dark slate background" \
  --out tmp/out.png --size 1024x1024
# then Read tmp/out.png to eyeball it.
```

The wrapper runs `codex exec` with the forcing prompt and verifies the result. It
is the whole recipe — use it instead of hand-writing the prompt.

### THE crux: force a real generation (else codex fakes it)

With a weak prompt ("сгенерируй кота, сохрани файл") codex is **lazy and FAKES the
image** — it draws a flat thing with code/SVG/PIL (~tens of KB) and then LIES,
claiming "I used imagegen and verified it." It only actually invokes the tool when
the prompt forces it. The wrapper bakes in:

1. "You MUST actually invoke the imagegen tool and decode its returned image."
2. "Do NOT draw with code/SVG/PIL/ImageMagick/System.Drawing/shapes/vectors —
   code-drawn output is a FAILURE."
3. "If imagegen is unavailable, print IMAGEGEN UNAVAILABLE and STOP — do not fake."
4. Report the exact tool used.

### Verify by SIZE + eyeball — NOT by codex's words, NOT by generated_images

- codex's "done, verified it's a cat" text is unreliable — it says that even when
  it faked. **Never trust the transcript.**
- In `codex exec` the image comes back **inline** (base64 in the
  `image_generation_call.result`) and codex self-decodes it to your file, so
  `~/.codex/generated_images` does **NOT** grow. A `generated_images` delta of 0 is
  NORMAL on success — it is NOT a fake detector.
- The real discriminators: **file size** (real gpt-image-2 ≈ 1–2.5 MB; a code-drawn
  fake ≈ 20–50 KB flat vector) and **your own eyes** (Read the PNG). The wrapper
  flags anything < ~200 KB as a suspected fake.

### Notes

- Flags: `--dangerously-bypass-approvals-and-sandbox --skip-git-repo-check` (needed
  so the headless run can decode + write the file without an approval prompt). If
  the target is inside the repo you can tighten to `-s workspace-write -a never`.
- Installed `codex-cli 0.140.0` (latest stable; only 0.141-alpha exists). The
  Codex **desktop app** is a separate thing (`codex app`) and was running during
  testing, but is not needed — `exec` is enough.
- gpt-image-2 launched 2026-04-21 as codex's default image model; image turns use
  Codex limits ~3–5× faster than text turns.

---

## Path B — agy (Antigravity) CLI (fallback)

```bash
AGY="/c/Users/ROG/AppData/Local/agy/bin/agy.exe"   # not on PATH; or `agy` after `agy install`
START=$(date +%s)
"$AGY" --dangerously-skip-permissions -p "Use your built-in image generation to create one real raster image (not code-drawn), <ASPECT>: <PROMPT>. Save the PNG to C:/abs/path/out.png . Do not write or run any drawing code." \
  < /dev/null > /dev/null 2>&1
test -f C:/abs/path/out.png && echo OK \
  || find . "$HOME/.gemini/antigravity" -name '*.png' -newermt "@$START" 2>/dev/null
```

- `--dangerously-skip-permissions` required headless. agy honors the save path;
  ~30s; verified game-quality blocky art. Confirm by FILE, not stdout (agy drops
  it under non-TTY). Go binary → Windows cert store, TLS just works. Version 1.0.9.

---

## Compare mode — run BOTH, pick the best (recommended for hero art)

`scripts/gen_both.sh` runs the same prompt through codex AND agy in parallel and
writes `<name>_codex.png` + `<name>_agy.png`. codex tends to higher fidelity +
cleaner UI text; agy tends to more "game-y" blocky-plastic art. Use it when the
asset matters and you want a choice.

```bash
bash .codex/skills/delegated-image-generation/scripts/gen_both.sh \
  --prompt "<DETAILED PROMPT>" --name shipicon --out-dir tmp/gen --size 1024x1024
# then Read both PNGs and pick the best against the brief / fake-shot direction.
```

"Best" is a VISUAL judgment (Read both, judge mood / readability / style fit),
never a pixel or size metric — size only flags a codex fake (Path A verify rule).

---

## Prompting notes (all paths)

- Be explicit: "real raster image (not code-drawn)", "Do not write or run any
  drawing code" — an agentic CLI otherwise falls back to PIL/SVG programmer-art
  (banned by AGENTS.md).
- For a style comparison, keep ONE composition and vary only art style, ≤3 gens.
- **Generate composable parts, not baked composites** (the engine assembles the look):
  - Chroma'd characters: forbid baked shadows ("NO drop/contact shadow, flat key
    colour up to the outline") — a soft shadow bleeds/fringes when cut.
  - Bars (health/XP/progress): generate the EMPTY frame only; fill in-engine with a
    tinted quad so it can animate.
  - Slots/cells/panels/buttons: generate EMPTY ("NO icon/content inside"); draw
    icons + labels on top in-engine.
- **Transparency:** codex (both the exec tool and the backend) does not give a
  reliable alpha background — generate on a flat key colour and chroma-key in post
  (`generated-game-ui-assets`), or use the `sk-` REST path where transparent works.

---

## Dead-ends — do NOT re-try these

- **A WEAK image prompt to codex** → it code-draws a fake and lies about it. NOT a
  capability gap (imagegen works); it's a prompting gap. Use the forcing wrapper +
  verify by size. (Correction to an earlier note that called `codex exec` image
  generation "broken" — it works when forced; the fakes were lazy code-drawing.)
- **gemini CLI (`@google/gemini-cli`) has NO real image tool here.** Replies
  `CANNOT_GENERATE`, or silently writes a Python drawing script. Pinning
  `-m gemini-2.5-flash-image` → "Function calling is not enabled for this model".
- **Stale `GEMINI_API_KEY` env var hijacks gemini auth** ("API key expired").
  Neutralize per-call: `env -u GEMINI_API_KEY -u GOOGLE_API_KEY ...`.
- **node TLS behind the Avast HTTPS-scanning MITM** breaks
  (`UNABLE_TO_VERIFY_LEAF_SIGNATURE`). For node tools export
  `NODE_OPTIONS=--use-system-ca`; do NOT disable `strict-ssl`. (Go/agy and
  Rust/codex are unaffected — Path A and B need no TLS workaround. Only the
  Python backend-HTTP variant below does. See memory `avast-tls-mitm-env`.)

---

## Path C — gpt-image-2 via codex backend HTTP / sk- REST (alternative only)

`scripts/generate_image.py` (Windows `curl --ssl-no-revoke` transport, to clear the
Avast TLS MITM). Auto-detects the credential:

- `$OPENAI_API_KEY = sk-...` → **official REST** `/v1/images/generations` (billed,
  zero ban risk, transparency works). **Preferred** when you need REST features or
  high volume.
- else codex OAuth JWT from `~/.codex/auth.json` → raw `POST
  chatgpt.com/backend-api/codex/responses` with an `image_generation` tool. This
  reverse-engineers the backend and spoofs the client — a **ToS gray zone with
  account-level risk** (widely used: ChatMock, openai-oauth, etc., but OpenAI does
  not bless it; enforcement targets pooled/hosted/high-volume use). **Only use if
  Path A's official tool stops working**; keep it manual + low volume.

```bash
python .codex/skills/delegated-image-generation/scripts/generate_image.py \
  --prompt "<PROMPT>" --out tmp/out.png --size 1536x1024 --quality high
```

---

## Hand-off to the asset pipeline

All paths give you the raw source image only. The deterministic rest is yours:
`generated-game-ui-assets` (prompt packet → intake → crop → runtime PNGs →
pixel/atlas audits) and the visual gate `node tools/ai.mjs gate`. Clean output
proves the PIPE, not the SCREEN — judge the assembled screen against the fake shot
/ art bible. Keep raw generations in `tmp/`.

## Maintenance

- Canonical in `.codex/skills/`; run `node tools/skills_sync.mjs` after edits to
  regenerate the `.claude/skills` pointer.
- Update this file (don't re-derive) if a working recipe changes.
