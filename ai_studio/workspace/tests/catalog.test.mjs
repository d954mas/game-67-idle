import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import {
  CATALOG_SCHEMA,
  listWorkspaceMounts,
  readWorkspaceCatalog,
  upsertWorkspaceMount,
  writeGameDependencies,
} from "../catalog.mjs";

function writeJson(root, rel, value) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function identity(root, kind, id, title = id, storageNamespace = id) {
  const folder = kind === "game" ? "games" : "templates";
  writeJson(root, `${folder}/${id}/${kind}.json`, {
    schema: `ai_studio.${kind}.v1`, id, title, storageNamespace,
  });
  if (kind === "game") {
    writeJson(root, `${folder}/${id}/dependencies.json`, {
      schema: "ai_studio.game.dependencies.v1",
      engine: { source: "external/neotolis-engine", revision: "0000000000000000000000000000000000000000", compatibility: "exact" },
      features: [],
      compatibility: "Tested with the listed revisions.",
    });
  }
}

function mount(kind, id, visibility = "public") {
  return {
    kind,
    root: `${kind === "game" ? "games" : "templates"}/${id}`,
    visibility,
    gitRoot: visibility === "public" ? "" : `games/${id}`,
    commitPolicy: visibility === "public" ? "parent-public" : "nested-private",
    enabledStores: kind === "game" ? ["assets", "taskboard", "canvas", "evidence"] : ["assets"],
    aliases: [],
  };
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "workspace-catalog-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writeJson(root, "ai_studio/workspace/catalog.json", { schema: CATALOG_SCHEMA, mounts: [] });
  return root;
}

test("catalog derives identity and store ids from manifests, never mount rows", (t) => {
  const root = fixture(t);
  identity(root, "game", "public-game", "Public Game", "public-game-store");
  identity(root, "template", "template", "Template", "base-template");
  writeJson(root, "ai_studio/workspace/catalog.json", {
    schema: CATALOG_SCHEMA,
    mounts: [mount("game", "public-game"), mount("template", "template")],
  });
  const mounts = listWorkspaceMounts(root);
  assert.deepEqual(mounts.map((entry) => [entry.kind, entry.id, entry.title, entry.storeId]), [
    ["game", "public-game", "Public Game", "game:public-game-store"],
    ["template", "template", "Template", "template:base-template"],
  ]);
  assert.equal(Object.hasOwn(readWorkspaceCatalog(root).mounts[0], "id"), false);
});

test("local catalog uses the same schema and is excluded unless explicitly selected", (t) => {
  const root = fixture(t);
  identity(root, "game", "secret-game", "Secret Game");
  writeJson(root, "ai_studio/workspace/catalog.local.json", {
    schema: CATALOG_SCHEMA,
    mounts: [mount("game", "secret-game", "private")],
  });
  assert.deepEqual(listWorkspaceMounts(root), []);
  assert.deepEqual(
    listWorkspaceMounts(root, { includePrivate: true, skipPreflight: true }).map((entry) => entry.id),
    ["secret-game"],
  );
});

test("unknown schemas, identity fields in mounts, invalid roots, and missing manifests fail loudly", (t) => {
  const root = fixture(t);
  writeJson(root, "ai_studio/workspace/catalog.json", { schema: "unknown", mounts: [] });
  assert.throws(() => listWorkspaceMounts(root), /expected schema/);

  writeJson(root, "ai_studio/workspace/catalog.json", {
    schema: CATALOG_SCHEMA,
    mounts: [{ ...mount("game", "x"), id: "duplicated-identity" }],
  });
  assert.throws(() => listWorkspaceMounts(root), /unknown field 'id'/);

  writeJson(root, "ai_studio/workspace/catalog.json", {
    schema: CATALOG_SCHEMA,
    mounts: [{ ...mount("game", "x"), root: "../escape" }],
  });
  assert.throws(() => listWorkspaceMounts(root), /repo-relative/);

  writeJson(root, "ai_studio/workspace/catalog.json", {
    schema: CATALOG_SCHEMA,
    mounts: [mount("game", "missing")],
  });
  mkdirSync(join(root, "games", "missing"), { recursive: true });
  assert.throws(() => listWorkspaceMounts(root), /missing identity manifest/);
});

test("duplicate roots, derived ids, namespaces, aliases, and public-private collisions fail", (t) => {
  const root = fixture(t);
  identity(root, "game", "one", "One", "shared");
  identity(root, "game", "two", "Two", "shared");
  writeJson(root, "ai_studio/workspace/catalog.json", {
    schema: CATALOG_SCHEMA,
    mounts: [mount("game", "one"), mount("game", "two")],
  });
  assert.throws(() => listWorkspaceMounts(root), /duplicate storage namespace/);

  identity(root, "game", "two", "Two", "two");
  writeJson(root, "ai_studio/workspace/catalog.json", {
    schema: CATALOG_SCHEMA,
    mounts: [{ ...mount("game", "one"), aliases: ["same"] }],
  });
  writeJson(root, "ai_studio/workspace/catalog.local.json", {
    schema: CATALOG_SCHEMA,
    mounts: [{ ...mount("game", "two", "private"), aliases: ["same"] }],
  });
  assert.throws(
    () => listWorkspaceMounts(root, { includePrivate: true, skipPreflight: true }),
    /alias/,
  );

  writeJson(root, "ai_studio/workspace/catalog.local.json", {
    schema: CATALOG_SCHEMA,
    mounts: [mount("game", "one", "private")],
  });
  assert.throws(
    () => listWorkspaceMounts(root, { includePrivate: true, skipPreflight: true }),
    /duplicate root|duplicate derived id/,
  );
});

test("upsert writes mount facts only to the selected catalog", (t) => {
  const root = fixture(t);
  identity(root, "template", "new-template", "New Template");
  const resolved = upsertWorkspaceMount(root, mount("template", "new-template"));
  assert.equal(resolved.id, "new-template");
  const stored = readWorkspaceCatalog(root).mounts[0];
  assert.deepEqual(Object.keys(stored).sort(), [
    "aliases", "commitPolicy", "enabledStores", "gitRoot", "kind", "root", "visibility",
  ]);
});

test("upsert validates cross-catalog collisions before writing", (t) => {
  const root = fixture(t);
  identity(root, "game", "public-game", "Public", "shared");
  identity(root, "game", "private-game", "Private", "shared");
  writeJson(root, "ai_studio/workspace/catalog.json", {
    schema: CATALOG_SCHEMA,
    mounts: [mount("game", "public-game")],
  });

  assert.throws(() => upsertWorkspaceMount(root, {
    ...mount("game", "private-game", "private"),
    gitRoot: "games/private-game",
    commitPolicy: "nested-private",
  }, { local: true }), /duplicate storage namespace/);
  assert.equal(existsSync(join(root, "ai_studio", "workspace", "catalog.local.json")), false);
});

test("dependency writer validates before replacing an existing record", (t) => {
  const root = fixture(t);
  const valid = {
    engine: { source: "engine", revision: "0000000000000000000000000000000000000000", compatibility: "tested" },
    features: [],
    compatibility: "tested",
  };
  writeGameDependencies(root, "stable-game", valid);
  const path = join(root, "games", "stable-game", "dependencies.json");
  const before = readFileSync(path, "utf8");

  assert.throws(() => writeGameDependencies(root, "stable-game", {
    ...valid,
    engine: { ...valid.engine, revision: "placeholder" },
  }), /exact Git revision/);
  assert.equal(readFileSync(path, "utf8"), before);
});
