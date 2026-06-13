#!/usr/bin/env node
import { appendRecord, buildRecord, parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/event.mjs --phase <phase> --category <category> --intent <text> [options]

options:
  --result <pass|fail|mixed|blocked|skipped|unknown>   default: unknown
  --value <productive|necessary_overhead|rework|waste|unknown>
  --profile <path>                                    default: tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
  --work-item <id>                                    task/issue/phase id for segmenting long profiles
  --iteration <name>                                  small iteration or batch label
  --context-risk <low|medium|high|unknown>
  --duration-ms <number>
  --tool <name>                 repeatable
  --command <text>              repeatable
  --file-read <path>            repeatable
  --file-written <path>         repeatable
  --evidence <path>             repeatable
  --context-input <path:chars:reason> repeatable
  --waste-reason <text>
  --blocked-by <text>
  --notes <text>

Environment defaults:
  AI_PROFILE_WORK_ITEM       fallback for --work-item
  AI_PROFILE_ITERATION       fallback for --iteration
  tools/ai_profile/scope.mjs fallback after env vars`);
  process.exit(2);
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

try {
  const profilePath = stringArg(values, "profile", "");
  const target = appendRecord(profilePath, buildRecord(values));
  console.log(`profile event appended: ${target}`);
} catch (error) {
  console.error(`profile event failed: ${error.message}`);
  usage();
}
