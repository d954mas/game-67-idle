# T0251 — Can the agy ("gemini") engine consume LOCAL REFERENCE IMAGES?

Date: 2026-07-03. Author: deep-reasoner. Scope: research only, no production files touched, not committed.
Budget used: **1** live agy generation (of 3 allowed) + 1 codex vision judge.

## VERDICT: YES — agy can be influenced by local reference images.

agy (Google **Antigravity** CLI, Gemini 3.5 Flash, agentic) is NOT a flag-based
image API — it has no `--image`/`-i` attach flag (its `-i` is
`--prompt-interactive`, unrelated). But it IS a **multimodal agent**: given a
local image file path (inside an allowed workspace) plus permission to use its
tools, it opens/views the image and conditions both its understanding and its
built-in image generation on it. The refusal in `recipe_generate.mjs` / ops
(design R2 "support unverified") can be lifted — support is now **verified**.

## Empirical proof (the decisive test)

Ref: `tmp/dual_t0237/wings_test_white.png` — distinctive gold+white angel wings
on white. Discriminator: the prompt **never named the subject** ("wings"), it
only pointed agy at the file. If wings appear, agy must have *seen* the file.

Run 1 (`--add-dir <repo>` + `--dangerously-skip-permissions -p "<instruction>"`):
- agy auto-read the file (its stdout linked the `file:///.../wings_test_white.png` it opened).
- Sidecar proof it wrote (`desc1.txt`): *"a pair of ornate, spread-open angelic
  wings made of soft white feathers detailed with shimmering gold trim and
  surrounded by glowing gold sparkles against a solid white background"* — an
  exact description of the ref → agy genuinely ingested the pixels.
- Output `out1.png` (1024x1024): gold+white angel wings, same style/palette, as a
  genuine *variation* (ornate gold heart-filigree base vs the ref's mirrored
  hooks) — not a copy, exactly "influenced by".
- Independent judge `codex exec -i <ref> -i <out>`: **"YES - angel wings."**

Files for inspection:
- `C:\projects\game-67-idle\tmp\research_agy_refs\out1.png`
- `C:\projects\game-67-idle\tmp\research_agy_refs\desc1.txt`
- Ref: `C:\projects\game-67-idle\tmp\dual_t0237\wings_test_white.png`

## Capabilities on paper (corroborating)

- `agy --help`: no image flag. `--add-dir` (repeatable) adds a dir to the
  workspace; `--dangerously-skip-permissions` auto-approves tool calls;
  `-p/--print` runs one headless prompt (5m default `--print-timeout`).
- Antigravity docs advertise **multimodal input (text/image/audio/video)** and an
  `@file` / `@dir/` context syntax (gemini-cli lineage). We relied on the plain
  absolute path + `--add-dir`, which is more robust headlessly than `@`.
- Auth on this box: agy is logged in via keyring as a separate Google account
  (`3000pptmp@gmail.com`), model **Gemini 3.5 Flash (Medium)** — independent of
  codex creds. (`cli.log` prints a benign "not logged into Antigravity" for one
  token path; code-assist auth still works — Run 1 succeeded in 34s.)

## Exact working invocation shape

```bash
AGY="C:/Users/ROG/AppData/Local/agy/bin/agy.exe"
"$AGY" \
  --add-dir "<dir containing ref #1>" \
  --add-dir "<dir containing ref #2>" \   # repeat --add-dir per distinct ref dir
  --dangerously-skip-permissions \
  -p "You are given N reference image file(s) that define the desired subject/style.
      FIRST open and view each with your tools: <absRef1> ; <absRef2> ; ...
      Then write ONE sentence describing what you saw to <outPath>.seen.txt .
      Then, using your built-in image generation, create ONE real raster image
      (not code-drawn), <ASPECT>: <PROMPT>. Match the subject/style/palette of the
      reference image(s). Save the PNG to <outPath>. Do not write or run drawing code." \
  < /dev/null > /dev/null 2>&1
# verify: test -f <outPath>   (and, for refs, test -s <outPath>.seen.txt)
```

### Integration template for `recipe_generate.mjs` (gemini builder)

Today `buildAgyInstruction` deliberately never mentions refs and
`generateImageGemini` has no ref plumbing; ops.mjs refuses a gemini card that
carries refs. To lift the refusal, change three things (keep the file-existence
verification law):

1. `buildAgyCommand({ prompt, size, outPath, refPaths })` — prepend one
   `--add-dir <dirname(ref)>` per distinct ref parent dir, before
   `--dangerously-skip-permissions`. (Canvas refs live under
   `C:/Users/ROG/YandexDisk/gamedev/ai_studio/canvas_projects/.../files/` —
   **outside the repo** — so add-dir is mandatory; a ref agy can't reach is the
   failure mode below.)
2. `buildAgyInstruction` — when `refPaths.length`, inject the "FIRST open and view
   each: <abs paths>" clause + the "match subject/style/palette" clause + the
   mandatory `<outPath>.seen.txt` proof-write step. Pass absolute paths.
3. `generateImageGemini` — plumb `refPaths` through; after the run, keep the
   `existsSync(outPath)` loud-error, AND when refs were supplied also require the
   `.seen.txt` sidecar to exist and be non-trivial (proves agy actually read a ref
   rather than silently generating from text alone).

## Reliability caveats (the loud-error enemy)

- **Silent divergence, not a loud crash, is the real risk.** agy is an agent: if a
  ref path is unreadable (outside every `--add-dir`, or permissions not skipped)
  it can still emit *an* image from the text prompt alone and exit 0. File-exists
  alone would pass falsely. Mitigation = the mandatory `.seen.txt` proof-write +
  content check above; optional hardening = a post-gen `codex exec -i <ref> -i
  <out>` subject-match gate (heavy, codex-dependent — reserve for hero cards).
- **Fidelity tier:** Gemini 3.5 Flash gives a *stylistic variation*, not a faithful
  reproduction. Correct for "reference-influenced" recipe cards; do not promise
  pixel/identity replication.
- **Variadic-arg footgun (tooling note):** `codex exec -i <FILE>...` is greedy and
  will swallow a trailing positional prompt — pass the prompt via **stdin** (`… -i
  a.png -i b.png -` with the prompt piped) when judging. Does not affect agy.
- **Separate account/quota:** agy runs on its own Google login; failures there are
  independent of codex and won't surface in codex logs.

## If it had NOT worked (for the record / future)

Other Gemini surfaces could do image-to-image: the **gemini CLI proper** or the
**Gemini API REST** (`generativelanguage.googleapis.com`, inline_data base64
image parts) with an API key. On this box both would hit the **Avast TLS MITM**
that breaks node/python TLS — needing the local precedent workarounds (curl
transport, `NODE_OPTIONS=--use-system-ca`, or `truststore`). Not needed: agy
already works headlessly with no TLS custom transport.
