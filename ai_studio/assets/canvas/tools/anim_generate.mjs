// Anim-card generation seam for ops.generateAnimFromCard (T0265 increment 1). Structural
// mirror of tools/recipe_generate.mjs: the DEFAULT generator ops.mjs falls back to when the
// caller does not inject its own `generators`. Where recipe_generate.mjs shells the image
// generators, this module is the ONE place the canvas reaches into the Track B video pipeline
// (ai_studio/assets/tools/video): it orchestrates the generate -> frames -> matte STAGES on a
// single run folder and STOPS at matte (the sprite SHEET is NOT packed here — that is a
// derived export, exportFlipbookSheet in increment 2, built from the EDITED frame sequence).
//
// Seam shape (the ONE contract ops.mjs and the tests both hold):
//   run({ keyframePaths, motion, profile, seed, matte, gen_fps })
//     -> { framePaths: string[], meta: { frame_w, frame_h, fps, seed }, runDir }
// `meta.fps` is the ACTUAL generation fps (gen_fps steers the WORKFLOW now, not playback — so
// the flipbook plays at 1:1, never slow-motion); `meta.seed` is the RESOLVED seed the run
// actually used (runGenerate rolls a random one when the card left seed=null) — provenance
// records the reproducible value, not `null`.
//
// `framePaths` are the per-frame RGBA matte PNGs on disk under `runDir` (OUTSIDE the repo,
// under videoGenRoot); ops.mjs imports them ONE BY ONE into the project's files/ via
// store.addFile (content-addressed) — never a packed sheet. `runDir` is provenance only.
//
// v1 does NOT autostart ComfyUI: the generate stage checks 127.0.0.1:8188 and throws a LOUD
// error printing the exact start command when it is down (see video/generate/generate.mjs).
// Increment 1 is PLAIN I2V — exactly ONE keyframe; >1 keyframe (FLF / piecewise) is
// increment 3 and is refused loudly here too, mirroring the op's own guard.
//
// The generator is INJECTABLE: ops.generateAnimFromCard takes an optional `generators`
// object and uses `generators.run` when present, so the test suite passes a fake that returns
// N synthetic RGBA frames and NEVER spawns ComfyUI / CorridorKey (the T0238 GPU-free contract,
// carried to the video route).
import { existsSync, readdirSync } from "node:fs";
import { join } from "node:path";

import { REPO_ROOT } from "../../tools/video/_lib.mjs";
import { runGenerate } from "../../tools/video/generate/generate.mjs";
import { runFrames } from "../../tools/video/frames/frames.mjs";
import { runMatte } from "../../tools/video/matte/matte.mjs";

// The matte stage writes frame_%03d.png (RGBA) + report.json into <runDir>/matte. Collect the
// frame PNGs in NUMERIC order (%03d is a MINIMUM width — a run of >999 frames writes frame_1000
// which sorts BEFORE frame_999 lexically; parse the index and sort by number), skipping
// report.json.
function collectMatteFrames(matteDir) {
  if (!existsSync(matteDir)) return [];
  return readdirSync(matteDir)
    .filter((f) => /^frame_\d+\.png$/i.test(f))
    .map((f) => ({ f, n: Number(f.match(/\d+/)[0]) }))
    .sort((a, b) => a.n - b.n)
    .map(({ f }) => join(matteDir, f));
}

// The DEFAULT anim generator ops.generateAnimFromCard falls back to. Runs the real Track B
// stages in order and stops at matte. Each stage is LOUD on its own failure (ComfyUI down, a
// missing interpreter, a missing venv) — no silent fallback, mirroring the video run.mjs laws.
export async function runAnimGenerate({
  root = REPO_ROOT,
  keyframePaths,
  motion,
  profile = "draft",
  seed = null,
  matte = "corridorkey",
  gen_fps = null,
} = {}) {
  if (!Array.isArray(keyframePaths) || keyframePaths.length !== 1) {
    throw new Error(
      `anim_generate: increment 1 is plain I2V — exactly 1 keyframe required, got ${
        Array.isArray(keyframePaths) ? keyframePaths.length : typeof keyframePaths
      } (multi-keyframe FLF/piecewise is increment 3)`,
    );
  }
  if (!motion || !String(motion).trim()) throw new Error("anim_generate requires a non-empty motion text");

  // 1. generate: (art image + motion) -> mp4. LOUD when ComfyUI is not reachable (v1 does not
  //    autostart — the stage prints the exact start command). gen_fps is passed HERE (§1.1: it
  //    steers the workflow's render fps), not applied to playback afterwards.
  const gen = await runGenerate({
    root,
    image: keyframePaths[0],
    text: String(motion),
    profile,
    seed: seed == null ? undefined : seed,
    fps: gen_fps == null ? undefined : gen_fps,
  });
  const runDir = gen.runDir;

  // 2. frames: mp4 -> RGB PNG frames (ComfyUI portable embedded Python / PyAV).
  await runFrames({ root, runDir });

  // 3. matte: RGB frames -> RGBA frames. STOP here (no sheet). `matte` is the card's tool
  //    choice (corridorkey | key_matte); screenColor stays the pipeline default (green).
  const matteRes = await runMatte({ root, runDir, tool: matte, screenColor: "green" });

  const framePaths = collectMatteFrames(matteRes.matteDir);
  if (!framePaths.length) throw new Error(`anim_generate: matte produced no RGBA frames in ${matteRes.matteDir}`);

  // fps is the ACTUAL generation fps (gen_fps already steered the workflow above, so
  // gen.params.fps IS the override when one was set) — the flipbook plays back at this exact
  // rate, no slow-motion. seed is the RESOLVED seed runGenerate rolled/used (never null, even
  // when the card left seed=null) so provenance is reproducible. frame_w/frame_h are provenance
  // only — ops.mjs derives the flipbook box from the ACTUAL imported frame pixels
  // (store.addFile), never from these numbers.
  const fps = Number(gen.params?.fps);
  if (!Number.isFinite(fps) || fps <= 0) {
    throw new Error(`anim_generate: generation reported a non-positive fps (${JSON.stringify(gen.params?.fps)})`);
  }
  return {
    framePaths,
    meta: {
      frame_w: gen.params?.width ?? null,
      frame_h: gen.params?.height ?? null,
      fps,
      seed: gen.params?.seed ?? null,
    },
    runDir,
  };
}

// The injectable default: ops.generateAnimFromCard uses `generators.run` when a `generators`
// object is supplied, else this. Tests pass their own `{ run }`.
export const defaultAnimGenerators = { run: runAnimGenerate };
