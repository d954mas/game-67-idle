import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "quality-profile-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  mkdirSync(join(dir, "ai_studio", "taskboard", "items", "active"), { recursive: true });
  return dir;
}

function writeTask(root, id, body) {
  writeFileSync(join(root, "ai_studio", "taskboard", "items", "active", `${id}-test.md`), `---
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
  writeTask(root, "T0002", "- 2026-06-26: Quality: QCLR_001 review; QTECH_001=skip; QTECH_001=unverified; evidence: lead review.\n");

  const result = run(root, "--json");
  assert.equal(result.status, 0, result.stderr);
  const profile = JSON.parse(result.stdout);
  const clarity001 = profile.rules.find((item) => item.rule === "QCLR_001");
  const art001 = profile.rules.find((item) => item.rule === "QART_001");
  const tech001 = profile.rules.find((item) => item.rule === "QTECH_001");

  assert.equal(profile.report_kind, "advisory-task-log-summary");
  assert.equal(profile.entries, 5);
  assert.equal(clarity001.total, 2);
  assert.equal(clarity001.group, "player_clarity");
  assert.equal(clarity001.outcomes.pass, 1);
  assert.equal(clarity001.outcomes.review, 1);
  assert.deepEqual(clarity001.tasks, ["T0001", "T0002"]);
  assert.equal(art001.outcomes.block, 1);
  assert.equal(tech001.group, "technical");
  assert.equal(tech001.outcomes.skip, 1);
  assert.equal(tech001.outcomes.unverified, 1);
});

test("quality profile text marks the report as advisory", (t) => {
  const root = tempRoot(t);
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /Report kind: advisory task-log summary; not enforcement\./);
});

test("quality profile counts only canonical Taskboard Log records", (t) => {
  const root = tempRoot(t);
  writeTask(root, "T0001", `- 2026-06-26: Quality: QCLR_001=pass; QTECH_001=review; evidence: canonical record.
Quality: QART_001=block; evidence: bare example.
\`\`\`text
- 2026-06-26: Quality: QGDD_001=pass; evidence: fenced example.
\`\`\`
<!-- - 2026-06-26: Quality: QDES_001=pass; evidence: comment example. -->
# Appendix

- 2026-06-26: Quality: QASSET_001=pass; evidence: appendix example.
`);

  const result = run(root, "--json");
  assert.equal(result.status, 0, result.stderr);
  const profile = JSON.parse(result.stdout);
  assert.equal(profile.quality_lines, 1);
  assert.equal(profile.entries, 2);
  assert.deepEqual(profile.rules.map((item) => item.rule), ["QCLR_001", "QTECH_001"]);
});
