// Items Viewer ops.mjs tests (T0316 phase 1).
// Run: node --test ai_studio/assets/items_viewer/tests/
//
// Fixtures: the LIVE template (templates/template/content/*, committed, stable, 6
// items) is the happy-path fixture (spec §6). Every failure branch uses a throwaway
// temp folder built from the template's own valid schema/state-schema (read-only
// copies), so a fixture is never a second guess at what "valid" looks like.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { localWorkspaceCatalogRelPath } from "../../../workspace/games.mjs";
import {
  getCatalogView,
  listCatalogs,
  loadCatalogView,
  parseJsonOrThrow,
  routeIssues,
  runItemsOpsRaw,
} from "../ops.mjs";

const REPO_ROOT = fileURLToPath(new URL("../../../..", import.meta.url)).replace(/[\\/]$/, "");

const FIELD_SCHEMA = JSON.parse(readFileSync(join(REPO_ROOT, "templates", "template", "content", "item_fields.schema.json"), "utf8"));
const STATE_SCHEMA_V2 = {
  ...JSON.parse(readFileSync(join(REPO_ROOT, "templates", "template", "state", "items.schema.json"), "utf8")),
  version: 2, // lock-workflow fixtures need version >= MIN_REMOVED_FRAGMENT_VERSION (2)
};

function tempFixtureDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "items-viewer-ops-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function writeJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify(value, null, 2), "utf8");
}

function writePrivateGameMount(root, gameId = "secret-game") {
  writeGameIdentity(root, gameId);
  writeJson(join(root, "ai_studio", "workspace", "catalog.json"), { schema: "ai_studio.workspace.catalog.v1", mounts: [] });
  writeJson(join(root, "ai_studio", "workspace", "catalog.local.json"), { schema: "ai_studio.workspace.catalog.v1", mounts: [
    { kind: "game", root: `games/${gameId}`, visibility: "private", gitRoot: `games/${gameId}`, commitPolicy: "nested-private", enabledStores: ["assets", "taskboard", "canvas", "evidence"], aliases: [] },
  ] });
}

function writeGameIdentity(root, gameId) {
  writeJson(join(root, "games", gameId, "game.json"), { schema: "ai_studio.game.v1", id: gameId, title: gameId, storageNamespace: gameId });
  writeJson(join(root, "games", gameId, "dependencies.json"), { schema: "ai_studio.game.dependencies.v2", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" });
}

function writeNeutralPublicWorkspace(root, gameId = "fixture-game") {
  writeGameIdentity(root, gameId);
  writeJson(join(root, "templates", "fixture-template", "template.json"), {
    schema: "ai_studio.template.v1",
    id: "fixture-template",
    title: "Fixture Template",
    storageNamespace: "fixture-template",
  });
  writeJson(join(root, "ai_studio", "workspace", "catalog.json"), {
    schema: "ai_studio.workspace.catalog.v1",
    mounts: [
      { kind: "template", root: "templates/fixture-template", visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] },
      { kind: "game", root: `games/${gameId}`, visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] },
    ],
  });
}

function privateGameFixture(root, gameId = "secret-game") {
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), `${localWorkspaceCatalogRelPath()}\n`, "utf8");
  writeFileSync(join(root, ".git", "info", "exclude"), `games/${gameId}/\n`, "utf8");
  mkdirSync(join(root, "games", gameId, "content"), { recursive: true });
  execFileSync("git", ["init"], { cwd: join(root, "games", gameId), stdio: "ignore" });
  writePrivateGameMount(root, gameId);
}

function makeItem(id, overrides = {}) {
  return {
    id,
    created: "2026-07-08",
    display_name: id,
    icon_asset_id: `icons/${id}`, // T0316: slash-form contract (atlas/region), not the old dotted icon.<id>
    kind: "material",
    tags: [],
    base_value: 1,
    stack: 10,
    ...overrides,
  };
}

// Minimal valid catalog + field schema + state schema, written under `dir`. `items`
// defaults to two catalog items ("fx.a", "fx.b"); callers add content/items.lock.json
// (or not) to exercise the conditional --baseline branch.
function writeValidFixture(dir, { namespace = "fx", items, stateSchema = STATE_SCHEMA_V2 } = {}) {
  const catalogItems = items || [makeItem("fx.a"), makeItem("fx.b")];
  writeJson(join(dir, "content", "items.json"), {
    schema: "game_seed.items_catalog",
    namespace,
    item_kinds: [{ id: "material", label: "Material" }],
    containers: [],
    items: catalogItems,
  });
  writeJson(join(dir, "content", "item_fields.schema.json"), FIELD_SCHEMA);
  writeJson(join(dir, "state", "items.schema.json"), stateSchema);
}

test("listCatalogs merges neutral template and game fixtures with hasItems flags", (t) => {
  const root = tempFixtureDir(t);
  writeNeutralPublicWorkspace(root);
  writeJson(join(root, "templates", "fixture-template", "content", "items.json"), {});

  const { catalogs } = listCatalogs(root);
  assert.deepEqual(catalogs.map((catalog) => catalog.id), ["template:fixture-template", "game:fixture-game"]);
  assert.equal(catalogs[0].kind, "template");
  assert.equal(catalogs[0].hasItems, true);
  assert.equal(catalogs[0].folder, "templates/fixture-template");
  assert.equal(catalogs[1].kind, "game");
  assert.equal(catalogs[1].hasItems, false);
});

test("listCatalogs hides private game mounts unless explicitly included", () => {
  const root = mkdtempSync(join(tmpdir(), "items-viewer-private-catalogs-"));
  try {
    privateGameFixture(root);
    writeGameIdentity(root, "public-game");
    writeJson(join(root, "ai_studio", "workspace", "catalog.json"), { schema: "ai_studio.workspace.catalog.v1", mounts: [
      { kind: "game", root: "games/public-game", visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] },
    ] });
    mkdirSync(join(root, "games", "public-game", "content"), { recursive: true });
    writeFileSync(join(root, "games", "public-game", "content", "items.json"), "{}", "utf8");
    writeFileSync(join(root, "games", "secret-game", "content", "items.json"), "{}", "utf8");

    assert.deepEqual(listCatalogs(root).catalogs.map((catalog) => catalog.id), ["game:public-game"]);
    assert.deepEqual(listCatalogs(root, { includePrivate: true }).catalogs.map((catalog) => catalog.id), [
      "game:public-game",
      "game:secret-game",
    ]);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("getCatalogView happy path: live template renders 6 items, schema, containers, validate.ok", async () => {
  const view = await getCatalogView(REPO_ROOT, "template:template");
  assert.ok(view, "template:template must resolve");
  assert.equal(view.meta.hasItems, true);
  assert.equal(view.meta.kind, "template");
  assert.equal(view.namespace, "tmpl");
  assert.equal(view.items.length, 6);
  assert.equal(view.containers.length, 2);
  assert.equal(view.item_kinds.length, 4);
  assert.ok(view.schema && view.schema.core && view.schema.core.display_name, "schema --json parsed");
  assert.equal(view.content_error, undefined);

  // validate --json parsed with the real lock passed as --baseline (it exists).
  assert.equal(view.validate.available, true);
  assert.equal(view.validate.ok, true);
  assert.deepEqual(view.validate.errors, []);

  // Items-core stack contract: regression guard on both the schema shape
  // and the `list --json` output shape -- `stack` must be a raw int (0/1/N), NOT
  // the old derived {stackable,max_stack,unlimited} object (which would render as
  // "[object Object]" on every card under schema v2's i64 type).
  assert.equal(view.schema.core.stack.type, "i64");
  const sword = view.items.find((item) => item.id === "tmpl.sword");
  assert.ok(sword, "tmpl.sword must be in the live template");
  assert.equal(sword.stack, 1);
  const gold = view.items.find((item) => item.id === "tmpl.gold");
  assert.ok(gold, "tmpl.gold must be in the live template");
  assert.equal(gold.stack, 0);

  // lock status_by_id: every shipped tmpl.* id maps to "shipped" (all 6 are in the
  // committed items.lock.json def_ids), removed is empty (nothing removed today).
  for (const item of view.items) {
    assert.equal(view.lock.status_by_id[item.id], "shipped", `${item.id} should be shipped`);
  }
  assert.deepEqual(view.lock.removed, {});
});

test("getCatalogView returns null for an id that matches neither registry", async () => {
  assert.equal(await getCatalogView(REPO_ROOT, "template:does-not-exist"), null);
  assert.equal(await getCatalogView(REPO_ROOT, "game:does-not-exist"), null);
  assert.equal(await getCatalogView(REPO_ROOT, "bogus:template"), null);
  assert.equal(await getCatalogView(REPO_ROOT, "no-colon-at-all"), null);
});

test("getCatalogView: a registered game with no content/items.json -> hasItems:false, honest empty state", async (t) => {
  const root = tempFixtureDir(t);
  writeNeutralPublicWorkspace(root);
  const view = await getCatalogView(root, "game:fixture-game");
  assert.ok(view);
  assert.equal(view.meta.hasItems, false);
  assert.deepEqual(view.items, []);
  assert.equal(view.content_error, undefined, "no content_error for the decision-(д) empty state, only for broken data");
  assert.equal(view.validate.available, false);
  assert.equal(typeof view.validate.reason, "string");
  assert.ok(view.validate.reason.length > 0);
});

test("loadCatalogView: a folder with no content/ dir at all -> hasItems:false (decoupled from any registry)", async (t) => {
  const dir = tempFixtureDir(t);
  const view = await loadCatalogView(REPO_ROOT, dir, { id: "x:y", kind: "game", title: "Y", folder: "x/y" });
  assert.equal(view.meta.hasItems, false);
  assert.deepEqual(view.items, []);
  assert.equal(view.schema, null);
  assert.deepEqual(view.lock, { status_by_id: {}, removed: {} });
  assert.equal(view.validate.available, false);
  assert.deepEqual(view.icons.regions, {});
  assert.match(view.icons.reason, /pack not built/);
});

test("lock status_by_id: shipped (def_ids) / draft (neither) / removed (restored-but-still-in-removed) + homeless removed entries", async (t) => {
  const dir = tempFixtureDir(t);
  // fx.a is shipped (in def_ids); fx.b is a plain draft (in neither section); fx.c is
  // present in the catalog AND in lock.removed -- the legal "removed-def-restored"
  // restoration edge case (spec §3: "removed if key in removed", checked after
  // "shipped if in def_ids"); fx.zzz is a normal removal with NO catalog item.
  writeValidFixture(dir, { items: [makeItem("fx.a"), makeItem("fx.b"), makeItem("fx.c")] });
  writeJson(join(dir, "content", "items.lock.json"), {
    schema: "game_seed.items_lock",
    schema_version: 2,
    def_ids: ["fx.a"],
    removed: {
      "fx.c": { fragment_version: 2, note: "restored after removal" },
      "fx.zzz": { fragment_version: 2, note: "never coming back" },
    },
  });

  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.deepEqual(view.lock.status_by_id, { "fx.a": "shipped", "fx.b": "draft", "fx.c": "removed" });
  assert.deepEqual(Object.keys(view.lock.removed).sort(), ["fx.c", "fx.zzz"]);
  // fx.zzz never appears in status_by_id -- it has no catalog item (spec §4: "each
  // Removed entry has NO catalog item, so it can never be a card").
  assert.equal(view.lock.status_by_id["fx.zzz"], undefined);
});

test("routeIssues: an issue attaches to a card IFF its id is present in the catalog's current items", () => {
  const itemIds = ["fx.a", "fx.b"];
  const issues = [
    { rule: "created-missing", id: "fx.a", field: "created", msg: "a" },
    { rule: "created-missing", id: "fx.a", field: "created", msg: "a again, same item" },
    { rule: "generator-check", id: null, field: null, msg: "catalog-level, no card" },
    { rule: "removed-without-reaction", id: "fx.deleted", field: null, msg: "shipped id vanished from the catalog" },
  ];

  const { byItem, unrouted } = routeIssues(itemIds, issues);
  assert.equal(byItem.size, 1);
  assert.equal(byItem.get("fx.a").length, 2, "both fx.a issues route to the same card bucket");
  assert.equal(byItem.has("fx.deleted"), false, "a deleted id has no card to live on");
  assert.equal(unrouted.length, 2);
  assert.deepEqual(unrouted.map((i) => i.rule).sort(), ["generator-check", "removed-without-reaction"]);
});

test("validate exit 1 is parsed from stdout as validate.ok:false, not thrown", async (t) => {
  const dir = tempFixtureDir(t);
  // Drop the required 'created' field -> a real validation FAIL (exit 1), not a crash.
  const broken = makeItem("fx.a");
  delete broken.created;
  writeValidFixture(dir, { items: [broken, makeItem("fx.b")] });

  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.equal(view.content_error, undefined, "the catalog still parses fine for list/schema -- only validate fails");
  assert.equal(view.items.length, 2, "list --json still renders both items despite the validate FAIL");
  assert.equal(view.validate.available, true);
  assert.equal(view.validate.ok, false);
  assert.ok(view.validate.errors.some((e) => e.rule === "created-missing" && e.id === "fx.a"));
});

test("validate exit 2 (broken state/items.schema.json) -> validate.available:false, content_error absent", async (t) => {
  const dir = tempFixtureDir(t);
  writeValidFixture(dir);
  // Overwrite the state schema with invalid JSON after writeValidFixture wrote a good one.
  writeFileSync(join(dir, "state", "items.schema.json"), "{ not valid json", "utf8");

  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.equal(view.content_error, undefined, "list/schema never read state/items.schema.json, so they still succeed");
  assert.equal(view.items.length, 2);
  assert.ok(view.schema, "schema --json still parsed");
  assert.equal(view.validate.available, false);
  assert.equal(view.validate.ok, null);
  assert.equal(typeof view.validate.reason, "string");
  assert.ok(view.validate.reason.length > 0);
});

test("list exit 2 (malformed items.json) -> content_error source:catalog, schema still renders", async (t) => {
  const dir = tempFixtureDir(t);
  writeValidFixture(dir);
  writeFileSync(join(dir, "content", "items.json"), "{ not valid json", "utf8");

  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.ok(view.content_error);
  assert.equal(view.content_error.source, "catalog");
  assert.ok(view.content_error.stderr.length > 0);
  assert.deepEqual(view.items, [], "nothing parsed from the broken catalog");
  assert.ok(view.schema, "schema --json is independent of the catalog and still parses");
  assert.equal(view.validate.available, false, "validate reads the same broken catalog and also degrades");
});

test("schema exit 2 (malformed item_fields.schema.json) -> content_error source:schema, items still render", async (t) => {
  const dir = tempFixtureDir(t);
  writeValidFixture(dir);
  writeFileSync(join(dir, "content", "item_fields.schema.json"), "{ not valid json", "utf8");

  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.ok(view.content_error);
  assert.equal(view.content_error.source, "schema");
  assert.equal(view.items.length, 2, "list --json never reads the field schema, so items still render");
  assert.equal(view.schema, null);
  assert.equal(view.validate.available, false, "validate reads the same broken schema and also degrades");
});

test("conditional --baseline: no items.lock.json -> flag omitted, rename-guard-skipped warning", async (t) => {
  const dir = tempFixtureDir(t);
  writeValidFixture(dir); // no content/items.lock.json written

  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.equal(view.validate.available, true);
  assert.equal(view.validate.ok, true);
  assert.ok(
    view.validate.warnings.some((w) => w.rule === "rename-guard-skipped"),
    "an absent default baseline must warn, not silently skip",
  );
});

test("conditional --baseline: a present items.lock.json is passed explicitly -> no rename-guard-skipped warning", async (t) => {
  const dir = tempFixtureDir(t);
  writeValidFixture(dir);
  writeJson(join(dir, "content", "items.lock.json"), {
    schema: "game_seed.items_lock",
    schema_version: 2,
    def_ids: ["fx.a", "fx.b"],
    removed: {},
  });

  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.equal(view.validate.available, true);
  assert.equal(view.validate.ok, true);
  assert.ok(
    !view.validate.warnings.some((w) => w.rule === "rename-guard-skipped"),
    "a real baseline was passed -- lock checks ran, no skip warning",
  );
  assert.deepEqual(view.lock.status_by_id, { "fx.a": "shipped", "fx.b": "shipped" });
});

test("runItemsOpsRaw rejects (throws) when the python interpreter cannot be spawned (ENOENT)", async () => {
  await assert.rejects(
    () => runItemsOpsRaw(REPO_ROOT, ["list", "--catalog", "x", "--json"], { bin: "py-does-not-exist-t0316-probe" }),
    /could not be spawned/,
  );
});

test("parseJsonOrThrow throws on unparseable text (the 'unparseable stdout on a successful exit' branch)", () => {
  assert.throws(() => parseJsonOrThrow("not json at all", "list"), /unparseable JSON/);
  assert.deepEqual(parseJsonOrThrow('{"ok":true}', "list"), { ok: true });
});
