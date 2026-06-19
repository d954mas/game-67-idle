#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { spawnSync } from "node:child_process";

function usage() {
  console.error(`usage:
  node tools/product_gate/review.mjs --project <game-id> --task <task-id> --screenshot <path> --verdict pass|fail [options]

Options:
  --surface <name>       desktop, portrait, tablet, web, or another short label
  --output <path>        markdown output path
  --json-output <path>   optional JSON output path
  --where <text>         answer: where am I?
  --action <text>        answer: what should I do now?
  --response <text>      answer: what changed after input?
  --reward <text>        answer: what did I get / why continue?
  --game-look <text>     answer: why does this look like a game?
  --visual-strict        require structured visual critique scores/issues
  --visual-score <axis=n> repeatable; axes: composition, readability, ui_controls, action_direction, art_quality, audience_fit
  --visual-issue <severity:axis:text> repeatable; severity: blocker, major, minor
  --state-matrix <path> JSON matrix with required/covered/not-covered states
  --require-state <tag>  repeatable; state required by this slice's acceptance matrix
  --covered-state <tag[:evidence]> repeatable; state covered by this screenshot/gate
  --not-covered-state <tag:reason> repeatable; required state explicitly not covered by this gate
  --problem <text>       required for strict fail
  --next <text>          required for strict fail
  --index-output <path>  latest-gate JSON index path
  --task-log             append a compact evidence line to the task log
  --strict               fail if a pass lacks strong answers or a fail lacks next action
  --verify               request one independent clean-context re-check (records a pending verification; default off)`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = { visualScores: [], visualIssues: [], requiredStates: [], coveredStates: [], notCoveredStates: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--strict") values.strict = true;
    else if (arg === "--visual-strict") values.visualStrict = true;
    else if (arg === "--task-log") values.taskLog = true;
    else if (arg === "--verify") values.verify = true;
    else if (arg === "--visual-score") {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.visualScores.push(value);
      index += 1;
    } else if (arg === "--visual-issue") {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.visualIssues.push(value);
      index += 1;
    } else if (arg === "--require-state") {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.requiredStates.push(value);
      index += 1;
    } else if (arg === "--covered-state") {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.coveredStates.push(value);
      index += 1;
    } else if (arg === "--not-covered-state") {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.notCoveredStates.push(value);
      index += 1;
    }
    else if (arg.startsWith("--")) {
      const key = arg.slice(2);
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values[key] = value;
      index += 1;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return values;
}

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function sanitizeToken(value) {
  return String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9_-]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 80) || "gate";
}

function defaultOutput(project, surface) {
  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  return `gamedesign/projects/${sanitizeToken(project)}/reviews/product_read_gate_${stamp}_${sanitizeToken(surface)}.md`;
}

function defaultJsonOutput(markdownPath) {
  return markdownPath.replace(/\.md$/i, ".json");
}

function defaultIndexOutput(project) {
  return `gamedesign/projects/${sanitizeToken(project)}/reviews/product_read_gate_latest.json`;
}

function hasUsefulAnswer(value) {
  return String(value || "").trim().length >= 8;
}

const VISUAL_AXES = [
  "composition",
  "readability",
  "ui_controls",
  "action_direction",
  "art_quality",
  "audience_fit",
];
const VISUAL_SEVERITIES = new Set(["blocker", "major", "minor"]);
const VISUAL_PASS_THRESHOLD = 4;

function parseStateToken(raw, separator, needsDetail, detailName) {
  const value = String(raw || "").trim();
  const index = value.indexOf(separator);
  const tag = (index >= 0 ? value.slice(0, index) : value).trim();
  const detail = (index >= 0 ? value.slice(index + separator.length) : "").trim();
  const tagPattern = /^[a-z0-9][a-z0-9_-]*$/;
  const errors = [];
  if (!tagPattern.test(tag)) {
    errors.push(`invalid state tag: ${tag || "(missing)"}`);
  }
  if (needsDetail && !hasUsefulAnswer(detail)) {
    errors.push(`state ${tag || "(missing)"} needs ${detailName}`);
  }
  return { tag, detail, errors };
}

function parseRequiredStates(rawStates) {
  const required = [];
  const errors = [];
  const seen = new Set();
  for (const raw of rawStates || []) {
    const parsed = parseStateToken(raw, ":", false, "a value");
    errors.push(...parsed.errors);
    if (!parsed.errors.length && !seen.has(parsed.tag)) {
      required.push(parsed.tag);
      seen.add(parsed.tag);
    }
  }
  return { required, errors };
}

function parseStateEntries(rawEntries, { needsDetail = false, detailName = "detail" } = {}) {
  const entries = [];
  const errors = [];
  const seen = new Set();
  for (const raw of rawEntries || []) {
    const parsed = parseStateToken(raw, ":", needsDetail, detailName);
    errors.push(...parsed.errors);
    if (!parsed.errors.length && !seen.has(parsed.tag)) {
      entries.push({ tag: parsed.tag, detail: parsed.detail });
      seen.add(parsed.tag);
    }
  }
  return { entries, errors };
}

function normalizeMatrixStateEntry(tag, raw) {
  const value = raw && typeof raw === "object" ? raw : {};
  const status = String(value.status || (value.covered ? "covered" : "") || "").trim();
  return {
    tag: String(value.tag || tag || "").trim(),
    required: value.required !== false,
    status,
    evidence: String(value.evidence || value.proof || value.path || "").trim(),
    reason: String(value.reason || value.debt || value.not_covered_reason || "").trim(),
  };
}

function loadStateMatrix(matrixPath) {
  const fullPath = resolve(matrixPath);
  if (!existsSync(fullPath)) fail(`state matrix does not exist: ${matrixPath}`);
  let matrix;
  try {
    matrix = JSON.parse(readFileSync(fullPath, "utf8"));
  } catch (error) {
    fail(`state matrix is not valid JSON: ${matrixPath}: ${error.message}`);
  }

  const required = [];
  const covered = [];
  const notCovered = [];
  const pushRequired = (tag) => {
    if (tag) required.push(String(tag).trim());
  };

  for (const tag of matrix.required_states || matrix.requiredStates || []) {
    pushRequired(tag);
  }

  const stateEntries = [];
  if (Array.isArray(matrix.states)) {
    for (const entry of matrix.states) stateEntries.push(normalizeMatrixStateEntry("", entry));
  } else if (matrix.states && typeof matrix.states === "object") {
    for (const [tag, entry] of Object.entries(matrix.states)) {
      stateEntries.push(normalizeMatrixStateEntry(tag, entry));
    }
  }

  for (const entry of stateEntries) {
    if (entry.required) pushRequired(entry.tag);
    if (entry.status === "covered") {
      covered.push(entry.evidence ? `${entry.tag}:${entry.evidence}` : entry.tag);
    } else if (entry.status === "not_covered" || entry.status === "not-covered" || entry.status === "debt") {
      notCovered.push(`${entry.tag}:${entry.reason || "not covered by this gate"}`);
    }
  }

  return { required, covered, notCovered };
}

function mergeStateMatrix(values) {
  const matrixPath = values["state-matrix"];
  if (!matrixPath) return values;
  const matrix = loadStateMatrix(matrixPath);
  return {
    ...values,
    requiredStates: [...matrix.required, ...(values.requiredStates || [])],
    coveredStates: [...(values.coveredStates || []), ...matrix.covered],
    notCoveredStates: [...(values.notCoveredStates || []), ...matrix.notCovered],
  };
}

function parseVisualScores(rawScores) {
  const scores = {};
  const errors = [];
  for (const raw of rawScores || []) {
    const [axisRaw, scoreRaw] = String(raw || "").split("=");
    const axis = String(axisRaw || "").trim();
    const score = Number(String(scoreRaw || "").trim());
    if (!VISUAL_AXES.includes(axis)) {
      errors.push(`unknown visual score axis: ${axis || "(missing)"}`);
      continue;
    }
    if (!Number.isInteger(score) || score < 1 || score > 5) {
      errors.push(`visual score for ${axis} must be an integer 1-5`);
      continue;
    }
    scores[axis] = score;
  }
  return { scores, errors };
}

function parseVisualIssues(rawIssues) {
  const issues = [];
  const errors = [];
  for (const raw of rawIssues || []) {
    const parts = String(raw || "").split(":");
    const severity = String(parts.shift() || "").trim();
    const axis = String(parts.shift() || "").trim();
    const text = parts.join(":").trim();
    if (!VISUAL_SEVERITIES.has(severity)) errors.push(`unknown visual issue severity: ${severity || "(missing)"}`);
    if (!VISUAL_AXES.includes(axis)) errors.push(`unknown visual issue axis: ${axis || "(missing)"}`);
    if (!hasUsefulAnswer(text)) errors.push(`visual issue for ${axis || "(missing)"} needs concrete text`);
    if (VISUAL_SEVERITIES.has(severity) && VISUAL_AXES.includes(axis) && hasUsefulAnswer(text)) {
      issues.push({ severity, axis, text });
    }
  }
  return { issues, errors };
}

function relPath(path) {
  return resolve(path).startsWith(process.cwd()) ? resolve(path).slice(process.cwd().length + 1).replaceAll("\\", "/") : path;
}

function validate(values) {
  const errors = [];
  if (!values.project) errors.push("--project is required");
  if (!values.screenshot) errors.push("--screenshot is required");
  if (!values.verdict) errors.push("--verdict is required");
  if (values.verdict && !["pass", "fail"].includes(values.verdict)) errors.push("--verdict must be pass or fail");
  if (values.screenshot && !existsSync(resolve(values.screenshot))) errors.push(`screenshot does not exist: ${values.screenshot}`);

  const answers = ["where", "action", "response", "reward", "game-look"];
  if (values.strict || values.verdict === "pass") {
    for (const key of answers) {
      if (!hasUsefulAnswer(values[key])) errors.push(`--${key} needs a concrete player-read answer`);
    }
  }
  if (values.strict && values.verdict === "fail") {
    if (!hasUsefulAnswer(values.problem)) errors.push("--problem is required for strict fail");
    if (!hasUsefulAnswer(values.next)) errors.push("--next is required for strict fail");
  }

  const visualScores = parseVisualScores(values.visualScores);
  const visualIssues = parseVisualIssues(values.visualIssues);
  const requiredStates = parseRequiredStates(values.requiredStates);
  const coveredStates = parseStateEntries(values.coveredStates, { needsDetail: false, detailName: "evidence" });
  const notCoveredStates = parseStateEntries(values.notCoveredStates, { needsDetail: true, detailName: "a reason" });
  errors.push(...visualScores.errors, ...visualIssues.errors);
  errors.push(...requiredStates.errors, ...coveredStates.errors, ...notCoveredStates.errors);
  if ((values.strict || values.visualStrict) && values.verdict === "pass") {
    const covered = new Set(coveredStates.entries.map((entry) => entry.tag));
    const debt = new Set(notCoveredStates.entries.map((entry) => entry.tag));
    for (const tag of requiredStates.required) {
      if (!covered.has(tag) && !debt.has(tag)) {
        errors.push(`required state ${tag} is neither covered nor marked not covered`);
      }
    }
  }
  if (values.visualStrict) {
    for (const axis of VISUAL_AXES) {
      if (visualScores.scores[axis] === undefined) errors.push(`--visual-score ${axis}=1-5 is required for --visual-strict`);
    }
    if (values.verdict === "pass") {
      for (const axis of VISUAL_AXES) {
        const score = visualScores.scores[axis];
        if (score !== undefined && score < VISUAL_PASS_THRESHOLD) {
          errors.push(`visual pass requires ${axis} score >= ${VISUAL_PASS_THRESHOLD}`);
        }
      }
      const blockingIssue = visualIssues.issues.find((issue) => issue.severity === "blocker" || issue.severity === "major");
      if (blockingIssue) errors.push(`visual pass cannot include ${blockingIssue.severity} issue for ${blockingIssue.axis}`);
    }
    if (values.verdict === "fail" && visualIssues.issues.length === 0) {
      errors.push("--visual-issue is required for --visual-strict fail");
    }
  }
  return errors;
}

function renderMarkdown(record) {
  const lines = [
    "---",
    "type: ProductReadGate",
    `project: ${record.project}`,
    `task: ${record.task || ""}`,
    `surface: ${record.surface}`,
    `verdict: ${record.verdict}`,
    `timestamp: ${record.timestamp}`,
    "---",
    "",
    `# Product Read Gate - ${record.project} / ${record.surface}`,
    "",
    `Verdict: **${record.verdict.toUpperCase()}**`,
    "",
    `Screenshot: \`${record.screenshot}\``,
    "",
    "## Player Read",
    "",
    `- Where am I? ${record.answers.where || "(missing)"}`,
    `- What should I do now? ${record.answers.action || "(missing)"}`,
    `- What changed after input? ${record.answers.response || "(missing)"}`,
    `- What is the reward / why continue? ${record.answers.reward || "(missing)"}`,
    `- Why does this look like a game? ${record.answers.game_look || "(missing)"}`,
    "",
    "## State Coverage",
    "",
    "Required states:",
    ...(record.state_coverage.required.length > 0 ? record.state_coverage.required.map((tag) => `- ${tag}`) : ["- (none)"]),
    "",
    "Covered states:",
    ...(record.state_coverage.covered.length > 0
      ? record.state_coverage.covered.map((entry) => `- ${entry.tag}${entry.evidence ? `: ${entry.evidence}` : ""}`)
      : ["- (none)"]),
    "",
    "Not covered / debt:",
    ...(record.state_coverage.not_covered.length > 0
      ? record.state_coverage.not_covered.map((entry) => `- ${entry.tag}: ${entry.reason}`)
      : ["- (none)"]),
    "",
    "## Review",
    "",
    `Problem: ${record.problem || "(none)"}`,
    "",
    `Next: ${record.next || "(none)"}`,
    "",
  ];
  if (record.visual_critique.strict || Object.keys(record.visual_critique.scores).length > 0 || record.visual_critique.issues.length > 0) {
    lines.push("## Visual Critique");
    lines.push("");
    lines.push(`Strict: ${record.visual_critique.strict ? "yes" : "no"}`);
    lines.push(`Pass threshold: ${record.visual_critique.pass_threshold}`);
    lines.push("");
    lines.push("Scores:");
    for (const axis of VISUAL_AXES) {
      lines.push(`- ${axis}: ${record.visual_critique.scores[axis] ?? "(missing)"}`);
    }
    lines.push("");
    lines.push("Issues:");
    if (record.visual_critique.issues.length === 0) {
      lines.push("- (none)");
    } else {
      for (const issue of record.visual_critique.issues) {
        lines.push(`- ${issue.severity} / ${issue.axis}: ${issue.text}`);
      }
    }
    lines.push("");
  }
  if (record.verification && record.verification.required) {
    lines.push("## Verification");
    lines.push("");
    lines.push(`Status: ${record.verification.status} (independent confirmation requested)`);
    lines.push(`Recheck: ${record.verification.recheck}`);
    lines.push("");
  }
  return lines.join("\n");
}

function runTaskLog(record, markdownPath) {
  if (!record.task) fail("--task is required when --task-log is set");
  const log = [
    `product gate ${record.verdict.toUpperCase()} (${record.surface})`,
    `review: ${markdownPath}`,
    `screenshot: ${record.screenshot}`,
    `next: ${record.verdict === "pass" ? "continue to the next narrow slice" : (record.next || "fix the screen before adding content")}`,
  ].join("; ");
  const result = spawnSync(process.execPath, ["tools/taskboard/cli.mjs", "set", record.task, "--log", log], {
    cwd: process.cwd(),
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.status !== 0) {
    if (result.stdout) process.stdout.write(result.stdout);
    if (result.stderr) process.stderr.write(result.stderr);
    fail(`task log update failed for ${record.task}`);
  }
}

const values = mergeStateMatrix(parseArgs(process.argv.slice(2)));
if (values.help) usage();
const surface = values.surface || "desktop";
const output = values.output || defaultOutput(values.project, surface);
const jsonOutput = values["json-output"] || defaultJsonOutput(output);
const indexOutput = values["index-output"] || defaultIndexOutput(values.project);
const errors = validate(values);
if (errors.length > 0) fail(errors.join("\n"));
const visualScores = parseVisualScores(values.visualScores);
const visualIssues = parseVisualIssues(values.visualIssues);
const requiredStates = parseRequiredStates(values.requiredStates);
const coveredStates = parseStateEntries(values.coveredStates, { needsDetail: false, detailName: "evidence" });
const notCoveredStates = parseStateEntries(values.notCoveredStates, { needsDetail: true, detailName: "a reason" });

const record = {
  schema: "game.product_read_gate",
  version: 1,
  project: values.project,
  task: values.task || "",
  surface,
  verdict: values.verdict,
  timestamp: new Date().toISOString(),
  screenshot: relPath(values.screenshot),
  answers: {
    where: values.where || "",
    action: values.action || "",
    response: values.response || "",
    reward: values.reward || "",
    game_look: values["game-look"] || "",
  },
  visual_critique: {
    strict: Boolean(values.visualStrict),
    axes: VISUAL_AXES,
    pass_threshold: VISUAL_PASS_THRESHOLD,
    scores: visualScores.scores,
    issues: visualIssues.issues,
  },
  state_coverage: {
    required: requiredStates.required,
    covered: coveredStates.entries.map((entry) => ({ tag: entry.tag, evidence: entry.detail })),
    not_covered: notCoveredStates.entries.map((entry) => ({ tag: entry.tag, reason: entry.detail })),
  },
  problem: values.problem || "",
  next: values.next || "",
  verification: values.verify
    ? {
        required: true,
        status: "pending",
        recheck: "Re-run this gate's named check (screenshot + validator) in a clean, independent context and return CONFIRM or REFUTE. A green gate is not self-graded.",
      }
    : { required: false },
};

mkdirSync(dirname(resolve(output)), { recursive: true });
writeFileSync(resolve(output), renderMarkdown(record), "utf8");
mkdirSync(dirname(resolve(jsonOutput)), { recursive: true });
writeFileSync(resolve(jsonOutput), `${JSON.stringify(record, null, 2)}\n`, "utf8");
mkdirSync(dirname(resolve(indexOutput)), { recursive: true });
writeFileSync(resolve(indexOutput), `${JSON.stringify({ ...record, markdown: output, json: jsonOutput }, null, 2)}\n`, "utf8");

if (values.taskLog) runTaskLog(record, output);

console.log(`# Product Read Gate`);
console.log(`Verdict: ${record.verdict}`);
console.log(`Markdown: ${output}`);
console.log(`JSON: ${jsonOutput}`);
console.log(`Latest: ${indexOutput}`);
console.log(`Next: ${record.verdict === "pass" ? "continue to the next narrow slice" : (record.next || "fix the screen before adding content")}`);
if (record.verification.required) {
  console.log("Verification: pending independent confirmation (run one clean-context verifier; CONFIRM/REFUTE)");
}
