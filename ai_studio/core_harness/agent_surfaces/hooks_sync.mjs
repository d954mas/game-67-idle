// Generate Codex and Claude hook config from one canonical hook source.
//
//   node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs
//   node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs --check
//
// Codex and Claude use different matcher vocabularies. Claude settings are
// patched by replacing only the hooks key.

import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "../../..");

function isMain(moduleUrl) {
  return process.argv[1] && moduleUrl === pathToFileURL(process.argv[1]).href;
}

// --- Canonical source: the hooks, tool-agnostic. -------------------------
// Each event lists hook entries. `match` is a logical matcher key resolved per
// tool in TOOLS[].matchers; omit it for an unmatched event. `record` selects a
// recorder command template from RECORDERS.
const HOOK_SOURCE = {
  SessionStart: [{ record: "fast" }],
  PreToolUse: [{ match: "shell", record: "fast" }],
  PostToolUse: [{ match: "shell", record: "fast" }],
};

// Recorder command templates. `label` (the agent name) is appended as the last
// arg on Windows. Other hosts degrade profiling hooks to a no-op; CI can still
// build and test the C source explicitly without introducing a second recorder.
const RECORDERS = {
  fast: (label) => ({
    type: "command",
    command: ":",
    commandWindows: `ai_studio\\core_harness\\profiling\\hook_record_fast.exe ${label}`,
  }),
};

// Per-tool rendering config.
const TOOLS = {
  codex: {
    file: ".codex/hooks.json",
    label: "codex",
    matchers: { shell: "(?i)(bash|shell|exec)" },
    // The whole Codex file is exactly { hooks }.
    render: (hooks) => ({ hooks }),
  },
  claude: {
    file: ".claude/settings.json",
    label: "claude",
    matchers: { shell: "Bash" },
    // Preserve every existing Claude-only key (e.g. $comment); replace only hooks.
    render: (hooks, existing) => {
      const out = existing && typeof existing === "object" ? { ...existing } : {};
      out.hooks = hooks;
      return out;
    },
  },
};

function buildHooks(tool) {
  const events = {};
  for (const [event, entries] of Object.entries(HOOK_SOURCE)) {
    events[event] = entries.map((entry) => {
      const recorder = RECORDERS[entry.record];
      if (!recorder) throw new Error(`unknown recorder "${entry.record}"`);
      const hook = recorder(tool.label);
      if (entry.match) {
        const matcher = tool.matchers[entry.match];
        if (!matcher) throw new Error(`tool ${tool.label}: no matcher for "${entry.match}"`);
        return { matcher, hooks: [hook] };
      }
      return { hooks: [hook] };
    });
  }
  return events;
}

// Generate the exact file text a tool's config should contain.
export function generate(tool) {
  const filePath = resolve(root, tool.file);
  let existing = null;
  if (existsSync(filePath)) {
    try {
      existing = JSON.parse(readFileSync(filePath, "utf8"));
    } catch {
      existing = null;
    }
  }
  const obj = tool.render(buildHooks(tool), existing);
  return JSON.stringify(obj, null, 2) + "\n";
}

export { TOOLS, HOOK_SOURCE, buildHooks };

// Sync (or --check) every tool file. Returns the list of drifted file paths.
export function syncAll({ check = false } = {}) {
  const drift = [];
  for (const tool of Object.values(TOOLS)) {
    const filePath = resolve(root, tool.file);
    const next = generate(tool);
    const current = existsSync(filePath) ? readFileSync(filePath, "utf8") : "";
    if (current !== next) {
      drift.push(tool.file);
      if (!check) writeFileSync(filePath, next);
    }
  }
  return drift;
}

function main() {
  const args = process.argv.slice(2);
  if (args.includes("--help") || args.includes("-h")) {
    console.log("usage: node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs [--check]");
    process.exit(0);
  }
  const check = args.includes("--check");
  if (check) args.splice(args.indexOf("--check"), 1);
  if (args.length > 0) {
    console.error("usage: node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs [--check]");
    process.exit(2);
  }
  const drift = syncAll({ check });
  if (check) {
    if (drift.length) {
      console.error(`hooks drift: ${drift.join(", ")} - run: node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs`);
      process.exit(1);
    }
    console.log("hooks in sync (.codex/hooks.json + .claude/settings.json)");
    return;
  }
  console.log(drift.length ? `hooks synced: ${drift.join(", ")}` : "hooks already in sync");
}

// Run the CLI only when invoked directly, not when imported by a test.
if (isMain(import.meta.url)) {
  main();
}
