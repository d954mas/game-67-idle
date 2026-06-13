#!/usr/bin/env node
// Fast facade for AI workflow telemetry. Detailed tools stay in tools/ai_profile/.

import { spawnSync } from "node:child_process";
import { readProfileScope } from "./ai_profile/profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai.mjs start <work-item> [iteration]
  node tools/ai.mjs focus <iteration>
  node tools/ai.mjs context [--path <file> ...] [context options]
  node tools/ai.mjs context [context options] -- <command> [args...]
  node tools/ai.mjs checkpoint <intent> [--force] [--min-gap-min <n>] [checkpoint options]
  node tools/ai.mjs run [--phase <name>] [--category <name>] [--intent <text>] [--value <name>] -- <command> [args...]
  node tools/ai.mjs validate --change <kind> [--risk low|medium|high] [--tier <name>] [--dry-run]
  node tools/ai.mjs status
  node tools/ai.mjs reflect [--quick] [--strict]

Fast path:
  start    set current work item and append one profiling checkpoint
  focus    start a new iteration inside the current work item
  context  profile measured context; with no args, print the game-iteration packet
  checkpoint record a meaningful manual/research/review gap without noisy short pauses
  run      run a command and record duration/result
  validate run a planned validation batch with batch metadata
  status   show telemetry health
  reflect  prepare the full reflection handoff from current telemetry

Use tools/ai_profile/* directly only when debugging the profiler itself.`);
  process.exit(2);
}

function run(args) {
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
  process.exit(result.status ?? 1);
}

function shellText(args) {
  return args.map((arg) => (/\s/.test(arg) ? JSON.stringify(arg) : arg)).join(" ");
}

function splitRunArgs(argv) {
  const sep = argv.indexOf("--");
  if (sep === -1 || sep === argv.length - 1) usage();
  return { options: argv.slice(0, sep), command: argv.slice(sep + 1) };
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

const [command, ...argv] = process.argv.slice(2);

if (!command || command === "help" || command === "--help" || command === "-h") usage();

if (command === "start") {
  const [workItem] = argv;
  if (!workItem) usage();
  const maybeIteration = argv[1] || "";
  const iteration = maybeIteration && !maybeIteration.startsWith("--") ? maybeIteration : "";
  const options = argv.slice(iteration ? 2 : 1);
  const args = ["tools/ai_profile/start.mjs", "--work-item", workItem];
  if (iteration) args.push("--iteration", iteration);
  args.push(...options);
  run(args);
}

if (command === "focus") {
  const [iteration, ...options] = argv;
  if (!iteration || iteration.startsWith("--")) usage();
  const scope = readProfileScope(flagValue(options, "--scope") || undefined);
  if (!scope.valid || !scope.work_item) {
    console.error("ai focus requires an existing work item scope. Run `node tools/ai.mjs start <work-item> <iteration>` first.");
    process.exit(2);
  }
  run(["tools/ai_profile/start.mjs", "--work-item", scope.work_item, "--iteration", iteration, ...options]);
}

if (command === "context") {
  if (argv.includes("--")) {
    run(["tools/ai_profile/context_command.mjs", ...argv]);
  }
  if (hasFlag(argv, "--path")) {
    const args = ["tools/ai_profile/context.mjs", ...argv];
    if (!hasFlag(args, "--phase")) args.push("--phase", "context");
    if (!hasFlag(args, "--intent")) args.push("--intent", "Measure context files");
    run(args);
  }
  if (argv.length > 0) usage();
  run([
    "tools/ai_profile/context_command.mjs",
    "--intent",
    "Load game iteration context",
    "--reason",
    "pre-code context pack",
    "--",
    process.execPath,
    "tools/game_context/iteration_context.mjs",
  ]);
}

if (command === "checkpoint") {
  const force = hasFlag(argv, "--force");
  const args = withoutFlag(argv, "--force");
  const [intent, ...options] = args;
  if (!intent || intent.startsWith("--")) usage();
  const target = force ? "tools/ai_profile/checkpoint.mjs" : "tools/ai_profile/gap_checkpoint.mjs";
  const profileArgs = [target, "--intent", intent, ...options];
  if (!hasFlag(profileArgs, "--value")) profileArgs.push("--value", "necessary_overhead");
  if (!force && !hasFlag(profileArgs, "--min-gap-min")) profileArgs.push("--min-gap-min", "2");
  run(profileArgs);
}

if (command === "run") {
  const { options, command: commandArgs } = splitRunArgs(argv);
  const args = ["tools/ai_profile/run.mjs", ...options];
  if (!hasFlag(options, "--phase")) args.push("--phase", "work");
  if (!hasFlag(options, "--category")) args.push("--category", "validation");
  if (!hasFlag(options, "--intent")) args.push("--intent", `Run ${shellText(commandArgs)}`);
  if (!hasFlag(options, "--value")) args.push("--value", "productive");
  args.push("--", ...commandArgs);
  run(args);
}

if (command === "validate") {
  const args = [...argv];
  if (!hasFlag(args, "--plan") && !hasFlag(args, "--change")) usage();
  if (!hasFlag(args, "--plan") && !hasFlag(args, "--risk")) args.push("--risk", "medium");
  run(["tools/ai_profile/validation_run.mjs", ...args]);
}

if (command === "status") {
  run(["tools/ai_profile/status.mjs", ...argv]);
}

if (command === "reflect") {
  if (argv.includes("--quick")) {
    run(["tools/ai_profile/closeout.mjs", ...argv.filter((arg) => arg !== "--quick")]);
  }
  const strict = argv.includes("--strict");
  const args = argv.filter((arg) => arg !== "--strict");
  if (!strict && !hasFlag(args, "--allow-regression")) args.push("--allow-regression");
  run(["tools/ai_profile/prepare_reflection.mjs", ...args]);
}

usage();
