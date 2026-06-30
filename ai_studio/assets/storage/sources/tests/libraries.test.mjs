import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  defaultLibrarySourceRoot,
  libraryRegistryPath,
  listRegisteredLibraries,
  registerLibraryAssetSource,
  resolveRegisteredSourcePath,
} from "../libraries.mjs";

function tempRoot() {
  return mkdtempSync(join(tmpdir(), "ai-studio-libraries-registry-"));
}

test("listRegisteredLibraries falls back to the shared asset library", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const libraries = listRegisteredLibraries(root);

  assert.equal(libraries.length, 1);
  assert.equal(libraries[0].id, "global-library");
  assert.equal(libraries[0].title, "All Assets");
  assert.equal(libraries[0].status, "active");
  assert.match(libraries[0].assets, /ai_pipeline_assets$/);
});

test("registerLibraryAssetSource creates and lists a library asset source", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const registered = registerLibraryAssetSource(root, { id: "studio-library", title: "Studio Library", assets: "./shared/assets/" });

  assert.equal(registered.assets, "shared/assets");
  assert.equal(libraryRegistryPath(root), "ai_studio/assets/storage/sources/libraries.json");
  assert.deepEqual(listRegisteredLibraries(root), [
    {
      id: "global-library",
      title: "All Assets",
      assets: "C:/Users/ROG/YandexDisk/gamedev/assets/ai_pipeline_assets",
      status: "active",
    },
    {
      id: "studio-library",
      title: "Studio Library",
      assets: "shared/assets",
      status: "active",
    },
  ]);

  const parsed = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "storage", "sources", "libraries.json"), "utf8"));
  assert.equal(parsed.libraries.length, 2);
});

test("defaultLibrarySourceRoot resolves the first active registered library", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  registerLibraryAssetSource(root, { id: "global-library", title: "Disabled", assets: "./old-library", status: "disabled" });
  registerLibraryAssetSource(root, { id: "active-library", title: "Active", assets: "./shared/assets" });

  assert.equal(defaultLibrarySourceRoot(root), join(root, "shared", "assets"));
});

test("defaultLibrarySourceRoot prefers global-library while active", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  registerLibraryAssetSource(root, { id: "active-library", title: "Active", assets: "./shared/assets" });

  assert.match(defaultLibrarySourceRoot(root), /ai_pipeline_assets$/);
});

test("resolveRegisteredSourcePath keeps absolute paths and resolves relative paths under root", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.equal(resolveRegisteredSourcePath(root, "template/assets"), join(root, "template", "assets"));
  assert.equal(resolveRegisteredSourcePath(root, "C:/assets/library"), "C:/assets/library");
});

test("registerLibraryAssetSource rejects non-kebab ids", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(() => registerLibraryAssetSource(root, { id: "Bad Library" }), /lowercase kebab-case/);
});
