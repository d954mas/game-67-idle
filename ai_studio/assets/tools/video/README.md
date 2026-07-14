# Track B video-animation pipeline (T0263, v1)

Turns **(art image + motion text) -> a game-ready spritesheet** by running the
image through a local video generator, extracting frames, keying alpha, and
packing a flipbook. This is the generative "Track B" complement to the
procedural canvas track — it is for *animating* an existing sprite (flap, pulse,
bob), not for authoring.

**Experimental + removable by design** (lead's constraint): every stage lives in
its own folder and is independently re-runnable; the whole tool is
`ai_studio/assets/tools/video/**` plus one studio-config key. Delete the folder
and remove the `videoGenRoot` key and the pipeline is gone. It touches **nothing**
under `ai_studio/assets/canvas` (the canvas-seam integration is a later packet).

## Stages (each its own folder, each resumable)

| Stage | Folder | Runtime | In -> Out | Interpreter |
|---|---|---|---|---|
| 1 generate | `generate/` | node | image + motion -> `<run>/generate/*.mp4` + `params.json` | ComfyUI HTTP (WAN 2.2 I2V) |
| 2 frames | `frames/` | node -> py | video -> `<run>/frames/frame_%03d.png` | ComfyUI **embedded** python (PyAV) |
| 3 matte | `matte/` | node -> py | frames -> `<run>/matte/frame_%03d.png` RGBA + `report.json` | CorridorKey venv **or** repo `.venv` |
| 4 sheet | `sheet/` | python | RGBA frames -> `<run>/sheet/<name>_sheet.png` + `.json` | repo `.venv` (PIL) |

`run.mjs` chains all four on one run folder. Each stage skips if its output
already exists (resume); `--force` re-runs all, `--force-stage <name>` re-runs
one.

## Quick start

```
# 0. Boot the ComfyUI server yourself (v1 does NOT autostart):
cd C:\projects\video_gen_experiment\ComfyUI_windows_portable
python_embeded\python.exe -s ComfyUI\main.py --listen 127.0.0.1 --port 8188

# 1. Run the whole pipeline (from the repo root):
node ai_studio/assets/tools/video/run.mjs \
  --image <art.png> --text "the wings flap slowly and gently, soft glow pulsing" \
  --profile draft --tool corridorkey --seed 70263 --name wings
```

Artifacts land in a **run folder OUTSIDE the repo** (default under
`videoGenRoot/video_runs/`) — generated video/frames are big and machine-local.
`run.mjs` prints every artifact path + per-stage timings at the end.

### Individual stages (standalone / resume)

```
node generate/generate.mjs --image <png> --text "<motion>" --profile draft --seed N [--out <runDir>]
node frames/frames.mjs   --run-dir <runDir>
node matte/matte.mjs     --run-dir <runDir> --tool corridorkey|key_matte
.venv/Scripts/python.exe sheet/pack_sheet.py --run-dir <runDir> [--columns N] [--trim]
```

## Configuration

Two studio-config keys (committed in `ai_studio/studio.config.json`,
interpreted by this tool's `_lib.mjs` through the neutral Studio loader; the
matching environment variable wins):

```
"videoGenRoot": "C:/projects/video_gen_experiment"
"corridorKeyRoot": "C:/projects/ai_studio_tools/CorridorKey"
```

`videoGenRoot` (env `VIDEO_GEN_ROOT`) points at the **isolated,
wholesale-deletable** video-gen experiment (T0257): the portable ComfyUI stack,
the `draft_workflow_api.json` / `final_workflow_api.json` profiles, and the
MatAnyone reserve venv. `corridorKeyRoot` (env `CORRIDOR_KEY_ROOT`) points at
the **permanent** CorridorKey install — split out in T0335 because CorridorKey
is the canvas's first-priority glow/translucency alpha method and outlives the
experiment folder. Both accessors throw LOUDLY when unset and a stage needs
them — no silent fallback.

## Profiles (T0262 speed ladder)

- `draft` — 384x384 / 25 frames (~35 s warm, ~79 s cold on the 4080 Laptop).
  Iterate motion/seed here.
- `final` — 480x480 / 33 frames (~54 s warm). Run with the **same seed** as an
  accepted draft to reproduce its motion at higher fidelity.

Both bake the T0257 R1 prompt-hardening prefix
(`2d game art, flat colors, hand-drawn illustration, no photorealism`) — the
generate stage rebuilds the positive prompt from this prefix + your motion text
so it can never be dropped. The photoreal negative is left as the workflow ships.

## Matte tools (T0257 R2 verdict)

- `--tool corridorkey` (**default**) — neural unmixer for **glow / translucent /
  soft-edge identity-critical** assets. The ONLY tool that preserves the soft
  gold glow AND despills (R2: 11.6% soft alpha, clean gold edges). Wraps the
  exact R2 invocation. **Licence: CC-BY-NC-SA-4.0** — its README grants a
  carve-out to *process images for commercial projects* but forbids
  repackaging/reselling the tool or offering a paid inference API. Acceptable
  for internal asset production; flagged here for the record.
- `--tool key_matte` — the fast in-repo cutout for **opaque / non-glow**
  sprites (`ai_studio.assets.tools.image.alpha_matte.key_matte`). No licence
  constraint, ~0.28 s/frame, but cannot recover soft fractional alpha.

## v1 pragmatism / coupling notes

- **Frame extraction uses the ComfyUI portable *embedded* Python's PyAV**, not
  the repo `.venv`, via subprocess. The repo venv deliberately carries no heavy
  video deps and must not gain them for this. Documented coupling, revisit later.
- **CorridorKey runs in EAGER mode** (`CORRIDORKEY_SKIP_COMPILE=1`). On this
  Windows CUDA box the default `max-autotune` path builds triton/cudagraph
  kernels that error at replay (not caught by the tool's compile-time fallback);
  eager is exactly what T0257 R2 effectively ran in (~1.7-4 s/frame). The matte
  stage also sets `PYTHONUTF8=1` so CorridorKey's rich logging (which prints
  `->` glyphs) doesn't crash the captured cp1251 stdout pipe.
- **v1 does NOT autostart ComfyUI** and does NOT install anything — orchestration
  only. A down server / missing tool is a LOUD error naming the fix.
- The `matte` stage stages frames into `corridorKeyRoot/
  ClipsForInference/<shot>/` (machine-local, not the repo); `--skip-existing`
  means other shots that already have Output are skipped.
- `MatAnyone` is held in reserve (T0257 R2 PARTIAL — green-fringes glow); NOT
  wired into v1.

## Provenance

Every stage writes a JSON sidecar (`generate/params.json`, `frames/frames.json`,
`matte/report.json`, `sheet/<name>_sheet.json`). The spritesheet meta
(`schema: ai_studio.video.spritesheet.v1`) embeds the full source chain back to
the seed, prompt, workflow, models, and matte settings.

## Live golden run (T0263 acceptance)

Draft profile, wings-on-green fixture, motion "the wings flap slowly and gently,
soft glow pulsing", seed 70263, CorridorKey matte — full chain green:
generate 79.2 s (cold) -> frames 0.6 s (25f) -> matte 42.7 s (eager, ~1.7 s/f)
-> sheet 0.3 s. Sheet: 1920x1920, 5x5, 25 frames, fps 16, 12.8% soft-alpha glow
preserved. See the T0263 report for artifact paths.

## Tests

```
# sheet packing math + meta (pure PIL, no GPU):
PYTHONPATH=C:/projects/game-67-idle .venv/Scripts/python.exe -m unittest \
  ai_studio.assets.tools.video.sheet.pack_sheet_test
# node syntax:
node --check ai_studio/assets/tools/video/**/*.mjs
```
