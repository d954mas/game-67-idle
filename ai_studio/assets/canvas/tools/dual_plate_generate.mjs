// Dark-plate generation seam for ops.alphaDualPlateGenerate (T0238).
//
// ONE place for the codex edit-chain invocation the AUTOMATIC dual-plate flow needs: the
// subject-lock BLACK_PROMPT text is copied verbatim from
// .codex/skills/nt-asset-image-generation/scripts/gen_dual_plate.sh's black-plate step
// (the "edit the white plate to a black background, subject locked" chain), and the
// command line shells out to that SAME skill's generate_image.py — never a second,
// drifted copy of the generation call. The prompt/command builders here are pure (no
// spawn), so tests exercise argument construction without ever invoking codex; only
// `generateDarkPlate` (the DEFAULT generator ops.mjs falls back to) actually spawns a
// process, and only when a caller does not inject its own `generator`.
import { execFile } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath, URL } from "node:url";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);

// Repo-relative: ai_studio/assets/canvas/tools/ -> repo root -> .codex/skills/....
export const GENERATE_IMAGE_SCRIPT = fileURLToPath(
  new URL("../../../../.codex/skills/nt-asset-image-generation/scripts/generate_image.py", import.meta.url),
);

const DEFAULT_SIZE = "1024x1024";
const DEFAULT_QUALITY = "high";
// generate_image.py's own QUALITY_TIMEOUTS["high"] is 480s; give the Node-side spawn a
// little headroom over that so OUR timeout never races the script's own curl --max-time.
const GENERATE_TIMEOUT_MS = 500_000;

// Shared subject-lock clause — verbatim from gen_dual_plate.sh's $LOCK (the part that
// prevents the model from redrawing the subject between plates).
const SUBJECT_LOCK =
  "Keep the subject EXACTLY as in the input image: same position, same scale, same rotation, same silhouette, " +
  "every internal detail, every anti-aliased edge, and all soft elements (glow, particles, smoke, transparency, " +
  "cast shadow) pixel-for-pixel identical. Do NOT re-render, re-light, restyle, sharpen, denoise, upscale, crop, " +
  "pad, rotate, mirror, add, remove, or recolour the subject. If you cannot keep it identical, return it " +
  "unchanged rather than improving it.";

// Background-fill clause — verbatim from gen_dual_plate.sh's flatbg() template.
function flatBackgroundClause(colorLabel) {
  return (
    `Fill the ENTIRE canvas edge-to-edge with solid flat ${colorLabel}. No gradient, no vignette, no texture, ` +
    "no noise, no lighting falloff, no reflection, and do NOT add any new shadow on the background."
  );
}

// The black-plate prompt gen_dual_plate.sh sends as an EDIT of the white/light plate —
// verbatim text, frames the edit as a pure background-color swap (not a redraw), which is
// what keeps a glow/soft-alpha subject consistent enough to pass the pair gate. `extra`
// (T0238's optional `prompt?`) is an additional subject description APPENDED after the
// locked prompt — it never replaces or reorders the lock/background clauses.
export function buildBlackPlatePrompt(extra) {
  const base =
    "This is a BACKGROUND-COLOR SWAP only, not a redraw. The subject is final. Output the EXACT same image " +
    "pixel-for-pixel, with the ONLY change being the flat background recoloured to solid black #000000. " +
    `${flatBackgroundClause("black #000000")} ${SUBJECT_LOCK} Do NOT re-render or re-light the subject for the ` +
    "dark background. Output the same subject on pure black #000000.";
  const extraText = extra != null ? String(extra).trim() : "";
  return extraText ? `${base} ${extraText}` : base;
}

// Pure command-line builder (no spawn) — exactly what gen_dual_plate.sh's black-plate step
// runs (`python "$GEN" --input-image "$WHITE" --prompt "$BLACK_PROMPT" --out "$cand" --size
// "$SIZE" --quality high`), so tests can assert the argv without ever touching codex.
export function buildGenerateDarkPlateCommand({ lightPngPath, prompt, outPath, size = DEFAULT_SIZE, quality = DEFAULT_QUALITY } = {}) {
  if (!lightPngPath) throw new Error("buildGenerateDarkPlateCommand requires lightPngPath");
  if (!prompt) throw new Error("buildGenerateDarkPlateCommand requires prompt");
  if (!outPath) throw new Error("buildGenerateDarkPlateCommand requires outPath");
  return {
    command: "python",
    args: [GENERATE_IMAGE_SCRIPT, "--input-image", lightPngPath, "--prompt", prompt, "--out", outPath, "--size", size, "--quality", quality],
  };
}

// The DEFAULT dark-plate generator ops.alphaDualPlateGenerate falls back to when the
// caller does not inject its own `generator` — the injectable seam contract is
// `generateDarkPlate({lightPngPath, prompt}) -> Buffer`. Spawns generate_image.py exactly
// as gen_dual_plate.sh's black-plate step does (an EDIT of the light plate), into a fresh
// throwaway temp path per call (never generate_image.py's own hash-skip cache — every
// attempt, including a retry, must be a REAL new generation). Codex/network failures
// surface as a loud Error; there is no silent fallback.
export async function generateDarkPlate({ lightPngPath, prompt } = {}) {
  if (!lightPngPath) throw new Error("generateDarkPlate requires lightPngPath");
  if (!prompt) throw new Error("generateDarkPlate requires prompt");
  const workDir = mkdtempSync(join(tmpdir(), "canvas-dualgen-"));
  try {
    const outPath = join(workDir, "dark_plate.png");
    const { command, args } = buildGenerateDarkPlateCommand({ lightPngPath, prompt, outPath });
    await execFileAsync(command, args, { timeout: GENERATE_TIMEOUT_MS, maxBuffer: 16 * 1024 * 1024 });
    return readFileSync(outPath);
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}
