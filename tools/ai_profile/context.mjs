#!/usr/bin/env node
import { readFileSync, statSync } from "node:fs";
import { relative, resolve } from "node:path";
import { appendRecord, buildRecord, listArg, parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/context.mjs --phase <phase> --intent <text> --path <file> [--path <file> ...] [options]

options:
  --category <category>                      default: context
  --result <pass|fail|mixed|blocked|skipped|unknown> default: pass
  --value <productive|necessary_overhead|rework|waste|unknown> default: necessary_overhead
  --reason <text>                            reason for every path
  --context-risk <low|medium|high|unknown>   default: auto from total chars
  --profile <path>                           default: tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
  --work-item <id>                           task/issue/phase id for segmenting long profiles
  --iteration <name>                         small iteration or batch label
  --notes <text>

Records file-read and context_inputs entries with measured character counts.
It does not print file contents.

Environment defaults:
  AI_PROFILE_WORK_ITEM       fallback for --work-item
  AI_PROFILE_ITERATION       fallback for --iteration
  tools/ai_profile/scope.mjs fallback after env vars`);
  process.exit(2);
}

function measuredChars(path) {
  const absolute = resolve(path);
  try {
    return readFileSync(absolute, "utf8").length;
  } catch {
    return statSync(absolute).size;
  }
}

function autoRisk(totalChars) {
  if (totalChars >= 50000) return "high";
  if (totalChars >= 10000) return "medium";
  return "low";
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const paths = listArg(values, "path");
if (paths.length === 0) usage();

if (!values.category) values.category = "context";
if (!values.result) values.result = "pass";
if (!values.value) values.value = "necessary_overhead";

const reason = stringArg(values, "reason", "context input");
const contextInputs = [];
const filesRead = [];
let totalChars = 0;

try {
  for (const path of paths) {
    const chars = measuredChars(path);
    totalChars += chars;
    filesRead.push(path);
    contextInputs.push({ path: relative(process.cwd(), resolve(path)), chars, reason });
  }
  if (!values["context-risk"]) values["context-risk"] = autoRisk(totalChars);
  const record = buildRecord(values, {
    files_read: filesRead,
    context_inputs: contextInputs,
  });
  const profilePath = stringArg(values, "profile", "");
  const target = appendRecord(profilePath, record);
  console.log(`profile context appended: ${target}`);
  for (const input of contextInputs) {
    console.log(`- ${input.path}: ${input.chars} chars (${input.reason})`);
  }
} catch (error) {
  console.error(`profile context failed: ${error.message}`);
  process.exit(1);
}
