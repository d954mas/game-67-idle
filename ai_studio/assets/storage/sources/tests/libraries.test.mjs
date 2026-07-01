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

test("listRegisteredLibraries starts empty until a library is registered", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const libraries = listRegisteredLibraries(root);

  assert.deepEqual(libraries, []);
  assert.equal(defaultLibrarySourceRoot(root), "");
});

test("registerLibraryAssetSource creates and lists a library asset source", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const registered = registerLibraryAssetSource(root, { id: "studio-library", title: "Studio Library", assets: "./shared/assets/" });

  assert.equal(registered.assets, "shared/assets");
  assert.equal(libraryRegistryPath(root), "ai_studio/assets/storage/sources/libraries.json");
  assert.deepEqual(listRegisteredLibraries(root), [
    {
      id: "studio-library",
      title: "Studio Library",
      assets: "shared/assets",
      status: "active",
    },
  ]);

  const parsed = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "storage", "sources", "libraries.json"), "utf8"));
  assert.equal(parsed.libraries.length, 1);
});

test("defaultLibrarySourceRoot resolves the first active registered library", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  registerLibraryAssetSource(root, { id: "global-library", title: "Disabled", assets: "./old-library", status: "disabled" });
  registerLibraryAssetSource(root, { id: "active-library", title: "Active", assets: "./shared/assets" });

  assert.equal(defaultLibrarySourceRoot(root), join(root, "shared", "assets"));
});

test("defaultLibrarySourceRoot prefers registered global-library while active", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  registerLibraryAssetSource(root, { id: "active-library", title: "Active", assets: "./shared/assets" });
  registerLibraryAssetSource(root, { id: "global-library", title: "Global", assets: "./global/assets" });

  assert.equal(defaultLibrarySourceRoot(root), join(root, "global", "assets"));
});

test("resolveRegisteredSourcePath keeps absolute paths and resolves relative paths under root", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.equal(resolveRegisteredSourcePath(root, "templates/template/assets"), join(root, "templates", "template", "assets"));
  assert.equal(resolveRegisteredSourcePath(root, "C:/assets/library"), "C:/assets/library");
});

test("registerLibraryAssetSource rejects non-kebab ids", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(() => registerLibraryAssetSource(root, { id: "Bad Library" }), /lowercase kebab-case/);
});

test("registerLibraryAssetSource requires an explicit assets path", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(() => registerLibraryAssetSource(root, { id: "studio-library" }), /assets path is required/);
});
