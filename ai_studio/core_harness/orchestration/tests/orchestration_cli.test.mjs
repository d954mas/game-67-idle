// Orchestration CLI route tests. Run:
// node --test ai_studio/core_harness/orchestration/tests/orchestration_cli.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";

const orchestrationDir = dirname(import.meta.dirname);
const cli = join(orchestrationDir, "cli.mjs");

test("orchestration cli exposes the orchestration command surface", () => {
  const result = spawnSync(process.execPath, [cli, "--help"], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.match(result.stdout, /usage: node ai_studio\/core_harness\/orchestration\/cli\.mjs/);
  assert.match(result.stdout, /orchestration-template/);
  assert.match(result.stdout, /subagent-packet-template/);
  assert.match(result.stdout, /subagent-packet-check/);
  assert.match(result.stdout, /orchestration-check/);
  assert.match(result.stdout, /orchestration-bootstrap/);
});

test("orchestration cli prints template output", () => {
  const result = spawnSync(process.execPath, [cli, "orchestration-template"], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.match(result.stdout, /orchestration: used/);
  assert.match(result.stdout, /objective:/);
  assert.match(result.stdout, /stop condition:/);
});

test("orchestration cli validates packets", () => {
  const packet = `objective: Map the current orchestration route.
allowed files: ai_studio/core_harness/orchestration/cli.mjs
forbidden files: AGENTS.md
tool-use guard: verify exact repo paths before reading files
expected output: route summary
evidence command or artifact: node --test ai_studio/core_harness/orchestration/tests/orchestration_cli.test.mjs
stop condition: wrapper tests pass
handoff:
  findings: summary
  files: inspected files
  commands/evidence: commands run
  risks: residual risks
  owner action: next action
  not-done: gaps`;

  const result = spawnSync(
    process.execPath,
    [cli, "subagent-packet-check", "--text", packet, "--json"],
    { encoding: "utf8" },
  );

  assert.equal(result.status, 0);
  assert.equal(JSON.parse(result.stdout).ok, true);
});

test("orchestration cli rejects taskboard-only commands", () => {
  const result = spawnSync(process.execPath, [cli, "list"], { encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /not an orchestration command/);
});
