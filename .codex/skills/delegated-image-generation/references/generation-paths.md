# Generation Paths

Load this reference when you need exact image-generation commands or must choose
between codex, agy, compare mode, or REST/backend alternatives.

Paths verified on this machine (Windows, 2026-06-17):

- Path A: official codex CLI `imagegen` via `codex exec` (PRIMARY). This is
  codex's documented image feature (gpt-image-2), sanctioned, no ToS gray zone,
  free on the ChatGPT plan, and counts toward Codex limits.
- Path B: agy (Antigravity) CLI (FALLBACK). Separate product and credentials;
  fast enough for fallback or style comparison.
- Path C: gpt-image-2 through official REST with an `sk-` key, or through the
  codex backend HTTP path. Use only when Path A breaks or REST/high-volume
  features are required.

## Path A: Official Codex Imagegen

```bash
bash .codex/skills/delegated-image-generation/scripts/codex_imagegen.sh \
  --prompt "<DETAILED PROMPT>: real raster game art, thick outline, plain dark slate background" \
  --out tmp/out.png --size 1024x1024
```

The wrapper runs `codex exec`, forces a real `imagegen` call, writes the PNG,
and verifies the result. Prefer it over hand-written calls.

Notes:

- The wrapper uses `codex_imagegen.sh`.
- `codex exec` may need
  `--dangerously-bypass-approvals-and-sandbox --skip-git-repo-check` for
  headless decode/write. If the target is inside the repo, tighten to
  `-s workspace-write -a never` when possible.
- The Codex desktop app is not required; `exec` is enough.

## Path B: Agy Fallback

```bash
AGY="/c/Users/ROG/AppData/Local/agy/bin/agy.exe"
START=$(date +%s)
"$AGY" --dangerously-skip-permissions -p "Use your built-in image generation to create one real raster image (not code-drawn), <ASPECT>: <PROMPT>. Save the PNG to C:/abs/path/out.png . Do not write or run any drawing code." \
  < /dev/null > /dev/null 2>&1
test -f C:/abs/path/out.png && echo OK \
  || find . "$HOME/.gemini/antigravity" -name '*.png' -newermt "@$START" 2>/dev/null
```

Use `--dangerously-skip-permissions` for headless runs. Confirm by file, not
stdout; agy can be quiet under non-TTY.

## Compare Mode

Use compare mode for important hero art when two model styles are useful.

```bash
bash .codex/skills/delegated-image-generation/scripts/gen_both.sh \
  --prompt "<DETAILED PROMPT>" --name shipicon --out-dir tmp/gen --size 1024x1024
```

This writes `<name>_codex.png` and `<name>_agy.png`. Pick by visual fit, mood,
readability, and style, not by pixel similarity or file size.

## Path C: REST Or Codex Backend Alternative

```bash
python .codex/skills/delegated-image-generation/scripts/generate_image.py \
  --prompt "<PROMPT>" --out tmp/out.png --size 1536x1024 --quality high
```

`generate_image.py` auto-detects credentials:

- `$OPENAI_API_KEY = sk-...`: official REST `/v1/images/generations`; billed,
  sanctioned, and preferred for REST features or high volume.
- Codex OAuth JWT from `~/.codex/auth.json`: raw codex backend HTTP with an
  `image_generation` tool. This is a ToS gray zone and should stay manual,
  low-volume, and alternative-only.
