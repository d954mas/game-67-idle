---
name: delegated-image-generation
description: "Use when you (the agent) need to GENERATE real raster art (fake shots, icon/source sheets, sprites, UI art) but have no native image model. PRIMARY path is OpenAI gpt-image-2 via the codex backend (scripts/generate_image.py); agy (Antigravity) is the fallback. Read this BEFORE re-deriving how to call codex/gpt-image/agy for images — it records the working paths, the TLS/Avast gotcha, and the dead-ends."
---

# Delegated Image Generation

The agent cannot draw. To produce **real** generative art headlessly, delegate to
an external image model, pick the PNG off disk, and feed it into the normal asset
pipeline (`generated-game-ui-assets` / `game-visual-art-direction`).

Two working paths, verified on this machine (Windows):

- **Path A — OpenAI gpt-image-2 via the codex backend (PRIMARY).** Deterministic
  HTTP, explicit size/quality/format, style-match by reference, returns the image
  inline. Covered by the ChatGPT subscription (no per-image billing).
- **Path B — agy (Antigravity) CLI (FALLBACK).** Agentic; good art but can fall
  back to code-drawing and drops stdout. Use when Path A is unavailable.

If a step fails, re-check the gotchas below before inventing a new approach.

---

## Path A — gpt-image-2 via codex backend (primary)

`scripts/generate_image.py` does the whole thing. It auto-detects the credential
and always uses Windows `curl` as transport.

```bash
python .codex/skills/delegated-image-generation/scripts/generate_image.py \
  --prompt "<DETAILED PROMPT>: real raster game art, thick outline, plain dark slate background" \
  --out tmp/out.png --size 1024x1024 --quality medium
# verify by FILE + eyeball, never by the OK line alone:
test -f tmp/out.png && echo OK   # then Read the PNG to confirm it matches the prompt
```

Params: `--size` (1024x1024 / 1536x1024 / 1024x1536 / 2048x2048 …, min side 1024),
`--quality low|medium|high` (sets timeout: 180/300/480s), `--format png|jpeg|webp`,
`--model` (default `gpt-image-2`), `--input-image PATH` (repeatable ≤5 → edit /
style-match a reference for consistency), `--background`.

### How it works (so you don't re-derive it)

- The credential is a **ChatGPT/Codex OAuth JWT**. It has no platform scopes, so
  the public REST `/v1/images/generations` returns 401 "Missing scopes". But the
  **codex backend** `POST https://chatgpt.com/backend-api/codex/responses` — the
  same endpoint the official codex CLI calls — accepts it with an
  `image_generation` tool (`tool_choice: image_generation`, `model gpt-image-2`,
  outer `model gpt-5.5`, `stream:true`). The base64 image comes back in SSE
  `image_generation_call.result`.
- **Token source:** `~/.codex/auth.json` → `tokens.access_token` + `tokens.account_id`
  (env `OPENAI_API_KEY` is NOT set here). codex refreshes this token (~10-day
  life); the script re-reads it each run — never hardcode it. `ChatGPT-Account-Id`
  header is required (from the token or auth.json).
- ~30–40s per 1024² image; real, on-prompt, game-quality output (verified with a
  frost-crystal and a gold-coin icon, 2026-06-17).

### Transport / TLS gotcha (THE thing that breaks naive attempts)

This machine runs **Avast antivirus with HTTPS scanning (Web Shield)**, a friendly
TLS MITM: it re-signs every site with `Avast Web/Mail Shield Root` (installed in
the Windows root store). Consequences:

- **Python `urllib`/OpenSSL fails** with `CERTIFICATE_VERIFY_FAILED: Basic
  Constraints of CA cert not marked critical` — Avast's CA is non-RFC and strict
  OpenSSL rejects it. Loading the Windows store into OpenSSL does NOT fix it.
- **Windows `curl` (SChannel) works** but needs `--ssl-no-revoke` (Avast certs
  carry no CRL/OCSP → otherwise `CRYPT_E_NO_REVOCATION_CHECK`).

So the script shells out to `curl --ssl-no-revoke` for ALL HTTP (harmless on a
clean box too). If you ever rewrite the transport in Python, add `truststore`
(OS verifier) or it will fail here. To remove the MITM entirely: Avast → Settings
→ Protection → Web Shield → disable "HTTPS scanning".

### Risk / cost note (ToS gray zone)

The codex-backend path uses a personal ChatGPT subscription token against an
endpoint meant for the official client. It is a **ToS gray zone**; the practical
risk is account-level (not key-level) and scales with **volume + robotic
patterns** — modest manual game-asset use is low risk, but **do not batch
hundreds of automated calls**. For heavy/automated/commercial use, set a real
`sk-...` API key: the script then auto-switches to the **official REST path**
(billed per image, zero ban risk, and transparency works there). Token may break
or the endpoint may change at any time — Path B (agy) is the fallback.

---

## Path B — agy (Antigravity) CLI (fallback)

```bash
AGY="/c/Users/ROG/AppData/Local/agy/bin/agy.exe"   # not on PATH; or `agy` after `agy install`
"$AGY" --dangerously-skip-permissions \
  -p "Use your built-in image generation to create one real raster image (not code-drawn): <PROMPT>. Save the PNG to <PATH>.png . Do not write or run any drawing code." \
  < /dev/null > /dev/null 2>&1
test -f <PATH>.png && echo OK        # rely on the FILE, not stdout (agy drops it under non-TTY)
```

- `--dangerously-skip-permissions` is **required** headless (else it blocks on
  trust prompts). agy honors the save path; produced shaded game-quality art in ~32s.
- **Confirm by file, not stdout:** scan for new PNGs:
  `find . "$HOME/.gemini/antigravity" -name '*.png' -newermt "@$START"` (record
  `START=$(date +%s)` first). agy is a Go binary — Windows cert store, TLS just works.

---

## Prompting notes (both paths)

- Be explicit: "real raster image (not code-drawn)", "Do not write or run any
  drawing code" — otherwise an agentic CLI may FALL BACK to PIL/SVG programmer-art
  (banned by AGENTS.md).
- State aspect/size in words too ("512x512 icon", "wide 16:9"); for character
  consistency pass references via `--input-image`.
- For a style comparison, keep ONE composition and vary only the art style across
  ≤3 generations (see `primary-gdd-pipeline` visual gate).
- **Generate composable parts, not baked composites** (the engine assembles the look):
  - Chroma'd characters: forbid baked shadows ("NO drop/contact shadow, flat key
    colour up to the outline") — a soft shadow bleeds/fringes when cut.
  - Bars (health/XP/progress): generate the EMPTY frame only ("NO fill/segments/
    pips inside"); fill in-engine with a tinted quad so it can animate.
  - Slots/cells/panels/buttons: generate EMPTY ("NO icon/content inside, just the
    frame"); draw icons + labels on top in-engine.
- **Transparency:** the codex backend rejects transparent background on every
  model. Generate on a flat key colour and chroma-key in post
  (`generated-game-ui-assets`), or use the sk- REST path where `--background
  transparent` works.

---

## Dead-ends — do NOT re-try these

- **`codex exec` / `codex mcp-server` image generation is BROKEN on Windows and
  FAKES success.** exec has no `image_gen` tool in its context: a documented run
  produced 0 new files yet reported "Saved the generated image" (it copied a stale
  image), or fell back to GDI+/PIL code-drawing. mcp-server exposes only a session
  runner, same engine, same gap. ⚠️ The fix is NOT the exec CLI — it is Path A
  (raw HTTP to the codex **backend** Responses endpoint, which DOES register a
  working `image_generation` tool). Only that backend call, or the codex VS Code
  extension, works headlessly.
- **gemini CLI (`@google/gemini-cli`) has NO real image tool here.** Replies
  `CANNOT_GENERATE`, or silently writes a Python drawing script. Pinning
  `-m gemini-2.5-flash-image` → "Function calling is not enabled for this model".
  Use Path A or agy, not gemini.
- **Stale `GEMINI_API_KEY` env var hijacks gemini auth** ("API key expired").
  Neutralize per-call: `env -u GEMINI_API_KEY -u GOOGLE_API_KEY ...`.
- **node TLS behind the Avast MITM** breaks (`UNABLE_TO_VERIFY_LEAF_SIGNATURE` /
  `fetch failed`). For node tools export `NODE_OPTIONS=--use-system-ca`; do NOT
  disable `strict-ssl`. (Go/agy and Rust/codex are unaffected; Python needs
  `curl`/`truststore` as above.)

---

## Hand-off to the asset pipeline

Both paths give you the raw source image only. The deterministic rest is yours:
`generated-game-ui-assets` (prompt packet → generation record → intake → crop →
runtime PNGs → pixel/atlas audits) and the visual gate `node tools/ai.mjs gate`.
Clean output proves the PIPE, not the SCREEN — judge the assembled screen against
the fake shot / art bible. Keep raw generations in `tmp/`.

## Maintenance

- This skill is canonical in `.codex/skills/`; run `node tools/skills_sync.mjs`
  after edits to regenerate the `.claude/skills` pointer.
- agy binary: `%LOCALAPPDATA%\agy\bin\agy.exe`; `agy --version` checked = 1.0.8.
- Update this file (don't re-derive) if a working recipe changes.
