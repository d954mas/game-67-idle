import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function tempDir() {
  return mkdtempSync(join(tmpdir(), "export-base-test-"));
}

function cleanup(dir) {
  rmSync(dir, { recursive: true, force: true });
}

function runExport(target) {
  return spawnSync(process.execPath, ["tools/bootstrap/export_base.mjs", "--target", target], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
  });
}

function runInTarget(target, args) {
  return spawnSync(process.execPath, args, {
    cwd: target,
    encoding: "utf8",
    stdio: "pipe",
  });
}

test("portable export includes task guides and generated skill pointers", () => {
  const dir = tempDir();
  try {
    const target = join(dir, "portable-game");
    const result = runExport(target);
    assert.equal(result.status, 0, result.stderr);

    assert.equal(existsSync(join(target, "tasks", "STATUS.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "taskboard", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "taskboard", "task-store-reference.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "taskboard", "cli.mjs")), true);
    assert.equal(existsSync(join(target, "gamedesign", "README.md")), true);
    assert.equal(existsSync(join(target, "gamedesign", "sources", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "core_harness", "workflow", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "core_harness", "workflow", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "core_harness", "orchestration", "README.md")), true);
    assert.equal(existsSync(join(target, "docs", "ai-pipeline", "quality-validation.md")), true);
    assert.equal(existsSync(join(target, "docs", "ai-pipeline", "profiling-reuse.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "tree.json")), true);
    assert.equal(existsSync(join(target, "ai_studio", "core_harness", "README.md")), true);
    assert.equal(
      existsSync(join(target, "tools", "architecture_map", "build_architecture_map.mjs")),
      true,
    );
    assert.equal(existsSync(join(target, "tools", "README.md")), true);
    assert.equal(existsSync(join(target, "tools", "requirements", "ai-pipeline-full.txt")), true);
    assert.equal(existsSync(join(target, ".claude", "skills", "task-manager", "SKILL.md")), true);
    assert.equal(existsSync(join(target, "tools", "context_budget_config.mjs")), true);
    assert.equal(
      existsSync(join(target, "tools", "assets", "intake", "download_source_asset.mjs")),
      true,
    );
    assert.equal(
      existsSync(join(target, "tools", "assets", "intake", "audit_tileable_texture.py")),
      true,
    );

    const readme = readFileSync(join(target, "ai_studio", "taskboard", "README.md"), "utf8");
    assert.match(readme, /ai_studio\/taskboard\/task-store-reference\.md/);
    const guide = readFileSync(join(target, "ai_studio", "taskboard", "task-store-reference.md"), "utf8");
    assert.match(guide, /Task Store Reference/);
    const studio = readFileSync(join(target, "ai_studio", "README.md"), "utf8");
    assert.match(studio, /ai_studio\/core_harness\/workflow\/README\.md/);
    assert.match(studio, /ai_studio\/core_harness\/orchestration\/README\.md/);
    assert.match(studio, /docs\/ai-pipeline\/quality-validation\.md/);
    assert.match(studio, /docs\/ai-pipeline\/profiling-reuse\.md/);

    const docRefs = runInTarget(target, ["ai_studio/core_harness/validation/doc_reference_check.mjs"]);
    assert.equal(docRefs.status, 0, docRefs.stderr);
    assert.match(docRefs.stdout, /markdown file\(s\) checked/);

    // Context budgets are an END-OF-ITERATION check, not a during-work blocker:
    // a budget overage must not fail the quick validate that runs mid-edit (it
    // interferes with the work in flight). The export verifies STRUCTURE here;
    // budget growth is caught deliberately by `node tools/pipeline_validate.mjs
    // --review` (context_budget). [REFACTOR_PLAN Phase 1 / lead directive]
    const contextBudget = runInTarget(target, ["tools/context_budget.mjs"]);
    assert.match(contextBudget.stdout, /context budget/);
  } finally {
    cleanup(dir);
  }
});
