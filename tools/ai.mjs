#!/usr/bin/env node
// Fast facade for AI workflow telemetry. Detailed tools stay in tools/ai_profile/.

import { spawnSync } from "node:child_process";
import { readFileSync, statSync } from "node:fs";
import { relative, resolve } from "node:path";
import {
  appendRecord,
  buildRecord,
  readProfileScope,
  stringArg,
} from "./ai_profile/profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai.mjs start <work-item> [iteration]
  node tools/ai.mjs focus <iteration>
  node tools/ai.mjs summary [context options]
  node tools/ai.mjs context [--path <file> ...] [context options]
  node tools/ai.mjs context [context options] -- <command> [args...]
  node tools/ai.mjs checkpoint <intent> [--force] [--min-gap-min <n>] [checkpoint options]
  node tools/ai.mjs run [--profile-mode passive|full|off] [--profile-slow-ms <n>] [--phase <name>] [--category <name>] [--intent <text>] [--value <name>] -- <command> [args...]
  node tools/ai.mjs validate [--full] [--dry-run]
  node tools/ai.mjs critic --project <game-id> --task <task-id> --screenshot <path> --target <path|text> --output <packet.md> [critic options]
  node tools/ai.mjs gate --project <game-id> --screenshot <path> --verdict pass|fail [gate options]
  node tools/ai.mjs close-slice --task <task-id> --project <game-id> --evidence <text> [close options]
  node tools/ai.mjs status [--verbose] [--require-review-usable|--require-current-scope-usable]
  node tools/ai.mjs reflect [--deep] [--strict] [--no-gap-checkpoint]

Fast path:
  start    set current work item and append one profiling checkpoint
  focus    start a new iteration inside the current work item
  summary  print the low-context taskboard summary
  context  print or measure context; passive mode records only large/failing context
  checkpoint record a long manual/research/review gap without noisy short pauses
  run      run a command; passive mode records only slow/failing commands
  validate run quick reusable-pipeline validation (--full for the heavy export/runtime/release gate)
  critic   write a reusable visual/UI critic packet before strict product gate
  gate     write a product-read screenshot gate before expanding game content
  close-slice require product gate + evidence before handoff/review
  status   show passive telemetry health; guard flags fail unsafe handoffs
  reflect  write a short closeout by default; --gap-checkpoint records a long
           unprofiled work gap first; --deep prepares the full handoff

Use tools/ai_profile/* directly only when debugging the profiler itself.`);
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

function runOrExit(args) {
  const status = runNode(args);
  if (status !== 0) process.exit(status);
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

function profileMode(args) {
  return flagValue(args, "--profile-mode") || process.env.AI_PROFILE_MODE || "passive";
}

function numberFlag(args, flag, fallback) {
  const raw = flagValue(args, flag);
  if (!raw) return fallback;
  const parsed = Number(raw);
  return Number.isFinite(parsed) && parsed >= 0 ? parsed : fallback;
}

function stripFacadeFlags(args) {
  return stripFlagsWithValues(args, new Set(["--profile-mode", "--profile-slow-ms", "--profile-context-chars"]));
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

function appendProfile(values, extra = {}) {
  const profilePath = stringArg(values, "profile", "");
  return appendRecord(profilePath, buildRecord(values, extra));
}

function runCaptured(commandArgs) {
  const started = process.hrtime.bigint();
  const result = spawnSync(commandArgs[0], commandArgs.slice(1), {
    cwd: process.cwd(),
    env: process.env,
    encoding: "utf8",
    shell: false,
    stdio: ["ignore", "pipe", "pipe"],
  });
  const ended = process.hrtime.bigint();
  if (result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  return {
    result,
    durationMs: Number((ended - started) / 1000000n),
    exitCode: typeof result.status === "number" ? result.status : 1,
    outputChars: (result.stdout || "").length + (result.stderr || "").length,
  };
}

function parseOptionMap(args) {
  const values = {};
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (!arg.startsWith("--")) continue;
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

function measuredChars(path) {
  const absolute = resolve(path);
  try {
    return readFileSync(absolute, "utf8").length;
  } catch {
    return statSync(absolute).size;
  }
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

if (command === "summary") {
  if (argv.includes("--")) usage();
  const mode = profileMode(argv);
  const options = stripFacadeFlags(argv);
  const commandArgs = [process.execPath, "tools/taskboard/cli.mjs", "summary"];
  if (mode === "full") {
    const args = ["tools/ai_profile/context_command.mjs", ...options];
    if (!hasFlag(args, "--intent")) args.push("--intent", "Load taskboard summary");
    if (!hasFlag(args, "--reason")) args.push("--reason", "low-context orientation");
    args.push("--", ...commandArgs);
    run(args);
  }
  const captured = runCaptured(commandArgs);
  if (mode !== "off" && (captured.exitCode !== 0 || captured.outputChars >= numberFlag(argv, "--profile-context-chars", 10000))) {
    appendProfile({
      ...parseOptionMap(options),
      phase: "context",
      category: "context",
      intent: "Load taskboard summary",
      result: captured.exitCode === 0 ? "pass" : "fail",
      value: "necessary_overhead",
      "duration-ms": String(captured.durationMs),
      "context-risk": captured.outputChars >= 50000 ? "high" : captured.outputChars >= 10000 ? "medium" : "low",
      tool: "ai_profile/passive_context",
      command: shellText(commandArgs),
    }, {
      command_exit_code: captured.exitCode,
      context_inputs: [{ path: `command:${shellText(commandArgs)}`, chars: captured.outputChars, reason: "low-context orientation" }],
    });
  }
  process.exit(captured.exitCode);
}

if (command === "context") {
  const mode = profileMode(argv);
  const options = stripFacadeFlags(argv);
  if (argv.includes("--")) {
    if (mode === "full") run(["tools/ai_profile/context_command.mjs", ...options]);
    const { options: contextOptions, command: commandArgs } = splitRunArgs(options);
    const captured = runCaptured(commandArgs);
    if (mode !== "off" && (captured.exitCode !== 0 || captured.outputChars >= numberFlag(argv, "--profile-context-chars", 10000))) {
      const optionMap = parseOptionMap(contextOptions);
      appendProfile({
        ...optionMap,
        phase: optionMap.phase || "context",
        category: optionMap.category || "context",
        intent: optionMap.intent || `Read context from ${shellText(commandArgs)}`,
        result: captured.exitCode === 0 ? "pass" : "fail",
        value: optionMap.value || "necessary_overhead",
        "duration-ms": String(captured.durationMs),
        "context-risk": optionMap["context-risk"] || (captured.outputChars >= 50000 ? "high" : captured.outputChars >= 10000 ? "medium" : "low"),
        tool: optionMap.tool || "ai_profile/passive_context",
        command: optionMap.command || shellText(commandArgs),
      }, {
        command_exit_code: captured.exitCode,
        context_inputs: [{ path: `command:${shellText(commandArgs)}`, chars: captured.outputChars, reason: optionMap.reason || "command context" }],
      });
      console.error("passive profile: recorded large/failing context command");
    }
    process.exit(captured.exitCode);
  }
  if (hasFlag(argv, "--path")) {
    if (mode === "full") {
      const args = ["tools/ai_profile/context.mjs", ...options];
      if (!hasFlag(args, "--phase")) args.push("--phase", "context");
      if (!hasFlag(args, "--intent")) args.push("--intent", "Measure context files");
      run(args);
    }
    const optionMap = parseOptionMap(options);
    const paths = [];
    for (let index = 0; index < options.length; index += 1) {
      if (options[index] === "--path" && options[index + 1]) {
        paths.push(options[index + 1]);
        index += 1;
      }
    }
    const contextInputs = paths.map((path) => {
      const chars = measuredChars(path);
      return { path: relative(process.cwd(), resolve(path)), chars, reason: optionMap.reason || "context input" };
    });
    const totalChars = contextInputs.reduce((sum, input) => sum + input.chars, 0);
    const threshold = numberFlag(argv, "--profile-context-chars", 10000);
    if (mode !== "off" && totalChars >= threshold) {
      appendProfile({
        ...optionMap,
        phase: optionMap.phase || "context",
        category: optionMap.category || "context",
        intent: optionMap.intent || "Measure context files",
        result: optionMap.result || "pass",
        value: optionMap.value || "necessary_overhead",
        "context-risk": optionMap["context-risk"] || (totalChars >= 50000 ? "high" : "medium"),
        tool: optionMap.tool || "ai_profile/passive_context",
      }, {
        files_read: paths,
        context_inputs: contextInputs,
      });
      console.log(`passive profile: recorded large context batch (${totalChars} chars)`);
    } else {
      console.log(`passive profile: skipped small context batch (${totalChars} chars)`);
    }
    process.exit(0);
  }
  if (options.length > 0) usage();
  if (mode === "full") {
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
  const captured = runCaptured([process.execPath, "tools/game_context/iteration_context.mjs"]);
  if (mode !== "off" && (captured.exitCode !== 0 || captured.outputChars >= numberFlag(argv, "--profile-context-chars", 10000))) {
    appendProfile({
      phase: "context",
      category: "context",
      intent: "Load game iteration context",
      result: captured.exitCode === 0 ? "pass" : "fail",
      value: "necessary_overhead",
      "duration-ms": String(captured.durationMs),
      "context-risk": captured.outputChars >= 50000 ? "high" : captured.outputChars >= 10000 ? "medium" : "low",
      tool: "ai_profile/passive_context",
      command: `${process.execPath} tools/game_context/iteration_context.mjs`,
    }, {
      command_exit_code: captured.exitCode,
      context_inputs: [{ path: "command:node tools/game_context/iteration_context.mjs", chars: captured.outputChars, reason: "pre-code context pack" }],
    });
  }
  process.exit(captured.exitCode);
}

if (command === "checkpoint") {
  const force = hasFlag(argv, "--force");
  const args = withoutFlag(argv, "--force");
  const [intent, ...options] = args;
  if (!intent || intent.startsWith("--")) usage();
  const target = force ? "tools/ai_profile/checkpoint.mjs" : "tools/ai_profile/gap_checkpoint.mjs";
  const profileArgs = [target, "--intent", intent, ...options];
  if (!hasFlag(profileArgs, "--value")) profileArgs.push("--value", "necessary_overhead");
  if (!force && !hasFlag(profileArgs, "--min-gap-min")) profileArgs.push("--min-gap-min", "10");
  run(profileArgs);
}

if (command === "run") {
  const { options, command: commandArgs } = splitRunArgs(argv);
  const mode = profileMode(options);
  const facadeOptions = stripFacadeFlags(options);
  if (mode === "full") {
    const args = ["tools/ai_profile/run.mjs", ...facadeOptions];
    if (!hasFlag(facadeOptions, "--phase")) args.push("--phase", "work");
    if (!hasFlag(facadeOptions, "--category")) args.push("--category", "validation");
    if (!hasFlag(facadeOptions, "--intent")) args.push("--intent", `Run ${shellText(commandArgs)}`);
    if (!hasFlag(facadeOptions, "--value")) args.push("--value", "productive");
    args.push("--", ...commandArgs);
    run(args);
  }
  const started = process.hrtime.bigint();
  const result = spawnSync(commandArgs[0], commandArgs.slice(1), {
    cwd: process.cwd(),
    env: process.env,
    shell: false,
    stdio: "inherit",
  });
  const ended = process.hrtime.bigint();
  const durationMs = Number((ended - started) / 1000000n);
  const exitCode = typeof result.status === "number" ? result.status : 1;
  const slowMs = numberFlag(options, "--profile-slow-ms", 30000);
  const shouldRecord = mode !== "off" && (exitCode !== 0 || durationMs >= slowMs);
  if (shouldRecord) {
    const optionMap = parseOptionMap(facadeOptions);
    appendProfile({
      ...optionMap,
      phase: optionMap.phase || "work",
      category: optionMap.category || "validation",
      intent: optionMap.intent || `Run ${shellText(commandArgs)}`,
      result: exitCode === 0 ? "pass" : "fail",
      value: optionMap.value || "productive",
      "duration-ms": String(durationMs),
      tool: optionMap.tool || "shell_command",
      command: optionMap.command || shellText(commandArgs),
    }, {
      command_exit_code: exitCode,
      passive_reason: exitCode !== 0 ? "failed_command" : "slow_command",
      command_error: result.error ? result.error.message : undefined,
    });
    console.error(`passive profile: recorded ${exitCode !== 0 ? "failed" : "slow"} command (${durationMs}ms)`);
  }
  process.exit(exitCode);
}

if (command === "validate") {
  // Thin alias to the reusable pipeline validator. Quick by default; pass
  // --full for the heavy export/runtime/release gate. --dry-run prints the plan.
  const pipelineArgs = ["tools/pipeline_validate.mjs"];
  if (hasFlag(argv, "--full")) pipelineArgs.push("--full");
  if (hasFlag(argv, "--dry-run")) pipelineArgs.push("--dry-run");
  run(pipelineArgs);
}

if (command === "gate") {
  run(["tools/product_gate/review.mjs", ...argv]);
}

if (command === "critic") {
  run(["tools/product_gate/visual_critique_packet.mjs", ...argv]);
}

if (command === "close-slice") {
  run(["tools/product_gate/close_slice.mjs", ...argv]);
}

if (command === "status") {
  run(["tools/ai_profile/status.mjs", ...argv]);
}

if (command === "reflect") {
  // Passive by default: the gap checkpoint is opt-in via --gap-checkpoint so a
  // normal closeout adds no forced ceremony. --no-gap-checkpoint is still
  // accepted (now a no-op) for backward compatibility.
  const wantGapCheckpoint = argv.includes("--gap-checkpoint");
  const deep = argv.includes("--deep");
  const reflectArgs = withoutFlag(
    withoutFlag(withoutFlag(argv, "--gap-checkpoint"), "--no-gap-checkpoint"),
    "--deep",
  );
  if (wantGapCheckpoint) {
    const gapArgs = [
      "tools/ai_profile/gap_checkpoint.mjs",
      "--intent",
      "Capture pre-reflection unprofiled work gap",
      ...reflectArgs,
    ];
    if (!hasFlag(gapArgs, "--min-gap-min")) gapArgs.push("--min-gap-min", "10");
    runOrExit(gapArgs);
  }
  if (!deep) {
    run(["tools/ai_profile/closeout.mjs", "--no-review", "--no-followups", ...reflectArgs.filter((arg) => arg !== "--quick")]);
  }
  const strict = reflectArgs.includes("--strict");
  const args = reflectArgs.filter((arg) => arg !== "--strict");
  if (!strict && !hasFlag(args, "--allow-regression")) args.push("--allow-regression");
  run(["tools/ai_profile/prepare_reflection.mjs", ...args]);
}

usage();
