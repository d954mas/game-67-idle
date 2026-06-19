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

    assert.equal(existsSync(join(target, "tasks", "README.md")), true);
    assert.equal(existsSync(join(target, "tasks", "STATUS.md")), true);
    assert.equal(existsSync(join(target, "tasks", "guides", "task-store-reference.md")), true);
    assert.equal(existsSync(join(target, "gamedesign", "README.md")), true);
    assert.equal(existsSync(join(target, "gamedesign", "sources", "README.md")), true);
    assert.equal(existsSync(join(target, "docs", "ai-pipeline", "agent-workflow.md")), true);
    assert.equal(existsSync(join(target, ".claude", "skills", "task-manager", "SKILL.md")), true);
    assert.equal(existsSync(join(target, "tools", "context_budget_config.mjs")), true);

    const readme = readFileSync(join(target, "tasks", "README.md"), "utf8");
    assert.match(readme, /tasks\/guides\/task-store-reference\.md/);
    const guide = readFileSync(join(target, "tasks", "guides", "task-store-reference.md"), "utf8");
    assert.match(guide, /Task Store Reference/);
    const pipeline = readFileSync(join(target, "AI_PIPELINE.md"), "utf8");
    assert.match(pipeline, /docs\/ai-pipeline\/agent-workflow\.md/);

    const docRefs = runInTarget(target, ["tools/doc_reference_check.mjs"]);
    assert.equal(docRefs.status, 0, docRefs.stderr);
    assert.match(docRefs.stdout, /markdown file\(s\) checked/);

    const contextBudget = runInTarget(target, ["tools/context_budget.mjs"]);
    assert.equal(contextBudget.status, 0, contextBudget.stderr);
    assert.match(contextBudget.stdout, /ok: context budgets pass/);
  } finally {
    cleanup(dir);
  }
});
