// Studio config reader tests. Run:
//   node --test ai_studio/assets/canvas/tests/config.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { canvasHistoryDepth, canvasLocalCacheRoot, canvasProjectsRoot } from "../config.mjs";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-config-"));
  mkdirSync(join(dir, "ai_studio"), { recursive: true });
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function writeConfig(root, name, data) {
  writeFileSync(join(root, "ai_studio", name), `${JSON.stringify(data)}\n`, "utf8");
}

test("canvasProjectsRoot uses the local override over committed main", (t) => {
  delete process.env.CANVAS_PROJECTS_ROOT;
  const root = tempRoot(t);
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1", canvasProjectsRoot: "C:/main/root" });
  writeConfig(root, "studio.config.local.json", { canvasProjectsRoot: "C:/local/override" });

  assert.equal(canvasProjectsRoot(root), resolve("C:/local/override"));
});

test("canvasProjectsRoot uses committed main when no local override exists", (t) => {
  delete process.env.CANVAS_PROJECTS_ROOT;
  const root = tempRoot(t);
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1", canvasProjectsRoot: "C:/main/only" });

  assert.equal(canvasProjectsRoot(root), resolve("C:/main/only"));
});

test("CANVAS_PROJECTS_ROOT env overrides config and needs no config file", (t) => {
  const root = tempRoot(t);
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = "C:/env/override";
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
  });

  assert.equal(canvasProjectsRoot(root), resolve("C:/env/override"));
});

test("canvasHistoryDepth: env override > config value > default 200", (t) => {
  const previous = process.env.CANVAS_HISTORY_DEPTH;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_HISTORY_DEPTH;
    else process.env.CANVAS_HISTORY_DEPTH = previous;
  });
  delete process.env.CANVAS_HISTORY_DEPTH;

  const root = tempRoot(t);
  // Default when the key is absent from config.
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1", canvasProjectsRoot: "C:/x" });
  assert.equal(canvasHistoryDepth(root), 200);
  // Missing config entirely still resolves to the default (read-only history-safe).
  const bare = tempRoot(t);
  assert.equal(canvasHistoryDepth(bare), 200);
  // Committed config value is honored.
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1", canvasProjectsRoot: "C:/x", canvasHistoryDepth: 42 });
  assert.equal(canvasHistoryDepth(root), 42);
  // The env override wins over config.
  process.env.CANVAS_HISTORY_DEPTH = "7";
  assert.equal(canvasHistoryDepth(root), 7);
});

test("Canvas optional config defaults do not hide malformed config", (t) => {
  const saved = {
    CANVAS_HISTORY_DEPTH: process.env.CANVAS_HISTORY_DEPTH,
    CANVAS_PROJECTS_ROOT: process.env.CANVAS_PROJECTS_ROOT,
    CANVAS_LOCAL_CACHE_ROOT: process.env.CANVAS_LOCAL_CACHE_ROOT,
  };
  t.after(() => {
    for (const [key, value] of Object.entries(saved)) {
      if (value === undefined) delete process.env[key];
      else process.env[key] = value;
    }
  });
  delete process.env.CANVAS_HISTORY_DEPTH;
  delete process.env.CANVAS_PROJECTS_ROOT;
  delete process.env.CANVAS_LOCAL_CACHE_ROOT;

  const root = tempRoot(t);
  writeFileSync(join(root, "ai_studio", "studio.config.json"), "{bad json\n", "utf8");

  assert.throws(() => canvasHistoryDepth(root), /invalid studio config JSON/);
  assert.throws(() => canvasLocalCacheRoot(root), /invalid studio config JSON/);
});

test("canvasLocalCacheRoot: env override > CANVAS_PROJECTS_ROOT redirect > config value > repo-local default (T0259)", (t) => {
  const saved = {
    CANVAS_PROJECTS_ROOT: process.env.CANVAS_PROJECTS_ROOT,
    CANVAS_LOCAL_CACHE_ROOT: process.env.CANVAS_LOCAL_CACHE_ROOT,
  };
  t.after(() => {
    for (const [key, value] of Object.entries(saved)) {
      if (value === undefined) delete process.env[key];
      else process.env[key] = value;
    }
  });
  const root = tempRoot(t);
  writeConfig(root, "studio.config.json", {
    schema: "ai_studio.studio_config.v1",
    canvasProjectsRoot: "C:/x",
    canvasLocalCacheRoot: "C:/configured/cache",
  });

  // 1. Explicit CANVAS_LOCAL_CACHE_ROOT wins over everything.
  delete process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_LOCAL_CACHE_ROOT = "C:/env/cache";
  assert.equal(canvasLocalCacheRoot(root), resolve("C:/env/cache"));

  // 2. No explicit cache override, but the projects root is redirected via CANVAS_PROJECTS_ROOT
  //    (test/one-off) → the cache follows into the OS temp area, never a repo/fake root.
  delete process.env.CANVAS_LOCAL_CACHE_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = "C:/some/temp/projects";
  assert.equal(canvasLocalCacheRoot(root), join(tmpdir(), "ai_studio_canvas_cache"));

  // 3. Production (neither env set): the committed config value, resolved absolute.
  delete process.env.CANVAS_PROJECTS_ROOT;
  assert.equal(canvasLocalCacheRoot(root), resolve("C:/configured/cache"));

  // 4. Production with no config key: repo-local default under <root>/tmp/canvas_cache.
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1", canvasProjectsRoot: "C:/x" });
  assert.equal(canvasLocalCacheRoot(root), resolve(join(root, "tmp", "canvas_cache")));
});

test("canvasLocalCacheRoot: committed local override wins over main (production)", (t) => {
  const saved = {
    CANVAS_PROJECTS_ROOT: process.env.CANVAS_PROJECTS_ROOT,
    CANVAS_LOCAL_CACHE_ROOT: process.env.CANVAS_LOCAL_CACHE_ROOT,
  };
  t.after(() => {
    for (const [key, value] of Object.entries(saved)) {
      if (value === undefined) delete process.env[key];
      else process.env[key] = value;
    }
  });
  delete process.env.CANVAS_PROJECTS_ROOT;
  delete process.env.CANVAS_LOCAL_CACHE_ROOT;
  const root = tempRoot(t);
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1", canvasProjectsRoot: "C:/x", canvasLocalCacheRoot: "C:/main/cache" });
  writeConfig(root, "studio.config.local.json", { canvasLocalCacheRoot: "C:/local/cache" });
  assert.equal(canvasLocalCacheRoot(root), resolve("C:/local/cache"));
});
