// Canonical cross-harness HOOK config -> generates .codex/hooks.json (Codex
// format) and the hooks block of .claude/settings.json (Claude format) from ONE
// source. The single source of truth is HOOK_SOURCE below; both tool files are
// generated, so they can never drift.
//
//   node tools/hooks_sync.mjs            regenerate both files
//   node tools/hooks_sync.mjs --check    report drift, write nothing, exit 1 on drift
//
// Why this exists: Codex and Claude use different matcher vocabularies (Codex
// regex "(?i)(bash|shell|exec)" vs Claude "Bash"; Codex "(?i)spawn_agent" vs
// Claude "Agent|Task") and tag each recorded event with the agent label
// (codex|claude). Hand-maintaining the two files drifts. Declare the hooks once
// here; the generator renders each tool's format. The Claude file is patched
// SURGICALLY: only its `hooks` key is replaced, so `$comment`, permissions, env
// and any other Claude-only settings are preserved.

import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { isMain } from "./lib/cli.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..");

// --- Canonical source: the hooks, tool-agnostic. -------------------------
// Each event lists hook entries. `match` is a logical matcher key resolved per
// tool in TOOLS[].matchers; omit it for an unmatched event. `record` selects a
// recorder command template from RECORDERS.
const HOOK_SOURCE = {
  SessionStart: [{ record: "fast" }],
  PreToolUse: [{ match: "shell", record: "fast" }],
  PostToolUse: [
    { match: "shell", record: "fast" },
    { match: "spawnAgent", record: "node" },
  ],
};

// Recorder command templates. `label` (the agent name) is appended as the last
// arg; commandWindows is the native/.exe variant with Windows path separators.
const RECORDERS = {
  fast: (label) => ({
    type: "command",
    command: `tools/ai_profile/hook_record_fast ${label}`,
    commandWindows: `tools\\ai_profile\\hook_record_fast.exe ${label}`,
  }),
  node: (label) => ({
    type: "command",
    command: `node tools/ai_profile/hook_record.mjs ${label}`,
    commandWindows: `node tools\\ai_profile\\hook_record.mjs ${label}`,
  }),
};

// Per-tool rendering config.
const TOOLS = {
  codex: {
    file: ".codex/hooks.json",
    label: "codex",
    matchers: { shell: "(?i)(bash|shell|exec)", spawnAgent: "(?i)spawn_agent" },
    // The whole Codex file is exactly { hooks }.
    render: (hooks) => ({ hooks }),
  },
  claude: {
    file: ".claude/settings.json",
    label: "claude",
    matchers: { shell: "Bash", spawnAgent: "Agent|Task" },
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
    console.log("usage: node tools/hooks_sync.mjs [--check]");
    process.exit(0);
  }
  const check = args.includes("--check");
  const drift = syncAll({ check });
  if (check) {
    if (drift.length) {
      console.error(`hooks drift: ${drift.join(", ")} — run: node tools/hooks_sync.mjs`);
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
