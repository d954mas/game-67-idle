#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { createEpic, createTask, findRoot } from "../taskboard/lib.mjs";

const scriptDir = dirname(fileURLToPath(import.meta.url));

function usage() {
  console.error(`usage:
  node tools/game_context/new_prototype.mjs --game-id <id> --title <name> --brief <one sentence> [--root <repo>] [--force]

Creates the first project wiki/task/status skeleton for a new native-first game prototype,
then runs the prototype startup gate and writes tmp/prototype_startup_gate_context.*.`);
  process.exit(2);
}

function parseArgs(args) {
  const values = {};
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--help" || arg === "-h") usage();
    if (!arg.startsWith("--")) usage();
    const key = arg.slice(2);
    const next = args[index + 1];
    if (next !== undefined && !next.startsWith("--")) {
      values[key] = next;
      index += 1;
    } else {
      values[key] = true;
    }
  }
  return values;
}

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function validateGameId(value) {
  const id = String(value || "").trim();
  if (!/^[a-z0-9][a-z0-9-]{1,48}$/.test(id)) {
    fail("--game-id must be lowercase kebab-case, for example bubble-bay");
  }
  return id;
}

function readText(path) {
  return existsSync(path) ? readFileSync(path, "utf8") : "";
}

function writeNew(path, body, options) {
  if (existsSync(path) && !options.force) {
    fail(`${relative(options.root, path)} already exists; use --force only when intentionally replacing kickoff artifacts`);
  }
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, body, "utf8");
}

function activeTaskFiles(root) {
  const dir = join(root, "tasks", "active");
  if (!existsSync(dir)) return [];
  return readdirSync(dir).filter((name) => name.endsWith(".md") && name.toLowerCase() !== "readme.md");
}

function assertCleanKickoffTarget(root, gameId, options) {
  const projectDir = join(root, "gamedesign", "projects", gameId);
  if (existsSync(projectDir) && !options.force) {
    fail(`gamedesign/projects/${gameId} already exists`);
  }
  const activeTasks = activeTaskFiles(root);
  if (activeTasks.length > 0 && !options.force) {
    fail(`tasks/active already has ${activeTasks.length} task file(s); close or archive current work before kickoff`);
  }
  const agents = readText(join(root, "AGENTS.md"));
  if (/Active game concept:/i.test(agents) && !options.force) {
    fail("AGENTS.md already names an active game concept");
  }
}

function updateAgents(root, title, gameId, brief) {
  const path = join(root, "AGENTS.md");
  const current = readText(path);
  const line = `- Active game concept: \`${title}\` (${gameId}), ${brief}`;
  if (!current) {
    writeNew(path, `# AGENTS.md\n\n## Project\n\n${line}\n`, { root, force: false });
    return;
  }
  const replaced = current.replace(
    /^- No active game concept is selected\..*$/m,
    line,
  );
  if (replaced !== current) {
    writeFileSync(path, replaced, "utf8");
    return;
  }
  const withActive = current.replace(/(## Project\s*\r?\n)/i, `$1\n${line}\n`);
  writeFileSync(path, withActive === current ? `${current.trimEnd()}\n\n## Project\n\n${line}\n` : withActive, "utf8");
}

function projectReadme(title, gameId, brief) {
  return `# ${title}

Project wiki for the active prototype \`${gameId}\`.

## Concept

${brief}

## Stage 0 Startup Gate

- Native-first implementation only until an explicit web/mobile exception is approved.
- First playable slice must name a fake shot, product-read gate, and native screenshot proof before broad runtime work.
- Visual-first session contract is required before runtime visual work: goal,
  non-goal, proof, stop condition, likely files.
- Before visual/runtime coding, compare current native screenshot or capture
  plan against the accepted fake shot/target and write a mismatch list.
- Beautiful/casual/generated-UI/fake-shot slices use the strict visual product
  gate rubric: six visual scores and blocker/major issue reporting.
- Keep reusable process learnings in \`gamedesign/knowledge/\`; keep project-specific facts here.

## First Slice

- Define the smallest playable loop in \`gdd.md\`.
- Fill \`reviews/first_slice_visual_gate.md\` before broad runtime work.
- For visually important slices, create the critic packet named in that gate
  before writing the strict product gate verdict.
- Capture visual/product proof in \`reviews/\` before expanding content.
- Update screenshot-vs-target mismatches after meaningful render changes.
`;
}

function gdd(title, brief) {
  return `# ${title} GDD

## One-Line Concept

${brief}

## Audience

Casual players. Progression should be clear; controls and moment-to-moment play should stay simple.

## Core Loop

1. Do one readable action.
2. Get an immediate juicy result.
3. Spend rewards on a visible upgrade.
4. Unlock a new short-term goal.

## First Playable Slice

- One native PC scene.
- One clear player action.
- One reward/progression feedback moment.
- One visual proof screenshot for product-read review.
- One filled \`reviews/first_slice_visual_gate.md\` before broad runtime work.
- One visual-first session contract: goal, non-goal, proof, stop condition,
  likely files.
- One screenshot-vs-target mismatch list before runtime visual code and after
  meaningful render changes.
- If the slice depends on beauty, casual readability, generated UI, or a fake
  shot match, one strict visual product gate using \`--visual-strict\`.
- Optional critic packet from \`tools/product_gate/visual_critique_packet.mjs\`
  before the strict gate verdict.

## Art Direction Stub

Bright, saturated, friendly, readable at a glance. Avoid realistic, muddy, or low-contrast presentation.
`;
}

function taskBody(title, gameId) {
  return `## What

Build the first native playable slice for \`${title}\` after the Stage 0 startup gate is ready.

## Done when

- [ ] \`gamedesign/projects/${gameId}/gdd.md\` names the first playable loop and player-readable goal.
- [ ] A fake shot or visual target exists before runtime polish starts.
- [ ] A 5-line visual session contract exists: goal, non-goal, proof, stop
      condition, likely files.
- [ ] Current native screenshot or capture plan is compared against the fake
      shot/target in a mismatch list before visual code expands.
- [ ] Native PC build/run command is identified and captured in the task log.
- [ ] First native screenshot/product-read proof is captured before expanding content.

## Open questions

- Which named references or fake shots define the visual target?

## Log
`;
}

function firstSliceVisualGate(title, gameId) {
  return `# First Slice Visual Gate

Project: \`${gameId}\` / ${title}

Fill this before broad runtime or content expansion. This is a stop/go artifact,
not a notes dump.

## Session Contract

- Goal:
- Non-goal:
- Proof:
- Stop condition:
- Likely files:

## Target

- Fake shot / visual target path:
- Reference digest path, if any:
- Art bible / style target path, if any:

## Current Native Proof

- Native build/run command:
- Current native screenshot path or capture plan:
- Screenshot-vs-target mismatch list:
  - [ ] First-screen composition:
  - [ ] Main action readability:
  - [ ] UI text/readability:
  - [ ] Visual style/appeal:
  - [ ] Performance or capture blocker:

## Visual Critic Packet

- Packet command:
  \`\`\`powershell
  node tools/ai.mjs critic --project ${gameId} --task <task-id> --surface desktop --screenshot <native-screenshot.png> --target <fake-shot-or-target-path> --brief "<casual audience, core action, target style>" --output gamedesign/projects/${gameId}/reviews/first_slice_visual_critic_packet.md --json-output gamedesign/projects/${gameId}/reviews/first_slice_visual_critic_packet.json
  \`\`\`
- Packet Markdown path: \`gamedesign/projects/${gameId}/reviews/first_slice_visual_critic_packet.md\`
- Packet JSON path: \`gamedesign/projects/${gameId}/reviews/first_slice_visual_critic_packet.json\`
- Use this packet for a self-review or separate critic pass before writing the strict product gate verdict.

## Product-Read Gate

- Gate command:
  \`\`\`powershell
  node tools/ai.mjs gate --project ${gameId} --task <task-id> --surface desktop --screenshot <native-screenshot.png> --verdict fail --strict --visual-strict --where "<where am I?>" --action "<what can I do?>" --response "<what changed?>" --reward "<why continue?>" --game-look "<why game?>" --problem "<specific visual/player-read problem>" --next "<smallest next visual fix>" --visual-score composition=1 --visual-score readability=1 --visual-score ui_controls=1 --visual-score action_direction=1 --visual-score art_quality=1 --visual-score audience_fit=1 --visual-issue blocker:readability:"<concrete issue>"
  \`\`\`
- Gate artifact path:
- Verdict: pending
- Blocking player-read questions:
  - [ ] What can the player do in the first 5 seconds?
  - [ ] What is the reward/progress feedback?
  - [ ] What looks unclear, ugly, unreadable, or unlike the target?
- Strict visual rubric:
  - [ ] composition score 1-5
  - [ ] readability score 1-5
  - [ ] ui_controls score 1-5
  - [ ] action_direction score 1-5
  - [ ] art_quality score 1-5
  - [ ] audience_fit score 1-5
  - [ ] visual issues use severity \`blocker\`, \`major\`, or \`minor\`
  - [ ] pass requires all six scores >= 4 and no blocker/major issue

## Expansion Decision

- Decision: blocked until filled
- If blocked, smallest next fix:
- If passed, exact content/system expansion allowed next:
`;
}

function statusBody(title, gameId, taskId) {
  return `# Project Status

## Current Goal

Start the \`${title}\` (${gameId}) native-first prototype from a clean Stage 0
startup gate. Do not expand broad systems until the first fake shot/product-read
gate, native screenshot proof, and screenshot-vs-target mismatch list are
named. For beautiful/casual/generated-UI/fake-shot slices, the product gate
uses \`--visual-strict\`.

## Blocking Work

- No runtime implementation blocker is known yet; the next blocker should come
  from the first GDD/reference/fake-shot pass.

## Non-blocking Debt

- None recorded for this prototype yet.

## Current Gate

Stage 0 startup gate for ${gameId}: active concept, actionable task ${taskId},
project wiki, native/runtime harness, and fake shot/product-read/native
screenshot proof plan must be visible before implementation. For visual work,
the 5-line session contract and screenshot-vs-target mismatch list are required
before runtime visual coding. Strict visual product gates require six scores
and blocker/major issue reporting before any pass.

## Required Validation

\`\`\`powershell
node tools/game_context/iteration_context.mjs
node tools/taskboard/cli.mjs validate
\`\`\`

## Last Known Good Evidence

- \`tmp/prototype_startup_gate_context.json\` after kickoff.
- \`gamedesign/projects/${gameId}/reviews/first_slice_visual_gate.md\` is the
  first-slice visual/product gate template and must be filled before broad
  runtime work; it names the optional visual critic packet command.

## Next Priorities

1. Fill \`gamedesign/projects/${gameId}/gdd.md\` with the first playable loop,
   references/fake shot target, visual/product proof gate, and stop condition.
2. Fill \`gamedesign/projects/${gameId}/reviews/first_slice_visual_gate.md\`
   with the target, native screenshot/capture plan, mismatch list, gate command,
   critic packet command, strict visual rubric, and expansion decision.
3. Identify the native build/run command for this prototype.
4. Capture or plan the first native screenshot, compare it with the accepted
   target, and record the mismatch list before broad content.
`;
}

function runStartupGate(root) {
  const jsonOutput = join(root, "tmp", "prototype_startup_gate_context.json");
  const result = spawnSync(process.execPath, [
    join(scriptDir, "iteration_context.mjs"),
    "--root",
    root,
    "--json-output",
    jsonOutput,
  ], {
    cwd: root,
    encoding: "utf8",
    shell: false,
  });
  if (result.status !== 0) {
    process.stderr.write(result.stderr || result.stdout);
    process.exit(result.status || 1);
  }
  const markdownOutput = join(root, "tmp", "prototype_startup_gate_context.md");
  mkdirSync(dirname(markdownOutput), { recursive: true });
  writeFileSync(markdownOutput, result.stdout, "utf8");
  return { jsonOutput, markdownOutput, markdown: result.stdout };
}

const args = parseArgs(process.argv.slice(2));
const root = args.root ? resolve(String(args.root)) : findRoot();
const gameId = validateGameId(args["game-id"]);
const title = String(args.title || "").trim() || fail("--title is required");
const brief = String(args.brief || "").trim() || fail("--brief is required");
const options = { root, force: args.force === true };

assertCleanKickoffTarget(root, gameId, options);
updateAgents(root, title, gameId, brief);

const projectDir = join(root, "gamedesign", "projects", gameId);
writeNew(join(projectDir, "README.md"), projectReadme(title, gameId, brief), options);
writeNew(join(projectDir, "gdd.md"), gdd(title, brief), options);
mkdirSync(join(projectDir, "reviews"), { recursive: true });
mkdirSync(join(projectDir, "art"), { recursive: true });
mkdirSync(join(projectDir, "data"), { recursive: true });
writeNew(join(projectDir, "reviews", "first_slice_visual_gate.md"), firstSliceVisualGate(title, gameId), options);

const epic = createEpic(root, {
  title: `${title} prototype`,
  status: "active",
  priority: "P1",
  tags: ["prototype", gameId],
  body: `## Goal

Create the first native playable slice for \`${title}\` with a clear product-read proof gate.

## In scope

- GDD/fake-shot/reference setup for ${gameId}.
- First native PC playable slice.
- Visual/product proof evidence before content expansion.

## Out of scope

- Web prototypes unless explicitly approved.
- Broad economy/content expansion before the first proof gate passes.

## Log
`,
});
const task = createTask(root, {
  title: `First native playable slice for ${title}`,
  status: "doing",
  epic: epic.fields.id,
  priority: "P1",
  tags: ["prototype", gameId, "native-first"],
  body: taskBody(title, gameId),
});

writeFileSync(join(root, "tasks", "STATUS.md"), statusBody(title, gameId, task.fields.id), "utf8");
const gate = runStartupGate(root);

console.log(`created project: ${relative(root, projectDir)}`);
console.log(`created epic: ${epic.fields.id}`);
console.log(`created task: ${task.fields.id}`);
console.log(`startup gate: ${relative(root, gate.jsonOutput)}`);
