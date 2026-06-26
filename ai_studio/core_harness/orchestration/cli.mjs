#!/usr/bin/env node
// Public Core Harness orchestration CLI.

import { existsSync, readFileSync } from "node:fs";
import { isAbsolute, relative, resolve } from "node:path";
import {
  createTask,
  findDoc,
  findRoot,
  parseDoc,
} from "../../taskboard/lib.mjs";
import {
  DEFAULT_ORCHESTRATION_TOOL_USE_GUARD,
  isBoundedOrchestrationAllowedFiles,
  orchestrationPacketTemplate,
  orchestrationPreflightProblem,
  renderSubagentPacketPreset,
  subagentPacketPresetNames,
  subagentPacketProblem,
  subagentPacketTemplate,
} from "./lib.mjs";
import {
  currentDoingOrchestrationTaskIds,
  taskboardOrchestrationProblems,
} from "./taskboard_policy.mjs";
import { fail } from "../../../tools/lib/cli.mjs";

const root = findRoot();
const commandPath = "ai_studio/core_harness/orchestration/cli.mjs";
const ORCHESTRATION_BOOTSTRAP_REQUIRED_ARGS = [
  "title",
  "objective",
  "allowed-files",
  "expected-output",
  "evidence-command",
  "stop-condition",
  "independent-reviewer",
];

const [cmd, ...rest] = process.argv.slice(2);
const args = parseArgs(rest);

function usage() {
  return `usage: node ${commandPath} <command> [...]

Commands:
  orchestration-template
  subagent-packet-template [--preset [name] [--targets a,b]]
  subagent-packet-check --file packet.txt|--text "..."|--stdin [--json]
  orchestration-bootstrap --title "..." --objective "..." --allowed-files "..." --expected-output "..." --evidence-command "..." --stop-condition "..." --independent-reviewer "..." [--tool-use-guard "..."] [--tags a,b] [--json]
  orchestration-check <task-id>|--id <task-id>|--file tasks/active/T0001-example.md|--current [--json]
  taskboard-audit [--json]

Task state commands stay in:
  node ai_studio/taskboard/cli.mjs
`;
}

function parseArgs(values) {
  const out = { _: [] };
  for (let i = 0; i < values.length; i++) {
    if (values[i].startsWith("--")) {
      const key = values[i].slice(2);
      const next = values[i + 1];
      if (next !== undefined && !next.startsWith("--")) {
        out[key] = next;
        i++;
      } else {
        out[key] = true;
      }
    } else {
      out._.push(values[i]);
    }
  }
  return out;
}

function writeJson(value) {
  process.stdout.write(`${JSON.stringify(value, null, 2)}\n`);
}

function argText(values, key) {
  const value = values[key];
  return typeof value === "string" ? value.trim() : "";
}

function splitTags(value) {
  return value ? String(value).split(",").map((s) => s.trim()).filter(Boolean) : [];
}

function uniqueTags(values) {
  return [...new Set(values.filter(Boolean))];
}

function currentSelectorProblem(code, message, ids = [], nextAction = currentPreflightNextAction()) {
  return {
    code,
    selector: "current",
    taskIds: ids,
    message,
    nextAction,
  };
}

function currentPreflightNextAction() {
  return `create or refine exactly one \`doing\` pipeline/orchestration task, then run \`node ${commandPath} orchestration-check --current --json\``;
}

function ambiguousCurrentPreflightNextAction() {
  return `set exactly one pipeline/orchestration task to \`doing\`, then run \`node ${commandPath} orchestration-check --current --json\``;
}

function isSelectorProblem(error) {
  return error && typeof error === "object" && error.selector === "current" && typeof error.message === "string";
}

function missingBootstrapArgs(values) {
  return ORCHESTRATION_BOOTSTRAP_REQUIRED_ARGS
    .filter((key) => !argText(values, key))
    .map((key) => `--${key}`);
}

function orchestrationBootstrapUsage() {
  return `usage: node ${commandPath} orchestration-bootstrap --title "..." --objective "..." --allowed-files "..." --expected-output "..." --evidence-command "..." --stop-condition "..." --independent-reviewer "..." [--tool-use-guard "..."] [--tags a,b] [--json]

Creates one current \`doing\` pipeline/orchestration task with a complete packet.

Required:
  --title                 Short task title.
  --objective             Bounded work objective.
  --allowed-files         Repo-local files or bounded patterns, separated by comma or semicolon.
  --expected-output       Concrete output the task must produce.
  --evidence-command      Read-only command or artifact path proving the work.
  --stop-condition        Validation and closeout condition.
  --independent-reviewer  Reviewer/verifier plan.

After creation:
  node ${commandPath} orchestration-check --current --json`;
}

function orchestrationBootstrapBody(values) {
  return `## What

${argText(values, "objective")}

## Done when

- [ ] ${argText(values, "stop-condition")}

## Open questions

## Log

- orchestration: used
  objective: ${argText(values, "objective")}
  allowed files: ${argText(values, "allowed-files")}
  tool-use guard: ${argText(values, "tool-use-guard") || DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
  expected output: ${argText(values, "expected-output")}
  evidence command: ${argText(values, "evidence-command")}
  stop condition: ${argText(values, "stop-condition")}
  independent reviewer: ${argText(values, "independent-reviewer")}
`;
}

function readTaskFileArg(value) {
  const requested = value || fail("usage: orchestration-check <task-id>|--id <task-id>|--file <task.md>|--current");
  const file = isAbsolute(requested) ? resolve(requested) : resolve(root, requested);
  const rel = relative(root, file);
  if (rel.startsWith("..") || isAbsolute(rel)) {
    fail("--file must be inside the repository root");
  }
  if (!existsSync(file)) {
    fail(`no such file: ${requested}`);
  }
  const parsed = parseDoc(readFileSync(file, "utf8"));
  return {
    ...parsed,
    kind: "task",
    file,
    fields: {
      ...parsed.fields,
      display_id: parsed.fields.id || rel,
    },
  };
}

function readTaskForOrchestrationCheck(values) {
  const id = values.id || values._[0];
  const selectors = [Boolean(values.file), Boolean(id), Boolean(values.current)].filter(Boolean).length;
  if (selectors > 1) {
    fail("use only one selector: <task-id>, --id, --file, or --current");
  }
  if (values.current) {
    const ids = currentDoingOrchestrationTaskIds(root);
    if (ids.length === 0) {
      throw currentSelectorProblem(
        "current_task_missing",
        "no current doing pipeline/orchestration task; create or set exactly one task to doing first",
      );
    }
    if (ids.length > 1) {
      throw currentSelectorProblem(
        "current_task_ambiguous",
        `multiple current doing pipeline/orchestration tasks: ${ids.join(", ")}; select one explicitly`,
        ids,
        ambiguousCurrentPreflightNextAction(),
      );
    }
    const doc = findDoc(root, ids[0]);
    if (!doc || doc.kind !== "task") {
      throw currentSelectorProblem("current_task_unresolved", `current task ${ids[0]} could not be resolved`, ids);
    }
    return doc;
  }
  if (values.file) return readTaskFileArg(values.file);
  if (!id) {
    fail("usage: orchestration-check <task-id>|--id <task-id>|--file <task.md>|--current");
  }
  const doc = findDoc(root, id);
  if (!doc || doc.kind !== "task") {
    fail(`no task with id ${id}`);
  }
  return doc;
}

function readSubagentPacketArg(values) {
  const inputCount = [values.file, values.text, values.stdin].filter(Boolean).length;
  if (inputCount > 1) fail("use only one subagent-packet-check input: --file, --text, or --stdin");
  if (typeof values.text === "string") return values.text;
  if (values.stdin) return readFileSync(0, "utf8");
  if (values.file) {
    const requested = values.file;
    const file = isAbsolute(requested) ? resolve(requested) : resolve(root, requested);
    const rel = relative(root, file);
    if (rel.startsWith("..") || isAbsolute(rel)) {
      fail("--file must be inside the repository root");
    }
    if (!existsSync(file)) {
      fail(`no such file: ${requested}`);
    }
    return readFileSync(file, "utf8");
  }
  fail("usage: subagent-packet-check --file packet.txt|--text \"...\"|--stdin [--json]");
}

if (!cmd || cmd === "--help" || cmd === "-h" || cmd === "help") {
  console.log(usage());
  process.exit(0);
}

switch (cmd) {
  case "orchestration-template": {
    console.log(orchestrationPacketTemplate());
    break;
  }
  case "subagent-packet-template": {
    if (args.preset !== undefined) {
      if (args.preset === true) {
        console.log(`presets: ${subagentPacketPresetNames().join(", ")}`);
        break;
      }
      const targets = typeof args.targets === "string"
        ? args.targets.split(",").map((target) => target.trim()).filter(Boolean)
        : [];
      try {
        console.log(renderSubagentPacketPreset(String(args.preset), targets));
      } catch (error) {
        if (error && error.code === "unknown_preset") {
          fail(`unknown preset: ${args.preset}; available: ${error.presets.join(", ")}`);
        }
        throw error;
      }
      break;
    }
    console.log(subagentPacketTemplate());
    break;
  }
  case "subagent-packet-check": {
    const problem = subagentPacketProblem(readSubagentPacketArg(args));
    if (args.json) {
      writeJson({ ok: !problem, problem });
      process.exit(problem ? 1 : 0);
    }
    if (problem) {
      console.log(`problem: ${problem.message}`);
      console.log(`hint: start from \`node ${commandPath} subagent-packet-template\``);
      process.exit(1);
    }
    console.log("ok: subagent packet passed");
    break;
  }
  case "orchestration-bootstrap": {
    if (args.help === true || args.h === true) {
      console.log(orchestrationBootstrapUsage());
      break;
    }
    const missingArgs = missingBootstrapArgs(args);
    if (missingArgs.length) {
      const problem = {
        code: "missing_required_argument",
        missingArgs,
        message: `orchestration-bootstrap missing required argument(s): ${missingArgs.join(", ")}`,
      };
      if (args.json) {
        writeJson({ ok: false, problem });
        process.exit(1);
      }
      fail(problem.message);
    }
    if (!isBoundedOrchestrationAllowedFiles(argText(args, "allowed-files"))) {
      const problem = {
        code: "invalid_allowed_files",
        allowedFiles: argText(args, "allowed-files"),
        message: "--allowed-files must be bounded repo-local file paths or final-segment file patterns, separated by comma or semicolon",
      };
      if (args.json) {
        writeJson({ ok: false, problem });
        process.exit(1);
      }
      fail(problem.message);
    }
    const currentIds = currentDoingOrchestrationTaskIds(root);
    if (currentIds.length) {
      const problem = {
        code: "current_task_exists",
        taskIds: currentIds,
        message: `current doing pipeline/orchestration task already exists: ${currentIds.join(", ")}; finish or move it before bootstrapping another`,
      };
      if (args.json) {
        writeJson({ ok: false, problem });
        process.exit(1);
      }
      fail(problem.message);
    }
    const doc = createTask(root, {
      title: argText(args, "title"),
      status: "doing",
      epic: argText(args, "epic"),
      priority: argText(args, "priority"),
      tags: uniqueTags(["pipeline", "orchestration", "subagents", ...splitTags(args.tags)]),
      body: orchestrationBootstrapBody(args),
    });
    const payload = {
      ok: true,
      doc: {
        id: doc.fields.id,
        status: doc.fields.status,
        tags: doc.fields.tags || [],
        file: relative(root, doc.file),
      },
      nextAction: `node ${commandPath} orchestration-check --current --json`,
    };
    if (args.json) {
      writeJson(payload);
    } else {
      console.log(`created ${doc.fields.id}: ${relative(root, doc.file)}`);
      console.log(`next: ${payload.nextAction}`);
    }
    break;
  }
  case "orchestration-check": {
    let doc;
    try {
      doc = readTaskForOrchestrationCheck(args);
    } catch (error) {
      if (isSelectorProblem(error)) {
        if (args.json) {
          writeJson({ ok: false, file: null, problem: error });
          process.exit(1);
        }
        fail(error.message);
      }
      throw error;
    }
    const problem = orchestrationPreflightProblem(doc);
    if (args.json) {
      writeJson({ ok: !problem, file: relative(root, doc.file), problem });
      process.exit(problem ? 1 : 0);
    }
    if (problem) {
      console.log(`problem: ${problem.message}`);
      console.log(`hint: use a complete packet from \`node ${commandPath} orchestration-template\` before launching subagents:`);
      console.log(orchestrationPacketTemplate());
      process.exit(1);
    }
    console.log(`ok: orchestration packet preflight passed for ${relative(root, doc.file)}`);
    break;
  }
  case "taskboard-audit": {
    const problems = taskboardOrchestrationProblems(root);
    if (args.json) {
      writeJson({ ok: problems.length === 0, problems });
      process.exit(problems.length ? 1 : 0);
    }
    if (!problems.length) {
      console.log("ok: no taskboard orchestration problems found");
      break;
    }
    for (const problem of problems) {
      console.log(`problem: ${problem.message}`);
    }
    process.exit(1);
  }
  default:
    fail(`not an orchestration command: ${cmd}`);
}
