// Core Harness orchestration helpers: packet templates, packet linting,
// preflight checks, and bounded file-scope validation.

const ORCHESTRATION_CLI = "node ai_studio/core_harness/orchestration/cli.mjs";

export const DEFAULT_ORCHESTRATION_TOOL_USE_GUARD = "verify exact repo paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First for line windows; keep evidence commands read-only";
export const ORCHESTRATION_REQUIRED_FIELDS = [
  ["objective", /\bobjective\b/i],
  ["allowed files", /\b(?:allowed files?|inputs?)\b/i],
  ["expected output", /\bexpected output\b/i],
  ["evidence command", /\b(?:evidence command|evidence artifact|artifact)\b/i],
  ["stop condition", /\bstop condition\b/i],
  ["independent reviewer", /\bindependent\s+(?:reviewer|verifier)\b/i],
];
export const ORCHESTRATION_PREFLIGHT_FIELDS = [
  ...ORCHESTRATION_REQUIRED_FIELDS.slice(0, 2),
  ["tool-use guard", /\btool-use\s+guard\b/i],
  ...ORCHESTRATION_REQUIRED_FIELDS.slice(2),
];
export const ORCHESTRATION_ALLOWED_FILES_FIELD_PATTERN = /\b(?:allowed files?|inputs?)\b/i;
export const ORCHESTRATION_PACKET_TEMPLATE = `- orchestration: used
  objective: <non-empty>
  allowed files: <non-empty>
  tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
  expected output: <non-empty>
  evidence command: <non-empty>
  stop condition: <non-empty>
  independent reviewer: <non-empty>`;

const SUBAGENT_PACKET_TEMPLATE = `objective: <bounded subagent objective>
allowed files: <repo-local files or bounded patterns>
forbidden files: <files or areas the subagent must not touch>
tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
expected output: <concise final report or changed files>
evidence command or artifact: <read-only command, focused test, or artifact path>
stop condition: <when the subagent must stop>
handoff:
  findings: <facts or verdict>
  files: <files inspected or changed>
  commands/evidence: <commands run and results>
  risks: <remaining risk>
  owner action: <what the lead must do next>
  not-done: <explicit gaps>`;

export function orchestrationPacketTemplate() {
  return ORCHESTRATION_PACKET_TEMPLATE;
}

export function subagentPacketTemplate() {
  return SUBAGENT_PACKET_TEMPLATE;
}

export function subagentPacketProblem(text) {
  const packet = String(text || "");
  const missing = [];
  const required = [
    ["objective", /\bobjective\b/i],
    ["allowed files", /\b(?:allowed files?|inputs?)\b/i],
    ["forbidden files", /\bforbidden files?\b/i],
    ["tool-use guard", /\btool-use\s+guard\b/i],
    ["expected output", /\bexpected output\b/i],
    ["evidence command or artifact", /\b(?:evidence command or artifact|evidence command|evidence artifact|artifact)\b/i],
    ["stop condition", /\bstop condition\b/i],
    ["handoff", /\bhandoff\b/i],
  ];
  for (const [name, pattern] of required) {
    if (!hasText(packetFieldValue(packet, pattern))) missing.push(name);
  }
  const allowed = packetFieldValue(packet, ORCHESTRATION_ALLOWED_FILES_FIELD_PATTERN);
  if (allowed && boundedAllowedFilesProblem(allowed)) missing.push("bounded allowed files");
  const handoff = packetFieldValue(packet, /\bhandoff\b/i);
  for (const label of ["findings", "files", "commands/evidence", "risks", "owner action", "not-done"]) {
    const pattern = new RegExp(`\\b${escapeRegExp(label)}\\b`, "i");
    if (!pattern.test(handoff)) missing.push(`handoff ${label}`);
  }
  if (!missing.length) return null;
  return {
    code: "subagent_packet_invalid",
    missingFields: [...new Set(missing)],
    template: SUBAGENT_PACKET_TEMPLATE,
    message: `subagent packet failed (missing/invalid: ${[...new Set(missing)].join(", ")})`,
  };
}

const SUBAGENT_PACKET_LEAN_HANDOFF = `handoff:
  findings: <facts or verdict>
  files: <files inspected or changed>
  commands/evidence: <commands run and results, or artifact pointer>
  risks: <remaining risk>
  owner action: <what the lead decides/integrates>
  not-done: <explicit gaps>`;

function renderSubagentPacket(f) {
  return [
    `objective: ${f.objective}`,
    `allowed files: ${f.allowedFiles}`,
    `forbidden files: ${f.forbiddenFiles}`,
    `tool-use guard: ${f.toolGuard || DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}`,
    `expected output: ${f.expectedOutput}`,
    `evidence command or artifact: ${f.evidence}`,
    `stop condition: ${f.stop}`,
    SUBAGENT_PACKET_LEAN_HANDOFF,
  ].join("\n");
}

const PARALLEL_INTRO =
  `PARALLEL FAN-OUT: spawn one worker per packet below at the same time (Claude: multiple Agent-tool calls in one turn, or the Workflow tool; Codex: parallel spawn_agent). Each runs read-only in its own context and returns only its handoff; the lead concatenates and integrates. Disjoint scope, so workers cannot conflict. Lint one packet at a time via \`${ORCHESTRATION_CLI} subagent-packet-check --stdin\`.`;
const SINGLE_INTRO =
  "SINGLE WORKER: spawn one subagent (Claude Agent tool / Codex spawn_agent). It returns its handoff; the lead integrates.";
const SEQUENTIAL_INTRO =
  "SEQUENTIAL STAGES: run these packets in order; each stage's output feeds the next. The lead integrates between stages.";

const SUBAGENT_PACKET_PRESETS = {
  "codebase-map": {
    mode: "parallel",
    intro: PARALLEL_INTRO,
    defaultTargets: ["src/<area>/**", "tools/<area>/**"],
    build: (t) => ({
      label: `map ${t}`,
      text: renderSubagentPacket({
        objective: `Map how the ${t} area works and its public entry points; produce a <=200-word brief. Read only; make no edits.`,
        allowedFiles: t,
        forbiddenFiles: "everything outside the allowed files; make no edits anywhere",
        expectedOutput: "brief: entry points (file:line) + data flow + 3 risks or unknowns",
        evidence: "inline handoff (small)",
        stop: "entry points and data flow identified, or 10 files read",
      }),
    }),
  },
  review: {
    mode: "parallel",
    intro: PARALLEL_INTRO,
    defaultTargets: ["correctness", "readability", "scope"],
    build: (axis) => ({
      label: `review:${axis}`,
      text: renderSubagentPacket({
        objective: `Review <artifact-or-feature> on ONE axis: ${axis}. You are a fresh reviewer, not the builder. Give a verdict and concrete issues only.`,
        allowedFiles: "<dir>/<artifact-file.ext>",
        forbiddenFiles: "no edits anywhere",
        expectedOutput: "verdict (pass/concerns/fail) + issues (file:line + one-line fix), <=10 bullets",
        evidence: "inline handoff",
        stop: `the artifact is fully reviewed on the ${axis} axis`,
      }),
    }),
  },
  "asset-research": {
    mode: "single",
    intro: SINGLE_INTRO,
    build: () => ({
      label: "asset / source / license research",
      text: renderSubagentPacket({
        objective: "Find <N> CC0/CC-BY <asset-type> candidates for <game-id>; report license, provenance, and integrity for each. Do not import anything.",
        allowedFiles: "gamedesign/sources/**;gamedesign/knowledge/**;tmp/<game-id>-asset-candidates.md",
        forbiddenFiles: "src; state; any runtime pack; hot docs (AGENTS.md, AI_PIPELINE.md, tasks/STATUS.md)",
        toolGuard: `${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}; prefer authoritative sources over SEO content farms; verify each license URL`,
        expectedOutput: "<=5 candidates, each with URL + license + provenance + integrity check",
        evidence: "tmp/<game-id>-asset-candidates.md",
        stop: "5 viable candidates found OR 8 sources checked with none viable",
      }),
    }),
  },
  "texture-gen": {
    mode: "parallel",
    intro: `${PARALLEL_INTRO} Image gen uses the delegated-image-generation skill: Codex imagegen (Path A) first, Antigravity agy (Path B) as fallback; verify the PNG by size and eyeball.`,
    defaultTargets: ["<asset-a>", "<asset-b>"],
    build: (asset) => ({
      label: `gen ${asset}`,
      text: renderSubagentPacket({
        objective: `Generate ${asset} via the delegated-image-generation skill (Codex imagegen Path A; Antigravity agy Path B fallback). Propose a project-local path; do NOT wire it into runtime.`,
        allowedFiles: `tmp/gen/${asset}.png`,
        forbiddenFiles: "src; state; any runtime pack or manifest; hot docs",
        toolGuard: `${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}; verify the output PNG exists and its dimensions match before returning; never trust the CLI transcript`,
        expectedOutput: "handoff with artifact path + dimensions + a one-line fake-shot self-judgment",
        evidence: `tmp/gen/${asset}.png`,
        stop: "one verified asset produced OR 3 generation attempts fail",
      }),
    }),
  },
  "asset-intake": {
    mode: "sequential",
    intro: SEQUENTIAL_INTRO,
    stages: () => [
      {
        label: "stage 1: source research",
        text: renderSubagentPacket({
          objective: "Decide a source for <asset> in <game-id>: find a CC0/CC-BY asset (URL+license+provenance) OR write a concrete generation prompt. Do not import or generate yet.",
          allowedFiles: "gamedesign/sources/**;gamedesign/knowledge/**;tmp/<game-id>-<asset>-intake.md",
          forbiddenFiles: "src; state; runtime packs; hot docs",
          toolGuard: `${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}; prefer authoritative sources; verify each license URL`,
          expectedOutput: "a licensed source OR a ready generation prompt, written to the intake note",
          evidence: "tmp/<game-id>-<asset>-intake.md",
          stop: "a source or a prompt is decided",
        }),
      },
      {
        label: "stage 2: generate",
        text: renderSubagentPacket({
          objective: "Generate <asset> from stage 1's source/prompt via the delegated-image-generation skill (Codex imagegen Path A; Antigravity agy Path B fallback).",
          allowedFiles: "tmp/gen/<asset>.png",
          forbiddenFiles: "src; state; runtime packs or manifests; hot docs",
          toolGuard: `${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}; verify the output PNG exists and dimensions match before returning; never trust the CLI transcript`,
          expectedOutput: "the generated PNG + path + dimensions",
          evidence: "tmp/gen/<asset>.png",
          stop: "one verified PNG produced OR 3 attempts fail",
        }),
      },
      {
        label: "stage 3: verify + propose",
        text: renderSubagentPacket({
          objective: "Verify the generated <asset> (dimensions, transparency, look vs the art reference) and propose a project-local destination + provenance note. Do NOT wire it into runtime.",
          allowedFiles: "tmp/gen/<asset>.png;tmp/<game-id>-<asset>-intake.md",
          forbiddenFiles: "src; state; runtime packs or manifests; hot docs",
          expectedOutput: "verdict + proposed destination path + provenance line",
          evidence: "tmp/<game-id>-<asset>-intake.md",
          stop: "verdict and proposed destination recorded",
        }),
      },
    ],
  },
};

export function subagentPacketPresetNames() {
  return Object.keys(SUBAGENT_PACKET_PRESETS);
}

export function subagentPacketPreset(name, targets = []) {
  const def = SUBAGENT_PACKET_PRESETS[name];
  if (!def) {
    const error = new Error(`unknown subagent packet preset: ${name}`);
    error.code = "unknown_preset";
    error.presets = subagentPacketPresetNames();
    throw error;
  }
  let packets;
  if (def.mode === "sequential") {
    packets = def.stages();
  } else if (def.mode === "single") {
    packets = [def.build()];
  } else {
    const list = Array.isArray(targets) && targets.length ? targets : def.defaultTargets;
    packets = list.map((t) => def.build(t));
  }
  return { name, mode: def.mode, intro: def.intro, packets };
}

export function renderSubagentPacketPreset(name, targets = []) {
  const { intro, packets } = subagentPacketPreset(name, targets);
  const total = packets.length;
  const blocks = packets.map((p, i) => `# packet ${i + 1}/${total} - ${p.label}\n${p.text}`);
  return `${intro}\n\n${blocks.join("\n\n")}`;
}

function packetFieldValue(text, fieldPattern) {
  const lines = String(text || "").split(/\r?\n/);
  const out = [];
  let collecting = false;
  for (const line of lines) {
    const match = line.match(/^\s*(?:[-*]\s*)?(.+?)\s*:\s*(.*)$/);
    if (match) {
      if (collecting && !/^\s/.test(line)) break;
      if (fieldPattern.test(match[1])) {
        collecting = true;
        out.push(match[2].trim());
        continue;
      }
    }
    if (collecting) out.push(line.trim());
  }
  return out.join(" ").trim();
}

export function orchestrationPreflightProblem(doc) {
  const log = sectionText(doc.body || "", "Log");
  const missing = missingOrchestrationFields(log, {
    requiredFields: ORCHESTRATION_PREFLIGHT_FIELDS,
    requireBoundedAllowedFiles: true,
  });
  if (!missing.length) return null;
  const taskId = doc.fields?.id || "";
  return {
    code: "orchestration_preflight_missing",
    taskId,
    status: doc.fields?.status || "",
    missingFields: missing,
    acceptedFields: ORCHESTRATION_PREFLIGHT_FIELDS.map(([name]) => name),
    template: ORCHESTRATION_PACKET_TEMPLATE,
    message: `${taskId || "task"}: orchestration packet preflight failed (missing/invalid: ${missing.join(", ")})`,
    nextAction: orchestrationPreflightNextAction(taskId),
  };
}

export function missingOrchestrationFields(log, options = {}) {
  const {
    requireBoundedAllowedFiles = false,
    requiredFields = ORCHESTRATION_REQUIRED_FIELDS,
  } = typeof options === "boolean" ? {} : options;
  const blocks = orchestrationUsedBlocks(log);
  if (!blocks.length) {
    return ["orchestration: used packet"];
  }
  const baseline = requiredFields.map(([name]) => name);
  if (requireBoundedAllowedFiles) baseline.push("allowed files bounds");
  let bestMissing = baseline;
  for (const block of blocks) {
    const missing = requiredFields
      .filter(([, pattern]) => !hasMeaningfulFieldValue(block, pattern))
      .map(([name]) => name);
    if (
      requireBoundedAllowedFiles
      && hasMeaningfulFieldValue(block, ORCHESTRATION_ALLOWED_FILES_FIELD_PATTERN)
      && !isBoundedOrchestrationAllowedFiles(fieldValue(block, ORCHESTRATION_ALLOWED_FILES_FIELD_PATTERN))
    ) {
      missing.push("allowed files bounds");
    }
    if (missing.length < bestMissing.length) {
      bestMissing = missing;
    }
  }
  return bestMissing;
}

export function isBoundedOrchestrationAllowedFiles(text) {
  return boundedAllowedFilesProblem(text) === "";
}

function orchestrationPreflightNextAction(taskId) {
  const selector = taskId || "<task-id>";
  return `add a complete orchestration packet from \`${ORCHESTRATION_CLI} orchestration-template\`, then rerun \`${ORCHESTRATION_CLI} orchestration-check ${selector} --json\``;
}

function hasText(value) {
  return typeof value === "string" && value.trim() && !/^(tbd|todo|none|n\/a|na|unknown|\.\.\.)$/i.test(value.trim());
}

function orchestrationUsedBlocks(log) {
  const lines = String(log || "").split(/\r?\n/);
  const blocks = [];
  for (let i = 0; i < lines.length; i += 1) {
    if (!/orchestration:\s*used\b/i.test(lines[i])) continue;
    const block = [lines[i]];
    for (let j = i + 1; j < lines.length; j += 1) {
      if (/^\s*[-*]\s+\S/.test(lines[j])) break;
      block.push(lines[j]);
    }
    blocks.push(block.join("\n"));
  }
  return blocks;
}

function hasMeaningfulFieldValue(text, fieldPattern) {
  for (const line of String(text || "").split(/\r?\n/)) {
    const match = line.match(/^\s*(?:[-*]\s*)?(.+?)\s*:\s*(.*)$/);
    if (!match || !fieldPattern.test(match[1])) continue;
    const value = match[2].trim();
    if (!value || /^(tbd|todo|none|n\/a|na|unknown|\.\.\.)$/i.test(value)) {
      return false;
    }
    return true;
  }
  return false;
}

function boundedAllowedFilesProblem(text) {
  const value = String(text || "").trim();
  if (!value || /^(tbd|todo|none|n\/a|na|unknown|\.\.\.)$/i.test(value)) return "empty allowed files";
  const entries = value
    .split(/[;,]/)
    .map((entry) => entry.trim().replace(/^[`'"]+|[`'"]+$/g, ""))
    .filter(Boolean);
  if (entries.length === 0) return "empty allowed files";
  if (entries.length > 16) return "too many allowed file entries";
  for (const entry of entries) {
    const normalized = entry.replaceAll("\\", "/");
    if (!normalized || /\s/.test(normalized)) return `invalid allowed file entry: ${entry}`;
    if (/^[a-z][a-z0-9+.-]*:/i.test(normalized)) return `non-local allowed file entry: ${entry}`;
    if (/^(?:[a-z]:|\/)/i.test(normalized)) return `absolute allowed file entry: ${entry}`;
    if (normalized.includes("//")) return `invalid allowed file entry: ${entry}`;
    if (normalized === "." || normalized === "*" || normalized === "**") return `too broad allowed file entry: ${entry}`;
    if (normalized.endsWith("/")) return `directory-only allowed file entry: ${entry}`;
    const segments = normalized.split("/");
    if (segments.some((segment) => !segment || segment === "." || segment === "..")) {
      return `path traversal allowed file entry: ${entry}`;
    }
    const recursiveIndex = segments.indexOf("**");
    if (recursiveIndex >= 0) {
      if (recursiveIndex !== segments.length - 1 || segments.length < 3) return `too broad allowed file entry: ${entry}`;
      continue;
    }
    if (segments.some((segment) => segment.includes("**"))) return `too broad allowed file entry: ${entry}`;
    const wildcardSegments = segments.filter((segment) => /[*?[\]{}]/.test(segment));
    if (wildcardSegments.length > 1 || (wildcardSegments.length === 1 && wildcardSegments[0] !== segments[segments.length - 1])) {
      return `unbounded wildcard allowed file entry: ${entry}`;
    }
    const leaf = segments[segments.length - 1];
    if (/[*?[\]{}]/.test(leaf) && (leaf === "*" || !/\.[^./*?[\]{}]+$/.test(leaf))) {
      return `unbounded wildcard allowed file entry: ${entry}`;
    }
    if (!/[*?[\]{}]/.test(leaf) && !leaf.includes(".")) {
      return `directory-like allowed file entry: ${entry}`;
    }
  }
  return "";
}

function fieldValue(text, fieldPattern) {
  const lines = String(text || "").split(/\r?\n/);
  const out = [];
  let collecting = false;
  for (const line of lines) {
    const match = line.match(/^\s*(?:[-*]\s*)?(.+?)\s*:\s*(.*)$/);
    if (match) {
      if (collecting && isOrchestrationFieldLabel(match[1])) break;
      if (fieldPattern.test(match[1])) {
        collecting = true;
        out.push(match[2].trim());
        continue;
      }
    }
    if (collecting) out.push(line.trim());
  }
  return out.join(" ").trim();
}

function isOrchestrationFieldLabel(label) {
  return ORCHESTRATION_PREFLIGHT_FIELDS.some(([, pattern]) => pattern.test(label));
}

function sectionText(body, title) {
  const pattern = new RegExp(`(?:^|\\n)## ${escapeRegExp(title)}[ \\t]*\\n([\\s\\S]*?)(?=\\n##\\s+|$)`, "i");
  const match = String(body || "").match(pattern);
  return match ? match[1].replace(/- \[ \]\s*$/, "").trim() : "";
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
