import { existsSync, readFileSync, statSync } from "node:fs";
import { readFile } from "node:fs/promises";
import { isAbsolute, join, relative, resolve, sep } from "node:path";
import { spawnSync } from "node:child_process";

import { sha256File } from "../../../../core_harness/tool_lib/hash.mjs";
import { validateLicenseRecord } from "../license/registry.mjs";
import { scanPackManifestSource } from "./manifest.mjs";

export const INVENTORY_SCHEMA = "ai_studio.asset_integrity_inventory.v1";
export const REPORT_SCHEMA = "ai_studio.asset_integrity_report.v1";

export const ISSUE_CODES = Object.freeze({
  MISSING_METADATA: "missing-metadata",
  MISSING_ASSET_ID: "missing-asset-id",
  MISSING_LICENSE: "missing-license",
  MISSING_PROVENANCE: "missing-provenance",
  MISSING_ORIGIN: "missing-origin",
  MISSING_CLASSIFICATION: "missing-classification",
  MISSING_SHA256: "missing-sha256",
  MALFORMED_METADATA: "malformed-metadata",
  MALFORMED_SHA256: "malformed-sha256",
  HASH_MISMATCH: "hash-mismatch",
  BYTES_MISMATCH: "bytes-mismatch",
  MISSING_FILE: "missing-file",
  UNTRACKED_FILE: "untracked-file",
  UNEXPECTED_FILE: "unexpected-file",
  UNEXPECTED_METADATA: "unexpected-metadata",
  PATH_ESCAPE: "path-escape",
  CASE_COLLISION: "case-collision",
  TWO_RECORDS_ONE_FILE: "two-records-one-file",
  CONFLICTING_DUPLICATE_ID: "conflicting-duplicate-id",
  DUPLICATE_INVENTORY: "duplicate-inventory",
  BOUNDARY_MISMATCH: "boundary-mismatch",
  INVALID_LICENSE_ORIGIN: "invalid-license-origin",
  INVALID_LICENSE_EVIDENCE: "invalid-license-evidence",
  INVALID_ARGUMENT: "invalid-argument",
  INVALID_SCOPE: "invalid-scope",
  GIT_SETUP: "git-setup",
});

export const BINARY_EXTENSIONS = new Set([
  ".glb", ".gltf", ".bin", ".fbx", ".obj", ".mtl", ".ntpack",
  ".png", ".jpg", ".jpeg", ".webp", ".gif", ".bmp", ".tga", ".ktx2", ".dds", ".hdr", ".exr",
  ".wav", ".ogg", ".mp3", ".flac", ".ttf", ".otf", ".woff", ".woff2",
  ".exe", ".dll", ".so", ".dylib",
]);
export const CLASSIFICATIONS = new Set(["product-asset", "generated-procedural-output", "test-fixture", "font"]);
export const ORIGINS = new Set(["mine", "ai", "sourced"]);

export function normalizeRepoPath(value) {
  return String(value || "").replace(/\\/g, "/").replace(/^\.\//, "").replace(/\/+/g, "/");
}

function safeRepoPath(value) {
  const path = normalizeRepoPath(value).replace(/\/$/, "");
  if (!path || isAbsolute(path) || /^[A-Za-z]:\//.test(path) || path.split("/").includes("..")) return null;
  return path;
}

function issue(code, path, message, extra = {}) {
  return { code, path: normalizeRepoPath(path), message, ...extra };
}

function report(issues, summary, { setup = false } = {}) {
  const sorted = issues.sort((a, b) => a.path.localeCompare(b.path) || a.code.localeCompare(b.code) || a.message.localeCompare(b.message));
  return {
    schema: REPORT_SCHEMA,
    ok: sorted.length === 0,
    setup,
    exitCode: setup ? 2 : sorted.length ? 1 : 0,
    summary: { ...summary, issueCount: sorted.length },
    issues: sorted,
  };
}

function isBinaryPath(path) {
  const normalized = normalizeRepoPath(path).toLowerCase();
  const dot = normalized.lastIndexOf(".");
  return dot >= 0 && BINARY_EXTENSIONS.has(normalized.slice(dot));
}

function pathInScope(path, scope) {
  return !scope || path === scope || path.startsWith(`${scope}/`);
}

function scopeRelates(a, b) {
  return a === b || a.startsWith(`${b}/`) || b.startsWith(`${a}/`);
}

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
}

function nonValue(value) {
  const text = String(value || "").trim().toLowerCase();
  return !text || ["unknown", "todo", "tbd", "unresolved"].includes(text) || text.startsWith("pending");
}

function normalizeRecord(raw, { sourceScope, metadataPath, pathPrefix = "", restrictedMetadata = false }) {
  const resource = raw.path || raw.resource || "";
  const path = safeRepoPath(pathPrefix ? `${pathPrefix}/${resource}` : resource);
  return {
    ...raw,
    asset_id: String(raw.asset_id || raw.id || ""),
    path,
    classification: String(raw.classification || ""),
    provenance: String(raw.provenance || ""),
    origin: String(raw.origin || ""),
    sha256: String(raw.sha256 || "").toLowerCase(),
    sourceScope,
    metadataPath: normalizeRepoPath(metadataPath),
    restrictedMetadata,
  };
}

function fontRecords(root, source) {
  const data = readJson(join(root, source.path));
  if (data.schema !== "ai_studio.canvas.fonts.v1") throw new Error(`unsupported fonts metadata schema '${data.schema || ""}'`);
  const base = normalizeRepoPath(source.path).split("/").slice(0, -1).join("/");
  return (data.families || []).flatMap((family) => (family.fonts || []).map((font) => normalizeRecord({
    asset_id: `${family.family}-${font.weight}-${font.style || "normal"}`,
    path: `${base}/${font.file}`,
    classification: "font",
    license: family.license,
    license_file: `${base}/${family.licenseFile}`,
    provenance: font.provenance || family.provenance || "",
    origin: "sourced",
    source_page: font.source_page || family.source_page || font.origin,
    author_vendor: font.author_vendor || family.author_vendor || family.author || "",
    publish: "true", redistribution_allowed: "true", commercial_use: "true", modification_allowed: "true",
    sha256: font.sha256, bytes: font.bytes,
  }, { sourceScope: source.scope, metadataPath: source.path })));
}

function sourceSelected(source, selectedRows, scope) {
  if (!scope) return true;
  const metadataScopes = new Set(selectedRows.map((row) => row.metadata_scope).filter(Boolean));
  if (metadataScopes.size) return metadataScopes.has(source.scope);
  const base = safeRepoPath(source.root || source.path || "");
  return base ? scopeRelates(base, scope) : false;
}

async function loadMetadata(root, inventory, selectedRows, scope, { metadataMode = false } = {}) {
  const records = [];
  const issues = [];
  for (const source of inventory.metadata_sources || []) {
    if (!sourceSelected(source, selectedRows, scope)) continue;
    try {
      if (source.type === "pack-manifest") {
        const rootPath = safeRepoPath(source.root);
        if (!rootPath) {
          issues.push(issue(ISSUE_CODES.PATH_ESCAPE, source.root, "metadata source root escapes repository"));
          continue;
        }
        const scanned = await scanPackManifestSource(join(root, rootPath), { packIds: source.packs || [] });
        for (const row of scanned.records) records.push(normalizeRecord(row, {
          sourceScope: source.scope,
          metadataPath: relative(root, row.metadataPath),
          pathPrefix: rootPath,
          restrictedMetadata: source.allow_restricted_untracked === true && normalizeRepoPath(relative(root, row.metadataPath)).includes("/restricted/packs/"),
        }));
      } else if (source.type === "records-json") {
        const sourcePath = safeRepoPath(source.path);
        if (!sourcePath) {
          issues.push(issue(ISSUE_CODES.PATH_ESCAPE, source.path, "metadata source path escapes repository"));
          continue;
        }
        const data = readJson(join(root, sourcePath));
        if (data.schema !== "ai_studio.asset_records.v1") throw new Error(`unsupported asset metadata schema '${data.schema || ""}'`);
        for (const row of data.assets || []) records.push(normalizeRecord(row, { sourceScope: source.scope, metadataPath: sourcePath }));
      } else if (source.type === "fonts-json") {
        records.push(...fontRecords(root, source));
      } else {
        issues.push(issue(ISSUE_CODES.MALFORMED_METADATA, source.path || source.root || "", `unknown metadata source type '${source.type || ""}'`));
      }
    } catch (error) {
      const code = /missing asset_id/i.test(error.message) ? ISSUE_CODES.MISSING_ASSET_ID
        : /escapes pack directory/i.test(error.message) ? ISSUE_CODES.PATH_ESCAPE
          : ISSUE_CODES.MALFORMED_METADATA;
      issues.push(issue(code, source.path || source.root || "", error.message));
    }
  }
  return { records: records.filter((record) => record.restrictedMetadata || (record.path && (metadataMode || pathInScope(record.path, scope)))), issues };
}

export function collectTrackedEntries(root, { spawn = spawnSync } = {}) {
  const result = spawn("git", ["ls-files", "-s", "-z"], { cwd: root, encoding: null, shell: false });
  if (result.error || result.status !== 0) {
    const stderr = Buffer.isBuffer(result.stderr) ? result.stderr.toString("utf8") : String(result.stderr || "");
    return { ok: false, exitCode: 2, issue: issue(ISSUE_CODES.GIT_SETUP, "", `git ls-files failed: ${result.error?.message || stderr.trim() || `exit ${result.status}`}`) };
  }
  const bytes = Buffer.isBuffer(result.stdout) ? result.stdout : Buffer.from(result.stdout || "");
  if (bytes.length && bytes[bytes.length - 1] !== 0) return { ok: false, exitCode: 2, issue: issue(ISSUE_CODES.GIT_SETUP, "", "git ls-files returned a non-NUL-terminated index row") };
  const rows = bytes.toString("utf8").split("\0");
  rows.pop();
  const entries = [];
  for (const row of rows) {
    const match = row.match(/^(\d{6}) [0-9a-f]{40,64} \d+\t([\s\S]+)$/i);
    if (!match) return { ok: false, exitCode: 2, issue: issue(ISSUE_CODES.GIT_SETUP, "", "git ls-files returned a malformed index row") };
    entries.push({ mode: match[1], path: normalizeRepoPath(match[2]) });
  }
  return { ok: true, entries };
}

function compareRecordIdentity(a, b) {
  return JSON.stringify([a.path, a.classification, a.license, a.provenance, a.origin, a.sha256]) === JSON.stringify([b.path, b.classification, b.license, b.provenance, b.origin, b.sha256]);
}

function validateScope(scope, inventory) {
  if (!scope) return { ok: true, scope: "", metadataScopes: [] };
  const normalized = safeRepoPath(scope);
  if (!normalized) return { ok: false, issue: issue(ISSUE_CODES.INVALID_SCOPE, scope, "scope must be a safe repository-relative path") };
  const rows = [...(inventory.entries || []), ...(inventory.boundaries || [])];
  const metadataScopes = new Set();
  for (const source of inventory.metadata_sources || []) {
    if (source.type === "pack-manifest") {
      const rootPath = safeRepoPath(source.root);
      for (const pack of source.packs || []) {
        const packRoot = rootPath ? `${rootPath}/packs/${pack}` : "";
        if (normalized === packRoot || normalized === `${packRoot}/pack.json` || normalized === `${packRoot}/assets.jsonl`) metadataScopes.add(source.scope);
      }
    } else {
      const metadataPath = safeRepoPath(source.path);
      if (metadataPath && normalized === metadataPath) metadataScopes.add(source.scope);
    }
  }
  if (metadataScopes.size) {
    const selected = rows.filter((row) => metadataScopes.has(row.metadata_scope));
    if (!selected.length) return { ok: false, issue: issue(ISSUE_CODES.INVALID_SCOPE, normalized, "metadata scope selects zero inventory rows") };
    return { ok: true, scope: normalized, metadataScopes: [...metadataScopes] };
  }
  const selected = rows.filter((row) => safeRepoPath(row.path) && pathInScope(safeRepoPath(row.path), normalized));
  if (!selected.length) return { ok: false, issue: issue(ISSUE_CODES.INVALID_SCOPE, normalized, "scope selects zero inventory rows") };
  return { ok: true, scope: normalized, metadataScopes: [] };
}

export async function auditAssetIntegrity({ root, tracked, inventory, scope = "", metadataScopes = [] }) {
  const normalizedScope = scope ? safeRepoPath(scope) : "";
  const metadataScopeSet = new Set(metadataScopes);
  const metadataMode = metadataScopeSet.size > 0;
  const allIssues = [];
  const rawEntries = Array.isArray(inventory?.entries) ? inventory.entries : [];
  const rawBoundaries = Array.isArray(inventory?.boundaries) ? inventory.boundaries : [];
  const entries = rawEntries.map((row) => ({ ...row, path: safeRepoPath(row.path) }));
  const boundaries = rawBoundaries.map((row) => ({ ...row, path: safeRepoPath(row.path) }));
  const rowSelected = (row) => row.path && (metadataMode ? metadataScopeSet.has(row.metadata_scope) : pathInScope(row.path, normalizedScope));
  const scopedEntries = entries.filter(rowSelected);
  const scopedBoundaries = boundaries.filter(rowSelected);
  const selectedRows = [...scopedEntries, ...scopedBoundaries];

  const inventorySeen = new Map();
  const caseSeen = new Map();
  for (const [index, row] of [...entries, ...boundaries].entries()) {
    const raw = [...rawEntries, ...rawBoundaries][index];
    if (!row.path) {
      if (!scope || pathInScope(normalizeRepoPath(raw.path), normalizedScope)) allIssues.push(issue(ISSUE_CODES.PATH_ESCAPE, raw.path, "inventory path escapes repository"));
      continue;
    }
    if (!rowSelected(row)) continue;
    if (inventorySeen.has(row.path)) allIssues.push(issue(ISSUE_CODES.DUPLICATE_INVENTORY, row.path, "inventory path is listed more than once"));
    else inventorySeen.set(row.path, row);
    const lower = row.path.toLowerCase();
    if (caseSeen.has(lower) && caseSeen.get(lower) !== row.path) allIssues.push(issue(ISSUE_CODES.CASE_COLLISION, row.path, `case-collides with '${caseSeen.get(lower)}'`));
    else caseSeen.set(lower, row.path);
  }
  for (const row of scopedEntries) if (!CLASSIFICATIONS.has(row.classification)) allIssues.push(issue(ISSUE_CODES.MISSING_CLASSIFICATION, row.path, "inventory classification is missing or invalid"));
  for (const row of scopedBoundaries) if (row.classification !== "external-boundary" || row.kind !== "gitlink") allIssues.push(issue(ISSUE_CODES.BOUNDARY_MISMATCH, row.path, "boundary must have classification=external-boundary and kind=gitlink"));

  const trackedRows = (tracked || []).map((row) => typeof row === "string" ? { path: normalizeRepoPath(row), mode: "100644" } : { ...row, path: normalizeRepoPath(row.path) });
  const allTrackedPaths = new Set(trackedRows.map((row) => row.path));
  const declaredEvidence = new Set((inventory.evidence_files || []).map((path) => safeRepoPath(path)).filter(Boolean));
  const selectedPaths = new Set(selectedRows.map((row) => row.path));
  const scopedTracked = trackedRows.filter((row) => metadataMode ? selectedPaths.has(row.path) : pathInScope(row.path, normalizedScope));
  const trackedByPath = new Map(scopedTracked.map((row) => [row.path, row]));
  const trackedBinary = scopedTracked.filter((row) => row.mode !== "160000" && isBinaryPath(row.path));
  const trackedBoundaries = scopedTracked.filter((row) => row.mode === "160000");
  const entrySet = new Set(scopedEntries.map((row) => row.path));
  const boundarySet = new Set(scopedBoundaries.map((row) => row.path));
  for (const row of trackedBinary) {
    if (boundarySet.has(row.path)) allIssues.push(issue(ISSUE_CODES.BOUNDARY_MISMATCH, row.path, "regular tracked binary cannot be classified as a gitlink boundary"));
    if (!entrySet.has(row.path)) allIssues.push(issue(ISSUE_CODES.UNEXPECTED_FILE, row.path, "tracked binary is absent from inventory entries"));
  }
  for (const row of trackedBoundaries) {
    if (entrySet.has(row.path)) allIssues.push(issue(ISSUE_CODES.BOUNDARY_MISMATCH, row.path, "gitlink cannot be classified as a regular inventory entry"));
    if (!boundarySet.has(row.path)) allIssues.push(issue(ISSUE_CODES.UNEXPECTED_FILE, row.path, "tracked gitlink is absent from boundary inventory"));
  }
  for (const row of scopedEntries) {
    const trackedRow = trackedByPath.get(row.path);
    if (trackedRow?.mode === "160000") allIssues.push(issue(ISSUE_CODES.BOUNDARY_MISMATCH, row.path, "inventory entry resolves to a gitlink"));
    const abs = resolve(root, row.path);
    const rootAbs = resolve(root);
    if (abs !== rootAbs && !abs.startsWith(rootAbs + sep)) allIssues.push(issue(ISSUE_CODES.PATH_ESCAPE, row.path, "inventory path resolves outside repository"));
    else if (!existsSync(abs)) allIssues.push(issue(ISSUE_CODES.MISSING_FILE, row.path, "inventory file is missing"));
    else if (!trackedRow) allIssues.push(issue(ISSUE_CODES.UNTRACKED_FILE, row.path, "inventory file exists but is not tracked"));
  }
  for (const row of scopedBoundaries) {
    const trackedRow = trackedByPath.get(row.path);
    if (!trackedRow || trackedRow.mode !== "160000") allIssues.push(issue(ISSUE_CODES.BOUNDARY_MISMATCH, row.path, "boundary is not a tracked mode-160000 gitlink"));
  }

  const loaded = await loadMetadata(root, inventory || {}, selectedRows, normalizedScope, { metadataMode });
  allIssues.push(...loaded.issues);
  const byPath = new Map();
  const byId = new Map();
  for (const record of loaded.records) {
    if (!record.asset_id) {
      allIssues.push(issue(ISSUE_CODES.MISSING_ASSET_ID, record.path || record.metadataPath, "metadata record is missing asset_id", { metadataPath: record.metadataPath }));
      continue;
    }
    if (!record.path) {
      allIssues.push(issue(ISSUE_CODES.PATH_ESCAPE, record.resource || "", `metadata record '${record.asset_id}' has an empty or escaping path`, { metadataPath: record.metadataPath }));
      continue;
    }
    if (!record.restrictedMetadata) {
      if (!entrySet.has(record.path)) allIssues.push(issue(ISSUE_CODES.UNEXPECTED_METADATA, record.path, "public metadata record has no inventory entry", { metadataPath: record.metadataPath }));
      const abs = join(root, record.path);
      if (!existsSync(abs)) allIssues.push(issue(ISSUE_CODES.MISSING_FILE, record.path, "metadata file is missing", { metadataPath: record.metadataPath }));
      else if (!trackedByPath.has(record.path)) allIssues.push(issue(ISSUE_CODES.UNTRACKED_FILE, record.path, "metadata file exists but is not tracked", { metadataPath: record.metadataPath }));
    }
    const recordsAtPath = byPath.get(record.path) || [];
    recordsAtPath.push(record);
    byPath.set(record.path, recordsAtPath);
    const key = `${record.sourceScope || ""}:${record.asset_id}`;
    const existing = byId.get(key);
    if (existing && !compareRecordIdentity(existing, record)) allIssues.push(issue(ISSUE_CODES.CONFLICTING_DUPLICATE_ID, record.path, `asset_id '${record.asset_id}' conflicts within source '${record.sourceScope}'`, { metadataPath: record.metadataPath }));
    else if (!existing) byId.set(key, record);
  }
  for (const [path, records] of byPath) if (records.length > 1) allIssues.push(issue(ISSUE_CODES.TWO_RECORDS_ONE_FILE, path, `${records.length} metadata records resolve to one file`));

  let verified = 0;
  for (const row of scopedEntries) {
    const records = (byPath.get(row.path) || []).filter((record) => !record.restrictedMetadata);
    if (!records.length) {
      allIssues.push(issue(ISSUE_CODES.MISSING_METADATA, row.path, "tracked binary has no owning metadata record"));
      continue;
    }
    const record = records[0];
    if (!CLASSIFICATIONS.has(record.classification) || record.classification !== row.classification) allIssues.push(issue(ISSUE_CODES.MISSING_CLASSIFICATION, row.path, "metadata classification is missing, invalid, or differs from inventory", { metadataPath: record.metadataPath }));
    if (nonValue(record.license)) allIssues.push(issue(ISSUE_CODES.MISSING_LICENSE, row.path, "license is missing or pending", { metadataPath: record.metadataPath }));
    if (nonValue(record.provenance)) allIssues.push(issue(ISSUE_CODES.MISSING_PROVENANCE, row.path, "provenance is missing or pending", { metadataPath: record.metadataPath }));
    if (nonValue(record.origin)) allIssues.push(issue(ISSUE_CODES.MISSING_ORIGIN, row.path, "origin is missing or pending", { metadataPath: record.metadataPath }));
    else if (!ORIGINS.has(record.origin)) allIssues.push(issue(ISSUE_CODES.INVALID_LICENSE_ORIGIN, row.path, `origin must be one of ${[...ORIGINS].join("|")}`, { metadataPath: record.metadataPath }));
    if (!record.sha256) allIssues.push(issue(ISSUE_CODES.MISSING_SHA256, row.path, "sha256 is missing", { metadataPath: record.metadataPath }));
    else if (!/^[0-9a-f]{64}$/.test(record.sha256)) allIssues.push(issue(ISSUE_CODES.MALFORMED_SHA256, row.path, "sha256 must be 64 hexadecimal characters", { metadataPath: record.metadataPath }));
    const license = validateLicenseRecord(record, { forPublicBinary: true, forRelease: true });
    const licenseUrl = String(record.license_url || record.licenseUrl || "").trim();
    if (licenseUrl && !/^https?:\/\/[^\s]+$/i.test(licenseUrl)) {
      allIssues.push(issue(ISSUE_CODES.INVALID_LICENSE_EVIDENCE, row.path, "license_url must be an absolute http(s) URL; use license_file for repository-relative evidence", { metadataPath: record.metadataPath }));
    }
    const licenseFileRaw = String(record.license_file || record.licenseFile || "").trim();
    if (licenseFileRaw) {
      const licenseFile = safeRepoPath(licenseFileRaw);
      if (!licenseFile) {
        allIssues.push(issue(ISSUE_CODES.INVALID_LICENSE_EVIDENCE, row.path, "license_file must be a safe repository-relative path", { metadataPath: record.metadataPath }));
      } else if (!existsSync(join(root, licenseFile))) {
        allIssues.push(issue(ISSUE_CODES.INVALID_LICENSE_EVIDENCE, row.path, `license_file does not exist: ${licenseFile}`, { metadataPath: record.metadataPath }));
      } else if (!allTrackedPaths.has(licenseFile) && !declaredEvidence.has(licenseFile)) {
        allIssues.push(issue(ISSUE_CODES.INVALID_LICENSE_EVIDENCE, row.path, `license_file is neither tracked nor declared by inventory evidence_files: ${licenseFile}`, { metadataPath: record.metadataPath }));
      }
    }
    if (!license.ok || nonValue(record.origin) || (!nonValue(record.origin) && !ORIGINS.has(record.origin))) allIssues.push(issue(ISSUE_CODES.INVALID_LICENSE_ORIGIN, row.path, [...license.issues, ...license.warnings].join("; ") || "origin is unresolved or invalid", { metadataPath: record.metadataPath }));
    const abs = join(root, row.path);
    if (existsSync(abs) && /^[0-9a-f]{64}$/.test(record.sha256)) {
      const actual = await sha256File(abs);
      if (actual !== record.sha256) allIssues.push(issue(ISSUE_CODES.HASH_MISMATCH, row.path, `sha256 mismatch: expected ${record.sha256}, got ${actual}`, { metadataPath: record.metadataPath }));
      else if (Number(record.bytes) > 0 && statSync(abs).size !== Number(record.bytes)) allIssues.push(issue(ISSUE_CODES.BYTES_MISMATCH, row.path, `byte size mismatch: expected ${record.bytes}, got ${statSync(abs).size}`, { metadataPath: record.metadataPath }));
      else verified += 1;
    }
  }
  return report(allIssues, {
    trackedBinaryBlobs: trackedBinary.length,
    externalBoundaries: trackedBoundaries.length,
    inventoryEntries: scopedEntries.length,
    inventoryBoundaries: scopedBoundaries.length,
    metadataRecords: loaded.records.filter((record) => !record.restrictedMetadata).length,
    verified,
    scope: normalizedScope || "all",
  });
}

export async function loadAndAuditRepository(root, { scope = "", inventoryPath = "ai_studio/assets/backlog/storage/manifests/tracked_binary_inventory.json", spawn = spawnSync } = {}) {
  const normalizedRoot = resolve(root).replace(/[\\/]$/, "");
  let inventory;
  try {
    inventory = JSON.parse((await readFile(join(normalizedRoot, inventoryPath), "utf8")).replace(/^\uFEFF/, ""));
  } catch (error) {
    return report([issue(ISSUE_CODES.MALFORMED_METADATA, inventoryPath, error.message)], { issueCount: 1, scope: scope || "all" }, { setup: true });
  }
  if (inventory.schema !== INVENTORY_SCHEMA) return report([issue(ISSUE_CODES.MALFORMED_METADATA, inventoryPath, `unsupported inventory schema '${inventory.schema || ""}'`)], { scope: scope || "all" }, { setup: true });
  const validatedScope = validateScope(scope, inventory);
  if (!validatedScope.ok) return report([validatedScope.issue], { scope: scope || "all" }, { setup: true });
  const collected = collectTrackedEntries(normalizedRoot, { spawn });
  if (!collected.ok) return report([collected.issue], { scope: validatedScope.scope || "all" }, { setup: true });
  return auditAssetIntegrity({ root: normalizedRoot, tracked: collected.entries, inventory, scope: validatedScope.scope, metadataScopes: validatedScope.metadataScopes });
}
