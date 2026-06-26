import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import test from "node:test";

const root = process.cwd();

function tempRoot(t, active = true) {
  const dir = mkdtempSync(join(tmpdir(), "visual-invariant-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  mkdirSync(join(dir, "src"), { recursive: true });
  writeFileSync(join(dir, "GAME_PROJECT.md"), active
    ? "# GAME_PROJECT\n\n## Active Game\n\nStatus: active\n\n- Game id: `test-game`\n- Game folder: `gamedesign/projects/test-game/`\n"
    : "# GAME_PROJECT\n\n## Active Game\n\nStatus: none\n\nThere is no active game concept.\n", "utf8");
  return dir;
}

function runGuard(dir) {
  return spawnSync(process.execPath, ["tools/visual_invariant_guard.mjs", "--root", dir, "--json"], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
  });
}

test("skips clean seed without an active concept", (t) => {
  const dir = tempRoot(t, false);
  writeFileSync(join(dir, "src", "clean_seed_main.c"), "void x(){ draw_text(); nt_shape_renderer_rect(); }\n", "utf8");
  const result = runGuard(dir);
  assert.equal(result.status, 0, result.stderr);
  const report = JSON.parse(result.stdout);
  assert.equal(report.skipped, true);
});

test("rejects handmade text and debug renderer in active game runtime", (t) => {
  const dir = tempRoot(t);
  writeFileSync(join(dir, "src", "main.c"), "void render(){ draw_text(); nt_shape_renderer_rect(); }\n", "utf8");
  const result = runGuard(dir);
  assert.notEqual(result.status, 0);
  const report = JSON.parse(result.stdout);
  assert.equal(report.problems.length, 2);
  assert.deepEqual(report.problems.map((p) => p.rule), ["engine-text-renderer", "debug-renderer"]);
});

test("rejects handmade bitmap font tables even with debug debt", (t) => {
  const dir = tempRoot(t);
  writeFileSync(join(dir, "src", "main.c"), "/* temporary debug debt */\nstatic const uint8_t glyph5[7] = {0};\nvoid render(){ draw_text5(); }\n", "utf8");
  const result = runGuard(dir);
  assert.notEqual(result.status, 0);
  const report = JSON.parse(result.stdout);
  assert.deepEqual(report.problems.map((p) => p.rule), ["no-handmade-fonts", "no-handmade-fonts"]);
});

test("allows explicit debug debt and boundary Y-down conversion", (t) => {
  const dir = tempRoot(t);
  writeFileSync(join(dir, "src", "main.c"), "/* debug_only prototype marker */\nvoid render(){ nt_shape_renderer_rect(); }\n", "utf8");
  writeFileSync(join(dir, "src", "input_adapter.c"), "float y = height - pointer.y; /* boundary conversion */\n", "utf8");
  const result = runGuard(dir);
  assert.equal(result.status, 0, result.stderr);
});

test("rejects Y-down convention outside boundary files", (t) => {
  const dir = tempRoot(t);
  writeFileSync(join(dir, "src", "main.c"), "float y = height - pointer.y;\n", "utf8");
  const result = runGuard(dir);
  assert.notEqual(result.status, 0);
  const report = JSON.parse(result.stdout);
  assert.equal(report.problems[0].rule, "y-up-boundary");
});
