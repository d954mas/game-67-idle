// Prompt-assist seam tests (T0239 increment 4: Expand-prompt + Extract). Only the PURE
// instruction/argv builders are exercised here — codex NEVER spawns in this suite (the
// T0238 contract, extended to text/vision). The default spawning impls (expandPrompt/
// extractFromImage) are covered indirectly through ops.mjs's own tests (tests/recipe.test.mjs),
// which always inject a fake `assistant` instead of letting the default impls run.
// Run: node --test ai_studio/assets/canvas/tests/prompt_assist.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import {
  buildExpandInstruction,
  buildExtractInstruction,
  buildTextCommand,
  buildVisionCommand,
  CODEX_JS,
} from "../tools/prompt_assist.mjs";

// ---- buildExpandInstruction ----------------------------------------------------

test("buildExpandInstruction: contains all 7 labeled sections, in order, no 'seed' substring", () => {
  const instruction = buildExpandInstruction({ prompt: "a red fox" });
  for (const section of ["[TASK]", "[SUBJECT]", "[STYLE]", "[COMPOSITION]", "[BACKGROUND]", "[CONSTRAINTS]", "[OUTPUT]"]) {
    assert.ok(instruction.includes(section), `missing section ${section}`);
  }
  // The exact adjacent, ordered template header block — a stronger check than sorting
  // individual indices (a section NAME can legitimately appear a second time in prose,
  // e.g. "write a [STYLE] section", so a naive indexOf-per-section sort is not reliable).
  assert.ok(
    instruction.includes("[TASK]\n[SUBJECT]\n[STYLE]\n[COMPOSITION]\n[BACKGROUND]\n[CONSTRAINTS]\n[OUTPUT]"),
    "the seven headers must appear together, in this exact order",
  );
  assert.match(instruction, /a red fox/);
  assert.doesNotMatch(instruction, /seed/i, "must never mention a seed — this pipeline has no usable seed control");
});

test("buildExpandInstruction: embeds a given styleBlock as the [STYLE] source material", () => {
  const withStyle = buildExpandInstruction({ prompt: "a knight", styleBlock: "painterly oil, warm gold rim light" });
  assert.match(withStyle, /painterly oil, warm gold rim light/);
  assert.match(withStyle, /STYLE SOURCE MATERIAL/);

  const withoutStyle = buildExpandInstruction({ prompt: "a knight" });
  assert.doesNotMatch(withoutStyle, /STYLE SOURCE MATERIAL/);

  // Whitespace-only styleBlock is treated as absent, same as omitted.
  const blankStyle = buildExpandInstruction({ prompt: "a knight", styleBlock: "   " });
  assert.doesNotMatch(blankStyle, /STYLE SOURCE MATERIAL/);
});

test("buildExpandInstruction: restates exclusions/negatives inside [CONSTRAINTS]", () => {
  const instruction = buildExpandInstruction({ prompt: "a dragon" });
  const constraintsIdx = instruction.indexOf("[CONSTRAINTS]");
  assert.ok(constraintsIdx >= 0);
  assert.match(instruction, /exclusion\/negative/i);
});

test("buildExpandInstruction requires prompt", () => {
  assert.throws(() => buildExpandInstruction({}), /requires prompt/);
  assert.throws(() => buildExpandInstruction(), /requires prompt/);
});

// ---- buildExtractInstruction ----------------------------------------------------

test("buildExtractInstruction: demands every schema key as a JSON field name, no params", () => {
  const instruction = buildExtractInstruction();
  for (const key of [
    "prompt_full",
    "prompt_subject",
    "style_block",
    "palette",
    "materials",
    "lighting",
    "composition",
    "constraints_block",
    "description",
  ]) {
    assert.ok(instruction.includes(`"${key}"`), `missing JSON key "${key}"`);
  }
  assert.match(instruction, /single JSON object/i);
});

test("buildExtractInstruction: prompt_full asks for a complete, self-contained, style-inclusive prompt", () => {
  const instruction = buildExtractInstruction();
  const promptFullClause = instruction.slice(instruction.indexOf('"prompt_full"'), instruction.indexOf('"prompt_subject"'));
  assert.match(promptFullClause, /complete/i);
  assert.match(promptFullClause, /self-contained/i);
  assert.match(promptFullClause, /style/i);
  assert.match(promptFullClause, /close reproduction/i);
});

test("buildExtractInstruction: prompt_subject explicitly excludes style descriptors", () => {
  const instruction = buildExtractInstruction();
  const promptSubjectClause = instruction.slice(instruction.indexOf('"prompt_subject"'), instruction.indexOf('"style_block"'));
  assert.match(promptSubjectClause, /SUBJECT/);
  assert.match(promptSubjectClause, /excluding/i);
});

test("buildExtractInstruction: output contract is ONLY the JSON object, no preamble", () => {
  const instruction = buildExtractInstruction();
  assert.match(instruction, /Output ONLY that single JSON object/);
  assert.match(instruction, /no preamble/i);
});

// ---- pure command builders (no spawn) --------------------------------------------

test("buildTextCommand: node <codex.js> exec with --output-last-message and the instruction as a plain positional arg", () => {
  const { command, args } = buildTextCommand({ instruction: "expand this", outputPath: "C:/tmp/last.txt" });
  // process.execPath + the shim's own target script — node's execFile can neither resolve
  // the extensionless "codex" npm shim nor spawn a .cmd without shell (see the module doc).
  assert.equal(command, process.execPath);
  assert.deepEqual(args, [CODEX_JS, "exec", "--skip-git-repo-check", "--output-last-message", "C:/tmp/last.txt", "expand this"]);
});

test("buildTextCommand requires instruction/outputPath", () => {
  assert.throws(() => buildTextCommand({ outputPath: "o" }), /requires instruction/);
  assert.throws(() => buildTextCommand({ instruction: "i" }), /requires outputPath/);
});

test("buildVisionCommand: codex exec -i <imagePath> with a bare '-' as the PROMPT positional (stdin), never the instruction text as an argv token", () => {
  const { command, args } = buildVisionCommand({ imagePath: "C:/tmp/ref.png", outputPath: "C:/tmp/last.txt" });
  assert.equal(command, process.execPath);
  assert.deepEqual(args, [CODEX_JS, "exec", "--skip-git-repo-check", "--output-last-message", "C:/tmp/last.txt", "-i", "C:/tmp/ref.png", "-"]);
  // T0251 variadic-argument guard: no instruction text
  // token anywhere in argv — it must travel over stdin instead, never as a positional that
  // -i could greedily swallow.
  assert.ok(!args.some((arg) => arg.includes(" ")), "no free-text instruction token in argv");
});

test("buildVisionCommand requires imagePath/outputPath", () => {
  assert.throws(() => buildVisionCommand({ outputPath: "o" }), /requires imagePath/);
  assert.throws(() => buildVisionCommand({ imagePath: "i" }), /requires outputPath/);
});
