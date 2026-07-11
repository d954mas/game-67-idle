// Items Viewer — read-only catalog ops (T0316 phase 1).
//
// Pure logic, no HTTP. Merges the template/game registries, spawns items_ops.py
// (list/schema/validate) through the strict studio.config root-venv resolver,
// with an absolute script path (cwd = repo root, every path ABSOLUTE — never items_ops.py's own
// script-relative argparse defaults, which point into features/items-core/content/,
// nonexistent after T0337), reads items.lock.json directly (a separate artifact, never
// a second parser of items.json — decision (а), docs/build_spec_phase1_2026-07-08.md
// §3/§8), and folds everything into one view object api.mjs marshals over HTTP. See
// the module README for the full contract and the spec for the design rationale.
import { execFile } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { listRegisteredTemplates } from "../backlog/storage/sources/templates.mjs";
import { listGameMounts } from "../../workspace/games.mjs";
import { studioPythonPath } from "../../core_harness/tool_lib/studio_config.mjs";
import { buildIconPreview } from "./icon_preview.mjs";

// The ordinary Studio interpreter is resolved once; no system launcher is used.
// NOT the mechanism the rest of the pipeline uses (node asset tools spawn an absolute
// venv python from studio config; ctest uses ${Python3_EXECUTABLE}) — acceptable for a
// local single-user studio tool (spec §8's honest caveat). `bin` is an override seam
// (below) so a test can force a real spawn failure without touching the real
// interpreter; production callers never pass it.
const MAX_BUFFER = 1024 * 1024; // 1MB — items_ops --json payloads are tiny (spec §8 reassurance)

function itemsOpsScriptPath(root) {
  return join(root, "features", "items-core", "scripts", "items_ops.py");
}

// Spawns `items_ops.py <args>` with cwd = repo root. Resolves with {code, stdout,
// stderr} for ANY controlled process exit (0/1/2/...); rejects only on a genuine spawn
// failure (interpreter missing — ENOENT), which callers turn into a loud 500
// (spec §3 "TOOL-FAILURE — the ONLY 500").
export function runItemsOpsRaw(root, args, { bin } = {}) {
  const scriptPath = itemsOpsScriptPath(root);
  const python = bin || studioPythonPath(root);
  return new Promise((resolveRun, rejectRun) => {
    execFile(python, [scriptPath, ...args], { cwd: root, maxBuffer: MAX_BUFFER }, (error, stdout, stderr) => {
      if (error && typeof error.code !== "number") {
        rejectRun(new Error(`items_ops.py could not be spawned (${python}): ${error.message}`));
        return;
      }
      resolveRun({ code: error ? error.code : 0, stdout: stdout ?? "", stderr: stderr ?? "" });
    });
  });
}

// Exported so a test can hit the "unparseable stdout on an otherwise-successful exit"
// throw directly (items_ops.py itself always emits valid JSON on a real exit 0/1, so
// this branch is otherwise unreachable through a fixture — spec §6 gate).
export function parseJsonOrThrow(text, what) {
  try {
    return JSON.parse(text);
  } catch (err) {
    throw new Error(`items_ops.py ${what} printed unparseable JSON on an otherwise-successful exit: ${err.message}`);
  }
}

// list: exit 0 -> parsed record; exit 2 -> CONTENT-INVALID (a broken items.json), not a
// crash — surfaced as content_error by the caller, never thrown. Any other exit is an
// unexpected tool failure (500).
async function runList(root, catalogPath) {
  const { code, stdout, stderr } = await runItemsOpsRaw(root, ["list", "--catalog", catalogPath, "--json"]);
  if (code === 0) return { ok: true, data: parseJsonOrThrow(stdout, "list") };
  if (code === 2) return { ok: false, source: "catalog", stderr: stderr.trim() };
  throw new Error(`items_ops.py list exited ${code} unexpectedly: ${stderr.trim()}`);
}

// schema: same three-way shape as list, over item_fields.schema.json specifically.
// `list` never reads the field schema (items_ops.py cmd_list), so a broken schema
// file degrades the SCHEMA half only — items still render (spec §4 degradation).
async function runSchema(root, schemaPath) {
  const { code, stdout, stderr } = await runItemsOpsRaw(root, ["schema", "--schema", schemaPath, "--json"]);
  if (code === 0) return { ok: true, data: parseJsonOrThrow(stdout, "schema") };
  if (code === 2) return { ok: false, source: "schema", stderr: stderr.trim() };
  throw new Error(`items_ops.py schema exited ${code} unexpectedly: ${stderr.trim()}`);
}

// validate: exit 0/1 both print the full {ok,errors,warnings} JSON on stdout — exit 1
// is a validation FAIL, not a crash, so it is parsed the SAME way as exit 0 (spec §3).
// Exit 2 is narrower than list/schema's content_error: a missing/broken --state-schema,
// or an explicit --baseline miss — degrades to {available:false, reason}, never
// content_error (items/schema can still render). Any other exit is unexpected (500).
async function runValidate(root, { catalogPath, schemaPath, stateSchemaPath, srcDirPath, baselinePath }) {
  const args = ["validate", "--catalog", catalogPath, "--schema", schemaPath, "--state-schema", stateSchemaPath, "--src-dir", srcDirPath, "--json"];
  // --baseline is passed ONLY when the lock file actually exists; otherwise the flag is
  // OMITTED so validate still runs (its own default points at a path under
  // features/items-core/content/, nonexistent after T0337) and emits its own
  // rename-guard-skipped warning — never always-omit, a clean template DOES ship a
  // lock and always-omitting would fake that warning (spec §3).
  if (baselinePath) args.push("--baseline", baselinePath);
  const { code, stdout, stderr } = await runItemsOpsRaw(root, args);
  if (code === 0 || code === 1) {
    const parsed = parseJsonOrThrow(stdout, "validate");
    return { available: true, ok: parsed.ok, errors: parsed.errors || [], warnings: parsed.warnings || [] };
  }
  if (code === 2) return { available: false, ok: null, errors: [], warnings: [], reason: stderr.trim() };
  throw new Error(`items_ops.py validate exited ${code} unexpectedly: ${stderr.trim()}`);
}

// items.lock.json is read directly (plain JSON, not through items_ops.py) — a separate
// artifact, not a second model of items (decision (а)). A malformed lock degrades the
// lock DISPLAY only (empty status/removed) rather than a 500 for the whole catalog;
// items_ops.py's own --baseline read of the SAME file already reports the real reason
// via validate.reason (validate_baseline_shape, exit 2).
function readLockRaw(lockPath) {
  if (!existsSync(lockPath)) return null;
  try {
    const raw = readFileSync(lockPath, "utf8");
    // Strip a leading UTF-8 BOM (0xFEFF) via numeric code point, not a regex escape —
    // matches templates.mjs/games.mjs intent without embedding an invisible char literal.
    const text = raw.charCodeAt(0) === 0xfeff ? raw.slice(1) : raw;
    const parsed = JSON.parse(text);
    return {
      defIds: Array.isArray(parsed.def_ids) ? parsed.def_ids : [],
      removed: parsed.removed && typeof parsed.removed === "object" ? parsed.removed : {},
    };
  } catch {
    return null;
  }
}

// shipped if id in def_ids, removed if key in removed, else draft (spec §3) — checked
// in exactly that order, over the items CURRENTLY in the catalog only. An id in
// lock.removed with NO catalog item (the normal removal case) never appears here — it
// lives only in lock.removed, for the Removed/lock section (spec §4). An id present in
// BOTH items[] and lock.removed (the legal-but-flagged "removed-def-restored"
// restoration case) shows here as "removed" per the literal order, alongside its own
// warning on the SAME card (issue routing below).
function buildStatusById(items, lock) {
  const statusById = {};
  const defIds = new Set(lock ? lock.defIds : []);
  const removed = lock ? lock.removed : {};
  for (const item of items) {
    const id = item && item.id;
    if (id == null) continue;
    if (defIds.has(id)) statusById[id] = "shipped";
    else if (Object.prototype.hasOwnProperty.call(removed, id)) statusById[id] = "removed";
    else statusById[id] = "draft";
  }
  return statusById;
}

// Pure "issue routing" rule (spec §4, load-bearing): a validate error/warning attaches
// to a card IFF its `id` is present in the catalog's CURRENT items; a catalog-level
// rule (`id: null`) or a rule keyed on a DELETED id (the removed-without-reaction
// family) has no card to live on and routes to the summary/Removed section instead.
// The site re-implements this SAME tiny partition in vanilla JS (it cannot import this
// Node-only module — see README "Why the site can't import ops.mjs").
export function routeIssues(itemIds, issues) {
  const ids = itemIds instanceof Set ? itemIds : new Set(itemIds);
  const byItem = new Map();
  const unrouted = [];
  for (const issue of issues || []) {
    if (issue && issue.id != null && ids.has(issue.id)) {
      if (!byItem.has(issue.id)) byItem.set(issue.id, []);
      byItem.get(issue.id).push(issue);
    } else {
      unrouted.push(issue);
    }
  }
  return { byItem, unrouted };
}

// Build the full view for one catalog from an ABSOLUTE folder path (a registered
// game/template folder). Decoupled from the registry lookup so tests can point it at a
// throwaway temp folder without touching workspace catalog state.
export async function loadCatalogView(root, folderAbs, meta) {
  const catalogPath = join(folderAbs, "content", "items.json");
  const hasItems = existsSync(catalogPath);
  const viewMeta = { ...meta, hasItems };

  // decision (д): a game/template with no content/items.json is a valid empty state,
  // never an error — no subprocess spawn at all (nothing to read).
  if (!hasItems) {
    return {
      meta: viewMeta,
      namespace: null,
      items: [],
      containers: [],
      item_kinds: [],
      schema: null,
      lock: { status_by_id: {}, removed: {} },
      validate: { available: false, ok: null, errors: [], warnings: [], reason: "content/items.json not found for this catalog" },
      // T0316: icon preview is independent of items.json (it reads the build
      // tree, not content/) — computed even for the empty state so the page
      // never has to special-case a missing view.icons.
      icons: buildIconPreview(folderAbs),
    };
  }

  const schemaPath = join(folderAbs, "content", "item_fields.schema.json");
  const lockPath = join(folderAbs, "content", "items.lock.json");
  const stateSchemaPath = join(folderAbs, "state", "items.schema.json");
  const srcDirPath = join(folderAbs, "src", "features", "items");

  // list/schema are independent subprocess calls (items_ops.py cmd_list never reads
  // the field schema) — run them concurrently; up to 3 short `py` spawns per load total
  // with validate below, well inside the "no caching needed" budget (spec §8).
  const [listResult, schemaResult] = await Promise.all([runList(root, catalogPath), runSchema(root, schemaPath)]);

  const lockExists = existsSync(lockPath);
  const validateResult = await runValidate(root, {
    catalogPath,
    schemaPath,
    stateSchemaPath,
    srcDirPath,
    baselinePath: lockExists ? lockPath : undefined,
  });

  // First failure wins: a broken catalog is the more fundamental breakage: report it
  // over a (possibly also broken) schema. state_schema never lands here — only
  // `validate` reads it, and its own exit 2 routes to validate.available/reason, not
  // content_error (spec §3's content_error.source union names "state_schema" as a
  // possibility, but no phase-1 code path actually produces it — noted deviation).
  let contentError = null;
  if (!listResult.ok) contentError = { source: listResult.source, stderr: listResult.stderr };
  else if (!schemaResult.ok) contentError = { source: schemaResult.source, stderr: schemaResult.stderr };

  const items = listResult.ok ? listResult.data.items : [];
  const namespace = listResult.ok ? listResult.data.namespace : null;
  const containers = listResult.ok ? listResult.data.containers : [];
  const itemKinds = listResult.ok ? listResult.data.item_kinds : [];
  const schema = schemaResult.ok ? schemaResult.data : null;

  const lockRaw = lockExists ? readLockRaw(lockPath) : null;

  const view = {
    meta: viewMeta,
    namespace,
    items,
    containers,
    item_kinds: itemKinds,
    schema,
    lock: { status_by_id: buildStatusById(items, lockRaw), removed: lockRaw ? lockRaw.removed : {} },
    validate: validateResult,
    // T0316 (spec §5): pixel crops for the catalog's icon_asset_id values, read
    // from the BUILT pack (not committed source) — sync, no subprocess.
    icons: buildIconPreview(folderAbs),
  };
  if (contentError) view.content_error = contentError;
  return view;
}

function resolveCatalogEntry(root, id, options = {}) {
  const raw = String(id || "");
  const sep = raw.indexOf(":");
  if (sep <= 0) return null;
  const kind = raw.slice(0, sep);
  const registryId = raw.slice(sep + 1);
  if (kind !== "template" && kind !== "game") return null;
  const list = kind === "template"
    ? listRegisteredTemplates(root)
    : listGameMounts(root, { activeGameId: registryId, includePrivate: options.includePrivate === true });
  const found = list.find((entry) => entry.id === registryId);
  if (!found && kind === "game") {
    const mount = list.find((entry) => entry.gameId === registryId);
    if (!mount) return null;
    return { kind, title: mount.publicAlias || mount.title, folder: mount.root, status: mount.status || mount.visibility };
  }
  if (!found) return null;
  return { kind, title: found.title, folder: found.folder, status: found.status };
}

// GET /api/items-viewer/catalogs — the dropdown list (spec §3). `id` = `<kind>:<id>`
// disambiguates a template and a game sharing an id (mirrors the gallery's
// `game:${id}` convention, assets/gallery/api.mjs:48). hasItems is never hidden or
// fatal (decision (д)) — a game with no items still appears, flagged.
export function listCatalogs(root, options = {}) {
  const templates = listRegisteredTemplates(root).map((entry) => ({
    id: `template:${entry.id}`,
    kind: "template",
    title: entry.title,
    folder: entry.folder,
    hasItems: existsSync(join(root, entry.folder, "content", "items.json")),
    status: entry.status,
  }));
  const games = listGameMounts(root, { includePrivate: options.includePrivate === true }).map((entry) => ({
    id: entry.storeId,
    kind: "game",
    title: entry.title,
    folder: entry.root,
    hasItems: existsSync(join(root, entry.root, "content", "items.json")),
    status: entry.status,
    visibility: entry.visibility,
  }));
  return { catalogs: [...templates, ...games] };
}

// GET /api/items-viewer/catalog?id=<id> — the whole view for one catalog (spec §3).
// Returns null when `id` matches neither registry; api.mjs turns that into a 404. The
// page always calls this for the selected id (single code path, no dropdown-flag
// short-circuit — spec §3/§4).
export async function getCatalogView(root, id, options = {}) {
  const entry = resolveCatalogEntry(root, id, options);
  if (!entry) return null;
  const folderAbs = join(root, entry.folder);
  return loadCatalogView(root, folderAbs, { id, kind: entry.kind, title: entry.title, folder: entry.folder });
}
