#!/usr/bin/env node
// Source-first asset search (keystone of the source-before-generate rule).
//
// Before generating or proceduralizing art, search the shared asset library
// catalog. On a miss, this prints the canonical free CC0/OFL sources so the
// agent searches those next, and only generates what cannot be sourced.
//
// It also exports scanLibrary() so the asset viewer reuses one catalog reader.
//
//   node ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs --tags "sofa,furniture" --kind model
//   node ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs --query robot --origin sourced --json
//   node ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs --record --family "room-furniture" \
//        --decision source+intake --reason "Kenney furniture kit fits the diorama"
import { readFile, readdir, writeFile, mkdir } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve, dirname, basename, relative } from "node:path";
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

function list(value) {
  return Array.isArray(value) ? value.filter(Boolean) : value ? [value] : [];
}

function uniqueList(...values) {
  return [...new Set(values.flatMap(list).map(String).filter(Boolean))];
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

async function mapLimit(items, limit, worker) {
  const out = new Array(items.length);
  let next = 0;
  const workers = Array.from({ length: Math.min(limit, items.length) }, async () => {
    while (next < items.length) {
      const index = next;
      next += 1;
      out[index] = await worker(items[index], index);
    }
  });
  await Promise.all(workers);
  return out;
}

async function previewMap(libraryPath) {
  const previewsDir = join(libraryPath, "previews");
  if (!existsSync(previewsDir)) return new Map();
  const media = (await walk(previewsDir)).filter((f) => /\.(png|jpg|jpeg|webp|gif)$/i.test(f)).sort();
  const map = new Map();
  for (const file of media) {
    const [assetId] = relative(previewsDir, file).split(/[\\/]/).filter(Boolean);
    if (assetId && !map.has(assetId)) map.set(assetId, file);
  }
  return map;
}

async function modelMap(libraryPath, knownFiles = null) {
  const filesDir = join(libraryPath, "files");
  if (!existsSync(filesDir)) return new Map();
  const files = Array.isArray(knownFiles) ? knownFiles : await walk(filesDir);
  const models = files.filter((f) => /\.(glb|gltf)$/i.test(f)).sort();
  const map = new Map();
  for (const file of models) {
    const dir = dirname(file);
    if (!map.has(dir)) map.set(dir, file);
  }
  return map;
}

async function parsePackFiles(files) {
  const packs = await mapLimit(files, 24, async (f) => {
    let text;
    try { text = await readFile(f, "utf8"); } catch { return null; }
    const fm = parseFrontmatter(text);
    const m = text.match(/^п»ї?---\r?\n[\s\S]*?\r?\n---\r?\n([\s\S]*)$/);
    return {
      pack: fm.pack || basename(dirname(f)),
      title: fm.title || fm.pack || "",
      source: fm.source || "",
      kind: fm.kind || "",
      license: fm.license || "",
      license_url: fm.license_url || "",
      origin: ORIGINS.includes(fm.origin) ? fm.origin : "unknown",
      count: Number(fm.count) || 0,
      genre: list(fm.genre),
      style: list(fm.style),
      tags: list(fm.tags),
      cover: fm.cover || "",
      description: fm.description || "",
      body: m ? m[1].trim() : "",
      catalogDir: dirname(f),
    };
  });
  return packs.filter(Boolean);
}

// Read every catalog/**/*.md record into a normalized list (one reader for the
// search CLI and the library viewer).
export async function scanLibraryWithPacks(libraryPath = DEFAULT_LIBRARY, options = {}) {
  const catalogDir = join(libraryPath, "catalog");
  if (!existsSync(catalogDir)) return { records: [], packs: [] };
  const catalogFiles = Array.isArray(options.catalogFiles) ? options.catalogFiles : await walk(catalogDir);
  const files = catalogFiles.filter(
    (f) => f.endsWith(".md") && !/[\\/](README|index)\.md$/i.test(f) && !/[\\/]_[^\\/]*\.md$/.test(f),
  );
  const packFiles = catalogFiles.filter((f) => /[\\/]_pack\.md$/i.test(f));
  // Pack genre/style live only on the _pack.md unit record; join them onto each
  // asset so individual assets are discoverable by genre (sci-fi/fantasy/food).
  const packs = await parsePackFiles(packFiles);
  const packMeta = new Map(packs.map((p) => [p.pack, p]));
  const previews = await previewMap(libraryPath);
  const models = await modelMap(libraryPath, options.assetFiles);
  const records = await mapLimit(files, 64, async (f) => {
    let text;
    try {
      text = await readFile(f, "utf8");
    } catch {
      return null;
    }
    const fm = parseFrontmatter(text);
    if (!fm.asset_id && !fm.title) return null;
    const assetId = fm.asset_id || "";
    const filesDir = fm.resource ? join(libraryPath, fm.resource) : "";
    const pm = packMeta.get(fm.pack || "");
    return {
      asset_id: assetId,
      title: fm.title || assetId,
      description: fm.description || "",
      kind: fm.kind || "",
      status: fm.status || "",
      license: fm.license || "",
      origin: ORIGINS.includes(fm.origin) ? fm.origin : "unknown",
      pack: fm.pack || "",
      source_id: fm.source_id || "",
      author: fm.author || "",
      packs: uniqueList(fm.pack, fm.packs, fm.member_of, fm.bundles),
      tags: Array.isArray(fm.tags) ? fm.tags : fm.tags ? [fm.tags] : [],
      genre: pm ? pm.genre : [],
      style: pm ? pm.style : [],
      resource: fm.resource || "",
      filesDir,
      modelPath: filesDir ? models.get(filesDir) || "" : "",
      preview: assetId ? previews.get(assetId) || "" : "",
      catalogPath: f,
    };
  });
  return { records: records.filter(Boolean), packs };
}

export async function scanLibrary(libraryPath = DEFAULT_LIBRARY) {
  return (await scanLibraryWithPacks(libraryPath)).records;
}

// Read pack unit records (catalog/**/_pack.md) — the bundle metadata that
// scanLibrary deliberately skips. Powers the packs/bundles overview.
export async function scanPacks(libraryPath = DEFAULT_LIBRARY) {
  const catalogDir = join(libraryPath, "catalog");
  if (!existsSync(catalogDir)) return [];
  const files = (await walk(catalogDir)).filter((f) => /[\\/]_pack\.md$/i.test(f));
  const list = (v) => (Array.isArray(v) ? v : v ? [v] : []);
  const packs = [];
  for (const f of files) {
    let text;
    try { text = await readFile(f, "utf8"); } catch { continue; }
    const fm = parseFrontmatter(text);
    const m = text.match(/^﻿?---\r?\n[\s\S]*?\r?\n---\r?\n([\s\S]*)$/);
    packs.push({
      pack: fm.pack || basename(dirname(f)),
      title: fm.title || fm.pack || "",
      source: fm.source || "",
      kind: fm.kind || "",
      license: fm.license || "",
      license_url: fm.license_url || "",
      origin: ORIGINS.includes(fm.origin) ? fm.origin : "unknown",
      count: Number(fm.count) || 0,
      genre: list(fm.genre),
      style: list(fm.style),
      tags: list(fm.tags),
      cover: fm.cover || "",
      description: fm.description || "",
      body: m ? m[1].trim() : "",
      catalogDir: dirname(f),
    });
  }
  return packs;
}

function kindMatches(recKind, wanted) {
  if (!wanted) return true;
  const w = wanted.toLowerCase();
  return recKind === w || KIND_DIR[recKind] === w || `${recKind}s` === w;
}

// OR-match on tags for recall; query is a substring over the searchable text;
// genre is an exact (case-insensitive) match against the asset's pack genre.
export function filterRecords(records, { tags = [], kind = "", origin = "", query = "", genre = "" } = {}) {
  return records.filter((rec) => {
    if (!kindMatches(rec.kind, kind)) return false;
    if (origin && rec.origin !== origin) return false;
    if (genre && !(rec.genre || []).map((g) => g.toLowerCase()).includes(genre.toLowerCase())) return false;
    const hay = `${rec.tags.join(" ")} ${(rec.genre || []).join(" ")} ${(rec.style || []).join(" ")} ${rec.title} ${rec.description} ${rec.asset_id} ${rec.pack}`.toLowerCase();
    if (query && !hay.includes(query.toLowerCase())) return false;
    if (tags.length && !tags.some((t) => hay.includes(t.toLowerCase()))) return false;
    return true;
  });
}

export function parseArgs(argv) {
  const a = { library: DEFAULT_LIBRARY, tags: "", kind: "", origin: "", query: "", genre: "", out: "", json: false, record: false, family: "", decision: "", reason: "" };
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
    else if (arg === "--genre") a.genre = next;
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
  console.log('  node ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs --record --family "<name>" --decision generate --reason "<why nothing fit>"');
}

export async function main() {
  const a = parseArgs(process.argv.slice(2));
  if (a.record) { await recordDecision(a); return; }
  const tags = a.tags ? a.tags.split(",").map((s) => s.trim()).filter(Boolean) : [];
  const records = await scanLibrary(a.library);
  const hits = filterRecords(records, { tags, kind: a.kind, origin: a.origin, query: a.query, genre: a.genre });
  if (a.json) { console.log(JSON.stringify({ library: a.library, total: records.length, hits }, null, 2)); return; }

  console.log(`library: ${a.library}  (catalog records: ${records.length})`);
  const crit = [tags.length ? `tags=${tags.join("|")}` : "", a.kind ? `kind=${a.kind}` : "", a.genre ? `genre=${a.genre}` : "", a.origin ? `origin=${a.origin}` : "", a.query ? `query="${a.query}"` : ""].filter(Boolean).join("  ");
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
