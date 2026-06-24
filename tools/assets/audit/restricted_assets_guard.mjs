#!/usr/bin/env node
// Leak guard: no paid/licensed asset binary may be committed to this open repo.
//
// The risk is a purchased asset (e.g. a CGTrader pack) ending up in git — whether
// pulled from the library or dropped in by hand. This guard enforces, against the
// git index, that EVERY committed binary asset has a recorded license that allows
// publication:
//
//   1. No file under assets/restricted/ is tracked (it is gitignored; catch -f adds).
//   2. Every tracked binary under assets/{source,previews,meshes}/ either
//      - resolves to a catalog record whose license is publishable, or
//      - is on the pre-catalog legacy allowlist.
//      A missing catalog (no recorded license) or a non-publishable license fails.
//
// Pure core (auditTrackedAssets / deriveAssetId) is unit-tested; the CLI wires
// `git ls-files` + the filesystem. Wired into tools/pipeline_validate.mjs.
import { readFileSync, existsSync, readdirSync } from "node:fs";
import { join, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { parseFrontmatter } from "../source/find_assets.mjs";
import { isPublishable, isAssetBinary } from "../restricted.mjs";

function repoRoot() {
  let dir = dirname(fileURLToPath(import.meta.url));
  for (let i = 0; i < 8; i += 1) {
    if (existsSync(join(dir, "AGENTS.md"))) return dir;
    const up = dirname(dir);
    if (up === dir) break;
    dir = up;
  }
  return process.cwd();
}

// assets/source/<kindDir>/<asset-id>/...  -> { kindDir, assetId }
// assets/previews/<asset-id>/...          -> { assetId }
export function deriveAssetId(path) {
  let m = path.match(/^assets\/source\/([^/]+)\/([^/]+)\//);
  if (m) return { kindDir: m[1], assetId: m[2] };
  m = path.match(/^assets\/previews\/([^/]+)\//);
  if (m) return { assetId: m[1] };
  return {};
}

// Pure auditor. trackedFiles: repo-relative paths (any slash). catalogByAssetId:
// Map<asset_id, frontmatter>. allowlistPrefixes: legacy path prefixes to skip.
export function auditTrackedAssets(trackedFiles, {
  catalogByAssetId = new Map(),
  allowlistPrefixes = [],
  isBinary = isAssetBinary,
  publishable = isPublishable,
} = {}) {
  const violations = [];
  for (const raw of trackedFiles) {
    const p = String(raw).replace(/\\/g, "/").trim();
    if (!p) continue;
    if (p.startsWith("assets/restricted/")) {
      violations.push({ path: p, reason: "tracked file under assets/restricted/ — this root is gitignored; unstage it (git rm --cached)" });
      continue;
    }
    if (!isBinary(p)) continue;
    if (!/^assets\/(source|previews|meshes)\//.test(p)) continue;
    if (allowlistPrefixes.some((pre) => p.startsWith(pre))) continue;
    const { assetId } = deriveAssetId(p);
    if (!assetId) {
      violations.push({ path: p, reason: "binary asset with no catalog mapping and not on the legacy allowlist — record a license (catalog) or move it to assets/restricted/" });
      continue;
    }
    const fm = catalogByAssetId.get(assetId);
    if (!fm) {
      violations.push({ path: p, reason: `no catalog record for asset_id '${assetId}' — every committed asset needs a recorded license` });
      continue;
    }
    if (!publishable(fm)) {
      violations.push({ path: p, reason: `asset '${assetId}' license is not publishable (license='${fm.license || "?"}') — move the binary to assets/restricted/ and commit only the catalog .md` });
    }
  }
  return { ok: violations.length === 0, violations };
}

function walk(dir) {
  const out = [];
  let entries;
  try { entries = readdirSync(dir, { withFileTypes: true }); } catch { return out; }
  for (const e of entries) {
    const p = join(dir, e.name);
    if (e.isDirectory()) out.push(...walk(p));
    else out.push(p);
  }
  return out;
}

function loadCatalog(root) {
  const dir = join(root, "assets", "catalog");
  const map = new Map();
  for (const f of walk(dir)) {
    if (!f.endsWith(".md") || /[\\/]_[^\\/]*\.md$/.test(f)) continue;
    let fm;
    try { fm = parseFrontmatter(readFileSync(f, "utf8")); } catch { continue; }
    if (fm.asset_id) map.set(fm.asset_id, fm);
  }
  return map;
}

function loadAllowlist(root) {
  const f = join(root, "tools", "assets", "audit", "restricted_assets_allowlist.json");
  if (!existsSync(f)) return [];
  try {
    const data = JSON.parse(readFileSync(f, "utf8"));
    return Array.isArray(data.prefixes) ? data.prefixes : [];
  } catch { return []; }
}

function gitTrackedAssets(root) {
  const r = spawnSync("git", ["ls-files", "assets"], { cwd: root, encoding: "utf8", shell: false });
  if (r.error || r.status !== 0) return null;
  return r.stdout.split(/\r?\n/).filter(Boolean);
}

function main() {
  const root = repoRoot();
  const tracked = gitTrackedAssets(root);
  if (tracked === null) {
    console.log("ok: restricted asset guard skipped (not a git repo / git unavailable)");
    return;
  }
  const { ok, violations } = auditTrackedAssets(tracked, {
    catalogByAssetId: loadCatalog(root),
    allowlistPrefixes: loadAllowlist(root),
  });
  if (!ok) {
    console.error(`error: restricted asset guard found ${violations.length} violation(s):`);
    for (const v of violations) console.error(`  - ${v.path}\n      ${v.reason}`);
    console.error("\nfix: catalog the asset with a publishable license, or move the binary under assets/restricted/ (gitignored) and re-pull. See tools/assets/restricted.mjs.");
    process.exit(1);
  }
  console.log(`ok: restricted asset guard — ${tracked.length} tracked asset path(s), no license/publish violations`);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main();
}
