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

function writeTaskWithBody(rootDir, id, fields, body) {
  const taskDir = join(rootDir, "tasks", "active");
  mkdirSync(taskDir, { recursive: true });
  writeFileSync(join(taskDir, `${id}-test.md`), `---
id: ${id}
title: ${fields.title || "Test task"}
status: ${fields.status || "doing"}
priority: ${fields.priority || "P1"}
tags: [${(fields.tags || ["test"]).join(", ")}]
created: 2026-06-19
updated: 2026-06-19
---

${body}
`, "utf8");
}

test("repeated failure guard rejects same strict gate loop without support task", () => {
  const dir = tempDir();
  try {
    writeTaskWithBody(dir, "T0200", {
      title: "Portal visual pass",
      tags: ["portal", "visual"],
    }, `## What

Improve the portal visual.

## Done when

- [ ] strict gate passes

## Log

- 2026-06-19: strict product gate FAIL for art quality and audience fit: screenshot still reads like shader trick; next: add more glow polish.
- 2026-06-19: strict product gate FAIL for art quality and audience fit: screenshot still reads like shader trick; next: add more trim polish.
- 2026-06-19: strict product gate FAIL for art quality and audience fit: screenshot still reads like shader trick; next: add more contrast polish.
`);
    const result = runRaw([
      "tools/product_gate/repeated_failure_guard.mjs",
      "--root", dir,
      "--max-repeat", "2",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /T0200 repeats strict\/product FAIL "art_quality\+audience_fit"/);
    assert.match(result.stderr, /support task or lead acceptance/);
  } finally {
    cleanup(dir);
  }
});

test("repeated failure guard allows same strict gate loop with support task", () => {
  const dir = tempDir();
  try {
    writeTaskWithBody(dir, "T0201", {
      title: "Portal visual pass",
      tags: ["portal", "visual"],
    }, `## What

Improve the portal visual.

## Done when

- [ ] strict gate passes

## Log

- 2026-06-19: strict product gate FAIL for art quality and audience fit: portal screenshot still reads like shader trick; next: add more glow polish.
- 2026-06-19: strict product gate FAIL for art quality and audience fit: portal screenshot still reads like shader trick; next: add more trim polish.
- 2026-06-19: strict product gate FAIL for art quality and audience fit: portal screenshot still reads like shader trick; next: move to render-target architecture.
`);
    writeTaskWithBody(dir, "T0202", {
      title: "Portal render-target architecture task",
      status: "backlog",
      tags: ["architecture", "tooling", "portal", "render-target"],
    }, `## What

Create the portal render-target architecture path instead of polishing the same screenshot.

## Done when

- [ ] render target API path exists

## Log
`);
    const result = runRaw([
      "tools/product_gate/repeated_failure_guard.mjs",
      "--root", dir,
      "--max-repeat", "2",
    ]);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    assert.match(result.stdout, /ok: no repeated strict\/product gate FAIL loop/);
  } finally {
    cleanup(dir);
  }
});

test("repeated failure guard catches an interleaved (non-consecutive) FAIL loop", () => {
  const dir = tempDir();
  try {
    writeTaskWithBody(dir, "T0203", { title: "Portal visual pass", tags: ["portal", "visual"] }, `## What

Improve the portal visual.

## Done when

- [ ] strict gate passes

## Log

- 2026-06-19: strict product gate FAIL for art quality: lighting still flat; next: add more glow polish.
- 2026-06-19: strict product gate FAIL for readability: ui text unreadable; next: bigger font.
- 2026-06-19: strict product gate FAIL for art quality: lighting still flat; next: add more trim polish.
- 2026-06-19: strict product gate FAIL for readability: ui text unreadable; next: bolder font.
- 2026-06-19: strict product gate FAIL for art quality: lighting still flat; next: add more contrast polish.
`);
    const result = runRaw([
      "tools/product_gate/repeated_failure_guard.mjs",
      "--root", dir,
      "--max-repeat", "2",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /T0203 repeats strict\/product FAIL "art_quality"/);
  } finally {
    cleanup(dir);
  }
});

test("repeated failure guard clusters an explicit [GATE]: FAIL verdict line", () => {
  const dir = tempDir();
  try {
    writeTaskWithBody(dir, "T0204", { title: "Source palette pass", tags: ["source"] }, `## What

Lock the source palette.

## Done when

- [ ] gate passes

## Log

- 2026-06-19: [ART-SOURCE]: FAIL palette drifts off the bible; next: regenerate the swatch sheet.
- 2026-06-19: [ART-SOURCE]: FAIL palette drifts off the bible; next: regenerate the swatch sheet.
- 2026-06-19: [ART-SOURCE]: FAIL palette drifts off the bible; next: regenerate the swatch sheet.
`);
    const result = runRaw([
      "tools/product_gate/repeated_failure_guard.mjs",
      "--root", dir,
      "--max-repeat", "2",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /T0204 repeats strict\/product FAIL "ART-SOURCE:/);
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
    const shot = join(dir, "screen.png");
    writeFileSync(shot, "png", "utf8");
    writeFileSync(gate, `${JSON.stringify({ verdict: "pass", surface: "desktop", screenshot: shot, markdown: "gate.md", next: "Next slice" })}\n`, "utf8");
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

test("close slice refuses strict close of lead-rejection task without resolved rejection proof", () => {
  const dir = tempDir();
  try {
    writeTaskWithBody(dir, "T0096", {
      title: "Fix lead-rejected hero visual",
      tags: ["visual", "lead-rejection"],
    }, `## What

Fix the lead rejection: hero looks like plastic cubes instead of authored art.

## Done when

- [ ] strict gate passes

## Log
`);
    const gate = join(dir, "gate.json");
    const shot = join(dir, "screen.png");
    writeFileSync(shot, "png", "utf8");
    writeFileSync(gate, `${JSON.stringify({ verdict: "pass", surface: "desktop", screenshot: shot, markdown: "gate.md", next: "Review with lead" })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/close_slice.mjs",
      "--project", "visual-test",
      "--task", "T0096",
      "--gate", gate,
      "--evidence", "build passed",
      "--strict",
    ], { env: { TASKBOARD_ROOT: dir } });
    assert.equal(result.status, 1);
    assert.match(result.stderr, /lead-rejection work/);
    assert.match(result.stderr, /--resolved-rejection/);
  } finally {
    cleanup(dir);
  }
});

test("close slice logs resolved rejection proof for lead-rejection task", () => {
  const dir = tempDir();
  try {
    writeTaskWithBody(dir, "T0095", {
      title: "Fix lead-rejected hero visual",
      tags: ["visual", "lead-rejection"],
    }, `## What

Fix the lead rejection: hero looks like plastic cubes instead of authored art.

## Done when

- [ ] strict gate passes

## Log
`);
    const gate = join(dir, "gate.json");
    const shot = join(dir, "screen.png");
    writeFileSync(shot, "png", "utf8");
    writeFileSync(gate, `${JSON.stringify({ verdict: "pass", surface: "desktop", screenshot: shot, markdown: "gate.md", next: "Review with lead" })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/close_slice.mjs",
      "--project", "visual-test",
      "--task", "T0095",
      "--gate", gate,
      "--evidence", "build passed",
      "--resolved-rejection", "Lead rejected plastic cube hero; screenshot and gate prove authored parts, material separation, and animation read.",
      "--strict",
    ], { env: { TASKBOARD_ROOT: dir } });
    assert.equal(result.status, 0, result.stderr);
    const task = readFileSync(join(dir, "tasks", "active", "T0095-test.md"), "utf8");
    assert.match(task, /resolved rejection: Lead rejected plastic cube hero/);
  } finally {
    cleanup(dir);
  }
});

test("close slice refuses a pass close when the gate screenshot file is missing", () => {
  const dir = tempDir();
  try {
    writeTask(dir, "T0099");
    const gate = join(dir, "gate.json");
    writeFileSync(gate, `${JSON.stringify({ verdict: "pass", surface: "desktop", screenshot: join(dir, "never-made.png"), markdown: "gate.md", next: "n" })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/close_slice.mjs",
      "--project", "rune-marches",
      "--task", "T0099",
      "--gate", gate,
      "--evidence", "build passed",
      "--strict",
    ], { env: { TASKBOARD_ROOT: dir } });
    assert.equal(result.status, 1);
    assert.match(result.stderr, /do not exist on disk/);
    assert.match(result.stderr, /never-made\.png/);
  } finally {
    cleanup(dir);
  }
});

test("close slice allows a partial close with a missing artifact under --allow-fail", () => {
  const dir = tempDir();
  try {
    writeTask(dir, "T0100");
    const gate = join(dir, "gate.json");
    writeFileSync(gate, `${JSON.stringify({ verdict: "fail", surface: "desktop", screenshot: join(dir, "never-made.png"), markdown: "gate.md", next: "n" })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/close_slice.mjs",
      "--project", "rune-marches",
      "--task", "T0100",
      "--gate", gate,
      "--evidence", "partial handoff",
      "--allow-fail",
    ], { env: { TASKBOARD_ROOT: dir } });
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /Close Slice/);
  } finally {
    cleanup(dir);
  }
});

test("product read gate --verify records a pending verification section", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    writeFileSync(screenshot, "png", "utf8");
    const output = join(dir, "gate.md");
    const json = join(dir, "gate.json");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "rune-marches",
      "--screenshot", screenshot,
      "--verdict", "pass",
      "--where", "A fantasy road screen.",
      "--action", "Tap the Scout button.",
      "--response", "The route advances.",
      "--reward", "Coins and progress appear.",
      "--game-look", "Map art replaces debug widgets.",
      "--output", output,
      "--json-output", json,
      "--index-output", join(dir, "latest.json"),
      "--verify",
    ]);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /Verification: pending independent confirmation/);
    assert.equal(readFileSync(json, "utf8").includes("\"required\": true"), true);
    assert.match(readFileSync(output, "utf8"), /## Verification/);
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

test("product read gate records explicit state coverage", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    const markdown = join(dir, "gate.md");
    const json = join(dir, "gate.json");
    const latest = join(dir, "latest.json");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "matrix-test",
      "--screenshot", screenshot,
      "--verdict", "pass",
      "--where", "A bright combat screen.",
      "--action", "Press the primary action button.",
      "--response", "The enemy takes damage.",
      "--reward", "A reward chip flies to the resource HUD.",
      "--game-look", "It has runtime art, readable UI, and game feedback.",
      "--require-state", "first_screen",
      "--require-state", "primary_action_ready",
      "--require-state", "reward_active",
      "--covered-state", "first_screen:screen.png",
      "--covered-state", "primary_action_ready:screen.png",
      "--not-covered-state", "reward_active:reward state belongs to the next slice",
      "--output", markdown,
      "--json-output", json,
      "--index-output", latest,
      "--strict",
    ]);
    assert.equal(result.status, 0, result.stderr);
    const report = JSON.parse(readFileSync(json, "utf8"));
    assert.deepEqual(report.state_coverage.required, ["first_screen", "primary_action_ready", "reward_active"]);
    assert.equal(report.state_coverage.covered[1].tag, "primary_action_ready");
    assert.equal(report.state_coverage.not_covered[0].tag, "reward_active");
    const md = readFileSync(markdown, "utf8");
    assert.match(md, /## State Coverage/);
    assert.match(md, /reward_active: reward state belongs to the next slice/);
    const latestReport = JSON.parse(readFileSync(latest, "utf8"));
    assert.equal(latestReport.state_coverage.not_covered[0].tag, "reward_active");
  } finally {
    cleanup(dir);
  }
});

test("product read gate rejects strict pass with missing required state coverage", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "matrix-test",
      "--screenshot", screenshot,
      "--verdict", "pass",
      "--where", "A bright combat screen.",
      "--action", "Press the primary action button.",
      "--response", "The enemy takes damage.",
      "--reward", "A reward chip flies to the resource HUD.",
      "--game-look", "It has runtime art, readable UI, and game feedback.",
      "--require-state", "first_screen",
      "--require-state", "transient_stress_state",
      "--covered-state", "first_screen:screen.png",
      "--strict",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /required state transient_stress_state is neither covered nor marked not covered/);
  } finally {
    cleanup(dir);
  }
});

test("product read gate rejects invalid state tags and debt without reason", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    writeFileSync(screenshot, "png", "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "matrix-test",
      "--screenshot", screenshot,
      "--verdict", "fail",
      "--problem", "Bad UI.",
      "--next", "Fix it.",
      "--require-state", "Bad State",
      "--not-covered-state", "reward_active",
      "--strict",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /invalid state tag: Bad State/);
    assert.match(result.stderr, /state reward_active needs a reason/);
  } finally {
    cleanup(dir);
  }
});

test("product read gate loads reusable state matrix JSON", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    const matrix = join(dir, "matrix.json");
    const markdown = join(dir, "gate.md");
    const json = join(dir, "gate.json");
    const latest = join(dir, "latest.json");
    writeFileSync(screenshot, "png", "utf8");
    writeFileSync(matrix, `${JSON.stringify({
      schema: "game.live_state_acceptance_matrix",
      required_states: ["first_screen"],
      states: {
        primary_action_ready: { status: "covered", evidence: "screen.png" },
        reward_active: { status: "not_covered", reason: "reward belongs to the next slice" },
      },
    })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "matrix-test",
      "--screenshot", screenshot,
      "--verdict", "pass",
      "--where", "A bright combat screen.",
      "--action", "Press the primary action button.",
      "--response", "The enemy takes damage.",
      "--reward", "A reward chip flies to the resource HUD.",
      "--game-look", "It has runtime art, readable UI, and game feedback.",
      "--state-matrix", matrix,
      "--covered-state", "first_screen:screen.png",
      "--output", markdown,
      "--json-output", json,
      "--index-output", latest,
      "--strict",
    ]);
    assert.equal(result.status, 0, result.stderr);
    const report = JSON.parse(readFileSync(json, "utf8"));
    assert.deepEqual(report.state_coverage.required, ["first_screen", "primary_action_ready", "reward_active"]);
    assert.deepEqual(report.state_coverage.covered.map((entry) => entry.tag), ["first_screen", "primary_action_ready"]);
    assert.equal(report.state_coverage.not_covered[0].tag, "reward_active");
  } finally {
    cleanup(dir);
  }
});

test("product read gate rejects matrix required state with no coverage or debt", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    const matrix = join(dir, "matrix.json");
    writeFileSync(screenshot, "png", "utf8");
    writeFileSync(matrix, `${JSON.stringify({
      states: {
        transient_stress_state: { required: true },
      },
    })}\n`, "utf8");
    const result = runRaw([
      "tools/product_gate/review.mjs",
      "--project", "matrix-test",
      "--screenshot", screenshot,
      "--verdict", "pass",
      "--where", "A bright combat screen.",
      "--action", "Press the primary action button.",
      "--response", "The enemy takes damage.",
      "--reward", "A reward chip flies to the resource HUD.",
      "--game-look", "It has runtime art, readable UI, and game feedback.",
      "--state-matrix", matrix,
      "--strict",
    ]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /required state transient_stress_state is neither covered nor marked not covered/);
  } finally {
    cleanup(dir);
  }
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
