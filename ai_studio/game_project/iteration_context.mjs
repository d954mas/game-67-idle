#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const scriptDir = dirname(fileURLToPath(import.meta.url));
const harnessRoot = resolve(scriptDir, "..", "..");

function usage() {
  console.error(`usage:
  node ai_studio/game_project/iteration_context.mjs [--root <repo>] [--json-output <file>] [--status-max-chars <n>]

Builds a compact pre-implementation context pack for playable game work.`);
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

function findRoot(start = process.cwd()) {
  let current = resolve(start);
  while (true) {
    if (existsSync(join(current, "AGENTS.md"))) return current;
    const parent = dirname(current);
    if (parent === current) return resolve(start);
    current = parent;
  }
}

function readIfExists(path) {
  try {
    return existsSync(path) ? readFileSync(path, "utf8") : "";
  } catch {
    return "";
  }
}

function sectionText(markdown, title) {
  const pattern = new RegExp(`(?:^|\\r?\\n)## ${escapeRegExp(title)}[ \\t]*\\r?\\n([\\s\\S]*?)(?=\\r?\\n##\\s+|$)`, "i");
  const match = markdown.match(pattern);
  return match ? match[1].trim() : "";
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function bulletContaining(text, patterns, fallback = "") {
  const lines = bulletBlocks(text);
  for (const pattern of patterns) {
    const found = lines.find((line) => pattern.test(line));
    if (found) return found;
  }
  return fallback;
}

function bulletBlocks(text) {
  const blocks = [];
  let current = "";
  for (const rawLine of String(text || "").split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line) continue;
    if (/^[-*]\s+/.test(line)) {
      if (current) blocks.push(current.trim());
      current = line.replace(/^[-*]\s+/, "");
    } else if (current && !/^#{1,6}\s+/.test(line)) {
      current = `${current} ${line}`;
    }
  }
  if (current) blocks.push(current.trim());
  return blocks;
}

function truncate(text, maxChars) {
  const clean = String(text || "").trim();
  if (clean.length <= maxChars) return clean;
  return `${clean.slice(0, Math.max(0, maxChars - 80)).replace(/\s+$/, "")}\n\n... truncated ${clean.length - maxChars} chars; inspect source file for full context.`;
}

function hasMeaningfulText(text) {
  return Boolean(String(text || "").trim());
}

function isActiveConcept(text) {
  const value = String(text || "");
  return hasMeaningfulText(value) && !/status:\s*none|no active|no concept|none selected|clean template/i.test(value);
}

function gameProjectConcept(markdown) {
  const activeGame = sectionText(markdown, "Active Game");
  if (!activeGame) return "Status: none";
  const status = activeGame.match(/status:\s*([^\r\n]+)/i)?.[1]?.trim() || "";
  if (/^none\b/i.test(status)) return "Status: none";
  const gameId = activeGame.match(/game id:\s*`?([a-z0-9][a-z0-9-]{1,64})`?/i)?.[1] || "";
  const summary = activeGame.split(/\r?\n/).find((line) => line.trim() && !/^status:|^-/.test(line.trim()))?.trim() || "";
  return [`Status: ${status || "active"}`, gameId ? `Game id: ${gameId}` : "", summary].filter(Boolean).join("; ");
}

function taskContextHasActionableWork(text) {
  return /-\s+T\d{4}\s+(doing|todo|backlog)\s+/i.test(String(text || ""));
}

function hasProjectWikiSource(sources) {
  return sources.some((source) => /^gamedesign\/projects\/[^/]+\//.test(source));
}

function hasLiveStateMatrixSource(sources) {
  return sources.some((source) => /^gamedesign\/projects\/[^/]+\/visual\/live_state_acceptance_matrix\.json$/.test(source));
}

function mentionsReviewProof(text) {
  return /review evidence|visual review|review proof|fake shot|visual proof|screenshot proof|native proof|first playable screen/i.test(String(text || ""));
}

function buildStartupGate({ concept, designSources, runtimeSources, taskContext, currentGate, nextPriorities }) {
  const activeConcept = isActiveConcept(concept);
  const requirements = [
    {
      id: "active_concept",
      ok: activeConcept,
      evidence: concept || "No active concept found.",
      fix: "Create or select one active game concept before implementation.",
    },
    {
      id: "active_task",
      ok: activeConcept && taskContextHasActionableWork(taskContext),
      evidence: activeConcept && taskContextHasActionableWork(taskContext) ? "Taskboard has actionable work for the active concept." : "No active-concept task is ready.",
      fix: "Create/refine one P0/P1 task for the active concept with checkable Done when and evidence gates.",
    },
    {
      id: "project_wiki",
      ok: activeConcept && hasProjectWikiSource(designSources),
      evidence: activeConcept && hasProjectWikiSource(designSources) ? "Project wiki/design sources found for an active concept." : "No active-concept project wiki source found.",
      fix: "Create gamedesign/projects/<game-id>/ for the active concept with concept/GDD/evidence before runtime work.",
    },
    {
      id: "runtime_harness",
      ok: runtimeSources.length > 0,
      evidence: runtimeSources.length ? runtimeSources.join(", ") : "No runtime source/build files found.",
      fix: "Identify the native/runtime harness and validation command.",
    },
    {
      id: "visual_review_plan",
      ok: activeConcept && mentionsReviewProof(`${currentGate}\n${nextPriorities}`),
      evidence: activeConcept && mentionsReviewProof(`${currentGate}\n${nextPriorities}`) ? "Status names visual review/native proof for the active concept." : "No active-concept visual review proof found in status.",
      fix: "Name the first fake shot, review evidence, and native screenshot proof before broad implementation.",
    },
    {
      id: "live_state_acceptance_matrix",
      ok: activeConcept && hasLiveStateMatrixSource(designSources),
      evidence: activeConcept && hasLiveStateMatrixSource(designSources)
        ? "Live-state acceptance matrix found for the active concept."
        : "No active-concept live-state acceptance matrix found.",
      fix: "Create gamedesign/projects/<game-id>/visual/live_state_acceptance_matrix.json and use it as review evidence before accepting UI/visual work.",
    },
    {
      id: "core_loop_model",
      ok: activeConcept && designSources.some((source) => /\/data\/core_loop\.json$/.test(source)),
      evidence: activeConcept && designSources.some((source) => /\/data\/core_loop\.json$/.test(source))
        ? "Core-loop model found (data/core_loop.json) for the active concept."
        : "No core-loop model (data/core_loop.json) for the active concept.",
      fix: "Design the core loop FIRST: write gamedesign/projects/<id>/data/core_loop.json (player verbs, rules, feedback, risk, goals, replay reason, and reference grounding) before building. A pretty screen is not a game (AGENTS.md Game/core-loop gate).",
    },
  ];
  const missing = requirements.filter((requirement) => !requirement.ok);
  return {
    status: missing.length === 0 ? "ready_for_first_slice" : "not_ready_for_implementation",
    hard_stop: missing.length > 0,
    missing: missing.map((requirement) => requirement.id),
    requirements,
  };
}

function buildVisualFirstContract({ concept }) {
  const activeConcept = isActiveConcept(concept);
  return {
    status: activeConcept ? "required_before_visual_runtime_work" : "inactive_until_active_concept",
    session_contract_fields: [
      {
        id: "goal",
        prompt: "One player-facing visual/product outcome for this slice.",
      },
      {
        id: "non_goal",
        prompt: "What feature/content expansion is explicitly out of scope.",
      },
      {
        id: "proof",
        prompt: "Native screenshot plus review evidence that proves the slice.",
      },
      {
        id: "stop_condition",
        prompt: "The blocker that stops expansion, including failed review or lead visual rejection.",
      },
      {
        id: "likely_files",
        prompt: "The small set of docs/assets/runtime files expected to change.",
      },
    ],
    before_coding_required_evidence: [
      "Accepted fake shot, reference digest, art bible, or visual target path.",
      "Current native screenshot path, or the exact native capture command if the runtime does not exist yet.",
      "Screenshot-vs-target mismatch list before runtime/code changes.",
      "One vertical art slice plan: character/world/water/UI focal surface before broad content.",
      "Review artifact that can fail the slice.",
      "Live-state acceptance matrix path and required state coverage for HUD, primary action, feedback, modal, blocked, return, and stress states.",
    ],
    after_meaningful_render_change: [
      "Capture a new native screenshot.",
      "Update the screenshot-vs-target mismatch list.",
      "Run or record review evidence before adding features/content.",
    ],
    generated_ui_runtime_gate: [
      "Non-empty crop plan covering every prepared UI asset id.",
      "Non-empty prepared asset manifest matching crop ids and output files.",
      "Passing visual review of the assembled screen before game integration claims.",
      "If the review fails, fix source/crop/prepared assets before compensating in code.",
    ],
    stop_conditions: [
      "Failed review blocks feature/content expansion unless the lead explicitly accepts the debt.",
      "Lead rejection such as ugly, unclear, unreadable, or not like the fake shot blocks broad implementation.",
      "Procedural/programmer art cannot satisfy generated/final visual work without a recorded exception.",
    ],
  };
}

function runTaskContext(root, maxChars) {
  const result = spawnSync(process.execPath, [
    join(harnessRoot, "ai_studio", "taskboard", "cli.mjs"),
    "context",
    "--status-max-chars",
    String(maxChars),
    "--tasks-limit",
    "12",
  ], {
    cwd: root,
    env: { ...process.env, TASKBOARD_ROOT: root },
    encoding: "utf8",
    shell: false,
  });
  if (result.status !== 0) return "";
  return result.stdout || "";
}

function existing(root, paths) {
  return paths.filter((path) => existsSync(join(root, path)));
}

function directories(path) {
  try {
    return readdirSync(path, { withFileTypes: true }).filter((entry) => entry.isDirectory()).map((entry) => entry.name);
  } catch {
    return [];
  }
}

function projectDesignSources(root) {
  const sources = [];
  for (const projectId of directories(join(root, "gamedesign", "projects"))) {
    sources.push(...existing(root, [
      `gamedesign/projects/${projectId}/README.md`,
      `gamedesign/projects/${projectId}/gdd.md`,
      `gamedesign/projects/${projectId}/GDD.md`,
      `gamedesign/projects/${projectId}/art/art_direction.md`,
      `gamedesign/projects/${projectId}/reviews/first_slice_review.md`,
      `gamedesign/projects/${projectId}/visual/live_state_acceptance_matrix.md`,
      `gamedesign/projects/${projectId}/visual/live_state_acceptance_matrix.json`,
      `gamedesign/projects/${projectId}/data/core_loop.json`,
    ]));
  }
  for (const legacyId of directories(join(root, "gamedesign"))) {
    if (legacyId === "knowledge" || legacyId === "sources" || legacyId === "projects") continue;
    sources.push(...existing(root, [
      `gamedesign/${legacyId}/gdd.md`,
      `gamedesign/${legacyId}/GDD.md`,
      `gamedesign/${legacyId}/art/art_direction.md`,
      `gamedesign/${legacyId}/data/core_loop.json`,
    ]));
  }
  return [...new Set(sources)];
}

function buildContext(root, options = {}) {
  const maxStatusChars = Number.isFinite(Number(options.statusMaxChars)) ? Number(options.statusMaxChars) : 5000;
  const agents = readIfExists(join(root, "AGENTS.md"));
  const gameProject = readIfExists(join(root, "GAME_PROJECT.md"));
  const activeGame = sectionText(gameProject, "Active Game");
  const project = sectionText(agents, "Project");
  const direction = sectionText(agents, "Direction");
  const validation = sectionText(agents, "Validation");
  const taskContext = runTaskContext(root, Math.max(1200, maxStatusChars));

  const concept = gameProjectConcept(gameProject);
  const activeConcept = isActiveConcept(concept);
  const productTarget = bulletContaining(direction, [/current product target/i, /release-quality/i, /product target/i, /Current runtime surface/i], "");
  const runtimeSurface = bulletContaining(direction, [/current runtime surface/i, /src\/clean_seed_main\.c/i, /src\/main\.c/i, /placeholder/i], "");
  const nativeGate = bulletContaining(validation, [/Native desktop\/PC/i, /native PC/i, /preferred development/i], "");
  const webGate = bulletContaining(validation, [/web prototype/i, /web server/i, /localhost/i, /browser\/frontend/i], "");
  const referenceGate = bulletContaining(direction, [/Reference study is a hard implementation gate/i, /Reference Lock/i], "");
  const visualGate = bulletContaining(direction, [/generated-art/i, /visual work/i, /quality/i], "");

  const designSources = existing(root, [
    "gamedesign/knowledge/README.md",
    "gamedesign/knowledge/reference_deconstruction.md",
  ]).concat(activeConcept ? projectDesignSources(root) : []);
  const runtimeCandidates = activeConcept
    ? [
        "src/clean_seed_main.c",
        "src/main.c",
        "state/game_state.schema.json",
        "CMakePresets.json",
        "tools/devapi",
      ]
    : [
        existsSync(join(root, "src", "clean_seed_main.c")) ? "src/clean_seed_main.c" : "src/main.c",
        "state/game_state.schema.json",
        "CMakePresets.json",
        "tools/devapi",
      ];
  const runtimeSources = existing(root, runtimeCandidates);

  const currentGate = sectionText(taskContext, "Current Gate") || sectionText(taskContext, "Current Goal") || activeGame;
  const nextPriorities = sectionText(taskContext, "Next Priorities") || activeGame;
  const blockers = sectionText(taskContext, "Blocking Work") || sectionText(taskContext, "Blockers");
  const requiredValidation = sectionText(taskContext, "Required Validation");

  const hardGates = [
    nativeGate || "Use the primary native/game runtime first; discover local build/run rules before implementation.",
    webGate || "Do not use web/browser work unless the current user request explicitly allows it.",
    referenceGate || "If a named reference drives gameplay/art/balance, create or update a durable reference deconstruction before implementation.",
    visualGate || "For polished/generated visual work, use an art target/job, prepared assets, and quality evidence before code polish.",
  ];

  const startupGate = buildStartupGate({
    concept,
    designSources,
    runtimeSources,
    taskContext,
    currentGate,
    nextPriorities,
  });
  const visualFirstContract = buildVisualFirstContract({ concept });

  return {
    schema_version: 1,
    root,
    generated_at: new Date().toISOString(),
    concept,
    product_target: productTarget,
    runtime_surface: runtimeSurface,
    hard_gates: hardGates,
    design_sources: designSources,
    runtime_sources: runtimeSources,
    current_gate: truncate(currentGate, 1600),
    prototype_startup_gate: startupGate,
    visual_first_contract: visualFirstContract,
    next_priorities: truncate(nextPriorities, 1200),
    blockers: truncate(blockers, 1000),
    required_validation: truncate(requiredValidation, 1400),
    before_coding_checklist: [
      "Check `prototype_startup_gate.status`; if it is not ready, do not start broad runtime implementation.",
      "Write the 5-line visual session contract when the slice has visual, UI, FTUE, feel, or audience-test risk.",
      "Open exactly one actionable task and one active project wiki before code.",
      "Name the visual review evidence that can stop feature expansion.",
      "Use `visual/live_state_acceptance_matrix.json` as evidence and cover or explicitly debt every required state.",
      "Compare current native screenshot against the accepted fake shot/target and list mismatches before visual code.",
      "Name the selected runtime harness and why it is allowed.",
      "If reference-driven, cite the durable deconstruction/digest and next native proof.",
      "If generated UI work, prove non-empty crop/prepared manifests and a visual review before game integration claims.",
      "If visual/UI work, cite the accepted target or art request and reusable asset strategy.",
      "Name the smallest playable slice and the native screenshot/scenario that will prove it.",
      "Use passive AI profiling only for long or risky work where stall evidence would help.",
    ],
  };
}

function renderMarkdown(context) {
  const lines = [];
  lines.push("# Game Iteration Context Pack");
  lines.push("");
  lines.push(`Generated: ${context.generated_at}`);
  lines.push(`Root: ${context.root}`);
  lines.push("");
  lines.push("## Active Direction");
  lines.push(`- concept: ${context.concept}`);
  if (context.product_target) lines.push(`- product target: ${context.product_target}`);
  if (context.runtime_surface) lines.push(`- runtime surface: ${context.runtime_surface}`);
  lines.push("");
  lines.push("## Hard Gates Before Coding");
  for (const gate of context.hard_gates) lines.push(`- ${gate}`);
  lines.push("");
  lines.push("## Current Project Gate");
  lines.push(context.current_gate || "- none found; run `node ai_studio/taskboard/cli.mjs context --json`.");
  lines.push("");
  lines.push("## Prototype Startup Gate");
  lines.push(`- status: ${context.prototype_startup_gate.status}`);
  lines.push(`- hard stop: ${context.prototype_startup_gate.hard_stop ? "yes" : "no"}`);
  if (context.prototype_startup_gate.missing.length > 0) {
    lines.push(`- missing: ${context.prototype_startup_gate.missing.join(", ")}`);
  }
  for (const requirement of context.prototype_startup_gate.requirements) {
    lines.push(`- ${requirement.ok ? "PASS" : "FAIL"} ${requirement.id}: ${requirement.ok ? requirement.evidence : requirement.fix}`);
  }
  lines.push("");
  lines.push("## Visual-First Contract");
  lines.push(`- status: ${context.visual_first_contract.status}`);
  lines.push("- session contract fields:");
  for (const field of context.visual_first_contract.session_contract_fields) {
    lines.push(`  - ${field.id}: ${field.prompt}`);
  }
  lines.push("- before coding evidence:");
  for (const item of context.visual_first_contract.before_coding_required_evidence) lines.push(`  - ${item}`);
  lines.push("- after render-change evidence:");
  for (const item of context.visual_first_contract.after_meaningful_render_change) lines.push(`  - ${item}`);
  lines.push("- generated UI runtime gate:");
  for (const item of context.visual_first_contract.generated_ui_runtime_gate) lines.push(`  - ${item}`);
  lines.push("- stop conditions:");
  for (const item of context.visual_first_contract.stop_conditions) lines.push(`  - ${item}`);
  lines.push("");
  lines.push("## Next Priorities");
  lines.push(context.next_priorities || "- none found.");
  lines.push("");
  lines.push("## Blocking Work");
  lines.push(context.blockers || "- none found.");
  lines.push("");
  lines.push("## Required Validation");
  lines.push(context.required_validation || "- none found; use native build plus targeted scenario proof.");
  lines.push("");
  lines.push("## Source Files To Inspect");
  lines.push("- design:");
  for (const path of context.design_sources) lines.push(`  - ${path}`);
  if (context.design_sources.length === 0) lines.push("  - none found");
  lines.push("- runtime:");
  for (const path of context.runtime_sources) lines.push(`  - ${path}`);
  if (context.runtime_sources.length === 0) lines.push("  - none found");
  lines.push("");
  lines.push("## Before Coding Checklist");
  for (const item of context.before_coding_checklist) lines.push(`- ${item}`);
  return `${lines.join("\n")}\n`;
}

const args = parseArgs(process.argv.slice(2));
const root = args.root ? resolve(String(args.root)) : findRoot();
const context = buildContext(root, { statusMaxChars: args["status-max-chars"] });
const markdown = renderMarkdown(context);

if (args["json-output"]) {
  const target = resolve(String(args["json-output"]));
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(context, null, 2)}\n`, "utf8");
}

process.stdout.write(markdown);
