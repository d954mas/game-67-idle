#!/usr/bin/env node
// Visual critic RUNNER: assembles a vision art-lead critic instruction over one
// or more state screenshots plus the per-game art contract and reference banks,
// optionally executes a configurable vision model (critic pass + independent
// refute pass), reconciles disagreement into a `review` verdict, and writes a
// `game.visual_critique` JSON that `review.mjs --critique` ingests.
//
// Two modes, both deterministic except the model call itself:
//   emit (default)        write the critic instruction + print the next command.
//                         No model required; this is the graceful fallback.
//   run (--model-cmd ...) execute the model on the instruction, parse JSON, run
//                         the refute pass, reconcile, and write the critique.
//
// References (approved/rejected images) and the contract go straight into the
// prompt IN CONTEXT. There is no embedding store, vector DB, or fine-tuning.

import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, basename, extname, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fail } from "../lib/cli.mjs";
import { relCwdPosix } from "../lib/paths.mjs";

const VISUAL_AXES = [
  "composition",
  "readability",
  "ui_controls",
  "action_direction",
  "art_quality",
  "audience_fit",
];

function usage() {
  console.error(`usage:
  node tools/product_gate/visual_critic_run.mjs --project <game-id> --shot <tag:path> [--shot ...] [options]

Options:
  --contract <path>      art contract JSON (default: gamedesign/projects/<id>/art/art_contract.json)
  --screenshot <path>    repeatable; equivalent to --shot <basename>:<path>
  --shot <tag:path>      repeatable; a named state screenshot to judge
  --state-matrix <path>  game.live_state_acceptance_matrix JSON; covered states' screenshots
                         are used as shots when no --shot/--screenshot is given
  --surface <name>       desktop, portrait, etc. (default desktop)
  --out <path>           critique JSON output (default gamedesign/projects/<id>/art/latest_critique.json)
  --instruction-out <path>  critic instruction markdown (default .../art/critic_instruction.md)
  --model-cmd <cmd>      shell command that runs the vision model; {IMAGES} expands to the
                         quoted screenshot paths and {INSTRUCTION_FILE} to the instruction
                         file; the instruction is also piped on stdin. Omit for emit mode.
  --no-refute            skip the independent refute pass (run mode only)

Verified vision command on this box (codex CLI, gpt-5.5):
  --model-cmd "codex exec --dangerously-bypass-approvals-and-sandbox --skip-git-repo-check -i {IMAGES} -"`);
  process.exit(2);
}

function sanitizeToken(value) {
  return String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9_-]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 80) || "game";
}

// Paths get interpolated into the --model-cmd shell string, so reject any that
// carry shell-significant characters. A crafted screenshot filename or a path
// read out of a shared state matrix must not be able to inject commands.
const SHELL_UNSAFE = /["'`$;|&<>\r\n%!]/;
function assertSafeModelPath(path, what) {
  if (SHELL_UNSAFE.test(String(path))) {
    fail(`${what} contains shell-unsafe characters; cannot pass to --model-cmd: ${path}`);
  }
}

function parseArgs(argv) {
  const values = { shots: [], screenshots: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") usage();
    else if (arg === "--no-refute") values.noRefute = true;
    else if (arg === "--shot") {
      const value = argv[++index];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.shots.push(value);
    } else if (arg === "--screenshot") {
      const value = argv[++index];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.screenshots.push(value);
    } else if (arg.startsWith("--")) {
      const value = argv[++index];
      if (value === undefined || value.startsWith("--")) fail(`${arg} requires a value`);
      values[arg.slice(2)] = value;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return values;
}

// Pull covered-state screenshots from a game.live_state_acceptance_matrix so the
// critic can be driven straight from the captured state matrix with no --shot list.
function matrixShots(matrixPath) {
  if (!existsSync(resolve(matrixPath))) fail(`state matrix does not exist: ${matrixPath}`);
  let matrix;
  try {
    matrix = JSON.parse(readFileSync(resolve(matrixPath), "utf8"));
  } catch (error) {
    fail(`state matrix is not valid JSON: ${matrixPath}: ${error.message}`);
  }
  const pairs = [];
  const consider = (tag, entry) => {
    const value = entry && typeof entry === "object" ? entry : {};
    const status = String(value.status || (value.covered ? "covered" : "")).trim();
    const evidence = String(value.evidence || value.proof || value.path || "").trim();
    if (status === "covered" && /\.(png|jpg|jpeg|webp)$/i.test(evidence)) {
      pairs.push([String(value.tag || tag).trim(), evidence]);
    }
  };
  if (Array.isArray(matrix.states)) {
    for (const entry of matrix.states) consider(entry && entry.tag, entry);
  } else if (matrix.states && typeof matrix.states === "object") {
    for (const [tag, entry] of Object.entries(matrix.states)) consider(tag, entry);
  }
  return pairs;
}

function parseShots(values) {
  const shots = [];
  const seen = new Set();
  const push = (tag, path) => {
    const cleanTag = sanitizeToken(tag);
    if (seen.has(cleanTag)) return;
    if (!existsSync(resolve(path))) fail(`screenshot does not exist: ${path}`);
    shots.push({ tag: cleanTag, path: relCwdPosix(path), abs: resolve(path).replaceAll("\\", "/") });
    seen.add(cleanTag);
  };
  for (const raw of values.shots) {
    const idx = String(raw).indexOf(":");
    if (idx < 0) fail(`--shot needs tag:path, got: ${raw}`);
    push(raw.slice(0, idx), raw.slice(idx + 1));
  }
  for (const raw of values.screenshots) {
    push(basename(raw, extname(raw)), raw);
  }
  if (shots.length === 0 && values["state-matrix"]) {
    for (const [tag, path] of matrixShots(values["state-matrix"])) push(tag, path);
  }
  if (shots.length === 0) {
    fail("no screenshots: pass --shot tag:path / --screenshot <path>, or a --state-matrix with covered evidence");
  }
  return shots;
}

function defaultContractPath(project) {
  return `gamedesign/projects/${sanitizeToken(project)}/art/art_contract.json`;
}

function loadContract(values) {
  const explicit = Boolean(values.contract);
  const contractPath = values.contract || defaultContractPath(values.project);
  if (!existsSync(resolve(contractPath))) {
    if (explicit) fail(`art contract does not exist: ${contractPath}`);
    return { path: "", data: null };
  }
  try {
    return { path: relCwdPosix(contractPath), data: JSON.parse(readFileSync(resolve(contractPath), "utf8")) };
  } catch (error) {
    fail(`art contract is not valid JSON: ${contractPath}: ${error.message}`);
  }
}

function listImages(dir) {
  if (!dir || !existsSync(resolve(dir))) return [];
  return readdirSync(resolve(dir))
    .filter((name) => /\.(png|jpg|jpeg|webp)$/i.test(name))
    .map((name) => `${dir.replace(/\/$/, "")}/${name}`);
}

function refBank(contract) {
  const refs = (contract && contract.references) || {};
  return {
    approved: listImages(refs.approved_dir),
    rejected: listImages(refs.rejected_dir),
    yes: Array.isArray(refs.yes) ? refs.yes : [],
    no: Array.isArray(refs.no) ? refs.no : [],
  };
}

const CRITIQUE_SHAPE = `{
  "schema": "game.visual_critique",
  "verdict": "pass | review | fail",
  "scores": { "composition": 1-5, "readability": 1-5, "ui_controls": 1-5, "action_direction": 1-5, "art_quality": 1-5, "audience_fit": 1-5 },
  "issues": [ { "severity": "blocker|major|minor", "axis": "<one of the six axes>", "text": "concrete visual evidence" } ],
  "answers": { "where": "", "action": "", "response": "", "reward": "", "game_look": "" },
  "smallest_next_fix": "the single highest-impact fix before any new content"
}`;

function buildCriticInstruction({ project, contract, refs, shots }) {
  const lines = [];
  lines.push(`You are the ART LEAD doing visual acceptance for the game "${project}".`);
  lines.push("Judge the ACTUAL game screen(s) as a player's first contact, not as illustrations in a vacuum.");
  lines.push("");
  lines.push("## Taste contract");
  lines.push(contract.data ? "```json" : "(no per-game art contract found; judge against the universal six axes only)");
  if (contract.data) {
    lines.push(JSON.stringify(contract.data, null, 2));
    lines.push("```");
  }
  lines.push("");
  lines.push("## Reference banks (in context)");
  lines.push(`Approved target references (the screen should move toward these): ${refs.approved.length ? refs.approved.join(", ") : "(none seeded yet)"}`);
  lines.push(`Rejected references (the screen must NOT look like these): ${refs.rejected.length ? refs.rejected.join(", ") : "(none seeded yet)"}`);
  if (refs.yes.length) lines.push(`Yes-direction notes: ${refs.yes.map((r) => `${r.ref} (${r.why})`).join("; ")}`);
  if (refs.no.length) lines.push(`No-direction notes: ${refs.no.map((r) => `${r.ref} (${r.why})`).join("; ")}`);
  lines.push("");
  lines.push("## Screens to judge (one per key state)");
  for (const shot of shots) lines.push(`- ${shot.tag}: ${shot.abs}`);
  lines.push("");
  lines.push("## How to judge");
  lines.push(`Score each of the six axes 1-5: ${VISUAL_AXES.join(", ")}.`);
  lines.push("- Do not forgive placeholder/debug-like UI, unreadable text, or a prototype look.");
  lines.push("- If a screen only works as a pretty still but not as a live game state, that is a fail.");
  lines.push("- If the primary action is not clear within 3-5 seconds, that is a fail.");
  lines.push("- If the style is closer to the rejected references than the approved ones, that is a fail.");
  lines.push("- Name the SINGLE highest-impact next fix, not a cosmetics list.");
  lines.push("- Do NOT draw, generate, or write any code. Only look and judge.");
  lines.push("");
  lines.push("## Output");
  lines.push("Return ONLY a JSON object (no prose) matching exactly:");
  lines.push("```json");
  lines.push(CRITIQUE_SHAPE);
  lines.push("```");
  return lines.join("\n");
}

function buildRefuteInstruction({ project, contract, refs, shots, critique }) {
  const topIssue = (critique.issues && critique.issues[0] && critique.issues[0].text) || critique.smallest_next_fix || "(no reason given)";
  const lines = [];
  lines.push(`You are an INDEPENDENT SKEPTIC reviewing an art-lead's verdict on the game "${project}". Look at the SAME screens and contract.`);
  lines.push(`The art-lead returned verdict "${critique.verdict}". Their main reason: ${topIssue}`);
  lines.push("");
  lines.push("Try HARD to refute that verdict:");
  lines.push("- If it was \"pass\", argue why the screen is actually NOT shippable to the target audience.");
  lines.push("- If it was \"fail\" or \"review\", argue why it might actually be acceptable.");
  lines.push("Default to disagreement if you are genuinely uncertain - uncertainty should route to a human review.");
  lines.push("");
  lines.push("## Screens");
  for (const shot of shots) lines.push(`- ${shot.tag}: ${shot.abs}`);
  if (contract.data) {
    lines.push("");
    lines.push("## Taste contract");
    lines.push("```json");
    lines.push(JSON.stringify(contract.data, null, 2));
    lines.push("```");
  }
  if (refs.approved.length || refs.rejected.length) {
    lines.push("");
    lines.push(`Approved refs: ${refs.approved.join(", ") || "(none)"}`);
    lines.push(`Rejected refs: ${refs.rejected.join(", ") || "(none)"}`);
  }
  lines.push("");
  lines.push("Return ONLY this JSON (no prose):");
  lines.push("```json");
  lines.push('{ "agree": true|false, "counter_verdict": "pass|review|fail", "reason": "one concrete sentence" }');
  lines.push("```");
  lines.push("Do NOT draw, generate, or write any code.");
  return lines.join("\n");
}

function tryBalancedJson(text) {
  const source = String(text || "");
  let best = null;
  for (let i = source.indexOf("{"); i >= 0 && i < source.length; i = source.indexOf("{", i + 1)) {
    let depth = 0;
    let inStr = false;
    let esc = false;
    for (let j = i; j < source.length; j += 1) {
      const ch = source[j];
      if (inStr) {
        if (esc) esc = false;
        else if (ch === "\\") esc = true;
        else if (ch === '"') inStr = false;
        continue;
      }
      if (ch === '"') inStr = true;
      else if (ch === "{") depth += 1;
      else if (ch === "}") {
        depth -= 1;
        if (depth === 0) {
          const candidate = source.slice(i, j + 1);
          try {
            const parsed = JSON.parse(candidate);
            if (parsed && typeof parsed === "object" && (!best || candidate.length > best.length)) {
              best = { value: parsed, length: candidate.length };
            }
          } catch {
            // keep scanning
          }
          break;
        }
      }
    }
  }
  return best ? best.value : null;
}

function extractJson(text) {
  const fences = [...String(text || "").matchAll(/```(?:json)?\s*([\s\S]*?)```/gi)].map((match) => match[1]);
  for (const fence of fences.reverse()) {
    const parsed = tryBalancedJson(fence);
    if (parsed) return parsed;
  }
  return tryBalancedJson(text);
}

function validateCritique(critique) {
  const errors = [];
  if (!critique || typeof critique !== "object") return ["model did not return a JSON object"];
  if (!["pass", "review", "fail"].includes(critique.verdict)) errors.push("verdict must be pass, fail, or review");
  if (!critique.scores || typeof critique.scores !== "object") errors.push("scores object is required");
  else {
    for (const axis of VISUAL_AXES) {
      const score = Number(critique.scores[axis]);
      if (!Number.isInteger(score) || score < 1 || score > 5) errors.push(`score ${axis} must be an integer 1-5`);
    }
  }
  return errors;
}

// Verified working vision invocation on this box (codex CLI 0.140.0, gpt-5.5):
// instruction piped on stdin (trailing `-`), screenshots attached with -i.
const CODEX_MODEL_CMD = "codex exec --dangerously-bypass-approvals-and-sandbox --skip-git-repo-check -i {IMAGES} -";

function callModel(modelCmd, instruction, instructionFile, imageArgs) {
  const command = String(modelCmd)
    .replaceAll("{INSTRUCTION_FILE}", `"${instructionFile}"`)
    .replaceAll("{IMAGES}", imageArgs);
  const result = spawnSync(command, {
    shell: true,
    input: instruction,
    encoding: "utf8",
    maxBuffer: 64 * 1024 * 1024,
  });
  if (result.error) fail(`model command failed to start: ${result.error.message}`);
  return result.stdout || "";
}

const values = parseArgs(process.argv.slice(2));
if (!values.project) fail("--project is required");
const project = values.project;
const surface = values.surface || "desktop";
const shots = parseShots(values);
const contract = loadContract(values);
const refs = refBank(contract.data);

const artDir = `gamedesign/projects/${sanitizeToken(project)}/art`;
const instructionOut = values["instruction-out"] || `${artDir}/critic_instruction.md`;
const out = values.out || `${artDir}/latest_critique.json`;

const criticInstruction = buildCriticInstruction({ project, contract, refs, shots });
const imageArgs = shots.map((shot) => `"${shot.abs}"`).join(" ");
mkdirSync(dirname(resolve(instructionOut)), { recursive: true });
writeFileSync(resolve(instructionOut), `${criticInstruction}\n`, "utf8");

// Emit mode: no model call. Write the instruction and print the next command.
if (!values["model-cmd"]) {
  console.log("# Visual Critic (emit mode)");
  console.log(`Project: ${project}`);
  console.log(`Contract: ${contract.path || "(none)"}`);
  console.log(`States: ${shots.map((shot) => shot.tag).join(", ")}`);
  console.log(`Instruction: ${instructionOut}`);
  console.log("");
  console.log("No --model-cmd given. Run the critic now with the verified vision path:");
  console.log("");
  console.log(`  node tools/ai.mjs critique --project ${project} ${shots.map((shot) => `--shot ${shot.tag}:${shot.path}`).join(" ")} \\`);
  console.log(`    --out ${out} --model-cmd "${CODEX_MODEL_CMD}"`);
  console.log("");
  console.log(`Then: node tools/ai.mjs gate --project ${project} --surface ${surface} --screenshot ${shots[0].path} --critique ${out}`);
  process.exit(0);
}

// Run mode. Every path interpolated into the shell command must be shell-safe.
for (const shot of shots) assertSafeModelPath(shot.abs, `screenshot path for state '${shot.tag}'`);
assertSafeModelPath(resolve(instructionOut), "instruction file path");

// Run mode: critic pass.
const criticRaw = callModel(values["model-cmd"], criticInstruction, resolve(instructionOut).replaceAll("\\", "/"), imageArgs);
const critique = extractJson(criticRaw);
const critiqueErrors = validateCritique(critique);
if (critiqueErrors.length) {
  console.error(criticRaw.slice(0, 2000));
  fail(`critic model did not return a valid game.visual_critique JSON:\n- ${critiqueErrors.join("\n- ")}`);
}

critique.schema = "game.visual_critique";
critique.issues = Array.isArray(critique.issues) ? critique.issues : [];
critique.answers = critique.answers && typeof critique.answers === "object" ? critique.answers : {};

// Independent refute pass: disagreement routes to a human review.
let refute = null;
if (!values.noRefute) {
  const refuteInstruction = buildRefuteInstruction({ project, contract, refs, shots, critique });
  const refuteInstructionFile = resolve(instructionOut).replace(/\.md$/i, ".refute.md");
  writeFileSync(refuteInstructionFile, `${refuteInstruction}\n`, "utf8");
  const refuteRaw = callModel(values["model-cmd"], refuteInstruction, refuteInstructionFile.replaceAll("\\", "/"), imageArgs);
  const refuteJson = extractJson(refuteRaw);
  if (refuteJson && typeof refuteJson.agree === "boolean") {
    refute = {
      agree: refuteJson.agree,
      counter_verdict: ["pass", "review", "fail"].includes(refuteJson.counter_verdict) ? refuteJson.counter_verdict : "",
      reason: String(refuteJson.reason || "").trim(),
    };
  } else {
    refute = { agree: false, counter_verdict: "review", reason: "refute pass returned no usable verdict; routing to review" };
  }
}

const criticVerdict = critique.verdict;
if (refute && !refute.agree) {
  critique.verdict = "review";
  critique.issues.push({
    severity: "major",
    axis: "audience_fit",
    text: `Independent refute pass disagreed with the "${criticVerdict}" verdict: ${refute.reason || "no reason given"}`,
  });
  if (!/lead/i.test(critique.smallest_next_fix || "")) {
    critique.smallest_next_fix = `Lead to arbitrate (critic said ${criticVerdict}, refute disagreed). ${critique.smallest_next_fix || ""}`.trim();
  }
}

critique.critic_verdict = criticVerdict;
critique.refute = refute;
critique.surface = surface;
critique.contract = contract.path;
critique.shots = shots.map((shot) => ({ tag: shot.tag, path: shot.path }));

mkdirSync(dirname(resolve(out)), { recursive: true });
writeFileSync(resolve(out), `${JSON.stringify(critique, null, 2)}\n`, "utf8");

console.log("# Visual Critic (run mode)");
console.log(`Project: ${project}`);
console.log(`Critic verdict: ${criticVerdict}`);
if (refute) console.log(`Refute: ${refute.agree ? "agreed" : `disagreed -> review (${refute.reason})`}`);
console.log(`Final verdict: ${critique.verdict}`);
console.log(`Critique: ${out}`);
console.log("");
console.log(`Next: node tools/ai.mjs gate --project ${project} --surface ${surface} --screenshot ${shots[0].path} --critique ${out}`);
