import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "quality-profile-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  mkdirSync(join(dir, "tasks", "active"), { recursive: true });
  return dir;
}

function writeTask(root, id, body) {
  writeFileSync(join(root, "tasks", "active", `${id}-test.md`), `---
id: ${id}
title: Test ${id}
status: doing
priority: P1
tags: [test]
created: 2026-06-26
updated: 2026-06-26
---

## What

Test.

## Done when

- [ ] done

## Open questions

## Log

${body}
`, "utf8");
}

function run(root, ...args) {
  return spawnSync(process.execPath, [join(process.cwd(), "ai_studio/quality/profile.mjs"), "--root", root, ...args], {
    cwd: root,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

test("quality profile counts rule outcomes from task logs", (t) => {
  const root = tempRoot(t);
  writeTask(root, "T0001", "- 2026-06-26: Quality: QCLR_001=pass; QART_001=block; evidence: screenshot.\n");
  writeTask(root, "T0002", "- 2026-06-26: Quality: QCLR_001 review; QTECH_COMMON=skip; evidence: lead review.\n");

  const result = run(root, "--json");
  assert.equal(result.status, 0, result.stderr);
  const profile = JSON.parse(result.stdout);
  const clarity001 = profile.rules.find((item) => item.rule === "QCLR_001");
  const art001 = profile.rules.find((item) => item.rule === "QART_001");
  const techCommon = profile.rules.find((item) => item.rule === "QTECH_COMMON");

  assert.equal(profile.entries, 4);
  assert.equal(clarity001.total, 2);
  assert.equal(clarity001.group, "player_clarity");
  assert.equal(clarity001.outcomes.pass, 1);
  assert.equal(clarity001.outcomes.review, 1);
  assert.deepEqual(clarity001.tasks, ["T0001", "T0002"]);
  assert.equal(art001.outcomes.block, 1);
  assert.equal(techCommon.group, "technical");
  assert.equal(techCommon.outcomes.skip, 1);
});
