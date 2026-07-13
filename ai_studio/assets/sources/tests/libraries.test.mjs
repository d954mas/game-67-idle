import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
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

function repoRoot() {
  return fileURLToPath(new URL("../../../../", import.meta.url));
}

test("listRegisteredLibraries starts empty until a library is registered", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const libraries = listRegisteredLibraries(root);

  assert.deepEqual(libraries, []);
  assert.equal(defaultLibrarySourceRoot(root), "");
});

test("repository library registry keeps the active global library source", () => {
  const globalLibrary = listRegisteredLibraries(repoRoot()).find((library) => library.id === "global-library");

  assert.ok(globalLibrary, "global-library must stay registered for Asset Viewer");
  assert.equal(globalLibrary.title, "All Assets");
  assert.equal(globalLibrary.status, "active");
  assert.match(globalLibrary.assets, /ai_pipeline_assets$/);
});

test("registerLibraryAssetSource creates and lists a library asset source", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const registered = registerLibraryAssetSource(root, { id: "studio-library", title: "Studio Library", assets: "./shared/assets/" });

  assert.equal(registered.assets, "shared/assets");
  assert.equal(libraryRegistryPath(root), "ai_studio/assets/sources/libraries.json");
  assert.deepEqual(listRegisteredLibraries(root), [
    {
      id: "studio-library",
      title: "Studio Library",
      assets: "shared/assets",
      status: "active",
    },
  ]);

  const parsed = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "sources", "libraries.json"), "utf8"));
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

test("listRegisteredLibraries accepts UTF-8 BOM registry files", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const path = join(root, "ai_studio", "assets", "sources", "libraries.json");
  mkdirSync(join(root, "ai_studio", "assets", "sources"), { recursive: true });
  writeFileSync(path, `\uFEFF${JSON.stringify({
    schema: "ai_studio.assets.libraries.v1",
    libraries: [{ id: "studio-library", title: "Studio Library", assets: "shared/assets" }],
  })}`, "utf8");

  assert.deepEqual(listRegisteredLibraries(root), [{
    id: "studio-library",
    title: "Studio Library",
    assets: "shared/assets",
    status: "active",
  }]);
});
