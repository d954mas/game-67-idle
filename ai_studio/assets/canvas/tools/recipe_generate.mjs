// Recipe-card generation seam for ops.generateFromRecipe (T0239 increment 2). Structural
// mirror of tools/dual_plate_generate.mjs: pure argv/instruction builders (tested directly,
// no spawn) + the DEFAULT generators ops.mjs falls back to when the caller does not inject
// its own `generators`. Unlike dual_plate_generate.mjs's single generic seam, a recipe card
// has an ENGINE choice (R2/R3: codex | gemini | both), so this module exports TWO default
// generators behind the SAME seam shape: `generate({ prompt, refPaths, params }) ->
// Buffer|path` (mirrors dual_plate_generate.mjs's `generatePlate({inputPngPath, prompt}) ->
// Buffer` contract — outPath is chosen INTERNALLY by the generator, same as generatePlate
// never taking one; only the pure command builders below take an explicit outPath).
//
// codex -> shells the SAME .codex/skills/nt-asset-image-generation/scripts/generate_image.py
// dual_plate_generate.mjs uses, with --prompt/--out/--size/--quality/--model from
// recipe.params and one --input-image per ref path (T0240 landed: generate_image.py now
// forwards --input-image on BOTH credential paths — gen_codex AND gen_rest).
//
// gemini -> shells the agy (Antigravity) CLI per the skill's Path B (references/
// generation-paths.md): a headless, agentic image-gen call verified by OUTPUT FILE
// EXISTENCE, never stdout (agy "can be quiet under non-TTY" per the skill notes; gen_both.sh
// uses the same file-existence check). agy ref support is UNVERIFIED (R2) — ops.mjs is the
// ONE place that enforces the loud refusal law for a card with refs on engine=gemini; this
// module's `generateImageGemini` stays a plain "prompt -> image" generator with no ref
// plumbing at all, so there is no way for a ref to silently reach agy from here.
//
// Tests inject fake generators for BOTH engines, so neither codex nor agy ever spawns in the
// suite (the T0238 contract, extended to two engines).
import { execFile, spawn } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath, URL } from "node:url";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);

// ---- codex engine (Path C: generate_image.py, same script dual_plate_generate.mjs uses) --

// Repo-relative: ai_studio/assets/canvas/tools/ -> repo root -> .codex/skills/....
export const GENERATE_IMAGE_SCRIPT = fileURLToPath(
  new URL("../../../../.codex/skills/nt-asset-image-generation/scripts/generate_image.py", import.meta.url),
);

const DEFAULT_SIZE = "1024x1024";
const DEFAULT_QUALITY = "high";
const DEFAULT_MODEL = "gpt-image-2";
// Mirrors dual_plate_generate.mjs's own headroom over generate_image.py's
// QUALITY_TIMEOUTS["high"] (480s) so OUR timeout never races the script's own curl --max-time.
const GENERATE_TIMEOUT_MS = 500_000;

// Pure command-line builder (no spawn) — argv shape: --prompt, one --input-image per ref
// (0-5), --size/--quality/--model from recipe.params, --out last. No --format (defaults to
// png in generate_image.py, same as dual_plate_generate.mjs). No --background: bg_key is
// advisory metadata for a LATER cutout step (alphaDualPlateGenerate), never fed to
// generation itself in this increment.
export function buildGenerateCommand({ prompt, refPaths = [], size = DEFAULT_SIZE, quality = DEFAULT_QUALITY, model = DEFAULT_MODEL, outPath } = {}) {
  if (!prompt) throw new Error("buildGenerateCommand requires prompt");
  if (!outPath) throw new Error("buildGenerateCommand requires outPath");
  const args = [GENERATE_IMAGE_SCRIPT, "--prompt", prompt];
  for (const ref of refPaths || []) args.push("--input-image", ref);
  args.push("--size", size, "--quality", quality, "--model", model, "--out", outPath);
  return { command: "python", args };
}

// The DEFAULT codex generator ops.generateFromRecipe falls back to for engine="codex" (and
// the codex half of engine="both"). Spawns generate_image.py into a fresh throwaway temp
// path per call (never generate_image.py's own hash-skip cache — every recipe run must be a
// REAL new generation, same law generatePlate documents). Codex/network failure surfaces as
// a loud Error (execFile rejects on a nonzero exit / SystemExit) — no silent fallback.
export async function generateImageCodex({ prompt, refPaths = [], params = {} } = {}) {
  if (!prompt) throw new Error("generateImageCodex requires prompt");
  const workDir = mkdtempSync(join(tmpdir(), "canvas-recipegen-codex-"));
  try {
    const outPath = join(workDir, "gen.png");
    const { command, args } = buildGenerateCommand({
      prompt,
      refPaths,
      size: params.size,
      quality: params.quality,
      model: params.model,
      outPath,
    });
    await execFileAsync(command, args, { timeout: GENERATE_TIMEOUT_MS, maxBuffer: 16 * 1024 * 1024 });
    return readFileSync(outPath);
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// ---- gemini engine (Path B: agy / Antigravity CLI, headless) -----------------------------

// Windows-specific absolute path, verbatim from generation-paths.md / gen_both.sh — the
// SAME hardcoded location the skill's own compare-mode script uses (this machine only; no
// PATH lookup, matching the skill's own precedent).
export const AGY_PATH = "C:/Users/ROG/AppData/Local/agy/bin/agy.exe";
const AGY_TIMEOUT_MS = 300_000; // fallback/compare engine; generous but bounded

// A size string ("WxH") -> a short aspect label for the agy prompt instruction. Square sizes
// (the recipe default) read as "square 1:1" (gen_both.sh's own default aspect text); any
// other WxH reads as "<w>:<h> aspect ratio". An unparseable size falls back to square (the
// common case) rather than emitting a malformed instruction.
function aspectLabel(size) {
  const match = /^(\d+)x(\d+)$/.exec(String(size || "").trim());
  if (!match) return "square 1:1";
  const [, wStr, hStr] = match;
  return wStr === hStr ? "square 1:1" : `${wStr}:${hStr} aspect ratio`;
}

// Pure instruction-text builder (no spawn) — verbatim template from gen_both.sh's agy call,
// generalized with the aspect label + an explicit outPath. Never mentions reference images
// (agy ref support is unverified; ops.mjs refuses loudly before this is ever reached on a
// card with refs on engine=gemini).
export function buildAgyInstruction({ prompt, size, outPath } = {}) {
  if (!prompt) throw new Error("buildAgyInstruction requires prompt");
  if (!outPath) throw new Error("buildAgyInstruction requires outPath");
  const aspect = aspectLabel(size);
  return (
    `Use your built-in image generation to create one real raster image (not code-drawn), ${aspect}: ${prompt}. ` +
    `Save the PNG to ${outPath} . Do not write or run any drawing code.`
  );
}

// Pure command-line builder (no spawn) — mirrors gen_both.sh's agy invocation:
// `agy.exe --dangerously-skip-permissions -p "<instruction>"`.
export function buildAgyCommand({ prompt, size, outPath } = {}) {
  const instruction = buildAgyInstruction({ prompt, size, outPath });
  return { command: AGY_PATH, args: ["--dangerously-skip-permissions", "-p", instruction] };
}

// Run agy with stdin/stdout/stderr all redirected away (mirrors the reference script's
// `< /dev/null > /dev/null 2>&1`) and NEVER reject on a spawn/exit failure — agy "can be
// quiet under non-TTY" and the skill's own verification law is "confirm by file, not
// stdout", so a nonzero exit or a missing binary is not itself the failure signal; the
// caller checks for the output file afterward and raises the loud error itself if it is
// missing. `execFile`'s captured stdio isn't used here on purpose (spawn, not execFile) —
// we never need agy's stdout/stderr text, only whether the file landed.
function runAgy(command, args, timeoutMs) {
  return new Promise((resolvePromise) => {
    let child;
    try {
      child = spawn(command, args, { stdio: ["ignore", "ignore", "ignore"], windowsHide: true });
    } catch {
      resolvePromise(); // spawn itself failed (e.g. binary missing) -> fall through to the file check
      return;
    }
    const timer = setTimeout(() => {
      try {
        child.kill();
      } catch {
        // already exited
      }
    }, timeoutMs);
    const done = () => {
      clearTimeout(timer);
      resolvePromise();
    };
    child.on("close", done);
    child.on("error", done);
  });
}

// The DEFAULT gemini generator ops.generateFromRecipe falls back to for engine="gemini" (and
// the gemini half of engine="both"). A plain "prompt -> image" seam — no ref plumbing at all
// (see module doc). Verifies by output FILE EXISTENCE, never stdout; a missing file after
// the run is the loud-error signal (names the reference doc for a human to diagnose further).
export async function generateImageGemini({ prompt, refPaths = [], params = {} } = {}) {
  if (!prompt) throw new Error("generateImageGemini requires prompt");
  const workDir = mkdtempSync(join(tmpdir(), "canvas-recipegen-agy-"));
  try {
    const outPath = join(workDir, "gen.png");
    const { command, args } = buildAgyCommand({ prompt, size: params.size, outPath });
    await runAgy(command, args, AGY_TIMEOUT_MS);
    if (!existsSync(outPath)) {
      throw new Error(
        `agy produced no output file (expected ${outPath}) — see .codex/skills/nt-asset-image-generation/references/generation-paths.md Path B`,
      );
    }
    return readFileSync(outPath);
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}
