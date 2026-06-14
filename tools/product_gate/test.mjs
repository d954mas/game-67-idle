import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function tempDir() {
  return mkdtempSync(join(tmpdir(), "product-gate-test-"));
}

function cleanup(dir) {
  rmSync(dir, { recursive: true, force: true });
}

function runRaw(args, options = {}) {
  return spawnSync(process.execPath, args, {
    cwd: root,
    env: { ...process.env, ...(options.env || {}) },
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function writeTask(rootDir, id = "T0001") {
  const taskDir = join(rootDir, "tasks", "active");
  mkdirSync(taskDir, { recursive: true });
  writeFileSync(join(taskDir, `${id}-test.md`), `---
id: ${id}
title: Test task
status: doing
priority: P0
tags: [test]
created: 2026-06-14
updated: 2026-06-14
---

## What

Test task.

## Done when

- [ ] evidence exists

## Open questions

## Log
`, "utf8");
}

test("product read gate writes markdown and json for strict fail", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    const markdown = join(dir, "gate.md");
    const json = join(dir, "gate.json");
    const latest = join(dir, "latest.json");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "rune-marches",
      "--task", "T0006",
      "--surface", "desktop",
      "--screenshot", screenshot,
      "--verdict", "fail",
      "--where", "A forest road near the ruined tower.",
      "--action", "Click Scout Road to start the route.",
      "--response", "The route marker advances and a fight opens.",
      "--reward", "The player receives coins and a visible upgrade hook.",
      "--game-look", "Fantasy map, readable action button, and non-debug UI.",
      "--problem", "The hierarchy is still too busy.",
      "--next", "Reduce controls and rebuild the primary action area.",
      "--output", markdown,
      "--json-output", json,
      "--index-output", latest,
      "--strict",
    ]);
    assert.equal(result.status, 0, result.stderr);
    assert.equal(existsSync(markdown), true);
    assert.equal(existsSync(json), true);
    assert.equal(existsSync(latest), true);
    assert.match(readFileSync(markdown, "utf8"), /Verdict: \*\*FAIL\*\*/);
    assert.equal(JSON.parse(readFileSync(json, "utf8")).verdict, "fail");
    assert.equal(JSON.parse(readFileSync(latest, "utf8")).markdown, markdown);
  } finally {
    cleanup(dir);
  }
});

test("product read gate appends task log when requested", () => {
  const dir = tempDir();
  try {
    writeTask(dir, "T0099");
    const screenshot = join(dir, "screen.png");
    const markdown = join(dir, "gate.md");
    const json = join(dir, "gate.json");
    const latest = join(dir, "latest.json");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "rune-marches",
      "--task", "T0099",
      "--surface", "desktop",
      "--screenshot", screenshot,
      "--verdict", "fail",
      "--where", "A fantasy road screen.",
      "--action", "Click the scout button.",
      "--response", "The route advances.",
      "--reward", "A reward chip appears.",
      "--game-look", "It uses map art and game controls.",
      "--problem", "Still too busy.",
      "--next", "Reduce the HUD.",
      "--output", markdown,
      "--json-output", json,
      "--index-output", latest,
      "--task-log",
      "--strict",
    ], { env: { TASKBOARD_ROOT: dir } });
    assert.equal(result.status, 0, result.stderr);
    const task = readFileSync(join(dir, "tasks", "active", "T0099-test.md"), "utf8");
    assert.match(task, /product gate FAIL/);
    assert.match(task, /Reduce the HUD/);
  } finally {
    cleanup(dir);
  }
});

test("close slice refuses failed gate in strict mode", () => {
  const dir = tempDir();
  try {
    writeTask(dir, "T0098");
    const gate = join(dir, "gate.json");
    writeFileSync(gate, `${JSON.stringify({ verdict: "fail", surface: "desktop", screenshot: "tmp/screen.png", markdown: "gate.md", next: "Fix screen" })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/close_slice.mjs",
      "--project", "rune-marches",
      "--task", "T0098",
      "--gate", gate,
      "--evidence", "node --test tools/product_gate/test.mjs",
      "--strict",
    ], { env: { TASKBOARD_ROOT: dir } });
    assert.equal(result.status, 1);
    assert.match(result.stderr, /product gate is fail/);
  } finally {
    cleanup(dir);
  }
});

test("close slice logs passing gate evidence and can set status", () => {
  const dir = tempDir();
  try {
    writeTask(dir, "T0097");
    const gate = join(dir, "gate.json");
    writeFileSync(gate, `${JSON.stringify({ verdict: "pass", surface: "desktop", screenshot: "tmp/screen.png", markdown: "gate.md", next: "Next slice" })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/close_slice.mjs",
      "--project", "rune-marches",
      "--task", "T0097",
      "--gate", gate,
      "--evidence", "build passed",
      "--evidence", "scenario passed",
      "--next", "Review with lead",
      "--status", "review",
      "--strict",
    ], { env: { TASKBOARD_ROOT: dir } });
    assert.equal(result.status, 0, result.stderr);
    const task = readFileSync(join(dir, "tasks", "active", "T0097-test.md"), "utf8");
    assert.match(task, /status: review/);
    assert.match(task, /close-slice PASS gate/);
    assert.match(task, /build passed \| scenario passed/);
  } finally {
    cleanup(dir);
  }
});

test("product read gate refuses pass with missing player-read answers", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "rune-marches",
      "--screenshot", screenshot,
      "--verdict", "pass",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /--where needs a concrete player-read answer/);
  } finally {
    cleanup(dir);
  }
});

test("product read gate refuses missing screenshots", () => {
  const result = runRaw([
    "tools/product_gate/review.mjs",
    "--project", "rune-marches",
    "--screenshot", "tmp/not-here.png",
    "--verdict", "fail",
    "--strict",
  ]);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /screenshot does not exist/);
});

test("responsive layout audit passes clean portrait action stack", () => {
  const dir = tempDir();
  try {
    const tree = join(dir, "tree.json");
    writeFileSync(tree, `${JSON.stringify([
      { id: "root", role: "screen", x: 0, y: 0, w: 360, h: 640, visible: true, enabled: true },
      { id: "action.scout_road", role: "button", x: 10, y: 520, w: 340, h: 52, visible: true, enabled: true },
      { id: "action.rest", role: "button", x: 10, y: 582, w: 165, h: 48, visible: true, enabled: true },
      { id: "upgrade.spark_ward_1", role: "button", x: 185, y: 582, w: 165, h: 48, visible: true, enabled: false },
    ])}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/responsive_layout_audit.mjs",
      "--ui-tree", tree,
      "--surface", "portrait",
      "--primary", "action.scout_road",
      "--button", "action.rest",
      "--button", "upgrade.spark_ward_1",
    ]);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    assert.match(result.stdout, /pass: checked 3/);
  } finally {
    cleanup(dir);
  }
});

test("responsive layout audit rejects squeezed portrait action row", () => {
  const dir = tempDir();
  try {
    const tree = join(dir, "tree.json");
    writeFileSync(tree, `${JSON.stringify([
      { id: "root", role: "screen", x: 0, y: 0, w: 360, h: 640, visible: true, enabled: true },
      { id: "action.scout_road", role: "button", x: 130, y: 582, w: 100, h: 48, visible: true, enabled: true },
      { id: "action.rest", role: "button", x: 10, y: 582, w: 110, h: 48, visible: true, enabled: true },
      { id: "upgrade.spark_ward_1", role: "button", x: 240, y: 582, w: 110, h: 48, visible: true, enabled: false },
    ])}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/responsive_layout_audit.mjs",
      "--ui-tree", tree,
      "--surface", "portrait",
      "--primary", "action.scout_road",
      "--button", "action.rest",
      "--button", "upgrade.spark_ward_1",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stdout, /portrait width/);
    assert.match(result.stdout, /should sit below portrait primary action/);
  } finally {
    cleanup(dir);
  }
});
