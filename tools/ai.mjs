#!/usr/bin/env node
// Thin facade: reusable-pipeline validation, product gates, and passive-profile
// review.
//
// Profiling is FULLY PASSIVE. The PostToolUse hook (native
// tools/ai_profile/hook_record_fast.c hot path with the hook_record.mjs
// fallback, wired in .claude/settings.json and .codex/hooks.json) records every
// tool call to tmp/session_profiles/sessions/<date>__<harness>__<id>.jsonl
// automatically, so there is NO start/focus/checkpoint/run/context/reflect step
// for the agent to perform. Read the captured session with `status`.

import { spawnSync } from "node:child_process";
import { VALIDATE_BOOLEAN_FLAGS, VALIDATE_VALUE_FLAGS } from "./lib/validate_flags.mjs";

function usage({ exitCode = 2, stream = process.stderr } = {}) {
  stream.write(`usage:
  node tools/ai.mjs validate [--quick|--full] [--review] [--dry-run] [--reexport-tests] [--keep-exports <n>] [--no-prune] [--with-assets]
  node tools/ai.mjs status [--verbose] [--all] [--agents] [--since <Nm|Nh|Nd|ISO>] [--harness claude|codex] [--session <id>] [--profile <p>] [--json-output <p>] [--no-import-codex-session]
  node tools/ai.mjs import-codex-session [--profile <profile.jsonl>] [--session <codex-session.jsonl>]
  node tools/ai.mjs orchestration-template
  node tools/ai.mjs subagent-packet-template [--preset codebase-map|review|asset-research|texture-gen|asset-intake [--targets a,b,c]]
  node tools/ai.mjs subagent-packet-check --file packet.txt|--text "..."|--stdin [--json]
  node tools/ai.mjs orchestration-bootstrap --title "..." --objective "..." --allowed-files "..." --expected-output "..." --evidence-command "..." --stop-condition "..." --independent-reviewer "..." [--json]
  node tools/ai.mjs orchestration-check <task-id>|--id <task-id>|--file <task.md>|--current [--json]
  node tools/ai.mjs gate --project <game-id> --screenshot <path> --verdict pass|fail|review [gate options]
  node tools/ai.mjs visual-reject --project <game-id> --task <task-id> --screenshot <path> --problem <text> --next <text> [visual rejection options]
  node tools/ai.mjs critic --project <game-id> --task <task-id> --screenshot <path> --target <path|text> --output <packet.md> [critic options]
  node tools/ai.mjs critique --project <game-id> --shot <tag:path> [--model-cmd <cmd>] [critic-run options]
  node tools/ai.mjs close-slice --task <task-id> --project <game-id> --evidence <text> [--resolved-rejection <text>] [close options]

Commands:
  validate  run reusable-pipeline validation; unknown options fail instead of being silently ignored
  status    read the passive per-session profile (the hook auto-records every tool call);
            --verbose for the full per-record breakdown; --json-output writes the status JSON;
            --agents adds an advisory per-subagent rollup (objective + tool breakdown + duration + status) from
            native harness transcripts; --since <Nm|Nh|Nd|ISO> filters it to recent agents
  import-codex-session  recover missed failed Codex shell calls into the session log
  orchestration-template print the orchestration packet template for subagent tasks
  subagent-packet-template print a reusable delegation packet; --preset <name> emits ready packets (parallel fan-out for codebase-map/review/texture-gen via --targets, single for asset-research, staged for asset-intake); harness-neutral (Claude Agent/Workflow tool or Codex spawn_agent)
  subagent-packet-check validate a subagent prompt packet before launching a delegated run
  orchestration-bootstrap create a current task with a complete orchestration packet
  orchestration-check   preflight-check an orchestration packet before launching subagents
  gate      write a product-read screenshot gate before expanding game content
  visual-reject record a lead visual rejection as a strict visual FAIL gate
  critic    write a reusable visual/UI critic packet before a strict product gate
  critique  run the vision art-lead critic (critic + refute) over state screenshots into a game.visual_critique JSON
  close-slice require product gate + evidence before handoff/review

Profiling has no manual step. Use tools/ai_profile/* directly only when
debugging the profiler itself.\n`);
  process.exit(exitCode);
}

function runNode(args) {
  const result = spawnSync(process.execPath, args, {
    cwd: process.cwd(),
    env: process.env,
    stdio: "inherit",
    shell: false,
  });
  if (result.error) {
    console.error(result.error.message);
    process.exit(1);
  }
  return result.status ?? 1;
}

function run(args) {
  process.exit(runNode(args));
}

function hasFlag(args, flag) {
  return args.includes(flag);
}

function withoutFlag(args, flag) {
  return args.filter((arg) => arg !== flag);
}

function flagValue(args, flag) {
  const index = args.indexOf(flag);
  if (index === -1) return "";
  const value = args[index + 1];
  return value && !value.startsWith("--") ? value : "";
}

function statusArgs(args) {
  return withoutFlag(args, "--no-import-codex-session");
}

function maybeImportCodexSession(args) {
  if (hasFlag(args, "--no-import-codex-session")) return;
  const importArgs = ["tools/ai_profile/import_codex_session.mjs"];
  const profile = flagValue(args, "--profile");
  if (profile) importArgs.push("--profile", profile);
  const session = flagValue(args, "--session");
  if (session) importArgs.push("--session", session);
  const status = runNode(importArgs);
  if (status !== 0) process.exit(status);
}

function pipelineValidateArgs(args) {
  const allowedFlags = new Set(VALIDATE_BOOLEAN_FLAGS);
  const valueFlags = new Set(VALIDATE_VALUE_FLAGS);
  const out = ["tools/pipeline_validate.mjs"];
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (allowedFlags.has(arg)) {
      out.push(arg);
      continue;
    }
    if (valueFlags.has(arg)) {
      const value = args[index + 1];
      if (!value || value.startsWith("--")) {
        console.error(`error: ${arg} requires a value`);
        usage();
      }
      out.push(arg, value);
      index += 1;
      continue;
    }
    if (arg === "--file") {
      console.error("error: node tools/ai.mjs validate no longer supports --file; run the relevant focused test or use node tools/ai.mjs validate");
      process.exit(2);
    }
    console.error(`error: unsupported validate option: ${arg}`);
    usage();
  }
  return out;
}

const [command, ...argv] = process.argv.slice(2);
const HELP_FLAGS = new Set(["--help", "-h", "help"]);
const HELPABLE_COMMANDS = new Set([
  "validate",
  "status",
  "import-codex-session",
  "orchestration-template",
  "subagent-packet-template",
  "subagent-packet-check",
  "orchestration-bootstrap",
  "orchestration-check",
  "gate",
  "visual-reject",
  "critic",
  "critique",
  "close-slice",
]);

if (HELP_FLAGS.has(command)) usage({ exitCode: 0, stream: process.stdout });
if (!command) usage();
if (HELPABLE_COMMANDS.has(command) && argv.some((arg) => HELP_FLAGS.has(arg))) {
  usage({ exitCode: 0, stream: process.stdout });
}

if (command === "validate") run(pipelineValidateArgs(argv));

if (command === "status") {
  maybeImportCodexSession(argv);
  run(["tools/ai_profile/status.mjs", ...statusArgs(argv)]);
}

if (command === "import-codex-session") run(["tools/ai_profile/import_codex_session.mjs", ...argv]);

if (command === "orchestration-template") run(["tools/taskboard/cli.mjs", "orchestration-template", ...argv]);

if (command === "subagent-packet-template") run(["tools/taskboard/cli.mjs", "subagent-packet-template", ...argv]);

if (command === "subagent-packet-check") run(["tools/taskboard/cli.mjs", "subagent-packet-check", ...argv]);

if (command === "orchestration-bootstrap") run(["tools/taskboard/cli.mjs", "orchestration-bootstrap", ...argv]);

if (command === "orchestration-check") run(["tools/taskboard/cli.mjs", "orchestration-check", ...argv]);

if (command === "gate") run(["tools/product_gate/review.mjs", ...argv]);

if (command === "visual-reject") run(["tools/product_gate/visual_rejection_lock.mjs", ...argv]);

if (command === "critic") run(["tools/product_gate/visual_critique_packet.mjs", ...argv]);

if (command === "critique") run(["tools/product_gate/visual_critic_run.mjs", ...argv]);

if (command === "close-slice") run(["tools/product_gate/close_slice.mjs", ...argv]);

usage();
