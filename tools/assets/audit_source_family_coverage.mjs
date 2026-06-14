#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

const MIXED_OR_INCOMPLETE = /\b(mixed|candidate|temporary|debug|scaffold|partial|release replacement|not accepted|not final)\b/i;

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(2);
}

function usage() {
  console.error(`usage:
  node tools/assets/audit_source_family_coverage.mjs --job <art-job.json> [--json-output <report.json>] [--report <report.md>]

Checks that final generated UI art is backed by separate accepted source
families such as blank bases, icon sheets, and decor overlay sheets.`);
  process.exit(2);
}

function parseArgs(argv) {
  const out = {};
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") out.help = true;
    else if (arg.startsWith("--")) {
      const key = arg.slice(2);
      const value = argv[i + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      out[key] = value;
      i += 1;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return out;
}

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function readJsonOrNull(path) {
  try {
    return readJson(path);
  } catch {
    return null;
  }
}

function hasText(value) {
  return typeof value === "string" && value.trim().length > 0;
}

function normalizeText(value) {
  return String(value || "").trim().toLowerCase().replace(/\s+/g, " ");
}

function normalizePath(path) {
  return String(path || "").replaceAll("\\", "/").replace(/^\.\//, "");
}

function writeText(path, text) {
  mkdirSync(dirname(resolve(path)), { recursive: true });
  writeFileSync(path, text, "utf8");
}

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function loadRecord(entry, root) {
  if (typeof entry === "string") {
    const path = resolve(root, entry);
    if (!existsSync(path)) {
      return { record: null, path: entry, problems: [`generation record missing: ${entry}`] };
    }
    try {
      return { record: readJson(path), path: entry, problems: [] };
    } catch (error) {
      return { record: null, path: entry, problems: [`cannot read generation record ${entry}: ${error.message}`] };
    }
  }
  if (entry && typeof entry === "object") return { record: entry, path: "", problems: [] };
  return { record: null, path: "", problems: ["generation record must be a path string or object"] };
}

function loadPromptPacket(record, root) {
  if (!hasText(record?.prompt_packet)) return null;
  const path = resolve(root, record.prompt_packet);
  if (!existsSync(path)) return null;
  return readJsonOrNull(path);
}

function inferSourceFamily(record, packet, knownFamilies) {
  if (hasText(record?.source_family)) return record.source_family;
  if (hasText(packet?.source_family)) return packet.source_family;
  const role = normalizeText(record?.source_family_role);
  const matches = knownFamilies.filter((family) => role.includes(normalizeText(family)));
  return matches.length === 1 ? matches[0] : "";
}

function requiredFamilies(job) {
  const explicit =
    job.expected_outputs?.required_source_families ??
    job.generation_contract?.final_source_families_required ??
    job.generation_contract?.required_source_families;
  if (Array.isArray(explicit)) return explicit.filter(hasText);
  return [];
}

function audit(job, jobPath, root) {
  const knownFamilies = asArray(job.generation_contract?.source_families).filter(hasText);
  const required = requiredFamilies(job);
  const problems = [];
  if (required.length === 0) {
    problems.push("job needs expected_outputs.required_source_families or generation_contract.final_source_families_required");
  }
  for (const family of required) {
    if (!knownFamilies.some((known) => normalizeText(known) === normalizeText(family))) {
      problems.push(`required source family is not listed in generation_contract.source_families: ${family}`);
    }
  }

  const loaded = [];
  for (const entry of asArray(job.expected_outputs?.generation_records)) {
    const result = loadRecord(entry, root);
    problems.push(...result.problems);
    if (!result.record) continue;
    const packet = loadPromptPacket(result.record, root);
    const sourceFamily = inferSourceFamily(result.record, packet, knownFamilies);
    const recordProblems = [];
    const roleText = `${result.record.source_family_role || ""} ${result.record.rejected_candidate_notes || ""}`;
    if (!hasText(sourceFamily)) recordProblems.push("record needs canonical source_family or prompt_packet.source_family");
    if (MIXED_OR_INCOMPLETE.test(roleText)) recordProblems.push("record appears mixed, candidate, debug, partial, or not final-source accepted");
    if (!["generated", "artist"].includes(result.record.final_art_source || "generated")) {
      recordProblems.push("record final_art_source must be generated or artist");
    }
    if (!hasText(result.record.accepted_source_image)) {
      recordProblems.push("record needs accepted_source_image");
    } else if (!existsSync(resolve(root, result.record.accepted_source_image))) {
      recordProblems.push(`accepted source image missing: ${result.record.accepted_source_image}`);
    }
    loaded.push({
      id: result.record.id || result.path || "(unknown)",
      path: result.path || undefined,
      source_family: sourceFamily,
      source_family_role: result.record.source_family_role || "",
      accepted_source_image: result.record.accepted_source_image || "",
      status: recordProblems.length === 0 ? "pass" : "fail",
      problems: recordProblems,
    });
  }

  for (const family of required) {
    const candidates = loaded.filter((record) => normalizeText(record.source_family) === normalizeText(family));
    if (candidates.length === 0) {
      problems.push(`missing accepted source family: ${family}`);
    } else if (!candidates.some((record) => record.status === "pass")) {
      problems.push(`source family has no final-accepted record: ${family}`);
    }
  }

  for (const record of loaded) problems.push(...record.problems.map((problem) => `record ${record.id}: ${problem}`));

  return {
    schema: "game.source_family_coverage_audit",
    version: 1,
    art_job: normalizePath(jobPath),
    verdict: problems.length === 0 ? "pass" : "fail",
    required_source_families: required,
    records: loaded,
    problems,
  };
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) usage();
  if (!args.job) usage();
  const jobPath = args.job;
  const root = process.cwd();
  if (!existsSync(resolve(root, jobPath))) fail(`art job not found: ${jobPath}`);
  const job = readJson(resolve(root, jobPath));
  const report = audit(job, jobPath, root);
  if (args["json-output"]) writeText(args["json-output"], `${JSON.stringify(report, null, 2)}\n`);
  const lines = [
    "# Source Family Coverage Audit",
    "",
    `art_job: \`${report.art_job}\``,
    `verdict: **${report.verdict}**`,
    "",
    "## Required Source Families",
    "",
    ...report.required_source_families.map((family) => `- ${family}`),
    "",
    "## Records",
    "",
    ...report.records.map((record) => `- ${record.status.toUpperCase()} \`${record.id}\` -> ${record.source_family || "(unknown)"}${record.problems.length ? `: ${record.problems.join("; ")}` : ""}`),
    "",
  ];
  if (report.problems.length > 0) {
    lines.push("## Problems", "", ...report.problems.map((problem) => `- ${problem}`), "");
  }
  if (args.report) writeText(args.report, `${lines.join("\n")}\n`);
  else console.log(JSON.stringify(report, null, 2));
  if (report.problems.length > 0) process.exit(1);
}

main();
