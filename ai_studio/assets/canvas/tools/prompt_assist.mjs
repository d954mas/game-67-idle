// Prompt-assist seam for ops.expandRecipePrompt / ops.extractFromElement (T0239 increment
// 4). The ONE place the codex TEXT/VISION spawn lives — structural mirror of
// tools/recipe_generate.mjs / tools/dual_plate_generate.mjs: pure instruction builders
// (tested directly, no spawn) + the DEFAULT impls ops.mjs falls back to when the caller does
// not inject its own `assistant`. Tests always inject a fake assistant, so codex NEVER
// spawns in the suite (the T0238 contract, extended to text/vision).
//
// FINAL shape (lead, 2026-07-03, after two earlier drafts of this increment): extraction is
// ONE unified vision call (extractFromImage) that returns everything the "Extracted"
// inspector section + BOTH card-promotion gestures need in a single JSON object — a
// complete standalone prompt (prompt_full), a subject-only prompt (prompt_subject, no style
// descriptors, composes with a separately-linked style card), and the style breakdown
// (style_block/palette/materials/lighting/composition/constraints_block) + a one-line
// description. Minting a card from that data is a SEPARATE, non-codex "promotion" op in
// ops.mjs (promoteExtractedRecipe/promoteExtractedStyle) — this module only ever runs codex
// ONCE per extraction.
//
// Transport: `codex exec` — the CLI's own non-interactive mode, already used in this repo
// by scripts/generate_image.py's gen_codex path and .codex/skills/.../codex_imagegen.sh for
// image generation, and by the T0251 research note as an independent vision judge (`codex
// exec -i <ref> -i <out>`; T0251). Two invocation shapes,
// BOTH verified live on this box on 2026-07-03 (`codex exec --help`; two real spawns):
//
//   - TEXT ONLY (expandPrompt): the instruction as a plain positional PROMPT argument —
//     `codex exec --skip-git-repo-check --output-last-message <file> "<instruction>"`.
//   - VISION (extractFromImage): ONE `-i <imagePath>`. `-i, --image <FILE>...` is a
//     variadic clap option that GREEDILY consumes any further non-flag token — a plain-text
//     instruction placed AFTER `-i <imagePath>` would be silently swallowed as a SECOND
//     image path (the T0251 variadic-argument guard, verified
//     against this box's actual `codex exec --help`, not assumed). The documented,
//     empirically-verified workaround (same note): pass a bare "-" as the PROMPT positional
//     — codex's own --help: "If not provided as an argument (or if `-` is used),
//     instructions are read from stdin" — and pipe the instruction text over stdin instead:
//     `codex exec --skip-git-repo-check --output-last-message <file> -i <imagePath> -`.
//
//   Both shapes pass `--output-last-message <file>` and read THAT file for the answer,
//   never raw stdout: verified empirically (2026-07-03, a live `codex exec` call on this
//   repo) that plain stdout carries the full session banner + hook-warning + transcript
//   noise around the actual answer, so `--output-last-message` (the CLI's own documented
//   clean-answer file) is the only reliable place to read a parseable result from — matches
//   this repo's established "verify by file, not the transcript" law (codex_imagegen.sh's
//   own comment; the agy `.seen.txt` guard in recipe_generate.mjs). Neither shape needs
//   `--dangerously-bypass-approvals-and-sandbox`: a plain non-interactive `codex exec` text/
//   vision reply needs no tool execution, so it already runs unattended (verified: a live
//   call with neither that flag nor any sandbox override completed cleanly).
import { execFile } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);

// Generous but bounded: a text/vision reasoning call, lighter than image generation
// (recipe_generate.mjs's GENERATE_TIMEOUT_MS = 500s) but still real codex minutes.
const CODEX_TIMEOUT_MS = 300_000;

// Resolve the CLI through PATH. Windows uses its npm command shim; prompt text stays on stdin.
export const CODEX_COMMAND = process.platform === "win32" ? "cmd.exe" : "codex";
const CODEX_ARG_PREFIX = process.platform === "win32" ? ["/d", "/s", "/c", "codex.cmd"] : [];

// ---- Expand-prompt ---------------------------------------------------------------------

// Pure instruction builder (no spawn). Asks codex to expand a short idea into a labeled,
// seven-section generation-prompt template. When `styleBlock` is given (a linked style
// card's resolved prompt — see ops.expandRecipePrompt), it is embedded as the [STYLE]
// section's source material rather than left for codex to invent. [CONSTRAINTS] must
// restate every exclusion/negative implied by the idea or style source. Deliberately never
// brings up a random seed anywhere — this pipeline has no usable seed control, so there is
// nothing to restate about one.
export function buildExpandInstruction({ prompt, styleBlock } = {}) {
  if (!prompt) throw new Error("buildExpandInstruction requires prompt");
  const trimmedPrompt = String(prompt).trim();
  const trimmedStyle = styleBlock != null ? String(styleBlock).trim() : "";
  const styleClause = trimmedStyle
    ? `Use the following STYLE SOURCE MATERIAL verbatim as the basis for the [STYLE] section — do not invent a different style:\n"""${trimmedStyle}"""`
    : "No style reference was supplied — write a [STYLE] section that stays neutral/unspecified rather than inventing an arbitrary art style.";
  return (
    `Expand this short image-generation idea into a detailed, production-ready generation prompt: "${trimmedPrompt}"\n\n` +
    `${styleClause}\n\n` +
    "Write the result as a labeled template with EXACTLY these seven section headers, in this order, each on its own line, " +
    "each followed by 1-3 sentences of concrete detail:\n" +
    "[TASK]\n[SUBJECT]\n[STYLE]\n[COMPOSITION]\n[BACKGROUND]\n[CONSTRAINTS]\n[OUTPUT]\n\n" +
    "[CONSTRAINTS] must restate every exclusion/negative implied by the idea or the style source material as explicit " +
    'negative statements (what must NOT appear — e.g. "no text", "no watermark", "no extra limbs", "no background clutter").\n\n' +
    "Output ONLY the filled-in seven-section template above, nothing before it and nothing after it — no preamble, " +
    "no explanation, no markdown code fence."
  );
}

// Pure command builder (no spawn) — TEXT ONLY, no attached image, so there is no `-i`
// variadic-swallow risk; the instruction travels over stdin on every platform.
export function buildTextCommand({ instruction, outputPath } = {}) {
  if (!instruction) throw new Error("buildTextCommand requires instruction");
  if (!outputPath) throw new Error("buildTextCommand requires outputPath");
  return {
    command: CODEX_COMMAND,
    args: [...CODEX_ARG_PREFIX, "exec", "--skip-git-repo-check", "--output-last-message", outputPath, "-"],
  };
}

async function runCodexText(instruction) {
  const workDir = mkdtempSync(join(tmpdir(), "canvas-promptassist-text-"));
  try {
    const outputPath = join(workDir, "last.txt");
    const { command, args } = buildTextCommand({ instruction, outputPath });
    const promise = execFileAsync(command, args, { timeout: CODEX_TIMEOUT_MS, maxBuffer: 16 * 1024 * 1024 });
    // Close stdin IMMEDIATELY: execFile always gives the child a stdin pipe, and codex exec
    // reads "additional input from stdin" whenever stdin is a pipe — left open, it stalls
    // waiting for EOF and the call dies (verified live 2026-07-03: same argv succeeds with
    // stdin closed, fails with it open). The vision path below closes it via write+end.
    if (promise.child && promise.child.stdin) {
      promise.child.stdin.write(instruction);
      promise.child.stdin.end();
    }
    await promise;
    try {
      return readFileSync(outputPath, "utf8").trim();
    } catch {
      return ""; // codex exited 0 but wrote no last-message file — treated as empty below
    }
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// The DEFAULT expand impl ops.expandRecipePrompt falls back to. Codex/network failure is a
// loud Error (execFile rejects on a nonzero exit); an empty/whitespace answer is ALSO
// loud — no silent fallback to the un-expanded prompt.
export async function expandPrompt({ prompt, styleBlock } = {}) {
  if (!prompt) throw new Error("expandPrompt requires prompt");
  const instruction = buildExpandInstruction({ prompt, styleBlock });
  const result = await runCodexText(instruction);
  if (!result) throw new Error("expandPrompt: codex returned an empty result");
  return result;
}

// ---- Extract (single unified vision schema) ---------------------------------------------

// Pure instruction builder (no spawn), no params. ONE codex vision call extracts EVERYTHING
// the "Extracted" inspector section + both promotion gestures need — a single JSON object,
// so only one vision call is ever spent per extraction; promoteExtractedRecipe/
// promoteExtractedStyle (ops.mjs) spend NO further codex call, they just re-slice this same
// stored blob. `prompt_full` is a COMPLETE standalone prompt (subject + style + composition)
// meant to be pasted into a totally different generator with no other context; `prompt_subject`
// is SUBJECT/CONTENT ONLY with no style descriptors, so it composes cleanly with a
// separately-linked style card (recipe.style_ref) without doubling up style wording.
export function buildExtractInstruction() {
  return (
    "Look at the attached image and extract it as a single JSON object with EXACTLY these keys:\n" +
    '"prompt_full": ONE complete, self-contained image-generation prompt describing the whole image — subject, key ' +
    "attributes, pose/composition, AND its art style/palette/materials/lighting/rendering — written so that someone " +
    "pasting this single prompt into a completely different image generator, with no other context, would get a " +
    "close reproduction of this image;\n" +
    '"prompt_subject": ONE image-generation prompt describing ONLY the SUBJECT — what it is, its key visible ' +
    "attributes, its pose/composition — explicitly EXCLUDING any art-style, rendering, palette, lighting or material " +
    "descriptors (style is captured separately by the other keys below; this one must compose cleanly with a " +
    "different, separately-supplied style);\n" +
    '"style_block": a dense, reusable prose description of the rendering style, palette, materials, lighting and ' +
    "composition, written so it can be pasted as a style reference into a future generation prompt (no subject-" +
    "specific nouns);\n" +
    '"palette": an array of short colour-name strings (e.g. ["warm gold", "deep teal"]) describing the dominant ' +
    "colours seen;\n" +
    '"materials": a short phrase describing surface/material qualities (e.g. "glossy ceramic, soft velvet");\n' +
    '"lighting": a short phrase describing the lighting (e.g. "soft rim light from upper left, warm key");\n' +
    '"composition": a short phrase describing framing/composition style (e.g. "centered hero shot, shallow depth of ' +
    'field");\n' +
    '"constraints_block": a short phrase naming anything this style explicitly AVOIDS (exclusions/negatives) — empty ' +
    "string if none;\n" +
    '"description": ONE plain-language sentence summarizing the image for a human label.\n\n' +
    "Output ONLY that single JSON object — no preamble, no explanation, no markdown code fence, no trailing text."
  );
}

// Pure command builder (no spawn) — WITH ONE attached image. See the module doc for why the
// instruction is NEVER passed as a trailing positional here (the `-i` variadic-swallow
// footgun) — a bare "-" tells codex to read the prompt from stdin instead; the caller writes
// the instruction to the returned child's stdin (see runCodexVision below).
export function buildVisionCommand({ imagePath, outputPath } = {}) {
  if (!imagePath) throw new Error("buildVisionCommand requires imagePath");
  if (!outputPath) throw new Error("buildVisionCommand requires outputPath");
  return {
    command: CODEX_COMMAND,
    args: [...CODEX_ARG_PREFIX, "exec", "--skip-git-repo-check", "--output-last-message", outputPath, "-i", imagePath, "-"],
  };
}

// Strip a single leading/trailing ``` or ```json code fence, if present — codex sometimes
// wraps a JSON reply in one despite being told not to. Anything else (including a reply
// that never was JSON) passes through unchanged so JSON.parse's own error names it.
function stripCodeFence(text) {
  const trimmed = String(text || "").trim();
  const fenced = /^```(?:json)?\s*([\s\S]*?)\s*```$/i.exec(trimmed);
  return fenced ? fenced[1].trim() : trimmed;
}

async function runCodexVision(instruction, imagePath) {
  const workDir = mkdtempSync(join(tmpdir(), "canvas-promptassist-vision-"));
  try {
    const outputPath = join(workDir, "last.txt");
    const { command, args } = buildVisionCommand({ imagePath, outputPath });
    const promise = execFileAsync(command, args, { timeout: CODEX_TIMEOUT_MS, maxBuffer: 16 * 1024 * 1024 });
    // util.promisify(execFile) attaches the live ChildProcess to the returned promise as
    // `.child` (documented Node behavior, verified live on this box 2026-07-03) — this is
    // how the instruction text reaches codex despite the argv itself carrying only "-".
    if (promise.child && promise.child.stdin) {
      promise.child.stdin.write(instruction);
      promise.child.stdin.end();
    }
    await promise;
    try {
      return readFileSync(outputPath, "utf8").trim();
    } catch {
      return "";
    }
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

const REQUIRED_EXTRACT_KEYS = ["prompt_full", "prompt_subject", "style_block", "description"];

// The DEFAULT extract impl ops.extractFromElement falls back to. Parses the reply as JSON
// LOUDLY (strips a common code fence first; a non-JSON reply after that throws naming the
// raw reply head — no silent fallback), then loudly requires every key ops.extractFromElement
// depends on (prompt_full/prompt_subject/style_block/description, all non-empty strings)
// before returning the parsed object verbatim — the op itself reshapes style_block/palette/
// materials/lighting/composition/constraints_block under a `style` sub-object; this module
// only guarantees the required keys exist.
export async function extractFromImage({ imagePath } = {}) {
  if (!imagePath) throw new Error("extractFromImage requires imagePath");
  const instruction = buildExtractInstruction();
  const raw = await runCodexVision(instruction, imagePath);
  if (!raw) throw new Error("extractFromImage: codex returned an empty result");
  const stripped = stripCodeFence(raw);
  let parsed;
  try {
    parsed = JSON.parse(stripped);
  } catch {
    throw new Error(
      `extractFromImage: codex reply was not valid JSON after stripping code fences — raw reply started: ${JSON.stringify(stripped.slice(0, 160))}`,
    );
  }
  if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
    throw new Error(`extractFromImage: codex reply parsed to a non-object JSON value: ${JSON.stringify(stripped.slice(0, 160))}`);
  }
  for (const key of REQUIRED_EXTRACT_KEYS) {
    if (typeof parsed[key] !== "string" || !parsed[key].trim()) {
      throw new Error(`extractFromImage: codex JSON reply is missing a non-empty "${key}" — raw reply started: ${JSON.stringify(stripped.slice(0, 160))}`);
    }
  }
  return parsed;
}
