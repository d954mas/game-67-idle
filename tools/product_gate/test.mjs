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

function writeProfileGuard(rootDir, name = "profile-status.json", usable = true) {
  const path = join(rootDir, name);
  writeFileSync(path, `${JSON.stringify({
    current_scope_review_confidence: {
      level: usable ? "usable" : "broken",
      usable_for_review: usable,
      blocking_reasons: usable ? [] : ["scope_stale"],
    },
  })}\n`, "utf8");
  return path;
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

test("product read gate visual strict requires full rubric scores", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "visual-test",
      "--screenshot", screenshot,
      "--verdict", "pass",
      "--where", "A bright island fishing screen.",
      "--action", "Tap the cast button.",
      "--response", "The bobber lands in the water.",
      "--reward", "A fish reward card appears.",
      "--game-look", "Colorful world art and game UI are visible.",
      "--visual-strict",
      "--visual-score", "composition=4",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /--visual-score readability=1-5 is required/);
  } finally {
    cleanup(dir);
  }
});

test("product read gate visual strict rejects low-score pass", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "visual-test",
      "--screenshot", screenshot,
      "--verdict", "pass",
      "--where", "A bright island fishing screen.",
      "--action", "Tap the cast button.",
      "--response", "The bobber lands in the water.",
      "--reward", "A fish reward card appears.",
      "--game-look", "Colorful world art and game UI are visible.",
      "--visual-strict",
      "--visual-score", "composition=4",
      "--visual-score", "readability=3",
      "--visual-score", "ui_controls=4",
      "--visual-score", "action_direction=4",
      "--visual-score", "art_quality=4",
      "--visual-score", "audience_fit=4",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /visual pass requires readability score >= 4/);
  } finally {
    cleanup(dir);
  }
});

test("product read gate visual strict writes fail critique", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    const markdown = join(dir, "gate.md");
    const json = join(dir, "gate.json");
    const latest = join(dir, "latest.json");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "visual-test",
      "--screenshot", screenshot,
      "--verdict", "fail",
      "--where", "A bright island fishing screen.",
      "--action", "Tap the cast button.",
      "--response", "The bobber lands in the water.",
      "--reward", "A fish reward card appears.",
      "--game-look", "Colorful world art and game UI are visible.",
      "--problem", "Text, buttons, and action direction do not read.",
      "--next", "Rebuild the HUD and cast pose before adding content.",
      "--visual-strict",
      "--visual-score", "composition=2",
      "--visual-score", "readability=1",
      "--visual-score", "ui_controls=1",
      "--visual-score", "action_direction=2",
      "--visual-score", "art_quality=2",
      "--visual-score", "audience_fit=2",
      "--visual-issue", "blocker:readability:Text is too small and low contrast at gameplay size.",
      "--visual-issue", "major:ui_controls:Buttons look like debug rectangles instead of game UI.",
      "--output", markdown,
      "--json-output", json,
      "--index-output", latest,
      "--strict",
    ]);
    assert.equal(result.status, 0, result.stderr);
    const report = JSON.parse(readFileSync(json, "utf8"));
    assert.equal(report.visual_critique.strict, true);
    assert.equal(report.visual_critique.scores.readability, 1);
    assert.equal(report.visual_critique.issues[0].severity, "blocker");
    const md = readFileSync(markdown, "utf8");
    assert.match(md, /## Visual Critique/);
    assert.match(md, /blocker \/ readability: Text is too small/);
  } finally {
    cleanup(dir);
  }
});

test("visual critique packet requires screenshot evidence", () => {
  const dir = tempDir();
  try {
    const result = runRaw([
      "tools/product_gate/visual_critique_packet.mjs",
      "--project", "visual-test",
      "--task", "T0001",
      "--screenshot", join(dir, "missing.png"),
      "--target", "gamedesign/projects/visual-test/art/fake.png",
      "--output", join(dir, "packet.md"),
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /screenshot does not exist/);
  } finally {
    cleanup(dir);
  }
});

test("visual critique packet writes strict rubric and gate command", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    const markdown = join(dir, "packet.md");
    const json = join(dir, "packet.json");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/visual_critique_packet.mjs",
      "--project", "visual-test",
      "--task", "T0001",
      "--surface", "desktop",
      "--screenshot", screenshot,
      "--target", "gamedesign/projects/visual-test/art/fake.png",
      "--brief", "Bright casual game screen with readable UI.",
      "--output", markdown,
      "--json-output", json,
    ]);
    assert.equal(result.status, 0, result.stderr);
    const md = readFileSync(markdown, "utf8");
    assert.match(md, /Visual Critic Packet/);
    assert.match(md, /--visual-strict/);
    assert.match(md, /composition: score 1-5/);
    assert.match(md, /readability: score 1-5/);
    assert.match(md, /ui_controls: score 1-5/);
    assert.match(md, /action_direction: score 1-5/);
    assert.match(md, /art_quality: score 1-5/);
    assert.match(md, /audience_fit: score 1-5/);
    const report = JSON.parse(readFileSync(json, "utf8"));
    assert.equal(report.schema, "game.visual_critique_packet");
    assert.ok(report.axes.includes("readability"));
    assert.match(report.gate_command, /node tools\/ai\.mjs gate/);
  } finally {
    cleanup(dir);
  }
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

test("slice hygiene fails broad normal diff in strict mode", () => {
  const dir = tempDir();
  try {
    const json = join(dir, "hygiene.json");
    const changed = [];
    for (let index = 0; index < 31; index += 1) {
      changed.push("--changed-file", `src/file-${index}.c`);
    }
    const result = runRaw([
      "tools/product_gate/slice_hygiene.mjs",
      "--root", dir,
      "--threshold", "30",
      "--strict",
      "--json-output", json,
      "--build-evidence", "cmake --build --preset native-debug",
      "--probe-evidence", "playtest probe passed",
      "--product-gate", "gate.json",
      "--screenshot", "screen.png",
      ...changed,
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stdout, /Changed files: 31 \/ threshold 30/);
    const report = JSON.parse(readFileSync(json, "utf8"));
    assert.equal(report.verdict, "fail");
    assert.ok(report.problems.some((problem) => problem.includes("split the slice")));
  } finally {
    cleanup(dir);
  }
});

test("slice hygiene allows explicit snapshot over broad diff threshold", () => {
  const dir = tempDir();
  try {
    writeFileSync(join(dir, "screen.png"), "png", "utf8");
    writeFileSync(join(dir, "gate.json"), `${JSON.stringify({ verdict: "pass" })}\n`, "utf8");
    const profileGuard = writeProfileGuard(dir);
    const changed = [];
    for (let index = 0; index < 31; index += 1) {
      changed.push("--changed-file", `src/file-${index}.c`);
    }
    const result = runRaw([
      "tools/product_gate/slice_hygiene.mjs",
      "--root", dir,
      "--threshold", "30",
      "--strict",
      "--snapshot",
      "--build-evidence", "cmake --build --preset native-debug",
      "--probe-evidence", "playtest probe passed",
      "--product-gate", "gate.json",
      "--screenshot", "screen.png",
      "--profile-guard", profileGuard,
      ...changed,
    ]);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    assert.match(result.stdout, /snapshot accepted/);
    assert.match(result.stdout, /Verdict: \*\*WARN\*\*/);
  } finally {
    cleanup(dir);
  }
});

test("slice hygiene treats missing profiler guard as advisory in strict mode", () => {
  const dir = tempDir();
  try {
    writeFileSync(join(dir, "screen.png"), "png", "utf8");
    writeFileSync(join(dir, "gate.json"), `${JSON.stringify({ verdict: "pass" })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/slice_hygiene.mjs",
      "--root", dir,
      "--strict",
      "--changed-file", "src/main.c",
      "--build-evidence", "build passed",
      "--probe-evidence", "probe passed",
      "--product-gate", "gate.json",
      "--screenshot", "screen.png",
    ]);
    // Passive profiling must not block normal work: a missing profiler guard is
    // an advisory warning, not a blocking problem (supersedes T0028).
    assert.equal(result.status, 0, result.stdout + result.stderr);
    assert.match(result.stdout, /no --profile-guard \(advisory/);
  } finally {
    cleanup(dir);
  }
});

test("slice hygiene records passing profiler guard evidence", () => {
  const dir = tempDir();
  try {
    writeFileSync(join(dir, "screen.png"), "png", "utf8");
    writeFileSync(join(dir, "gate.json"), `${JSON.stringify({ verdict: "pass" })}\n`, "utf8");
    const profileGuard = writeProfileGuard(dir);
    const json = join(dir, "hygiene.json");
    const result = runRaw([
      "tools/product_gate/slice_hygiene.mjs",
      "--root", dir,
      "--strict",
      "--changed-file", "src/main.c",
      "--build-evidence", "build passed",
      "--probe-evidence", "probe passed",
      "--product-gate", "gate.json",
      "--screenshot", "screen.png",
      "--profile-guard", profileGuard,
      "--json-output", json,
    ]);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    assert.match(result.stdout, /profiler guard: .*profile-status\.json \(pass\)/);
    const report = JSON.parse(readFileSync(json, "utf8"));
    assert.equal(report.evidence.profile_guards[0].verdict, "pass");
  } finally {
    cleanup(dir);
  }
});

test("slice hygiene treats a stale profiler guard as advisory, not blocking", () => {
  const dir = tempDir();
  try {
    writeFileSync(join(dir, "screen.png"), "png", "utf8");
    writeFileSync(join(dir, "gate.json"), `${JSON.stringify({ verdict: "pass" })}\n`, "utf8");
    const profileGuard = writeProfileGuard(dir, "stale-profile-status.json", false);
    const result = runRaw([
      "tools/product_gate/slice_hygiene.mjs",
      "--root", dir,
      "--strict",
      "--changed-file", "src/main.c",
      "--build-evidence", "build passed",
      "--probe-evidence", "probe passed",
      "--product-gate", "gate.json",
      "--screenshot", "screen.png",
      "--profile-guard", profileGuard,
    ]);
    // A stale/broken profiler guard surfaces as an advisory warning and does not
    // fail the slice (passive profiling does not block normal work).
    assert.equal(result.status, 0, result.stdout + result.stderr);
    assert.match(result.stdout, /profiler guard is fail \(advisory\)/);
    assert.match(result.stdout, /scope_stale/);
  } finally {
    cleanup(dir);
  }
});

test("slice hygiene requires fail audit callout or refresh", () => {
  const dir = tempDir();
  try {
    mkdirSync(join(dir, "gamedesign", "projects", "test", "reviews"), { recursive: true });
    const audit = join("gamedesign", "projects", "test", "reviews", "latest-audit.json");
    writeFileSync(join(dir, audit), `${JSON.stringify({ verdict: "fail", problems: ["bad"] })}\n`, "utf8");
    const profileGuard = writeProfileGuard(dir);
    const result = runRaw([
      "tools/product_gate/slice_hygiene.mjs",
      "--root", dir,
      "--strict",
      "--changed-file", audit,
      "--build-evidence", "build passed",
      "--probe-evidence", "probe passed",
      "--product-gate", audit,
      "--screenshot", audit,
      "--profile-guard", profileGuard,
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stdout, /changed fail\/stale review artifact/);

    const accepted = runRaw([
      "tools/product_gate/slice_hygiene.mjs",
      "--root", dir,
      "--strict",
      "--changed-file", audit,
      "--build-evidence", "build passed",
      "--probe-evidence", "probe passed",
      "--product-gate", audit,
      "--screenshot", audit,
      "--profile-guard", profileGuard,
      "--known-red-gate", "accepted historical fail audit for review notes",
    ]);
    assert.equal(accepted.status, 0, accepted.stdout + accepted.stderr);
    assert.match(accepted.stdout, /known red review artifact/);
  } finally {
    cleanup(dir);
  }
});

test("slice hygiene fails promised push when push target is unavailable", () => {
  const dir = tempDir();
  try {
    const result = runRaw([
      "tools/product_gate/slice_hygiene.mjs",
      "--root", dir,
      "--promise-push",
      "--strict",
      "--changed-file", "src/main.c",
      "--build-evidence", "build passed",
      "--probe-evidence", "probe passed",
      "--product-gate", "gate.json",
      "--screenshot", "screen.png",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stdout, /cannot promise push/);
  } finally {
    cleanup(dir);
  }
});
