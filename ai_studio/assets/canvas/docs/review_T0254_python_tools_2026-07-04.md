# T0254 — Python-tools review (dimension 3 of 3)

Scope: `ai_studio/assets/tools/image/<tool>/` (alpha_matte, alpha_dualplate, route,
bg_fix, regions, slice, quantize, denoise, sources, _bridge) + `ai_studio/assets/canvas/tools/*.py`
(render_group, slice9, alpha_cutout, alpha_dualplate, crop_regions, export_images,
check_flat_background) + `ai_studio/assets/tools/lib/`. Read-only; measured on the live
`.venv` (numpy 2.1.1 / scipy 1.17.1 / Pillow 12.2.0) against real project PNGs.

---

## Strengths (verified, credited)

- **Per-tool decomposition is real and clean.** Every image tool is a self-contained
  folder: entry script + `<name>_test.py` + `README.md` + `__init__.py`. Easy to find,
  own, and test in isolation. This is a genuine win over a monolith.
- **Warm worker (T0202) is well-engineered.** `_bridge/worker.py` runs each target via
  `runpy.run_path(..., run_name="__main__")` with a synthesized `sys.argv`, so a served
  script is byte-for-byte the cold `python script.py` path (same argparse main, same
  `SystemExit`) — parity is structurally guaranteed, not asserted. Heavy imports stay in
  `sys.modules` across calls. Measured: warm call **< 100 ms** vs a cold worker-boot floor
  (`worker.test.mjs:45-57`). `worker.mjs` fails an in-flight request LOUDLY on crash and
  respawns without a silent retry, serializes FIFO to one in-flight, unref's while idle so
  a one-shot CLI still exits, and an exit/signal hook kills orphans. Tested thoroughly
  (crash recovery, loud failure not killing the worker, FIFO, idle kill).
- **No-fallbacks law is consistently enforced.** Interpreter is config-only
  (`bridge.mjs:130-146` `resolvePythonPath`, no PATH probe); a missing venv/dep is a loud
  error naming the exact one-shot setup command (`bridge.mjs:162-170`). Canvas reuse tools
  raise `RuntimeError` on a missing image-tools module (`alpha_cutout.py:52-61`,
  `alpha_dualplate.py:62-74`).
- **Error contract is loud end-to-end.** Traced: python `raise SystemExit(str(exc))`
  (`alpha_cutout.py:200-206`) -> worker.py captures stderr + builds `error` text
  (`worker.py:77-91`) -> `worker.mjs` rejects `Error(message.error)` with `.stderr`
  (`worker.mjs:161-176`) -> bridge maps ModuleNotFound / rethrows detail
  (`bridge.mjs:161-170`) -> op throws -> `api.mjs:889-901` records to `errors.jsonl` and
  returns **400 `{error: message}`**. Expected refusals travel as one clean line (no raw
  traceback); unexpected bugs still traceback.
- **Alpha-preservation discipline is real.** quantize/denoise split alpha out and reattach
  byte-identical (`quantize.py:58-75`, `denoise.py:50-63`); alpha_cutout enforces
  no-hidden-pixel-resurrection with `out[...,3] = min(keyed, src_alpha)` and flattens hidden
  RGB before keying (`alpha_cutout.py:91-118`).
- **Tool parity via verbatim reuse.** Canvas `alpha_cutout.py` imports `route_cutout` +
  `key_matte_cutout`; canvas `alpha_dualplate.py` imports the 3 dual-plate modules — one op
  layer, no second matte implementation.
- **atomic_io used almost everywhere; deterministic; no network.** `requirements.txt` is
  exact-pinned; `setup_python.mjs` verifies imports and has the Avast-TLS retry.

---

## Findings ranked

### F1 — key_matte's 4-pass bleed + edge-repair are DEAD WORK (HIGH, perf + correctness) 
**What:** In `key_matte_cutout` the finalize tail runs, in order (`key_matte.py:227-229`):
`bleed_transparent_rgb(passes=4)` -> `repair_transparent_edge_rgb` -> `zero_fully_transparent_rgb`.
Both bleed and repair write neighbour RGB into pixels **and set their alpha to 0**
(`chroma_key_alpha.py:250-251`, `:283-284`); the final `zero_fully_transparent_rgb` then
zeroes RGB of every `alpha==0` pixel (`:259-264`). So both expensive passes are entirely
overwritten.
**Evidence (measured):** On a 512² magenta crop the finalize tail is 202 ms of 218 ms total;
`bleed(passes=4)` alone is **136 ms (62% of the whole keyer)**, ~34.5 ms/pass. Skipping
bleed+repair entirely gives **0 / 147456 differing pixels** on both magenta and green keys,
and cuts key_matte from **120 ms -> 43 ms (2.8x)** on a 384² crop. `passes=4` vs `passes=1`
is also byte-identical.
**Why:** key_matte is the daily driver, run per crop, dozens of times per slice/cutout op.
62% of its cost produces nothing. The author's comment ("a 4px bleed covers the halo under
transparent pixels") predates the `zero_fully_transparent_rgb` no-hidden-pixels step, which
now supersedes it.
**Fix (S, but needs a decision):** Either (a) delete bleed+repair from key_matte's tail —
per the "atlas packer does its own 2px extrude / no hidden pixels" law this is the correct
2.8x win; or (b) if edge-extruded RGB under transparent pixels IS wanted, then
`zero_fully_transparent_rgb` is the bug silently degrading atlas edges. Lead picks which
invariant wins. Ship with a golden-bytes determinism test (none exists today — see F10).

### F2 — report-JSON schema naming is NOT uniform (MED-HIGH, consistency)
**What:** Four namespaces + three version conventions coexist:
- `game.dual_plate_alpha_report` / `game.dual_plate_pair_gate` (oldest, un-namespaced, `version:1` as a field)
- `ai_studio.raster2d.*` — bg_fix `background_normalize.v1`, regions `detected_regions.v1`, slice `slices.v1`
- `ai_studio.image_tools.*` — quantize `quantize_report.v1`, denoise `denoise_report.v1`
- `ai_studio.canvas.*` — the 6 canvas report/spec schemas
Version is expressed as in-string `.v1` (quantize/denoise/canvas), a separate `"version":1`
field (dual_plate family, `dual_plate_alpha.py:258-259`), or **both** (`detect_regions.py:276-277`).
**Why:** The task's hypothesis "`ai_studio.image_tools.*.v1` — uniform?" is false; only 2 of
~11 schemas use it. Consumers can't pattern-match one prefix; migrations are ambiguous.
**Fix (M):** Adopt one convention (`ai_studio.<module>.<tool>_report.v1`, version in the
string only). Alias the old `game.*` and `raster2d.*` strings for the frozen viewer contract
(bridge.mjs already pins the raster2d URL prefix — schema string can migrate independently).

### F3 — color-distance-to-key is reimplemented ≥5x with two different metrics (MED-HIGH, shared-lib)
**What:** The "one mandated shared point" is not shared. Inline distance-to-key lives in:
`key_matte.py:166` (Chebyshev `max|Δ|`), `normalize_background.py:103` (Chebyshev),
`detect_regions.py:58` (Chebyshev), `chroma_key_alpha.py:16` (Chebyshev), and
`route_cutout.py:86` (**Euclidean** `√ΣΔ²`).
**Why:** Beyond duplication, it's a latent inconsistency: the router decides key_matte-vs-dual
on a *Euclidean* metric, but the keyer that then acts on that decision uses *Chebyshev*.
Near the tolerance boundary the two disagree, so a crop routed as "opaque" can key differently
than the router's softness model assumed. Low practical incidence, real smell.
**Fix (M):** Add `tools/lib/color.py::key_distance(rgb, key, metric="chebyshev")` and route
all five call sites through it; pick one metric repo-wide (Chebyshev is the majority and the
keyer's own).

### F4 — border-key estimator duplicated 5x with divergent semantics (MED, shared-lib)
**What:** `estimate_border_key_color` (mode of opaque border, numpy) in
`normalize_background.py:78`; `estimate_border_key_color` (mode, **pure-python dict loop over
getdata**) in `slice_regions.py:57`; `_key_from_border` (**median**) in `route_cutout.py:67`;
`_border_median` (median) in `pair_align.py`; `border_ring` in `check_flat_background.py:60`.
Two use mode, two use median — they can return different keys for the same sheet.
**Fix (M):** One `tools/lib/color.py::border_key(image, stat="mode")`; the slice_regions
pure-python loop is also the slowest and should die first.

### F5 — alpha split/reattach + `changed_pixel_pct` boilerplate duplicated (MED, shared-lib)
**What:** `quantize.py:58-85` and `denoise.py:50-68` share ~identical scaffolding: convert
RGBA, split `rgb_before`/`alpha`, process RGB, `out=empty_like; out[:3]=after; out[3]=alpha`,
`changed_pixel_pct = mean(any(after!=before))`. `parse_color`/`_parse_rgb` hex parsing is
duplicated across 5 files (dual_plate_alpha, normalize_background, detect_regions,
route_cutout, slice_regions) plus a 6th variant in `render_group.parse_background`.
**Fix (S-M):** `tools/lib/alpha.py::process_rgb_preserving_alpha(image, fn) -> (result, changed_pct)`
and `tools/lib/color.py::parse_hex/format_hex`.

### F6 — warm worker has NO per-request execution timeout (MED, robustness)
**What:** `worker.mjs` has only `idleTimer` (idle-between-requests kill, `:210-216`). There is
no timeout on an in-flight request. A hung/pathological script (infinite loop, a pathologically
huge source) never resolves; because ops serialize FIFO, **every queued op behind it stalls
indefinitely** — only a full process exit clears it. Crash recovery (`onExit` reject+respawn)
and concurrency (FIFO serialize) are solid; the timeout is the one gap.
**Fix (S):** Add a per-request watchdog in `Worker.pump`/`run`: on expiry, `killNow()` +
reject the in-flight with a loud timeout error (the respawn path already exists). The
codex/dual-plate Node spawns already carry a 480 s cap (`dual_plate_generate.mjs:33`); the
warm path should too.

### F7 — slowest real-world paths + cheapest wins (MED, perf, measured)
**What:** On a 1.57 Mpx sheet: quantize **1.7–2.8 s** (non-monotonic in colours: 64c=2.8 s >
256c=2.1 s, MEDIANCUT tree effect), denoise 0.2/0.34/0.65 s (str 1/2/3), route 128 ms,
key_matte whole-sheet 1.46 s. Per-crop (the intended key_matte path): 12/46/219/969 ms at
128/256/512/1024².
**Named slowest real path:** key_matte per crop, dominated by the finalize hygiene tail
(F1). **Cheapest 2x win:** F1 (bleed removal, 2.8x, zero output change). Secondary: the
`max_dim=512` work-downscale doesn't help the hygiene tail (it runs at full source res,
`key_matte.py:214-229`) — after F1 the remaining `decontaminate` is already gated by
`_has_key_spill` (Q3, `:223`), which is correct and cheap (2.5 ms when clean).
**Fix (S):** F1 first; optionally extend the `_has_key_spill` gate stance to skip the whole
tail when no spill AND no soft band.

### F8 — one raw non-atomic write left (LOW, atomic-io coverage)
**What:** `pair_align.py:180 aligned_dark.save(args.output)` bypasses `save_image_atomic`
(the only remaining raw write in the tool dirs; it's the standalone-debug CLI entry).
`slice_regions.write_zip` (`:267-282`) hand-rolls tmp+`replace` (atomic, but re-implements
lib rather than importing it).
**Fix (S):** Route through `save_image_atomic`; add `write_bytes_atomic`/`write_zip_atomic`
to lib and use it in slice_regions.

### F9 — `__init__.py` placement is inconsistent; "two homes" helper drift (LOW, structure)
**What:** Every *leaf* tool folder has `__init__.py`, but **all parent packages**
(`ai_studio/`, `assets/`, `tools/`, `image/`, `canvas/`) have none — the tree works as PEP-420
namespace packages, which makes the leaf `__init__.py` files redundant/inconsistent (they're
harmless but signal confusion about the packaging model). Separately, `regions/`, `slice/`,
`sources/` carry an `api.mjs` JS bridge *inside* the tool folder while other tools' JS bridges
live in `_bridge`/`ops.mjs` — mild two-homes drift.
**Fix (S):** Decide namespace-vs-regular consistently (drop the leaf `__init__.py` OR add
parents). Doc the `image/<tool>` (raster2d op) vs `canvas/tools` (canvas-owned entry that
reuses image modules) split in `tools/image/README.md` so it reads as intentional.

### F10 — test coverage is behaviorally good but thin on edges + no golden pins (LOW-MED)
**What:** Fixtures are constructed (procedural rings/squares/gradients) — realistic enough
for the contracts they assert (alpha byte-identity, determinism-by-construction, refusals:
`quantize_test` rejects out-of-range colours, `denoise_test` rejects bad strength). Gaps:
no non-RGBA/palette-PNG input, no 1px/alpha-only/huge-image edges, and **no golden-bytes pin
on key_matte** — so F1's bleed removal would pass every existing test silently (nothing
asserts transparent-pixel RGB). Python `_test.py` import numpy/PIL at module top, so they
hard-error rather than skip when the venv is absent (only `worker.test.mjs` skips cleanly);
no `unittest discover` runner is wired in the ai_studio JS.
**Fix (M):** Add a key_matte golden-output test (guards F1 + future hygiene changes), a few
edge fixtures (grayscale input, 1px, all-transparent), and a `skipUnless(venv)` guard or a
discover-runner that reports skips.

---

## Do-differently (tradeoffs)

- **key_matte hygiene should be spill-gated, not unconditional.** The Q3 `_has_key_spill`
  gate on `decontaminate` is the right pattern; the bleed/repair tail should follow the same
  "only when there's something to fix" stance (or be deleted per F1). Trade-off: byte output
  changes if the atlas actually depends on under-transparent RGB — hence lead decision, not a
  silent edit.
- **Pick one key-distance metric repo-wide.** Chebyshev everywhere is simplest and matches the
  keyer; route's Euclidean is the lone outlier. Trade-off: route thresholds were calibrated
  (2026-06-18) on Euclidean, so switching route needs a re-calibration pass — or keep route
  Euclidean but make the divergence explicit and documented.
- **Consolidate shared math into `tools/lib` and let BOTH homes import it.** The image/<tool>
  vs canvas/tools split is fine (canvas tools are thin entrypoints reusing image modules), but
  the primitives (hex parse, border-key, key-distance, alpha-preserve, atomic-zip) belong in
  lib, imported by both — today lib owns only atomic_io.
- **Schema + version convention is a one-time sweep.** Cheap to do now while there are ~11
  schemas; expensive once external consumers pin the `game.*`/`raster2d.*` strings.

---

## Top-10 fixes

| # | Fix | Sev | Size | Evidence |
|---|-----|-----|------|----------|
| 1 | Delete (or spill-gate) key_matte's `bleed(passes=4)`+`repair` — overwritten by `zero_fully_transparent_rgb`; 2.8x on the daily driver, byte-identical output | HIGH | S+decision | key_matte.py:227-229; chroma_key_alpha.py:250-251,259-264,283-284; measured 120→43 ms, 0/147456 diff |
| 2 | Unify report schema namespace + version convention (`ai_studio.<module>.<tool>.v1`), alias old `game.*`/`raster2d.*` | MED-HIGH | M | dual_plate_alpha.py:258-259; detect_regions.py:276-277; quantize.py:96 |
| 3 | Extract `tools/lib/color.key_distance()` + pick ONE metric; route 5 call sites (fix Euclidean/Chebyshev split) | MED-HIGH | M | key_matte.py:166; route_cutout.py:86; normalize_background.py:103; detect_regions.py:58 |
| 4 | Add per-request execution timeout + watchdog kill to the warm worker | MED | S | worker.mjs:210-216 (only idleTimer); pump/run have no cap |
| 5 | Extract `tools/lib` border-key estimator (kill the 5x dup + mode/median divergence) | MED | M | normalize_background.py:78; slice_regions.py:57; route_cutout.py:67; pair_align._border_median |
| 6 | Shared `process_rgb_preserving_alpha()` + `parse_hex/format_hex` in lib | MED | S-M | quantize.py:58-85; denoise.py:50-68; parse_color ×5 |
| 7 | Golden-bytes key_matte test + edge fixtures (non-RGBA, 1px, all-transparent) | MED | M | key_matte_test.py (2 constructed tests, no RGB pin) |
| 8 | Route `pair_align.save` + slice_regions zip through atomic_io | LOW | S | pair_align.py:180; slice_regions.py:267-282 |
| 9 | Make `__init__.py` placement consistent; document image/<tool> vs canvas/tools split | LOW | S | leaf pkgs have `__init__.py`, parents don't |
| 10 | Add `skipUnless(venv)` / a discover-runner so Python tests skip cleanly without the venv | LOW | S | only worker.test.mjs skips; `_test.py` hard-import numpy |

Interpreter/venv health: `requirements.txt` exact-pinned (numpy==2.1.1, scipy==1.17.1,
Pillow==12.2.0); `setup_python.mjs` creates `.venv` from `py -3.12`, verifies imports, has the
Avast-TLS `--trusted-host` retry. `studio.config.json pythonPath=.venv/Scripts/python.exe`,
config-only resolution confirmed. No issues.
