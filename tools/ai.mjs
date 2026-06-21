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

function usage() {
  console.error(`usage:
  node tools/ai.mjs validate [--quick|--full] [--review] [--dry-run] [--reexport-tests] [--keep-exports <n>] [--no-prune] [--with-assets]
  node tools/ai.mjs status [--verbose] [--all] [--harness claude|codex] [--session <id>] [--profile <p>] [--no-import-codex-session]
  node tools/ai.mjs import-codex-session [--profile <profile.jsonl>] [--session <codex-session.jsonl>]
  node tools/ai.mjs gate --project <game-id> --screenshot <path> --verdict pass|fail|review [gate options]
  node tools/ai.mjs visual-reject --project <game-id> --task <task-id> --screenshot <path> --problem <text> --next <text> [visual rejection options]
  node tools/ai.mjs critic --project <game-id> --task <task-id> --screenshot <path> --target <path|text> --output <packet.md> [critic options]
  node tools/ai.mjs critique --project <game-id> --shot <tag:path> [--model-cmd <cmd>] [critic-run options]
  node tools/ai.mjs close-slice --task <task-id> --project <game-id> --evidence <text> [--resolved-rejection <text>] [close options]

Commands:
  validate  run reusable-pipeline validation; unknown options fail instead of being silently ignored
  status    read the passive per-session profile (the hook auto-records every tool call);
            --verbose for the full per-record breakdown
  import-codex-session  recover missed failed Codex shell calls into the session log
  gate      write a product-read screenshot gate before expanding game content
  visual-reject record a lead visual rejection as a strict visual FAIL gate
  critic    write a reusable visual/UI critic packet before a strict product gate
  critique  run the vision art-lead critic (critic + refute) over state screenshots into a game.visual_critique JSON
  close-slice require product gate + evidence before handoff/review

Profiling has no manual step. Use tools/ai_profile/* directly only when
debugging the profiler itself.`);
  process.exit(2);
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

function stripFlagsWithValues(args, flags) {
  const out = [];
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (flags.has(arg)) {
      if (args[index + 1] && !args[index + 1].startsWith("--")) index += 1;
      continue;
    }
    out.push(arg);
  }
  return out;
}

function stripImportFlags(args) {
  return stripFlagsWithValues(withoutFlag(args, "--no-import-codex-session"), new Set(["--session"]));
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
  const allowedFlags = new Set(["--quick", "--full", "--review", "--dry-run", "--reexport-tests", "--no-prune", "--with-assets"]);
  const valueFlags = new Set(["--keep-exports"]);
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

if (!command || command === "help" || command === "--help" || command === "-h") usage();

if (command === "validate") run(pipelineValidateArgs(argv));

if (command === "status") {
  maybeImportCodexSession(argv);
  run(["tools/ai_profile/status.mjs", ...stripImportFlags(argv)]);
}

if (command === "import-codex-session") run(["tools/ai_profile/import_codex_session.mjs", ...argv]);

if (command === "gate") run(["tools/product_gate/review.mjs", ...argv]);

if (command === "visual-reject") run(["tools/product_gate/visual_rejection_lock.mjs", ...argv]);

if (command === "critic") run(["tools/product_gate/visual_critique_packet.mjs", ...argv]);

if (command === "critique") run(["tools/product_gate/visual_critic_run.mjs", ...argv]);

if (command === "close-slice") run(["tools/product_gate/close_slice.mjs", ...argv]);

usage();
