// Dual-plate generation seam for ops.alphaDualPlateGenerate (T0238/T0248).
//
// ONE place for the codex edit-chain invocations the AUTOMATIC dual-plate flow needs. Both
// the WHITE-plate prompt (T0248) and the BLACK-plate prompt (T0238) are copied verbatim
// from .codex/skills/nt-asset-image-generation/scripts/gen_dual_plate.sh's white-plate and
// black-plate steps — the SAME chain the reference script runs: generate the white plate
// from arbitrary source art FIRST, then generate the black plate as an EDIT OF THE WHITE
// PLATE (never independently — that lets the model redraw the subject -> ghosting). The
// command line shells out to that SAME skill's generate_image.py — never a second, drifted
// copy of the generation call.
//
// `generatePlate` (the DEFAULT generator ops.mjs falls back to) is a GENERIC "edit this
// input image with this prompt" seam — it backs BOTH the white-plate and the black-plate
// steps; ops.mjs decides which prompt/input to send on each call (an injected `generator`
// stands in for it in every test, so codex never runs in the suite). The prompt/command
// builders here are pure (no spawn), so tests exercise argument construction directly.
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

// The WHITE-plate prompt gen_dual_plate.sh sends as an EDIT of the ARBITRARY source art —
// verbatim text (T0248: the step T0238 wrongly collapsed — an existing element's own
// pixels are NOT always a flat light background, so this generates one FIRST, exactly like
// the script's white-plate step, before any dark-plate edit runs). `extra` (T0238's
// optional `prompt?`) is appended the same way as the black prompt — additional subject
// description, never reordering the lock/background clauses.
export function buildWhitePlatePrompt(extra) {
  const base =
    "Edit the input image: replace its background with solid flat white #ffffff. " +
    `${flatBackgroundClause("white #ffffff")} ${SUBJECT_LOCK} Output the same subject on pure white #ffffff.`;
  const extraText = extra != null ? String(extra).trim() : "";
  return extraText ? `${base} ${extraText}` : base;
}

// The BLACK-plate prompt gen_dual_plate.sh sends as an EDIT of the white/light plate —
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

// Pure command-line builder (no spawn) — the ONE generic argv shape that backs BOTH
// gen_dual_plate.sh steps: its white-plate step (`python "$GEN" --input-image "$SOURCE"
// --prompt "$WHITE_PROMPT" --out "$WHITE" ...`) and its black-plate step (same shape,
// `--input-image "$WHITE" --prompt "$BLACK_PROMPT"`) — so tests can assert the argv without
// ever touching codex.
export function buildGeneratePlateCommand({ inputPngPath, prompt, outPath, size = DEFAULT_SIZE, quality = DEFAULT_QUALITY } = {}) {
  if (!inputPngPath) throw new Error("buildGeneratePlateCommand requires inputPngPath");
  if (!prompt) throw new Error("buildGeneratePlateCommand requires prompt");
  if (!outPath) throw new Error("buildGeneratePlateCommand requires outPath");
  return {
    command: "python",
    args: [GENERATE_IMAGE_SCRIPT, "--input-image", inputPngPath, "--prompt", prompt, "--out", outPath, "--size", size, "--quality", quality],
  };
}

// The DEFAULT plate generator ops.alphaDualPlateGenerate falls back to when the caller does
// not inject its own `generator` — the injectable seam contract is `generatePlate({
// inputPngPath, prompt}) -> Buffer`. It is GENERIC: ops.mjs calls it once with the element's
// own png + the white-plate prompt (T0248's new first step, non-flat sources only), then
// again with the stored white plate + the black-plate prompt (T0238's original step) —
// exactly the two edits gen_dual_plate.sh chains. Spawns generate_image.py into a fresh
// throwaway temp path per call (never generate_image.py's own hash-skip cache — every
// attempt, including a retry, must be a REAL new generation). Codex/network failures
// surface as a loud Error; there is no silent fallback.
export async function generatePlate({ inputPngPath, prompt } = {}) {
  if (!inputPngPath) throw new Error("generatePlate requires inputPngPath");
  if (!prompt) throw new Error("generatePlate requires prompt");
  const workDir = mkdtempSync(join(tmpdir(), "canvas-dualgen-"));
  try {
    const outPath = join(workDir, "plate.png");
    const { command, args } = buildGeneratePlateCommand({ inputPngPath, prompt, outPath });
    await execFileAsync(command, args, { timeout: GENERATE_TIMEOUT_MS, maxBuffer: 16 * 1024 * 1024 });
    return readFileSync(outPath);
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}
