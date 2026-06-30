#!/usr/bin/env node
// Leak guard: no paid/licensed asset binary may be committed to this open repo.
//
// The risk is a purchased asset (e.g. a CGTrader pack) ending up in git - whether
// pulled from the library or dropped in by hand. This guard enforces, against the
// git index, that EVERY committed binary asset has a recorded license that allows
// publication:
//
//   1. No file under assets/restricted/ is tracked (it is gitignored; catch -f adds).
//   2. Every tracked binary under assets/packs/*/files/ either
//      - resolves to a Pack Manifest record whose license is publishable, or
//      - is on the narrow existing-asset allowlist.
//      A missing manifest record or a non-publishable license fails.
//
// Pure core (auditTrackedAssets / deriveAssetId) is unit-tested; the CLI wires
// `git ls-files` + the filesystem. Run directly for asset-license validation.
import { readFileSync, existsSync, readdirSync } from "node:fs";
import { join, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { isPublishable, isAssetBinary, requiresAttribution, hasAttributionInfo, requiresNotice, hasNoticeInfo } from "./restricted.mjs";

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

// <game>/assets/packs/<pack>/files/<asset-id>/... -> { pack, assetId }
// <game>/assets/previews/<asset-id>/...           -> { assetId }
// The optional leading game-folder prefix makes this per-game (assets live inside
// each game folder now, not at repo root).
export function deriveAssetId(path) {
  let m = path.match(/(?:^|\/)assets\/packs\/([^/]+)\/files\/([^/]+)\//);
  if (m) return { pack: m[1], assetId: m[2] };
  m = path.match(/(?:^|\/)assets\/previews\/([^/]+)\//);
  if (m) return { assetId: m[1] };
  return {};
}

// Pure auditor. trackedFiles: repo-relative paths (any slash). recordsByAssetId:
// Map<asset_id, manifest record>. allowlistPrefixes: path prefixes to skip.
export function auditTrackedAssets(trackedFiles, {
  recordsByAssetId = new Map(),
  allowlistPrefixes = [],
  release = false,
  isBinary = isAssetBinary,
  publishable = isPublishable,
  attributionRequired = requiresAttribution,
  attributionInfo = hasAttributionInfo,
  noticeRequired = requiresNotice,
  noticeInfo = hasNoticeInfo,
} = {}) {
  const violations = [];
  const warnings = [];
  for (const raw of trackedFiles) {
    const p = String(raw).replace(/\\/g, "/").trim();
    if (!p) continue;
    if (/(?:^|\/)assets\/restricted\//.test(p)) {
      violations.push({ path: p, reason: "tracked file under <game>/assets/restricted/ - that root is gitignored; unstage it (git rm --cached)" });
      continue;
    }
    if (!isBinary(p)) continue;
    if (!/(?:^|\/)assets\/(packs|previews|meshes)\//.test(p)) continue;
    if (allowlistPrefixes.some((pre) => p.startsWith(pre))) continue;
    const { assetId } = deriveAssetId(p);
    if (!assetId) {
      violations.push({ path: p, reason: "binary asset with no manifest mapping and not on the legacy allowlist - record a license or move it to assets/restricted/" });
      continue;
    }
    const fm = recordsByAssetId.get(assetId);
    if (!fm) {
      violations.push({ path: p, reason: `no manifest record for asset_id '${assetId}' - every committed asset needs a recorded license` });
      continue;
    }
    if (!publishable(fm)) {
      violations.push({ path: p, reason: `asset '${assetId}' license is not publishable (license='${fm.license || "?"}') - move the binary to assets/restricted/` });
      continue;
    }
    if (attributionRequired(fm) && !attributionInfo(fm)) {
      warnings.push({ path: p, reason: `asset '${assetId}' requires attribution before release (license='${fm.license || "?"}') but manifest is missing author/credit and source page` });
    }
    if (noticeRequired(fm) && !noticeInfo(fm)) {
      warnings.push({ path: p, reason: `asset '${assetId}' requires a license notice before release (license='${fm.license || "?"}') but manifest is missing author/credit or license evidence` });
    }
  }
  if (release) violations.push(...warnings);
  return { ok: violations.length === 0, violations, warnings };
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

// Asset roots are per-game: a GAME folder (has CMakeLists.txt) with an assets/ dir
// - template/ and each <game>/. This deliberately excludes tooling and AI Studio
// module dirs. Returns repo-relative "<dir>/assets" paths. external/ = engine.
const SKIP_DIRS = new Set(["external", "build", "node_modules", ".git", "tmp"]);
function isGameFolder(root, name) {
  return existsSync(join(root, name, "assets")) && existsSync(join(root, name, "CMakeLists.txt"));
}
function assetRoots(root) {
  const roots = [];
  let entries;
  try { entries = readdirSync(root, { withFileTypes: true }); } catch { return roots; }
  for (const e of entries) {
    if (!e.isDirectory() || SKIP_DIRS.has(e.name)) continue;
    if (isGameFolder(root, e.name)) roots.push(`${e.name}/assets`);
  }
  // Legacy: a game still living at repo root (pre-template model).
  if (existsSync(join(root, "assets")) && existsSync(join(root, "CMakeLists.txt"))) roots.push("assets");
  return roots;
}

function readJsonl(path) {
  let text = "";
  try { text = readFileSync(path, "utf8"); } catch { return []; }
  return text.split(/\r?\n/).map((line) => line.trim()).filter(Boolean).flatMap((line) => {
    try { return [JSON.parse(line)]; } catch { return []; }
  });
}

function loadManifestRecords(root) {
  const map = new Map();
  for (const aroot of assetRoots(root)) {
    for (const f of walk(join(root, aroot, "packs"))) {
      if (!f.endsWith("assets.jsonl")) continue;
      for (const record of readJsonl(f)) {
        if (record.asset_id) map.set(record.asset_id, record);
      }
    }
  }
  return map;
}

function loadAllowlist(root) {
  const f = join(root, "ai_studio", "assets", "storage", "license", "restricted_assets_allowlist.json");
  if (!existsSync(f)) return [];
  try {
    const data = JSON.parse(readFileSync(f, "utf8"));
    return Array.isArray(data.prefixes) ? data.prefixes : [];
  } catch { return []; }
}

function gitTrackedAssets(root) {
  const r = spawnSync("git", ["ls-files"], { cwd: root, encoding: "utf8", shell: false });
  if (r.error || r.status !== 0) return null;
  // Only tracked paths under a per-game assets/ root.
  const roots = assetRoots(root);
  return r.stdout.split(/\r?\n/).filter(Boolean).filter((p) => roots.some((ar) => p === ar || p.startsWith(`${ar}/`)));
}

export function main() {
  const release = process.argv.includes("--release");
  const root = repoRoot();
  const tracked = gitTrackedAssets(root);
  if (tracked === null) {
    console.log("ok: restricted asset guard skipped (not a git repo / git unavailable)");
    return;
  }
  const { ok, violations, warnings } = auditTrackedAssets(tracked, {
    recordsByAssetId: loadManifestRecords(root),
    allowlistPrefixes: loadAllowlist(root),
    release,
  });
  if (!ok) {
    console.error(`error: restricted asset guard found ${violations.length} violation(s)${release ? " [release]" : ""}:`);
    for (const v of violations) console.error(`  - ${v.path}\n      ${v.reason}`);
    console.error("\nfix: record the asset in a manifest with a publishable license, or move the binary under assets/restricted/ (gitignored) and re-pull. See ai_studio/assets/storage/license/restricted.mjs.");
    process.exit(1);
  }
  if (warnings.length) {
    console.warn(`warn: restricted asset guard found ${warnings.length} release debt item(s):`);
    for (const v of warnings) console.warn(`  - ${v.path}\n      ${v.reason}`);
  }
  console.log(`ok: restricted asset guard - ${tracked.length} tracked asset path(s), no license/publish violations${release ? " [release]" : ""}`);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main();
}
