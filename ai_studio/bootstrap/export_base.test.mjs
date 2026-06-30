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
  return spawnSync(process.execPath, ["ai_studio/bootstrap/export_base.mjs", "--target", target], {
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

    assert.equal(existsSync(join(target, "GAME_PROJECT.md")), true);
    assert.equal(existsSync(join(target, "tasks", "STATUS.md")), false);
    assert.equal(existsSync(join(target, "ai_studio", "taskboard", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "taskboard", "task-store-reference.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "taskboard", "cli.mjs")), true);
    assert.equal(existsSync(join(target, "gamedesign", "README.md")), true);
    assert.equal(existsSync(join(target, "gamedesign", "sources", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "core_harness", "workflow", "README.md")), true);
    assert.equal(
      existsSync(join(target, "ai_studio", "core_harness", "workflow", "orchestration", "README.md")),
      true,
    );
    assert.equal(existsSync(join(target, "ai_studio", "quality", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "core_harness", "profiling", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "tree.json")), true);
    assert.equal(existsSync(join(target, "ai_studio", "core_harness", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "architecture_map", "index.html")), true);
    assert.equal(existsSync(join(target, "ai_studio", "architecture_map", "validate_map.mjs")), true);
    assert.equal(existsSync(join(target, "ai_studio", "studio_shell", "server.mjs")), true);
    assert.equal(existsSync(join(target, "ai_studio", "bootstrap", "export_base.mjs")), true);
    assert.equal(existsSync(join(target, "ai_studio", "bootstrap", "new_game.mjs")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "viewer", "build_review.mjs")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "viewer", "viewer.js")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "storage", "license", "README.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "storage", "license", "restricted.mjs")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "storage", "license", "restricted_assets_guard.mjs")), true);
    assert.equal(existsSync(join(target, "tools", "assets", "restricted.mjs")), false);
    assert.equal(existsSync(join(target, "tools", "assets", "audit", "restricted_assets_guard.mjs")), false);
    assert.equal(existsSync(join(target, "tools", "asset_review")), false);
    assert.equal(existsSync(join(target, "tools", "bootstrap")), false);
    assert.equal(existsSync(join(target, "tools", "README.md")), true);
    assert.equal(existsSync(join(target, "tools", "requirements", "ai-pipeline-full.txt")), true);
    assert.equal(existsSync(join(target, ".codex", "skills", "nt-taskboard-manager", "SKILL.md")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "storage", "intake", "stage.mjs")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "storage", "intake", "accept.mjs")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "prep", "textures", "audit_tileable_texture.py")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "prep", "crop", "plan_prepared_crops_from_intake.py")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "prep", "cutout", "key_matte.py")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "prep", "lib", "atomic_io.py")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "prep", "conversion", "obj_to_glb.py")), true);
    assert.equal(existsSync(join(target, "ai_studio", "assets", "workflow", "art_jobs", "new_art_job.mjs")), true);
    assert.equal(existsSync(join(target, "tools", "assets")), false);

    const readme = readFileSync(join(target, "ai_studio", "taskboard", "README.md"), "utf8");
    assert.match(readme, /ai_studio\/taskboard\/task-store-reference\.md/);
    const guide = readFileSync(join(target, "ai_studio", "taskboard", "task-store-reference.md"), "utf8");
    assert.match(guide, /Task Store Reference/);
    const studio = readFileSync(join(target, "ai_studio", "README.md"), "utf8");
    assert.match(studio, /ai_studio\/core_harness\/workflow\/README\.md/);
    assert.match(studio, /ai_studio\/core_harness\/workflow\/orchestration\/README\.md/);
    assert.match(studio, /ai_studio\/quality\/README\.md/);
    assert.match(studio, /ai_studio\/core_harness\/profiling\/README\.md/);
    assert.match(studio, /ai_studio\/studio_shell\/start_site_windows\.ps1/);

    const docRefs = runInTarget(target, ["ai_studio/core_harness/validation/doc_reference_check.mjs"]);
    assert.equal(docRefs.status, 0, docRefs.stderr);
    assert.match(docRefs.stdout, /markdown file\(s\) checked/);

  } finally {
    cleanup(dir);
  }
});
