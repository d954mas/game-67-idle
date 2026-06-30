#!/usr/bin/env node
// Bulk-import poly.pizza bundles into the shared OKF asset library.
//
//   node tools/assets/source/import_poly_pizza.mjs --enumerate
//   node tools/assets/source/import_poly_pizza.mjs --bundle <slug> [--dry-run]
//   node tools/assets/source/import_poly_pizza.mjs --all [--limit N] [--concurrency 8]
//
// poly.pizza is a React/Next site but every bundle/model page is server-rendered,
// so we read it directly (node fetch — curl fails here under Avast/schannel). No
// API key needed. Each model gives: name+author (og:title), license (CC0/CC-BY),
// and a direct glb at https://static.poly.pizza/<uuid>.glb plus a small preview.
//
// Engine-ready: downloads the plain .glb (not the .glb.br brotli variant).
// Adequate storage: glb-only, reuses poly's small preview, dedups by model id
// (a model can appear in several bundles) AND by sha256 (vs the existing library,
// e.g. the Kenney furniture kit already imported directly).
import { createHash } from "node:crypto";
import { existsSync, readFileSync, writeFileSync, mkdirSync, rmSync, readdirSync, statSync } from "node:fs";
import { join, dirname } from "node:path";
import { DEFAULT_LIBRARY } from "./find_assets.mjs";
import { catalogFrontmatter } from "../../lib/asset_catalog.mjs";
import { isMain } from "../../lib/cli.mjs";

const SITE = "https://poly.pizza";
const CDN = "https://static.poly.pizza";
const UA = { "user-agent": "Mozilla/5.0 (asset-librarian; +local pipeline)" };
const CACHE = "tmp/poly_index.json";

function parseArgs(argv) {
  const a = { library: DEFAULT_LIBRARY, concurrency: 8, limit: 0, all: false, dryRun: false, enumerate: false, coversOnly: false, bundle: null };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    if (k === "--all") a.all = true;
    else if (k === "--dry-run") a.dryRun = true;
    else if (k === "--enumerate") a.enumerate = true;
    else if (k === "--covers-only") a.coversOnly = true;
    else if (k === "--bundle") a.bundle = argv[++i];
    else if (k === "--limit") a.limit = Number(argv[++i]) || 0;
    else if (k === "--concurrency") a.concurrency = Number(argv[++i]) || 8;
    else if (k === "--library") a.library = argv[++i];
  }
  return a;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function fetchText(url, tries = 4) {
  for (let t = 0; t < tries; t += 1) {
    try {
      const r = await fetch(url, { headers: UA });
      if (r.status === 404) return null;
      if (!r.ok) throw new Error("HTTP " + r.status);
      return await r.text();
    } catch (e) { if (t === tries - 1) throw e; await sleep(400 * (t + 1)); }
  }
}
async function fetchBuf(url, tries = 4) {
  for (let t = 0; t < tries; t += 1) {
    try {
      const r = await fetch(url, { headers: UA });
      if (r.status === 404) return null;
      if (!r.ok) throw new Error("HTTP " + r.status);
      return Buffer.from(await r.arrayBuffer());
    } catch (e) { if (t === tries - 1) throw e; await sleep(500 * (t + 1)); }
  }
}

async function pool(items, n, fn, onTick) {
  const out = new Array(items.length); let i = 0, done = 0;
  await Promise.all(Array.from({ length: Math.min(n, items.length) }, async () => {
    while (i < items.length) {
      const k = i++;
      try { out[k] = await fn(items[k], k); } catch (e) { out[k] = { error: String(e.message || e) }; }
      done += 1; if (onTick) onTick(done, items.length, out[k]);
    }
  }));
  return out;
}

const kebab = (s) => String(s || "").toLowerCase().normalize("NFKD").replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "").slice(0, 60) || "item";
// poly.pizza bundle slugs end with a 10-char random id ("Survival-Kit-yGnSPFp2lH");
// strip it for a stable, human pack name (the og:title parse is unreliable).
const POLY_ID = /-[A-Za-z0-9]{10}$/;
const prettyPack = (slug) => slug.replace(POLY_ID, "").replace(/-+/g, " ").trim() || slug;

// Meaningful CONTENT tags from a model name. Structural facets (kind/source/
// license/pack/author) are dedicated viewer facets, so they stay out of tags.
const TAG_STOP = new Set(["the", "a", "an", "of", "and", "or", "with", "for", "to", "in", "on", "by",
  "free", "model", "low", "poly", "lowpoly", "kit", "pack", "set", "mesh", "glb", "prop", "props", "variant", "var"]);
function contentTags(name) {
  const toks = String(name).replace(/([a-z0-9])([A-Z])/g, "$1 $2").toLowerCase()
    .split(/[^a-z0-9]+/).filter((t) => t.length >= 2 && !/^\d+$/.test(t) && !TAG_STOP.has(t));
  return [...new Set(toks)].slice(0, 6);
}
const sha256 = (buf) => createHash("sha256").update(buf).digest("hex");
const decodeEnt = (s) => String(s || "").replace(/&amp;/g, "&").replace(/&#x27;|&#39;/g, "'").replace(/&quot;/g, '"').replace(/&lt;/g, "<").replace(/&gt;/g, ">");

function licenseOf(html) {
  if (/CC-?BY/i.test(html) && !/CC0/i.test(html)) return { code: "CC-BY-4.0", slug: "cc-by-4-0", url: "https://creativecommons.org/licenses/by/4.0/", attribution: true };
  return { code: "CC0-1.0", slug: "cc0-1-0", url: "https://creativecommons.org/publicdomain/zero/1.0/", attribution: false };
}

// --- enumerate -------------------------------------------------------------
async function enumerateBundles(concurrency) {
  const html = await fetchText(SITE + "/bundles");
  const slugs = [...new Set([...html.matchAll(/\/bundle\/([A-Za-z0-9-]+)/g)].map((m) => m[1]))];
  const bundles = await pool(slugs, concurrency, async (slug) => {
    const page = await fetchText(SITE + "/bundle/" + slug);
    if (!page) return null;
    const ids = [...new Set([...page.matchAll(/\/m\/([A-Za-z0-9_-]+)/g)].map((m) => m[1]))];
    return { slug, title: prettyPack(slug), ids };
  });
  return bundles.filter((b) => b && b.ids.length);
}

async function modelMeta(id) {
  const html = await fetchText(SITE + "/m/" + id);
  if (!html) return null;
  const og = decodeEnt((html.match(/property="og:title" content="([^"]+)"/) || [])[1] || id);
  // "Bedroll Frame - Free Model By Kenney"
  let name = og, author = "";
  const m = og.match(/^(.*?)\s*-\s*Free Model\s*By\s*(.+?)\s*$/i) || og.match(/^(.*?)\s*By\s*(.+?)\s*$/i);
  if (m) { name = m[1].trim(); author = m[2].trim(); }
  const uuid = (html.match(/static\.poly\.pizza\/([0-9a-f-]{36})\.glb/) || [])[1];
  const lic = licenseOf(html);
  // preview: prefer webp, else the poster jpg/png (same uuid)
  let preview = (html.match(/https?:\/\/static\.poly\.pizza\/[^\s"')]+\.webp/) || [])[0]
    || (uuid ? CDN + "/" + uuid + ".jpg" : null);
  return { id, name, author, uuid, license: lic, preview };
}

// --- existing library index (for sha256 dedup) -----------------------------
function existingHashes(lib) {
  const map = new Map(); // sha256 -> asset_id
  const root = join(lib, "files", "models");
  if (!existsSync(root)) return map;
  const walk = (dir) => {
    for (const e of readdirSync(dir, { withFileTypes: true })) {
      const p = join(dir, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.name.toLowerCase().endsWith(".glb")) {
        try { map.set(sha256(readFileSync(p)), p); } catch { /* ignore */ }
      }
    }
  };
  walk(root);
  return map;
}

function writeRecord(lib, rec) {
  const { assetId, packDir, fileName, name, author, license, bytes, hash, sourceUrl, ts } = rec;
  const catDir = join(lib, "catalog", "models", packDir);
  mkdirSync(catDir, { recursive: true });
  const tags = contentTags(name);
  if (!tags.length) tags.push(kebab(name));
  const frontmatter = catalogFrontmatter({
    title: name,
    description: `model imported from poly.pizza${author ? " by " + author : ""}`,
    resource: `files/models/${packDir}/${assetId}/`,
    tags,
    timestamp: ts,
    assetId,
    kind: "model",
    status: "accepted",
    origin: "sourced",
    license: license.code,
    licenseUrl: license.url,
    attributionRequired: license.attribution,
    commercialUse: "true",
    modificationAllowed: "true",
    redistributionAllowed: "true",
    publish: "true",
    shippingDecision: license.attribution ? "allowed-with-attribution" : "allowed",
  }, `pack: ${packDir}\nauthor: ${author || "unknown"}`);
  const md = `${frontmatter}

# ${name}

## Provenance

- Source/vendor: poly.pizza${author ? " (model by " + author + ")" : ""}
- Origin: sourced
- Source page: ${sourceUrl}
- Imported at: ${ts}
- SHA256: ${hash}
- Bytes: ${bytes}

## License Decision

- License: ${license.code}
- License URL: ${license.url}
- Attribution required: ${license.attribution}${author ? "\n- Attribute to: " + author + " (via poly.pizza)" : ""}
- Commercial use: true
- Modification allowed: true
- Redistribution allowed: true
- Shipping decision: ${license.attribution ? "allowed-with-attribution" : "allowed"}

## Runtime Notes

- Import boundary: copy selected files into project-local \`assets/source/...\` before runtime use.
- Engine-ready glb (plain, not brotli). Multi-primitive meshes may need the pack step (engine scene import reads primitives[0] material).
`;
  writeFileSync(join(catDir, assetId + ".md"), md, "utf8");
}

function writePack(lib, pack) {
  const catDir = join(lib, "catalog", "models", pack.dir);
  mkdirSync(catDir, { recursive: true });
  const md = `---
type: Asset Pack
title: ${pack.title}
pack: ${pack.dir}
source: poly.pizza
kind: model
license: ${pack.mixedLicense ? "mixed (CC0 / CC-BY-4.0)" : pack.license.code}
license_url: ${pack.license.url}
origin: sourced
count: ${pack.count}
genre: [${pack.genre.join(", ")}]
style: [low-poly]
tags: [${pack.tags.join(", ")}]
cover: ${pack.cover || ""}
description: ${pack.title} - ${pack.count} low-poly models imported from poly.pizza.
timestamp: ${pack.ts}
attribution: ${pack.authors.join("; ") || "various"}
---

# ${pack.title}

- Source/vendor: poly.pizza
- Pack: ${pack.dir}
- Source page: ${SITE}/bundle/${pack.slug}
- License: ${pack.mixedLicense ? "mixed — see per-asset records (CC0 and CC-BY-4.0)" : pack.license.code}
- Assets: ${pack.count} (model)
- Origin: sourced
- Authors (attribution): ${pack.authors.join("; ") || "various"}
- Prepared: ${pack.ts}

${pack.title} — ${pack.count} low-poly models imported from poly.pizza. CC-BY models
require crediting their author (recorded per asset). Copy individual assets into a
project's \`assets/source/...\`; do not load from the library directly.
`;
  writeFileSync(join(catDir, "_pack.md"), md, "utf8");
}

// coarse genre guess from bundle title keywords (viewer facet only)
const GENRE_MAP = [
  [/space|sci.?fi|robot|alien|cyber|mech|galax/i, "sci-fi"],
  [/fantasy|dungeon|medieval|castle|magic|rpg|dragon/i, "fantasy"],
  [/pirate|nautical|ship|ocean/i, "pirates"],
  [/food|kitchen|cook|fruit|restaurant|cafe/i, "food"],
  [/furniture|house|home|room|office|interior/i, "interior"],
  [/city|town|building|street|urban|vehicle|car/i, "city"],
  [/nature|forest|tree|plant|garden|farm|animal/i, "nature"],
  [/halloween|holiday|christmas|xmas/i, "holiday"],
  [/weapon|gun|fps|military|war|shooter/i, "combat"],
];
const genreOf = (title) => { for (const [re, g] of GENRE_MAP) if (re.test(title)) return g; return "misc"; };

async function main() {
  const a = parseArgs(process.argv.slice(2));
  const lib = a.library;
  const ts = new Date().toISOString();

  // 1) enumerate (cached)
  let bundles;
  if (existsSync(CACHE) && !a.enumerate) {
    bundles = JSON.parse(readFileSync(CACHE, "utf8"));
    console.log("using cached index:", bundles.length, "bundles");
  } else {
    console.log("enumerating bundles from poly.pizza ...");
    bundles = await enumerateBundles(a.concurrency);
    mkdirSync(dirname(CACHE), { recursive: true });
    writeFileSync(CACHE, JSON.stringify(bundles), "utf8");
    const total = new Set(bundles.flatMap((b) => b.ids)).size;
    console.log("bundles:", bundles.length, "| unique models:", total, "| cached ->", CACHE);
    if (a.enumerate) return;
  }

  if (a.bundle) bundles = bundles.filter((b) => b.slug === a.bundle || kebab(b.title) === a.bundle);
  if (!bundles.length) { console.error("no matching bundle"); process.exit(1); }

  // covers-only: fetch each bundle's poly.pizza preview image as the PACK cover
  // (build_review looks for previews/<pack>/cover.png). Models get our own render.
  if (a.coversOnly) {
    let got = 0;
    await pool(bundles, a.concurrency, async (b) => {
      const page = await fetchText(SITE + "/bundle/" + b.slug);
      if (!page) return;
      let img = decodeEnt((page.match(/property="og:image" content="([^"]+)"/) || [])[1] || "");
      // poly.pizza emits a malformed og:image with a doubled CDN prefix
      // ("https://static.poly.pizza/https://static.poly.pizza/listimg/x.webp")
      img = img.replace(/^https?:\/\/static\.poly\.pizza\/(?=https?:\/\/)/, "");
      if (!img) return;
      const buf = await fetchBuf(img).catch(() => null);
      if (!buf) return;
      const dir = join(lib, "previews", kebab(prettyPack(b.slug)));
      mkdirSync(dir, { recursive: true });
      writeFileSync(join(dir, "cover.png"), buf);
      got += 1;
    }, (d, t) => process.stdout.write(`\r  covers ${d}/${t} (${got} saved)   `));
    process.stdout.write("\n");
    console.log("pack covers saved:", got);
    return;
  }

  // 2) build unique model -> primary pack (first bundle wins)
  const seenModel = new Map(); // id -> packDir
  const work = []; // {id, slug, packDir, packTitle}
  const packTitleByDir = new Map();
  for (const b of bundles) {
    const dir = kebab(b.title);
    if (!packTitleByDir.has(dir)) packTitleByDir.set(dir, { slug: b.slug, title: b.title });
    for (const id of b.ids) {
      if (seenModel.has(id)) continue;
      seenModel.set(id, dir);
      work.push({ id, slug: b.slug, packDir: dir, packTitle: b.title });
    }
  }
  let todo = work;
  if (a.limit) todo = todo.slice(0, a.limit);
  console.log("models to consider:", todo.length, a.dryRun ? "(dry-run)" : "");

  // 3) preload existing hashes for cross-library dedup
  const hashes = existingHashes(lib);
  console.log("existing library glb (for dedup):", hashes.size);

  const stats = { imported: 0, skippedExisting: 0, dupHash: 0, dupModel: 0, noGlb: 0, errors: 0, bytes: 0, ccby: [] };
  const packAgg = new Map(); // dir -> {authors:Set, licenses:Set, count, cover, ccby:bool}

  await pool(todo, a.concurrency, async (w) => {
    // resumable: if any record for this model id already exists, skip
    const catDir = join(lib, "catalog", "models", w.packDir);
    const already = existsSync(catDir) && readdirSync(catDir).some((f) => f.includes(kebab(w.id)) && f.endsWith(".md"));
    if (already) { stats.skippedExisting += 1; return; }

    const meta = await modelMeta(w.id);
    if (!meta || !meta.uuid) { stats.noGlb += 1; return; }
    const assetId = `polypizza__${kebab(meta.name)}-${kebab(w.id)}__${meta.license.slug}`;
    const fileName = kebab(meta.name) + ".glb";

    if (a.dryRun) {
      stats.imported += 1;
      const pa = packAgg.get(w.packDir) || { authors: new Set(), count: 0, cover: "", ccby: false };
      pa.count += 1; if (meta.author) pa.authors.add(meta.author); if (!pa.cover) pa.cover = assetId; if (meta.license.attribution) pa.ccby = true;
      packAgg.set(w.packDir, pa);
      return;
    }

    const glb = await fetchBuf(CDN + "/" + meta.uuid + ".glb");
    if (!glb) { stats.noGlb += 1; return; }
    const hash = sha256(glb);
    if (hashes.has(hash)) { stats.dupHash += 1; return; } // identical to something already in the library
    hashes.set(hash, assetId);

    const filesDir = join(lib, "files", "models", w.packDir, assetId);
    mkdirSync(filesDir, { recursive: true });
    writeFileSync(join(filesDir, fileName), glb);

    // Per-model previews are rendered with our shared studio HDR (one consistent
    // look across the whole library) by ai_studio/assets/assets_storage/preview_pipeline/render_library_previews.mjs,
    // NOT downloaded from poly.pizza. Pack covers DO use poly's image (--covers-only).

    writeRecord(lib, { assetId, packDir: w.packDir, fileName, name: meta.name, author: meta.author, license: meta.license, bytes: glb.length, hash, sourceUrl: SITE + "/m/" + w.id, ts });

    stats.imported += 1; stats.bytes += glb.length;
    if (meta.license.attribution) stats.ccby.push(assetId + " — " + (meta.author || "?"));
    const pa = packAgg.get(w.packDir) || { authors: new Set(), count: 0, cover: "", ccby: false };
    pa.count += 1; if (meta.author) pa.authors.add(meta.author); if (!pa.cover) pa.cover = assetId; if (meta.license.attribution) pa.ccby = true;
    packAgg.set(w.packDir, pa);
  }, (done, total, res) => {
    if (done % 50 === 0 || done === total) {
      process.stdout.write(`\r  ${done}/${total}  imported=${stats.imported} dup=${stats.dupHash + stats.skippedExisting} noglb=${stats.noGlb} ${Math.round(stats.bytes / 1024 / 1024)}MB   `);
    }
  });
  process.stdout.write("\n");

  // 4) write/refresh pack records + licenses
  if (!a.dryRun) {
    for (const [dir, pa] of packAgg) {
      const meta = packTitleByDir.get(dir);
      const lic = pa.ccby ? { code: "CC-BY-4.0", url: "https://creativecommons.org/licenses/by/4.0/" } : { code: "CC0-1.0", url: "https://creativecommons.org/publicdomain/zero/1.0/" };
      writePack(lib, { dir, slug: meta.slug, title: meta.title, count: pa.count, cover: pa.cover, authors: [...pa.authors], genre: [genreOf(meta.title)], tags: [genreOf(meta.title)], license: lic, mixedLicense: pa.ccby, ts });
      // pack license note
      const licDir = join(lib, "licenses", dir);
      mkdirSync(licDir, { recursive: true });
      writeFileSync(join(licDir, "license.md"), `# License — ${meta.title}\n\nImported from poly.pizza (${SITE}/bundle/${meta.slug}).\nModels are CC0 or CC-BY-4.0; see each asset record. CC-BY models must credit:\n\n${[...pa.authors].map((x) => "- " + x).join("\n") || "- various"}\n`, "utf8");
    }
  }

  console.log("\n=== poly.pizza import summary ===");
  console.log(JSON.stringify({ imported: stats.imported, skippedExisting: stats.skippedExisting, dupHash: stats.dupHash, noGlb: stats.noGlb, packs: packAgg.size, totalMB: Math.round(stats.bytes / 1024 / 1024), ccbyCount: stats.ccby.length }, null, 2));
  if (stats.ccby.length) {
    mkdirSync("tmp", { recursive: true });
    writeFileSync("tmp/poly_ccby_attribution.txt", stats.ccby.join("\n"), "utf8");
    console.log("CC-BY attribution list -> tmp/poly_ccby_attribution.txt (", stats.ccby.length, "models )");
  }
}

if (isMain(import.meta.url)) {
  main().catch((e) => { console.error("\nFATAL:", e.message); process.exit(1); });
}

export { kebab, licenseOf, genreOf, modelMeta, enumerateBundles };
