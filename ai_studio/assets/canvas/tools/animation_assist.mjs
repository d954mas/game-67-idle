// Text -> animation seam for ops.animateElementFromText (T0264: the text->animation bridge).
// The ONE place the codex TEXT/VISION spawn for authoring an `ai_studio.canvas.animation.v1`
// spec lives — structural mirror of tools/prompt_assist.mjs: a pure instruction builder
// (buildAnimateInstruction, tested directly, no spawn) + the DEFAULT runner runAnimateFromText
// falls back to when the caller does not inject its own `runner`. Tests always inject a fake
// runner, so codex NEVER spawns in the suite (the T0238 contract, extended to this op).
//
// Shape: the lead selects art, types a description ("крылья медленно машут"), and gets a live
// preview playing. runAnimateFromText builds the labeled-sections instruction, spends ONE
// codex call, and parses the reply STRICTLY into the animation object {v:1, channels:[...]}.
// The op (ops.animateElementFromText) is the loud validateAnimation gate on the parsed object —
// this module only guarantees the reply parsed to an object, exactly as prompt_assist's own
// default impls leave schema-shaping to their op.
//
// Transport is IDENTICAL to prompt_assist's (see that module's doc for the empirical
// verification of every flag): `node <codex.js> exec --skip-git-repo-check
// --output-last-message <file> ...`, reply read from the last-message FILE never stdout, stdin
// CLOSED immediately (codex stalls on an open stdin pipe). Two shapes, reusing prompt_assist's
// already-verified command builders so the argv shape has ONE source of truth:
//   - TEXT ONLY  (imagePath null): buildTextCommand — the instruction is a plain positional
//     PROMPT argument (no `-i`, so no variadic-swallow risk). Used for TEXT elements, which
//     have no source image for the model to see.
//   - VISION     (imagePath set):  buildVisionCommand — `-i <imagePath>` + a bare "-" positional
//     so codex reads the instruction from stdin (the `-i` variadic-swallow footgun); the model
//     SEES the art, so amplitudes can be judged against what the sprite actually looks like.
// Unlike prompt_assist's extractFromImage, the reply is parsed STRICTLY with NO code-fence
// stripping — the instruction forbids a fence loudly and a stray fence is a signal to tighten
// the instruction, not to paper over silently (loud-not-lenient, the house law).
import { execFile } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { promisify } from "node:util";
import { buildTextCommand, buildVisionCommand } from "./prompt_assist.mjs";

const execFileAsync = promisify(execFile);

// Same class as prompt_assist's CODEX_TIMEOUT_MS: a text/vision reasoning call, lighter than
// image generation but still real codex minutes.
const CODEX_TIMEOUT_MS = 300_000;

// Pure instruction builder (no spawn). Asks codex to return ONLY the animation JSON for one
// element. Embeds, in labeled sections (prompt_assist house style):
//   [SCHEMA]  — a CONCISE `ai_studio.canvas.animation.v1` description WITH UNITS (the units are
//               load-bearing: the model must know off_y is world pixels, rot is degrees, scale/
//               opacity are multipliers, and that a seamless keyframes loop needs last.v ==
//               first.v). ONE channel per property.
//   [ELEMENT] — the element's name and w/h, with the amplitude-must-be-PROPORTIONAL rule made
//               concrete (a 4px bob on a 1254px wing is invisible) by naming ~5%-of-dimension
//               example magnitudes computed from this element's own size.
//   [REQUEST] — the user's description VERBATIM (never paraphrased — it may be Russian, may name
//               a specific speed/direction the model must honor).
//   [EDIT]    — when currentSpec is present, the current spec JSON + a MODIFY-MINIMALLY directive
//               ("медленнее" = change only period_ms, keep every other field/channel); absent, an
//               author-from-scratch directive.
//   [OUTPUT]  — STRICT JSON, ONLY the {v:1, channels:[...]} object, no prose/markdown/fence.
export function buildAnimateInstruction({ element, currentSpec, text } = {}) {
  if (!element) throw new Error("buildAnimateInstruction requires element");
  const description = text == null ? "" : String(text).trim();
  if (!description) throw new Error("buildAnimateInstruction requires text");
  const name = element.name ? String(element.name) : "element";
  const w = Number(element.w) || 0;
  const h = Number(element.h) || 0;
  const bobPx = Math.max(1, Math.round(h * 0.05));
  const swayPx = Math.max(1, Math.round(w * 0.05));

  const schema =
    "The animation object is {\"v\":1,\"channels\":[...]}, schema ai_studio.canvas.animation.v1. Each channel drives " +
    "exactly ONE property over time; AT MOST ONE channel per property. Properties and their UNITS (a missing channel " +
    "leaves the property at its identity, so it never moves):\n" +
    "  off_x, off_y : world-PIXEL offset ADDED to the element position (identity 0; +off_y moves DOWN)\n" +
    "  rot          : DEGREES added to the element rotation (identity 0; + is clockwise)\n" +
    "  scale        : MULTIPLIER on the element scale (identity 1; 1.1 = 10% bigger)\n" +
    "  opacity      : MULTIPLIER on the element opacity, clamped to 0..1 (identity 1)\n" +
    "Two channel KINDS:\n" +
    "  osc: {\"prop\":\"off_y\",\"kind\":\"osc\",\"amplitude\":N,\"period_ms\":M} — a sine wave, " +
    "value = center + amplitude*sin(2*PI*(t/period_ms + phase)). period_ms MUST be > 0. amplitude is in the property's " +
    "OWN units (pixels for off_x/off_y, degrees for rot, a multiplier delta for scale/opacity — e.g. amplitude 0.1 on " +
    "scale pulses between 0.9 and 1.1). Optional \"phase\" is a fraction of a cycle in [0,1); optional \"center\" " +
    "defaults to the property identity.\n" +
    "  keyframes: {\"prop\":\"scale\",\"kind\":\"keyframes\",\"points\":[{\"t_ms\":0,\"v\":1},{\"t_ms\":600,\"v\":1.1}," +
    "{\"t_ms\":1200,\"v\":1}]} — linear interpolation between points. The FIRST point MUST be t_ms 0; t_ms strictly " +
    "increasing; the loop length is the LAST point's t_ms (time wraps modulo it), so for a SEAMLESS loop the last " +
    "point's v MUST equal the first point's v.";

  const elementClause =
    `The element is named "${name}" and measures ${w}x${h} world units. Every amplitude MUST be PROPORTIONAL to that ` +
    `size — a 4px bob on a ${h}px-tall element is invisible. For THIS element a clearly visible vertical bob is around ` +
    `${bobPx}px (off_y), a visible horizontal sway is around ${swayPx}px (off_x), a visible rotation is a few degrees, ` +
    "and a visible pulse is scale amplitude ~0.05-0.1 or an opacity dip to ~0.6. Scale up or down from there to match " +
    "words like \"slightly\", \"gently\", or \"a lot\".";

  const editClause = currentSpec
    ? "The element ALREADY has this animation:\n" +
      `${JSON.stringify(currentSpec)}\n` +
      "MODIFY IT MINIMALLY to satisfy the request: change ONLY the field(s) the request implies and keep every other " +
      "field and every other channel EXACTLY as-is. \"slower\"/\"медленнее\" = increase period_ms (or the last " +
      "keyframe t_ms) and nothing else; \"faster\"/\"быстрее\" = decrease it; \"bigger\"/\"больше\" = increase " +
      "amplitude; \"less\"/\"меньше\"/\"smaller\" = decrease amplitude. Return the FULL updated animation object, not a diff."
    : "The element has no animation yet — author one from scratch that matches the request.";

  return (
    "You animate a single canvas element by returning its procedural-animation spec.\n\n" +
    `[SCHEMA]\n${schema}\n\n` +
    `[ELEMENT]\n${elementClause}\n\n` +
    `[REQUEST]\n${description}\n\n` +
    `[EDIT]\n${editClause}\n\n` +
    "[OUTPUT]\nOutput ONLY the animation JSON object {\"v\":1,\"channels\":[...]} — nothing before it and nothing " +
    "after it: no preamble, no explanation, no markdown code fence. It must be STRICT JSON that JSON.parse accepts directly."
  );
}

// The DEFAULT codex runner sends the instruction over stdin. Vision additionally passes
// `-i <imagePath>`. Reads the --output-last-message FILE, never
// stdout (see the module doc). Closes stdin immediately either way — codex stalls on an open
// stdin pipe (verified in prompt_assist).
async function runCodex({ instruction, imagePath }) {
  const workDir = mkdtempSync(join(tmpdir(), "canvas-animassist-"));
  try {
    const outputPath = join(workDir, "last.txt");
    const { command, args } = imagePath
      ? buildVisionCommand({ imagePath, outputPath })
      : buildTextCommand({ instruction, outputPath });
    const promise = execFileAsync(command, args, { timeout: CODEX_TIMEOUT_MS, maxBuffer: 16 * 1024 * 1024 });
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

// The DEFAULT impl ops.animateElementFromText falls back to. Builds the instruction, runs the
// injectable `runner` (default runCodex — tests inject a fake that returns a canned reply so
// codex never spawns, the prompt_assist injectable-runner precedent), then parses the reply as
// JSON STRICTLY: an empty reply, a non-JSON reply, or a non-object JSON value all throw loudly
// with the raw reply head named (NO code-fence stripping — the instruction forbids a fence; a
// stray one is a signal to tighten it, not to paper over). Returns the parsed animation object
// verbatim; ops.animateElementFromText is the loud validateAnimation gate on it.
export async function runAnimateFromText({ element, imagePath = null, currentSpec = null, text, runner = runCodex } = {}) {
  if (!element) throw new Error("runAnimateFromText requires element");
  if (!text || !String(text).trim()) throw new Error("runAnimateFromText requires text");
  const instruction = buildAnimateInstruction({ element, currentSpec, text });
  const raw = await runner({ instruction, imagePath });
  const reply = raw == null ? "" : String(raw).trim();
  if (!reply) throw new Error("runAnimateFromText: codex returned an empty result");
  let parsed;
  try {
    parsed = JSON.parse(reply);
  } catch {
    throw new Error(`runAnimateFromText: codex reply was not valid JSON — raw reply started: ${JSON.stringify(reply.slice(0, 200))}`);
  }
  if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
    throw new Error(`runAnimateFromText: codex reply parsed to a non-object JSON value: ${JSON.stringify(reply.slice(0, 200))}`);
  }
  return parsed;
}
