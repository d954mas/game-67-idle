#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, relative, resolve } from "node:path";
import { spawnSync } from "node:child_process";

function usage() {
  console.error(`usage:
  node tools/product_gate/slice_hygiene.mjs [options]

options:
  --root <repo>                 default: cwd
  --threshold <n>               changed-file warning threshold, default 30
  --snapshot                    mark broad diff as an accepted end-of-experiment snapshot
  --strict                      exit non-zero for missing evidence, broad diffs, or red gates
  --promise-push                fail if upstream/push remote is not configured
  --changed-file <path>         repeatable; defaults to git status changed files
  --build-evidence <text>       build/probe/check command evidence
  --probe-evidence <text>       gameplay/probe/scenario evidence
  --product-gate <path>         product gate JSON or Markdown evidence
  --screenshot <path>           screenshot evidence path
  --profile-guard <path|text>   optional profiler-review note from ai.mjs status
  --known-red-gate <text>       accepted red gate/stale fail audit note
  --json-output <path>
  --report <path>

Audits prototype slice handoff/commit hygiene: diff size, evidence checklist,
push target visibility, and stale/failing review artifacts.`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = {
    changedFiles: [],
    buildEvidence: [],
    probeEvidence: [],
    productGates: [],
    screenshots: [],
    profileGuards: [],
    knownRedGates: [],
  };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") usage();
    if (arg === "--snapshot") {
      values.snapshot = true;
      continue;
    }
    if (arg === "--strict") {
      values.strict = true;
      continue;
    }
    if (arg === "--promise-push") {
      values.promisePush = true;
      continue;
    }
    const next = argv[index + 1];
    if (!arg.startsWith("--") || next === undefined || next.startsWith("--")) usage();
    index += 1;
    if (arg === "--root") values.root = next;
    else if (arg === "--threshold") values.threshold = next;
    else if (arg === "--changed-file") values.changedFiles.push(next);
    else if (arg === "--build-evidence") values.buildEvidence.push(next);
    else if (arg === "--probe-evidence") values.probeEvidence.push(next);
    else if (arg === "--product-gate") values.productGates.push(next);
    else if (arg === "--screenshot") values.screenshots.push(next);
    else if (arg === "--profile-guard") values.profileGuards.push(next);
    else if (arg === "--known-red-gate") values.knownRedGates.push(next);
    else if (arg === "--json-output") values.jsonOutput = next;
    else if (arg === "--report") values.report = next;
    else usage();
  }
  return values;
}

function runGit(root, args) {
  const result = spawnSync("git", args, {
    cwd: root,
    encoding: "utf8",
    shell: false,
    stdio: ["ignore", "pipe", "pipe"],
  });
  return {
    ok: result.status === 0,
    status: result.status,
    stdout: result.stdout || "",
    stderr: result.stderr || "",
  };
}

function gitChangedFiles(root) {
  const result = runGit(root, ["status", "--porcelain=v1", "-z"]);
  if (!result.ok) return { files: [], git_ok: false, error: result.stderr.trim() || result.stdout.trim() };
  const files = [];
  const entries = result.stdout.split("\0").filter(Boolean);
  for (let index = 0; index < entries.length; index += 1) {
    const entry = entries[index];
    const status = entry.slice(0, 2);
    const path = entry.slice(3);
    if (!path) continue;
    if (status.includes("R") || status.includes("C")) {
      const next = entries[index + 1];
      if (next) {
        files.push(next);
        index += 1;
      } else {
        files.push(path);
      }
    } else {
      files.push(path);
    }
  }
  return { files: [...new Set(files)].sort(), git_ok: true, error: "" };
}

function pushPolicy(root) {
  const inside = runGit(root, ["rev-parse", "--is-inside-work-tree"]);
  if (!inside.ok) return { status: "no_git_repo", ok_for_promised_push: false, details: inside.stderr.trim() || "not a git repository" };
  const branch = runGit(root, ["rev-parse", "--abbrev-ref", "HEAD"]);
  const upstream = runGit(root, ["rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"]);
  const remote = runGit(root, ["remote", "get-url", "--push", "origin"]);
  const branchName = branch.ok ? branch.stdout.trim() : "";
  const upstreamName = upstream.ok ? upstream.stdout.trim() : "";
  const remoteUrl = remote.ok ? remote.stdout.trim() : "";
  if (upstreamName && remoteUrl) {
    return {
      status: "upstream_and_push_remote_configured",
      ok_for_promised_push: true,
      branch: branchName,
      upstream: upstreamName,
      push_remote: remoteUrl,
    };
  }
  if (remoteUrl) {
    return {
      status: "push_remote_without_upstream",
      ok_for_promised_push: false,
      branch: branchName,
      upstream: upstreamName,
      push_remote: remoteUrl,
      details: "origin push URL exists but current branch upstream is not configured",
    };
  }
  return {
    status: "push_remote_missing",
    ok_for_promised_push: false,
    branch: branchName,
    upstream: upstreamName,
    push_remote: remoteUrl,
    details: "no origin push URL found",
  };
}

function readIfExists(root, path) {
  const absolute = resolve(root, path);
  if (!existsSync(absolute)) return "";
  try {
    return readFileSync(absolute, "utf8");
  } catch {
    return "";
  }
}

function isReviewOrAudit(path) {
  return /(?:^|[\\/])(reviews?|audit|audits)(?:[\\/]|$)/i.test(path) || /(?:audit|review).*\.(?:json|md)$/i.test(path);
}

function redGateEvidence(root, paths) {
  const findings = [];
  for (const path of paths) {
    if (!isReviewOrAudit(path)) continue;
    const text = readIfExists(root, path);
    if (!text) continue;
    if (/"verdict"\s*:\s*"fail"/i.test(text) || /verdict:\s*\*\*fail\*\*/i.test(text) || /\bFAIL\b/.test(text)) {
      findings.push({ path, reason: "changed review/audit evidence contains FAIL/fail verdict" });
    }
  }
  return findings;
}

function gateVerdicts(root, productGates) {
  return productGates.map((path) => {
    const text = readIfExists(root, path);
    if (!text) return { path, exists: false, verdict: "missing" };
    try {
      const parsed = JSON.parse(text);
      return { path, exists: true, verdict: String(parsed.verdict || "unknown").toLowerCase() };
    } catch {
      const fail = /verdict:\s*\*\*fail\*\*/i.test(text) || /\bFAIL\b/.test(text);
      const pass = /verdict:\s*\*\*pass\*\*/i.test(text) || /\bPASS\b/.test(text);
      return { path, exists: true, verdict: fail ? "fail" : pass ? "pass" : "unknown" };
    }
  });
}

function fileExists(root, path) {
  return existsSync(resolve(root, path));
}

function profileGuardVerdicts(root, profileGuards) {
  return profileGuards.map((item) => {
    const text = readIfExists(root, item) || String(item || "");
    const source = readIfExists(root, item) ? "file" : "inline";
    try {
      const parsed = JSON.parse(text);
      const confidence = parsed.current_scope_review_confidence || {};
      if (confidence.usable_for_review === true || confidence.level === "usable") {
        return { path: item, source, verdict: "pass" };
      }
      if (confidence.usable_for_review === false || confidence.level === "broken") {
        return { path: item, source, verdict: "fail", reason: (confidence.blocking_reasons || []).join(", ") || "current scope is not usable" };
      }
      return { path: item, source, verdict: "unknown", reason: "current_scope_review_confidence missing or inconclusive" };
    } catch {
      if (/Current scope review confidence:\s*usable/i.test(text)) {
        return { path: item, source, verdict: "pass" };
      }
      if (/profile guard failed|scope_stale|Current scope review confidence:\s*broken/i.test(text)) {
        return { path: item, source, verdict: "fail", reason: "guard output reports broken current scope" };
      }
      return { path: item, source, verdict: "unknown", reason: "could not confirm usable current-scope profiler guard" };
    }
  });
}

function buildReport(options) {
  const root = resolve(options.root || process.cwd());
  const threshold = Number.isFinite(Number(options.threshold)) ? Number(options.threshold) : 30;
  const gitFiles = options.changedFiles.length > 0 ? { files: options.changedFiles, git_ok: true, error: "" } : gitChangedFiles(root);
  const changedFiles = [...new Set(gitFiles.files.map((file) => file.replaceAll("\\", "/")))].sort();
  const push = pushPolicy(root);
  const productGates = gateVerdicts(root, options.productGates);
  const profileGuards = profileGuardVerdicts(root, options.profileGuards);
  const redArtifacts = redGateEvidence(root, changedFiles);
  const missingScreenshots = options.screenshots.filter((path) => !fileExists(root, path));
  const knownRedGateAccepted = options.knownRedGates.some((item) => String(item).trim());
  const problems = [];
  const warnings = [];

  if (!gitFiles.git_ok) warnings.push(`git status unavailable: ${gitFiles.error || "unknown error"}`);

  if (changedFiles.length > threshold && !options.snapshot) {
    problems.push(`changed files ${changedFiles.length} > threshold ${threshold}; split the slice or rerun with --snapshot for an explicit end-of-experiment snapshot`);
  }
  if (options.snapshot && changedFiles.length > threshold) {
    warnings.push(`snapshot accepted for broad diff (${changedFiles.length} files > ${threshold})`);
  }

  if (options.strict && options.buildEvidence.length === 0) problems.push("missing --build-evidence");
  if (options.strict && options.probeEvidence.length === 0) problems.push("missing --probe-evidence");
  if (options.strict && options.productGates.length === 0) problems.push("missing --product-gate");
  if (options.strict && options.screenshots.length === 0) problems.push("missing --screenshot");
  // Profiler guard is advisory: passive profiling must not block normal game
  // work (supersedes T0028's strict requirement). Missing/weak guard -> warning.
  if (options.strict && options.profileGuards.length === 0) warnings.push("no --profile-guard (advisory: passive profiling does not block handoff)");
  if (missingScreenshots.length > 0) problems.push(`missing screenshot file(s): ${missingScreenshots.join(", ")}`);

  for (const gate of productGates) {
    if (!gate.exists) {
      problems.push(`product gate missing: ${gate.path}`);
    } else if (gate.verdict === "fail" && !knownRedGateAccepted) {
      problems.push(`product gate is fail: ${gate.path}; add --known-red-gate or refresh the gate before commit`);
    } else if (gate.verdict !== "pass" && gate.verdict !== "fail") {
      warnings.push(`product gate verdict unknown: ${gate.path}`);
    }
  }

  // Profiler guard findings are advisory warnings, never blocking problems.
  for (const guard of profileGuards) {
    if (guard.verdict === "fail") {
      warnings.push(`profiler guard is fail (advisory): ${guard.path}${guard.reason ? ` (${guard.reason})` : ""}`);
    } else if (guard.verdict !== "pass") {
      warnings.push(`profiler guard evidence is inconclusive (advisory): ${guard.path}${guard.reason ? ` (${guard.reason})` : ""}`);
    }
  }

  if (redArtifacts.length > 0 && !knownRedGateAccepted) {
    problems.push(`changed fail/stale review artifact(s) need refresh, archive, or final-note callout: ${redArtifacts.map((item) => item.path).join(", ")}`);
  }
  if (redArtifacts.length > 0 && knownRedGateAccepted) {
    warnings.push(`known red review artifact(s) accepted for this handoff: ${redArtifacts.map((item) => item.path).join(", ")}`);
  }

  if (options.promisePush && !push.ok_for_promised_push) {
    problems.push(`cannot promise push: ${push.status}${push.details ? ` (${push.details})` : ""}`);
  } else if (!push.ok_for_promised_push) {
    warnings.push(`push target not ready for a push promise: ${push.status}${push.details ? ` (${push.details})` : ""}`);
  }

  const verdict = problems.length > 0 ? "fail" : warnings.length > 0 ? "warn" : "pass";
  return {
    schema: "game.prototype_slice_hygiene",
    verdict,
    strict: Boolean(options.strict),
    snapshot: Boolean(options.snapshot),
    threshold,
    changed_file_count: changedFiles.length,
    changed_files: changedFiles,
    evidence: {
      build: options.buildEvidence,
      probe: options.probeEvidence,
      product_gates: productGates,
      screenshots: options.screenshots.map((path) => ({ path, exists: fileExists(root, path) })),
      profile_guards: profileGuards,
      known_red_gates: options.knownRedGates,
    },
    red_review_artifacts: redArtifacts,
    push_policy: push,
    problems,
    warnings,
  };
}

function renderMarkdown(report) {
  const lines = [];
  lines.push("# Prototype Slice Hygiene");
  lines.push("");
  lines.push(`Verdict: **${report.verdict.toUpperCase()}**`);
  lines.push(`Changed files: ${report.changed_file_count} / threshold ${report.threshold}${report.snapshot ? " (snapshot)" : ""}`);
  lines.push(`Push policy: ${report.push_policy.status}`);
  if (report.push_policy.branch) lines.push(`Branch: ${report.push_policy.branch}`);
  if (report.push_policy.upstream) lines.push(`Upstream: ${report.push_policy.upstream}`);
  lines.push("");
  lines.push("## Checklist");
  lines.push(`- build evidence: ${report.evidence.build.length > 0 ? "yes" : "missing"}`);
  lines.push(`- probe/scenario evidence: ${report.evidence.probe.length > 0 ? "yes" : "missing"}`);
  lines.push(`- product gate: ${report.evidence.product_gates.length > 0 ? report.evidence.product_gates.map((gate) => `${gate.path} (${gate.verdict})`).join(", ") : "missing"}`);
  lines.push(`- screenshot evidence: ${report.evidence.screenshots.length > 0 ? report.evidence.screenshots.map((shot) => `${shot.path}${shot.exists ? "" : " (missing)"}`).join(", ") : "missing"}`);
  lines.push(`- profiler guard: ${report.evidence.profile_guards.length > 0 ? report.evidence.profile_guards.map((guard) => `${guard.path} (${guard.verdict})`).join(", ") : "missing"}`);
  lines.push(`- known red gates: ${report.evidence.known_red_gates.length > 0 ? report.evidence.known_red_gates.join(" | ") : "none"}`);
  lines.push("");
  if (report.problems.length > 0) {
    lines.push("## Problems");
    for (const problem of report.problems) lines.push(`- ${problem}`);
    lines.push("");
  }
  if (report.warnings.length > 0) {
    lines.push("## Warnings");
    for (const warning of report.warnings) lines.push(`- ${warning}`);
    lines.push("");
  }
  lines.push("## Changed Files");
  if (report.changed_files.length === 0) {
    lines.push("- none");
  } else {
    for (const file of report.changed_files.slice(0, 80)) lines.push(`- ${file}`);
    if (report.changed_files.length > 80) lines.push(`- ... ${report.changed_files.length - 80} more`);
  }
  return `${lines.join("\n")}\n`;
}

const options = parseArgs(process.argv.slice(2));
const report = buildReport(options);
const markdown = renderMarkdown(report);

if (options.jsonOutput) {
  const target = resolve(options.jsonOutput);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(report, null, 2)}\n`, "utf8");
}
if (options.report) {
  const target = resolve(options.report);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, markdown, "utf8");
}
process.stdout.write(markdown);
if (options.strict && report.verdict === "fail") process.exit(1);
