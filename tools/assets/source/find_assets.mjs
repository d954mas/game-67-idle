#!/usr/bin/env node
// Source-first asset search (keystone of the source-before-generate rule).
//
// Before generating or proceduralizing art, search the shared asset library
// catalog. On a miss, this prints the canonical free CC0/OFL sources so the
// agent searches those next, and only generates what cannot be sourced.
//
// It also exports scanLibrary() so the asset viewer reuses one catalog reader.
//
//   node tools/assets/source/find_assets.mjs --tags "sofa,furniture" --kind model
//   node tools/assets/source/find_assets.mjs --query robot --origin sourced --json
//   node tools/assets/source/find_assets.mjs --record --family "room-furniture" \
//        --decision source+intake --reason "Kenney furniture kit fits the diorama"
import { readFile, readdir, writeFile, mkdir } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { pathToFileURL, fileURLToPath } from "node:url";

// Walk up from this module to the repo root (dir holding AGENTS.md), so tools
// anchor outputs to the repo regardless of the caller's cwd.
function repoRoot() {
  let dir = dirname(fileURLToPath(import.meta.url));
  for (let i = 0; i < 8; i += 1) {
    if (existsSync(join(dir, "AGENTS.md")) || existsSync(join(dir, "package.json"))) return dir;
    const up = dirname(dir);
    if (up === dir) break;
    dir = up;
  }
  return process.cwd();
}

export const DEFAULT_LIBRARY = "C:\\Users\\ROG\\YandexDisk\\gamedev\\assets\\ai_pipeline_assets";

// kind -> catalog/files subdirectory
export const KIND_DIR = {
  model: "models",
  texture: "textures",
  material: "materials",
  ui: "ui",
  audio: "audio",
  font: "fonts",
  reference: "references",
};

export const ORIGINS = ["mine", "ai", "sourced", "unknown"];

// Canonical free, legal asset sources a source-first search should try after the
// library before falling back to generation. Web-confirmed CC0/OFL.
export const FREE_SOURCES = [
  { name: "Kenney", url: "https://kenney.nl/assets", note: "40k+ CC0 2D/3D/UI/audio game assets" },
  { name: "Quaternius", url: "https://quaternius.com", note: "CC0 low-poly packs (characters, nature, props)" },
  { name: "Poly Haven", url: "https://polyhaven.com", note: "CC0 textures, HDRIs, models (PBR)" },
  { name: "Poly Pizza", url: "https://poly.pizza", note: "CC0 low-poly 3D models" },
  { name: "OpenGameArt", url: "https://opengameart.org", note: "filter by CC0; 2D/3D/audio" },
  { name: "ambientCG", url: "https://ambientcg.com", note: "CC0 PBR materials/textures" },
  { name: "itch.io (CC0)", url: "https://itch.io/game-assets/assets-cc0", note: "CC0 game-asset packs" },
  { name: "Google Fonts", url: "https://fonts.google.com", note: "OFL fonts, free to ship" },
  { name: "awesome-cc0", url: "https://github.com/madjin/awesome-cc0", note: "curated meta-list of CC0 sources" },
];

// Minimal YAML frontmatter parser for OKF catalog records (key: value + [lists]).
export function parseFrontmatter(text) {
  const m = String(text).match(/^﻿?---\r?\n([\s\S]*?)\r?\n---/);
  if (!m) return {};
  const out = {};
  for (const raw of m[1].split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const idx = line.indexOf(":");
    if (idx < 0) continue;
    const key = line.slice(0, idx).trim();
    let val = line.slice(idx + 1).trim();
    if (val.startsWith("[") && val.endsWith("]")) {
      out[key] = val.slice(1, -1).split(",").map((s) => s.trim()).filter(Boolean);
    } else {
      if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith("'") && val.endsWith("'"))) {
        val = val.slice(1, -1);
      }
      out[key] = val;
    }
  }
  return out;
}

async function walk(dir) {
  const out = [];
  let entries;
  try {
    entries = await readdir(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const e of entries) {
    const p = join(dir, e.name);
    if (e.isDirectory()) out.push(...(await walk(p)));
    else out.push(p);
  }
  return out;
}

// Read every catalog/**/*.md record into a normalized list (one reader for the
// search CLI and the library viewer).
export async function scanLibrary(libraryPath = DEFAULT_LIBRARY) {
  const catalogDir = join(libraryPath, "catalog");
  if (!existsSync(catalogDir)) return [];
  const files = (await walk(catalogDir)).filter(
    (f) => f.endsWith(".md") && !/[\\/](README|index)\.md$/i.test(f),
  );
  const records = [];
  for (const f of files) {
    let text;
    try {
      text = await readFile(f, "utf8");
    } catch {
      continue;
    }
    const fm = parseFrontmatter(text);
    if (!fm.asset_id && !fm.title) continue;
    const assetId = fm.asset_id || "";
    let preview = "";
    if (assetId) {
      try {
        const dir = join(libraryPath, "previews", assetId);
        const pf = (await readdir(dir)).find((n) => /\.(png|jpg|jpeg|webp|gif)$/i.test(n));
        if (pf) preview = join(dir, pf);
      } catch { /* no previews dir for this asset */ }
    }
    records.push({
      asset_id: assetId,
      title: fm.title || assetId,
      description: fm.description || "",
      kind: fm.kind || "",
      status: fm.status || "",
      license: fm.license || "",
      origin: ORIGINS.includes(fm.origin) ? fm.origin : "unknown",
      tags: Array.isArray(fm.tags) ? fm.tags : fm.tags ? [fm.tags] : [],
      resource: fm.resource || "",
      filesDir: fm.resource ? join(libraryPath, fm.resource) : "",
      preview,
      catalogPath: f,
    });
  }
  return records;
}

function kindMatches(recKind, wanted) {
  if (!wanted) return true;
  const w = wanted.toLowerCase();
  return recKind === w || KIND_DIR[recKind] === w || `${recKind}s` === w;
}

// OR-match on tags for recall; query is a substring over the searchable text.
export function filterRecords(records, { tags = [], kind = "", origin = "", query = "" } = {}) {
  return records.filter((rec) => {
    if (!kindMatches(rec.kind, kind)) return false;
    if (origin && rec.origin !== origin) return false;
    const hay = `${rec.tags.join(" ")} ${rec.title} ${rec.description} ${rec.asset_id}`.toLowerCase();
    if (query && !hay.includes(query.toLowerCase())) return false;
    if (tags.length && !tags.some((t) => hay.includes(t.toLowerCase()))) return false;
    return true;
  });
}

export function parseArgs(argv) {
  const a = { library: DEFAULT_LIBRARY, tags: "", kind: "", origin: "", query: "", out: "", json: false, record: false, family: "", decision: "", reason: "" };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--json") { a.json = true; continue; }
    if (arg === "--record") { a.record = true; continue; }
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--library") a.library = next;
    else if (arg === "--tags") a.tags = next;
    else if (arg === "--kind") a.kind = next;
    else if (arg === "--origin") a.origin = next;
    else if (arg === "--query") a.query = next;
    else if (arg === "--out") a.out = next;
    else if (arg === "--family") a.family = next;
    else if (arg === "--decision") a.decision = next;
    else if (arg === "--reason") a.reason = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  if (a.origin && !ORIGINS.includes(a.origin)) throw new Error(`--origin must be one of: ${ORIGINS.join(", ")}`);
  return a;
}

export async function recordDecision(a) {
  const decisions = ["reuse", "source+intake", "generate", "procedural-debug"];
  if (!a.family) throw new Error("--record needs --family");
  if (!decisions.includes(a.decision)) throw new Error(`--decision must be one of: ${decisions.join(", ")}`);
  const out = a.out ? resolve(a.out) : join(repoRoot(), "tmp", "asset_source_decision.json");
  await mkdir(dirname(out), { recursive: true });
  let list = [];
  if (existsSync(out)) {
    try { list = JSON.parse(await readFile(out, "utf8")); } catch { list = []; }
    if (!Array.isArray(list)) list = [];
  }
  list = list.filter((d) => d.family !== a.family);
  list.push({ family: a.family, decision: a.decision, reason: a.reason || "" });
  await writeFile(out, JSON.stringify(list, null, 2), "utf8");
  console.log(`recorded source decision: ${a.family} -> ${a.decision}`);
  console.log(out);
}

function printFreeSources() {
  console.log("\nNo library match. Search these free, legal sources next (source before you generate):");
  for (const s of FREE_SOURCES) console.log(`  - ${s.name.padEnd(16)} ${s.url}  (${s.note})`);
  console.log("\nIf you download one: intake it with tools/assets/intake/ (records license+provenance, origin: sourced).");
  console.log("Only GENERATE what you could not source. Record the call:");
  console.log('  node tools/assets/source/find_assets.mjs --record --family "<name>" --decision generate --reason "<why nothing fit>"');
}

async function main() {
  const a = parseArgs(process.argv.slice(2));
  if (a.record) { await recordDecision(a); return; }
  const tags = a.tags ? a.tags.split(",").map((s) => s.trim()).filter(Boolean) : [];
  const records = await scanLibrary(a.library);
  const hits = filterRecords(records, { tags, kind: a.kind, origin: a.origin, query: a.query });
  if (a.json) { console.log(JSON.stringify({ library: a.library, total: records.length, hits }, null, 2)); return; }

  console.log(`library: ${a.library}  (catalog records: ${records.length})`);
  const crit = [tags.length ? `tags=${tags.join("|")}` : "", a.kind ? `kind=${a.kind}` : "", a.origin ? `origin=${a.origin}` : "", a.query ? `query="${a.query}"` : ""].filter(Boolean).join("  ");
  console.log(`search: ${crit || "(all)"}`);
  if (hits.length) {
    console.log(`\n${hits.length} match(es) — reuse before generating:\n`);
    for (const h of hits) {
      console.log(`  ${h.asset_id}  [${h.kind} | ${h.origin} | ${h.license || "license?"}]`);
      console.log(`    ${h.title} — ${h.description}`);
      console.log(`    tags: ${h.tags.join(", ")}`);
      console.log(`    files: ${h.resource}`);
    }
    console.log("\nCopy selected files into project-local assets/source/... (keep provenance). Do not load from the library directly.");
  } else {
    printFreeSources();
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((e) => { console.error(e.message); process.exit(1); });
}
