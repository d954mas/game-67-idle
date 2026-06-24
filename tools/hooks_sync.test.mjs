// Tests for tools/hooks_sync.mjs: the generator must reproduce the committed
// Codex + Claude config files byte-exact (so it is provably the single source
// of truth), preserve Claude-only keys, and keep the two matcher vocabularies
// distinct.

import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

import { generate, buildHooks, TOOLS } from "./hooks_sync.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");

test("generator reproduces .codex/hooks.json byte-exact", () => {
  const committed = readFileSync(resolve(root, ".codex/hooks.json"), "utf8");
  assert.equal(generate(TOOLS.codex), committed);
});

test("generator reproduces .claude/settings.json byte-exact", () => {
  const committed = readFileSync(resolve(root, ".claude/settings.json"), "utf8");
  assert.equal(generate(TOOLS.claude), committed);
});

test("Claude render preserves non-hook keys (e.g. $comment)", () => {
  const settings = JSON.parse(readFileSync(resolve(root, ".claude/settings.json"), "utf8"));
  if (Object.prototype.hasOwnProperty.call(settings, "$comment")) {
    const regenerated = JSON.parse(generate(TOOLS.claude));
    assert.equal(regenerated["$comment"], settings["$comment"]);
  }
  // settings.local.json (permissions etc.) must never be a hooks_sync target.
  const targets = Object.values(TOOLS).map((t) => t.file);
  assert.ok(!targets.includes(".claude/settings.local.json"));
});

test("each tool tags hooks with its own agent label", () => {
  assert.match(JSON.stringify(buildHooks(TOOLS.codex)), /hook_record_fast codex/);
  assert.match(JSON.stringify(buildHooks(TOOLS.claude)), /hook_record_fast claude/);
});

test("matcher vocabularies stay tool-specific", () => {
  const codex = JSON.stringify(buildHooks(TOOLS.codex));
  const claude = JSON.stringify(buildHooks(TOOLS.claude));
  assert.match(codex, /\(\?i\)\(bash\|shell\|exec\)/);
  assert.match(codex, /\(\?i\)spawn_agent/);
  assert.match(claude, /"matcher":"Bash"/);
  assert.match(claude, /"matcher":"Agent\|Task"/);
  // Codex regex matchers must not leak into the Claude file and vice versa.
  assert.ok(!claude.includes("spawn_agent"));
  assert.ok(!codex.includes("Agent|Task"));
});

test("committed files actually exist (guards against a moved path)", () => {
  for (const tool of Object.values(TOOLS)) {
    assert.ok(existsSync(resolve(root, tool.file)), `${tool.file} missing`);
  }
});
