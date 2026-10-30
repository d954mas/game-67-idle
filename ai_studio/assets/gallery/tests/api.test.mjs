import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdirSync, readFileSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { Readable } from "node:stream";
import { createAssetViewerApi, listAssetViewerSources, readTagProposals, resolveAssetViewerGalleryPath, safeResolve, saveAssetReview, selectSource } from "../api.mjs";

function tempRoot() {
  return mkTemp("asset-viewer-api-");
}

function mkTemp(prefix) {
  const root = join(tmpdir(), `${prefix}${process.pid}-${Date.now()}-${Math.random().toString(16).slice(2)}`);
  mkdirSync(root, { recursive: true });
  return root;
}

function writeJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function writePrivateGameMount(root, gameId = "secret-game") {
  writeGameIdentity(root, gameId, gameId, true);
}

function writeGameIdentity(root, gameId, title, privateGame = false) {
  const rel = privateGame ? join("games", "private", gameId) : join("games", gameId);
  writeJson(join(root, rel, "game.json"), { schema: "ai_studio.game.v1", id: gameId, title, storageNamespace: gameId });
  writeJson(join(root, rel, "dependencies.json"), { schema: "ai_studio.game.dependencies.v3", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" });
}

function privateGameFixture(root, gameId = "secret-game") {
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  mkdirSync(join(root, "games", "private", gameId, "assets"), { recursive: true });
  execFileSync("git", ["init"], { cwd: join(root, "games", "private", gameId), stdio: "ignore" });
  writePrivateGameMount(root, gameId);
}

test("safeResolve keeps paths inside the selected base", () => {
  const root = resolve("C:/repo");
  assert.equal(safeResolve(root, "game/assets"), resolve(root, "game/assets"));
  assert.equal(safeResolve(root, "games/demo/../demo/assets"), resolve(root, "games/demo/assets"));
  assert.equal(safeResolve(root, "../outside"), null);
  assert.equal(safeResolve(root, "C:/outside/assets"), null);
});

test("selectSource returns registered source or confined custom game source", () => {
  const root = resolve("C:/repo");
  const sources = [{
    id: "template",
    type: "template",
    label: "Template",
    path: resolve(root, "templates/template/assets"),
    available: true,
  }];

  assert.equal(selectSource(sources, { sourceId: "template" }, root).id, "template");

  const custom = selectSource(sources, { type: "game", path: "games/demo/assets" }, root);
  assert.equal(custom.id, "game:games_demo_assets");
  assert.equal(custom.type, "game");
  assert.equal(custom.path, resolve(root, "games/demo/assets"));

  assert.throws(() => selectSource(sources, { type: "game", path: "../outside/assets" }, root), /inside the repository/);
  assert.throws(() => selectSource(sources, { sourceId: "missing" }, root), /unknown asset source/);
});

test("listAssetViewerSources keeps global library with workspace template and game registries", async () => {
  const root = tempRoot();
  try {
    mkdirSync(join(root, "shared-library", "packs"), { recursive: true });
    mkdirSync(join(root, "templates", "template", "assets"), { recursive: true });
    mkdirSync(join(root, "games", "test-game", "assets"), { recursive: true });
    mkdirSync(join(root, "ai_studio", "assets", "sources"), { recursive: true });
    writeFileSync(join(root, "ai_studio", "assets", "sources", "libraries.json"), JSON.stringify({
      schema: "ai_studio.assets.libraries.v1",
      libraries: [{
        id: "global-library",
        title: "All Assets",
        assets: "shared-library",
        status: "active",
      }],
    }), "utf8");
    writeJson(join(root, "templates", "template", "template.json"), { schema: "ai_studio.template.v1", id: "template", title: "Template", storageNamespace: "template" });
    writeGameIdentity(root, "test-game", "Test Game");

    const { sources } = await listAssetViewerSources(root);

    assert.deepEqual(
      sources.map((source) => ({ id: source.id, type: source.type, available: source.available })),
      [
        { id: "global-library", type: "library", available: true },
        { id: "template", type: "template", available: true },
        { id: "game:test-game", type: "game", available: true },
      ],
    );
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("listAssetViewerSources hides private game mounts unless explicitly included", async () => {
  const root = tempRoot();
  try {
    mkdirSync(join(root, "shared-library", "packs"), { recursive: true });
    mkdirSync(join(root, "games", "public-game", "assets"), { recursive: true });
    privateGameFixture(root);
    writeGameIdentity(root, "public-game", "Public Game");

    const visible = await listAssetViewerSources(root);
    assert.deepEqual(visible.sources.filter((source) => source.type === "game").map((source) => source.id), ["game:public-game"]);

    const included = await listAssetViewerSources(root, { includePrivate: true });
    assert.deepEqual(included.sources.filter((source) => source.type === "game").map((source) => source.id), [
      "game:public-game",
      "game:secret-game",
    ]);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("selectSource resolves private games by id and blocks raw private paths", () => {
  const root = tempRoot();
  try {
    privateGameFixture(root);

    const selected = selectSource([], { sourceId: "game:secret-game" }, root);
    assert.equal(selected.id, "game:secret-game");
    assert.equal(selected.visibility, "private");
    assert.equal(selected.path, resolve(root, "games/private/secret-game/assets"));

    assert.throws(
      () => selectSource([], { type: "game", path: "games/private/secret-game/assets" }, root),
      /private game assets must be selected by game id/,
    );
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("resolveAssetViewerGalleryPath confines gallery, library, and repo routes", () => {
  const root = tempRoot();
  try {
    const galleryDir = join(root, "tmp", "ai-studio-asset-viewer", "global-library");
    const libraryRoot = join(root, "library");
    mkdirSync(galleryDir, { recursive: true });
    mkdirSync(join(libraryRoot, "packs", "kit"), { recursive: true });
    writeFileSync(join(galleryDir, ".asset-viewer-source.json"), JSON.stringify({ libraryRoot }), "utf8");

    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/viewer.js"),
      join(galleryDir, "viewer.js"),
    );
    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/packs/kit/model.glb"),
      join(libraryRoot, "packs", "kit", "model.glb"),
    );
    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/repo/templates/template/assets/cube.glb"),
      join(root, "templates", "template", "assets", "cube.glb"),
    );

    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/../secret.txt"), null);
    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/../../secret.txt"), null);
    assert.equal(resolveAssetViewerGalleryPath(root, "/not-gallery/global-library/viewer.js"), null);
    assert.equal(resolveAssetViewerGalleryPath(root, "/viewer/gallery/global-library/lib/packs/kit/model.glb"), null);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("resolveAssetViewerGalleryPath rejects physical link escapes while accepting external library roots", () => {
  const fixtureRoot = tempRoot();
  const root = join(fixtureRoot, "library-root");
  try {
    const galleryDir = join(root, "tmp", "ai-studio-asset-viewer", "global-library");
    const libraryRoot = join(root, "library");
    const outside = join(fixtureRoot, "outside");
    const canary = join(outside, "canary.txt");
    mkdirSync(root, { recursive: true });
    mkdirSync(galleryDir, { recursive: true });
    mkdirSync(libraryRoot, { recursive: true });
    mkdirSync(outside, { recursive: true });
    writeFileSync(canary, "CANARY", "utf8");
    writeFileSync(join(galleryDir, ".asset-viewer-source.json"), JSON.stringify({ libraryRoot }), "utf8");

    const linkType = process.platform === "win32" ? "junction" : "dir";
    symlinkSync(outside, join(galleryDir, "linked-outside"), linkType);
    symlinkSync(outside, join(libraryRoot, "linked-outside"), linkType);
    symlinkSync(outside, join(root, "linked-outside"), linkType);

    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/linked-outside/canary.txt"),
      null,
    );
    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/linked-outside/missing.txt"),
      null,
    );
    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/linked-outside/canary.txt"),
      null,
    );
    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/repo/linked-outside/canary.txt"),
      null,
    );

    const externalLibrary = mkTemp("asset-viewer-external-library-");
    try {
      const canonicalFile = join(externalLibrary, "packs", "kit", "model.glb");
      mkdirSync(dirname(canonicalFile), { recursive: true });
      writeFileSync(canonicalFile, "model", "utf8");
      const libraryAlias = join(root, "external-library-alias");
      symlinkSync(externalLibrary, libraryAlias, process.platform === "win32" ? "junction" : "dir");
      writeFileSync(join(galleryDir, ".asset-viewer-source.json"), JSON.stringify({ libraryRoot: libraryAlias }), "utf8");
      assert.equal(
        resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/packs/kit/model.glb"),
        join(libraryAlias, "packs", "kit", "model.glb"),
      );
    } finally {
      rmSync(externalLibrary, { recursive: true, force: true });
    }
  } finally {
    rmSync(fixtureRoot, { recursive: true, force: true });
  }
});

for (const linkedRoot of ["slug", "cache"]) {
  test(`resolveAssetViewerGalleryPath rejects an escaping ${linkedRoot} root`, () => {
    const fixtureRoot = tempRoot();
    try {
      const root = join(fixtureRoot, "studio");
      const outside = join(fixtureRoot, "outside");
      const cacheRoot = join(root, "tmp", "ai-studio-asset-viewer");
      const link = linkedRoot === "slug" ? join(cacheRoot, "global-library") : cacheRoot;
      const target = linkedRoot === "slug" ? outside : join(outside, "global-library");
      mkdirSync(dirname(link), { recursive: true });
      mkdirSync(target, { recursive: true });
      writeFileSync(join(target, "canary.txt"), "CANARY", "utf8");
      symlinkSync(outside, link, process.platform === "win32" ? "junction" : "dir");

      assert.equal(
        resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/canary.txt"),
        null,
      );
    } finally {
      rmSync(fixtureRoot, { recursive: true, force: true });
    }
  });
}

test("resolveAssetViewerGalleryPath ignores unusable gallery meta", () => {
  const root = tempRoot();
  try {
    const galleryDir = join(root, "tmp", "ai-studio-asset-viewer", "global-library");
    mkdirSync(galleryDir, { recursive: true });
    const metaPath = join(galleryDir, ".asset-viewer-source.json");

    writeFileSync(metaPath, "{not json", "utf8");
    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/model.glb"), null);

    writeFileSync(metaPath, JSON.stringify({ sourceId: "global-library" }), "utf8");
    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/model.glb"), null);

    writeFileSync(metaPath, JSON.stringify({ libraryRoot: join(root, "missing-library") }), "utf8");
    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/model.glb"), null);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("asset viewer shell does not require external scripts", () => {
  const html = readFileSync(new URL("../index.html", import.meta.url), "utf8");
  assert.doesNotMatch(html, /<script[^>]+src=["']https?:\/\//i);
});

function reviewLibrary(root) {
  const pack = join(root, "shared-library", "packs", "sfx");
  mkdirSync(join(pack, "files"), { recursive: true });
  writeFileSync(join(pack, "files", "click.wav"), "wav", "utf8");
  writeJson(join(pack, "pack.json"), { pack: "sfx", kind: "audio", license: "CC0", origin: "sourced", tags: ["sfx"] });
  writeFileSync(join(pack, "assets.jsonl"), `${JSON.stringify({ asset_id: "sfx__click", title: "UI 001", kind: "audio", resource: "files/click.wav", tags: ["ui", "auto-tagged"] })}\n`, "utf8");
  writeJson(join(root, "ai_studio", "assets", "sources", "libraries.json"), {
    schema: "ai_studio.assets.libraries.v1",
    libraries: [{ id: "global-library", title: "All Assets", assets: "shared-library", status: "active" }],
  });
  return join(pack, "assets.jsonl");
}

test("saveAssetReview writes the manifest row and the index without a rebuild", async () => {
  const root = tempRoot();
  try {
    const manifest = reviewLibrary(root);
    const saved = await saveAssetReview(root, { sourceId: "global-library", id: "sfx__click", title: "UI 001 (click)", tags: ["sfx", "ui", "Button Click", "ui", "reviewed"] });

    assert.deepEqual(saved.tags, ["sfx", "ui", "button-click", "reviewed"], "the index still merges pack tags");
    const row = JSON.parse(readFileSync(manifest, "utf8").trim());
    assert.equal(row.title, "UI 001 (click)");
    assert.deepEqual(row.tags, ["ui", "button-click", "reviewed"], "pack tags are not copied onto the row");
    assert.equal(row.resource, "files/click.wav", "fields the review does not own are kept");
    await assert.rejects(saveAssetReview(root, { sourceId: "global-library", id: "sfx__click", tags: "ui" }), /tags must be a list/);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("asset review only accepts same-origin JSON requests", async () => {
  const root = tempRoot();
  try {
    reviewLibrary(root);
    const api = createAssetViewerApi(root);
    const call = async (headers) => {
      const req = Readable.from([JSON.stringify({ sourceId: "global-library", id: "sfx__click", tags: ["ui"] })]);
      Object.assign(req, { method: "POST", headers: { host: "127.0.0.1:8765", ...headers } });
      let status = 0;
      let body = "";
      const res = { writeHead(code) { status = code; }, end(text) { body = text; } };
      await api(req, res, new URL("http://127.0.0.1:8765/api/asset-viewer/asset-review"));
      return { status, body: JSON.parse(body) };
    };

    assert.match((await call({ "content-type": "text/plain" })).body.error, /JSON request/);
    assert.match((await call({ "content-type": "application/json", origin: "https://evil.test" })).body.error, /same-origin/);
    assert.equal((await call({ "content-type": "application/json", origin: "http://127.0.0.1:8765" })).status, 200);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("readTagProposals merges every model run for a pack and ignores other packs", () => {
  const root = tempRoot();
  try {
    const dir = join(root, "tmp", "ai_studio", "assets", "audio_tags");
    writeJson(join(dir, "sfx.json"), { model: "clap", assets: [{ asset_id: "a", summary: "click", tags: ["button-click"] }] });
    writeJson(join(dir, "sfx.qwen.json"), { model: "qwen", assets: [{ asset_id: "a", description: "a soft UI click", tags: ["click"] }] });
    writeJson(join(dir, "sfx-other.json"), { model: "clap", assets: [{ asset_id: "a", tags: ["wrong"] }] });

    const proposals = readTagProposals(root, "sfx");

    assert.deepEqual(proposals.a.map((p) => [p.model, p.tags]), [["clap", ["button-click"]], ["qwen", ["click"]]]);
    assert.deepEqual(readTagProposals(root, "../sfx"), {});
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});
