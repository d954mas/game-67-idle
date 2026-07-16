// Items Viewer — read-only catalog ops (T0316 phase 1).
//
// Pure logic, no HTTP. Merges the template/game registries and delegates every
// catalog read to the single-source semantic Items CLI. The Viewer only adapts
// its bounded Snapshot summaries to one web view object.
import { execFile } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { listRegisteredTemplates } from "../sources/ops.mjs";
import { listGameMounts } from "../../workspace/games.mjs";
import { studioPythonPath } from "../../dev_environment/python.mjs";
import { buildIconPreview } from "./icon_preview.mjs";

// Production uses the configured Studio interpreter. `bin` is only a test seam for
// exercising a real spawn failure.
const MAX_BUFFER = 1024 * 1024;

function itemsCliScriptPath(root) {
  return join(root, "features", "items-core", "scripts", "items_cli.py");
}

// Spawns `items_cli.py --project-root <game> <args>` with cwd = repo root. Resolves with {code, stdout,
// stderr} for ANY controlled process exit (0/1/2/...); rejects only on a genuine spawn
// failure (interpreter missing — ENOENT), which callers turn into a loud 500
// (spec §3 "TOOL-FAILURE — the ONLY 500").
export function runItemsCliRaw(root, projectRoot, args, { bin } = {}) {
  const scriptPath = itemsCliScriptPath(root);
  const python = bin || studioPythonPath(root);
  return new Promise((resolveRun, rejectRun) => {
    execFile(python, [scriptPath, "--project-root", projectRoot, ...args], { cwd: root, maxBuffer: MAX_BUFFER }, (error, stdout, stderr) => {
      if (error && typeof error.code !== "number") {
        rejectRun(new Error(`items_cli.py could not be spawned (${python}): ${error.message}`));
        return;
      }
      resolveRun({ code: error ? error.code : 0, stdout: stdout ?? "", stderr: stderr ?? "" });
    });
  });
}

// Exported so a test can hit the "unparseable stdout on an otherwise-successful exit"
// throw directly (items_cli.py itself always emits valid JSON on a real controlled exit, so
// this branch is otherwise unreachable through a fixture — spec §6 gate).
export function parseJsonOrThrow(text, what) {
  try {
    return JSON.parse(text);
  } catch (err) {
    throw new Error(`items_cli.py ${what} printed unparseable JSON on an otherwise-successful exit: ${err.message}`);
  }
}

async function runList(root, projectRoot) {
  const { code, stdout, stderr } = await runItemsCliRaw(root, projectRoot, ["list"]);
  if (code === 0) return { ok: true, data: parseJsonOrThrow(stdout, "list").result };
  if (code === 1) return { ok: false, source: "catalog", stderr: stderr.trim() };
  throw new Error(`items_cli.py list exited ${code} unexpectedly: ${stderr.trim()}`);
}

function issue(entry) {
  return {
    ...entry,
    rule: entry?.rule || entry?.code || "items-validation",
    msg: entry?.msg || entry?.message || JSON.stringify(entry),
  };
}

async function runValidate(root, projectRoot) {
  const { code, stdout, stderr } = await runItemsCliRaw(root, projectRoot, ["validate"]);
  if (code === 0 || code === 1) {
    if (!stdout.trim()) return { available: false, ok: null, errors: [], warnings: [], reason: stderr.trim() };
    const parsed = parseJsonOrThrow(stdout, "validate").result;
    const diagnostics = parsed.diagnostics || [];
    return {
      available: true,
      ok: parsed.ok,
      errors: [...diagnostics.filter((entry) => entry.severity === "error"), ...(parsed.receipt?.errors || [])].map(issue),
      warnings: [...diagnostics.filter((entry) => entry.severity !== "error"), ...(parsed.receipt?.warnings || [])].map(issue),
    };
  }
  throw new Error(`items_cli.py validate exited ${code} unexpectedly: ${stderr.trim()}`);
}

async function runFocusedRead(root, projectRoot, command, args = []) {
  const { code, stdout, stderr } = await runItemsCliRaw(root, projectRoot, [command, ...args]);
  if (code === 0) return { ok: true, data: parseJsonOrThrow(stdout, command).result };
  if (code === 1) return { ok: false, content_error: { source: command, stderr: stderr.trim() } };
  throw new Error(`items_cli.py ${command} exited ${code} unexpectedly: ${stderr.trim()}`);
}

function relevantFields(schema, item) {
  const fields = Array.isArray(schema?.fields) ? schema.fields : [];
  const members = new Set((item?.levels || []).flatMap((row) => Object.keys(row?.values || {})));
  return fields.filter((field) => field && field.section === "level_row" && members.has(field.member));
}

// Selected-item Workbench data remains a direct composition of bounded semantic
// CLI results. It carries Snapshot query/source objects unchanged and adds only
// the relevant generated field metadata needed to render the level grid.
export async function loadItemDetail(root, projectRoot, itemId) {
  const reads = await Promise.all([
    runFocusedRead(root, projectRoot, "inspect", ["--item", itemId]),
    runFocusedRead(root, projectRoot, "schema"),
    runFocusedRead(root, projectRoot, "source", ["--item", itemId]),
    runFocusedRead(root, projectRoot, "dependencies", ["--item", itemId]),
  ]);
  const failed = reads.find((result) => !result.ok);
  if (failed) return { content_error: failed.content_error };

  const [item, schema, source, dependencies] = reads.map((result) => result.data);
  return {
    item,
    fields: relevantFields(schema, item?.item),
    source,
    dependencies,
  };
}

// Charts are requested for one selected generated field instead of eagerly
// evaluating every series on item selection.
export async function loadItemChart(root, projectRoot, itemId, field) {
  const result = await runFocusedRead(root, projectRoot, "chart", [
    "--item", itemId,
    "--field", field,
  ]);
  return result.ok ? result.data : { content_error: result.content_error };
}

// items.lock.json is read directly — a separate
// artifact, not a second model of items (decision (а)). A malformed lock degrades the
// lock DISPLAY only (empty status/removed) rather than a 500 for the whole catalog;
// semantic CLI validation of the SAME file reports the authoritative failure.
function readLockRaw(lockPath) {
  if (!existsSync(lockPath)) return null;
  try {
    const raw = readFileSync(lockPath, "utf8");
    // Strip a leading UTF-8 BOM (0xFEFF) via numeric code point, not a regex escape —
    // matches templates.mjs/games.mjs intent without embedding an invisible char literal.
    const text = raw.charCodeAt(0) === 0xfeff ? raw.slice(1) : raw;
    const parsed = JSON.parse(text);
    return {
      defIds: Array.isArray(parsed.def_ids)
        ? parsed.def_ids
        : parsed.def_ids && typeof parsed.def_ids === "object"
          ? Object.keys(parsed.def_ids)
          : [],
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

function itemNamespace(items) {
  const namespaces = new Set(items.map((item) => String(item.id || "").split(".")[0]).filter(Boolean));
  return namespaces.size === 1 ? [...namespaces][0] : null;
}

function label(value) {
  const text = String(value || "").replace(/_/g, " ");
  return text ? text.charAt(0).toUpperCase() + text.slice(1) : text;
}

function itemKinds(items) {
  return [...new Set(items.map((item) => item.kind).filter(Boolean))]
    .sort()
    .map((id) => ({ id, label: label(id) }));
}

// Build the full view for one catalog from an ABSOLUTE folder path (a registered
// game/template folder). Decoupled from the registry lookup so tests can point it at a
// throwaway temp folder without touching workspace catalog state.
export async function loadCatalogView(root, folderAbs, meta) {
  const manifestPath = join(folderAbs, "items.lua.json");
  const hasItems = existsSync(manifestPath);
  const viewMeta = { ...meta, hasItems };

  // A game/template with no Items manifest is a valid empty state,
  // never an error — no subprocess spawn at all (nothing to read).
  if (!hasItems) {
    return {
      meta: viewMeta,
      namespace: null,
      items: [],
      item_kinds: [],
      lock: { status_by_id: {}, removed: {} },
      validate: { available: false, ok: null, errors: [], warnings: [], reason: "items.lua.json not found for this catalog" },
      // Icon preview is independent of the catalog source (it reads the build
      // tree, not content/) — computed even for the empty state so the page
      // never has to special-case a missing view.icons.
      icons: buildIconPreview(folderAbs),
    };
  }

  const lockPath = join(folderAbs, "content", "items.lock.json");
  const [listResult, validateResult] = await Promise.all([
    runList(root, folderAbs),
    runValidate(root, folderAbs),
  ]);

  const contentError = !listResult.ok ? { source: listResult.source, stderr: listResult.stderr } : null;
  const items = listResult.ok ? listResult.data : [];
  const lockRaw = existsSync(lockPath) ? readLockRaw(lockPath) : null;

  const view = {
    meta: viewMeta,
    namespace: itemNamespace(items),
    items,
    item_kinds: itemKinds(items),
    lock: { status_by_id: buildStatusById(items, lockRaw), removed: lockRaw ? lockRaw.removed : {} },
    validate: validateResult,
    // Pixel crops for the catalog's icon values, read
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
// disambiguates a template and a game sharing an id. hasItems is never hidden or
// fatal (decision (д)) — a game with no items still appears, flagged.
export function listCatalogs(root, options = {}) {
  const templates = listRegisteredTemplates(root).map((entry) => ({
    id: `template:${entry.id}`,
    kind: "template",
    title: entry.title,
    folder: entry.folder,
    hasItems: existsSync(join(root, entry.folder, "items.lua.json")),
    status: entry.status,
  }));
  const games = listGameMounts(root, { includePrivate: options.includePrivate === true }).map((entry) => ({
    id: entry.storeId,
    kind: "game",
    title: entry.title,
    folder: entry.root,
    hasItems: existsSync(join(root, entry.root, "items.lua.json")),
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

export async function getItemDetail(root, catalogId, itemId, options = {}) {
  const entry = resolveCatalogEntry(root, catalogId, options);
  if (!entry) return null;
  return loadItemDetail(root, join(root, entry.folder), itemId);
}

export async function getItemChart(root, catalogId, itemId, field, options = {}) {
  const entry = resolveCatalogEntry(root, catalogId, options);
  if (!entry) return null;
  return loadItemChart(root, join(root, entry.folder), itemId, field);
}
