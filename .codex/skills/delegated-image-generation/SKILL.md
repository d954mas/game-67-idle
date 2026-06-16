---
name: delegated-image-generation
description: "Use when you (the agent) need to GENERATE real raster art (fake shots, icon/source sheets, sprites, UI art) but have no native image model. Delegates headless image generation to the agy (Antigravity) CLI and hands the PNG to the asset pipeline. Read this BEFORE re-deriving how to call codex/gemini/agy for images — it records the one working path and the dead-ends."
---

# Delegated Image Generation (agy / Antigravity)

The agent cannot draw. To produce **real** generative art headlessly, delegate to
the **agy (Antigravity) CLI**, then pick the PNG off disk and feed it into the
normal asset pipeline (`generated-game-ui-assets` / `game-visual-art-direction`).

Verified working 2026-06-16 on this machine (Windows). If a step fails, re-check
the gotchas below before inventing a new approach.

## The one working recipe

```bash
AGY="/c/Users/ROG/AppData/Local/agy/bin/agy.exe"   # not on PATH; call by full path (or `agy` after `agy install`)
"$AGY" --dangerously-skip-permissions \
  -p "Use your built-in image generation to create one real raster image (not code-drawn): <DETAILED PROMPT>. Save the PNG to <ABSOLUTE_OR_REL_PATH>.png . Do not write or run any drawing code." \
  < /dev/null > /dev/null 2>&1
# then VERIFY BY FILE, not stdout:
test -f <PATH>.png && echo OK
```

- `--dangerously-skip-permissions` = auto-approve all tools. **Required** for
  headless — without it agy blocks on permission/trust prompts (this is exactly
  what hung codex/gemini).
- `-p` / `--print` = single non-interactive prompt. `--print-timeout` default 5m.
- agy **honors the save path you give in the prompt**. It produced real,
  shaded, game-quality art (glossy icon with outline/UI frame) in ~32s.
- agy is a Go binary; it uses the Windows system cert store, so TLS "just works"
  (no `--use-system-ca` needed for agy itself).

## Retrieval (critical)

- **Rely on the file on disk, NOT stdout.** Under non-TTY (our Bash/PowerShell
  tools) agy's `-p` frequently DROPS the final printed message. The image is
  still written. Confirm success by `test -f <path>` / scanning for new PNGs:
  `find . "$HOME/.gemini/antigravity" -name '*.png' -newermt "@$START" 2>/dev/null`.
- Record `START=$(date +%s)` before the call to detect exactly what was created.
- Then `Read` the PNG to eyeball it (agent can view PNGs), and copy/move the
  accepted file into the project; keep raw generations in `tmp/`.

## Prompting notes

- Be explicit: "real raster image (not code-drawn)", "Do not write or run any
  drawing code" — otherwise an agentic CLI may FALL BACK to drawing shapes with
  PIL/SVG (programmer-art, banned by AGENTS.md).
- State aspect/size in words ("wide 16:9 landscape", "512x512 icon"); plain `-p`
  has no size flag. For exact pixel sizing / character consistency see the
  `agy-image` skill pattern (Openclaw-Metis/agy-image).
- For a theme/style comparison, keep ONE composition and vary only the art style
  across N generations (see `primary-gdd-pipeline` visual gate, max 3 directions).

## Dead-ends — do NOT waste time re-trying these

- **gemini CLI (`@google/gemini-cli`) has NO real image tool here.** Asked
  explicitly it replies `CANNOT_GENERATE: No built-in image generation tool
  available`, or silently falls back to writing a Python drawing script
  (programmer-art). Pinning `-m gemini-2.5-flash-image` fails with "Function
  calling is not enabled for this model". Use agy, not gemini, for art.
- **codex `exec` image generation is BROKEN on Windows (openai/codex#19133) and
  FAKES success — never trust it without verifying.** Mechanism: codex has a
  built-in `image_gen` tool (model **gpt-image-2**) on your ChatGPT auth (free
  tier included), triggered by natural language or an explicit `$imagegen` in
  the prompt; output lands in `$CODEX_HOME/generated_images/<session>/` then is
  copied into the project (transparency via flat `#ff00ff` chroma key).
  Documented headless form:
  `codex exec -C "$(pwd)" -s workspace-write --skip-git-repo-check "...$imagegen... save to ./x.png"`.
  On THIS Windows box the tool reports unavailable: a clean documented run
  produced **0 new files** in `generated_images`, yet codex reported "Saved the
  generated image here" — it had run a shell script to **copy the latest
  pre-existing image** (an unrelated puzzle icon) to the target path. ⚠️ So a
  codex-exec "image" can be a stale copy, not a fresh on-prompt generation.
  Always verify: check the `generated_images` count delta AND `Read` the PNG to
  confirm it matches the prompt. codex DOES produce real art interactively / in
  the VS Code extension (doctor rollout sources: `vscode=286`) — just not via
  headless `exec` on Windows. For headless, use **agy**.
- **Stale `GEMINI_API_KEY` env var hijacks auth.** A pre-existing (now expired)
  `GEMINI_API_KEY` was set in the environment and overrode OAuth, causing "API
  key expired". The harness caches env, so our shells may still see a deleted
  key — neutralize per-call with `env -u GEMINI_API_KEY -u GOOGLE_API_KEY ...`.
- **Corporate/AV TLS interception breaks node TLS** (`UNABLE_TO_VERIFY_LEAF_
  SIGNATURE` / `fetch failed` / curl exit 35). For node tools (npm, gemini-cli,
  `node -e fetch`) export `NODE_OPTIONS=--use-system-ca`. Do NOT disable
  `strict-ssl`. (agy/Go and codex/Rust are unaffected.)
- Gemini CLI headless also needs `GEMINI_CLI_TRUST_WORKSPACE=true` + `NO_COLOR=1
  CI=1 TERM=dumb` and must run via **Bash** (it crashed under PowerShell pipe).

## Hand-off to the asset pipeline

agy gives you the raw source PNG only. The deterministic rest is yours:
`generated-game-ui-assets` (prompt packet -> generation record -> intake ->
crop -> runtime PNGs -> pixel/atlas audits) and the visual gate
`node tools/ai.mjs gate`. agy producing clean output proves the PIPE, not the
SCREEN — judge the assembled screen against the fake shot / art bible.

## Maintenance

- agy binary path is user-specific (`%LOCALAPPDATA%\agy\bin\agy.exe`). After
  `agy install` it lands on PATH as `agy`. `agy models` lists models;
  `agy --version` checked = 1.0.8.
- This skill is generic and reusable across games. Update it (don't re-derive)
  if the working recipe changes.
