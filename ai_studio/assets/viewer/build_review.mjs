#!/usr/bin/env node
// Asset viewer - one universal tool, two modes, self-contained served output.
//
//   review : after a game, gallery of NEW assets the run brought in
//            (origin-tagged) with checkboxes -> pick keepers to promote/export.
//            Writes review-manifest.json.
//   library: browse ALL shared-library assets - filter by kind/origin/pack,
//            search, thumbnails + an interactive 3D viewer for models.
//
// Media (previews + .glb) is copied into <out>/media and referenced RELATIVELY,
// so the page works opened locally AND served over a tunnel (no file:// refs).
//
//   node ai_studio/assets/viewer/build_review.mjs --mode library
//   node ai_studio/assets/viewer/build_review.mjs --mode review --game little-lives --base clean-seed
import { execFileSync } from "node:child_process";
import { readFile, readdir, writeFile, mkdir, cp } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve, dirname, basename, extname, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { scanPackManifestSource } from "../storage/manifests/manifest.mjs";
import { defaultLibrarySourceRoot } from "../storage/sources/libraries.mjs";
import { isMain } from "../../core_harness/tool_lib/cli.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));

const PRIMARY_EXT = {
  image: [".png", ".jpg", ".jpeg", ".webp", ".gif"],
  model: [".obj", ".glb", ".gltf", ".fbx"],
  font: [".ttf", ".otf", ".woff", ".woff2"],
  audio: [".wav", ".mp3", ".ogg"],
};
const VENDORS = ["kenney", "quaternius", "polyhaven", "poly-haven", "ambientcg", "poly-pizza", "opengameart"];
const ORIGINS = ["mine", "ai", "sourced", "unknown"];
const UI_PATH = /[\\/](ui|icons?|hud|gui|sprites?|buttons?)[\\/]/i;

function kindForExt(ext) {
  for (const [kind, exts] of Object.entries(PRIMARY_EXT)) if (exts.includes(ext)) return kind;
  return null;
}

// Map a discovered file to a real library kind (model|texture|ui|font|audio).
function libraryKind(ext, relPath) {
  const broad = kindForExt(ext);
  if (broad === "image") return UI_PATH.test(relPath) ? "ui" : "texture";
  return broad;
}

const safeName = (s) => String(s).replace(/[^a-zA-Z0-9_.-]/g, "_");

async function walk(dir) {
  const out = [];
  let entries;
  try { entries = await readdir(dir, { withFileTypes: true }); } catch { return out; }
  for (const e of entries) {
    const p = join(dir, e.name);
    if (e.isDirectory()) out.push(...(await walk(p)));
    else out.push(p);
  }
  return out;
}

// origin inferred conservatively; a bare license file is NOT enough for "sourced".
function detectOrigin(relPath, dirFiles) {
  const lower = relPath.toLowerCase();
  if (/[\\/](generated|imagegen|ai[-_]?gen|gen)[\\/]/.test(lower)) return "ai";
  if (/[\\/]source[\\/]/.test(lower) || VENDORS.some((v) => lower.includes(v))) return "sourced";
  return "unknown";
}

function findLicense(dirFiles) {
  return dirFiles.find((f) => /license|licence/i.test(basename(f))) || "";
}

async function discoverGameAssets({ base, repo }) {
  let names = [];
  try {
    const out = execFileSync(
      "git",
      ["-c", "core.quotepath=false", "diff", "-z", "--diff-filter=AR", "--name-only", `${base}..HEAD`, "--", "assets/"],
      { cwd: repo, encoding: "utf8" },
    );
    names = out.split("\0").map((s) => s.trim()).filter(Boolean);
  } catch (e) {
    throw new Error(`git diff ${base}..HEAD failed: ${e.message}`);
  }
  const dirCache = new Map();
  const assets = [];
  for (const rel of names) {
    const ext = extname(rel).toLowerCase();
    const kind = libraryKind(ext, rel);
    if (!kind) continue;
    const abs = join(repo, rel);
    if (!existsSync(abs)) continue;
    const dir = dirname(abs);
    if (!dirCache.has(dir)) dirCache.set(dir, await walk(dir));
    const dirFiles = dirCache.get(dir);
    let origin = detectOrigin(rel, dirFiles);
    const sidecar = dirFiles.find((f) => /\.(origin|provenance)\.json$/i.test(f));
    if (sidecar) {
      try { const j = JSON.parse(await readFile(sidecar, "utf8")); if (ORIGINS.includes(j.origin)) origin = j.origin; } catch { /* ignore */ }
    }
    const licenseFile = findLicense(dirFiles);
    const vendor = VENDORS.find((v) => rel.toLowerCase().includes(v)) || (origin === "ai" ? "ai-generated" : "unknown");
    assets.push({
      id: rel.replace(/[\\/]/g, "__"), name: basename(rel), relpath: rel, abspath: abs,
      kind, ext, origin, source: vendor,
      license: licenseFile ? "see license file" : "unknown",
      licenseFile: licenseFile ? relative(repo, licenseFile).replace(/\\/g, "/") : "",
    });
  }
  return assets;
}

const GLB_EXT = [".glb", ".gltf"];
const IMG_EXT = [".png", ".jpg", ".jpeg", ".webp", ".gif"];

const relPosix = (root, abs) => relative(root, abs).replace(/\\/g, "/");

// Build display cards + their media URLs. Default: COPY media into <mediaDir>
// (self-contained, tunnel-safe). ref mode: REFERENCE library files in place via
// a `lib/` prefix (a 2-root server maps it) - for huge libraries where copying
// every .glb would be hundreds of MB.
async function buildLibraryCards(records, { mediaDir, ref = false, libRoot = "" }) {
  const cards = [];
  for (const r of records) {
    const card = {
      id: r.asset_id, name: (r.title || r.asset_id).replace(/\.(glb|gltf|png|ttf)$/i, ""),
      kind: r.kind || "asset", origin: r.origin, pack: r.pack || "",
      source: (r.asset_id.split("__")[0]) || "", license: r.license || "", tags: r.tags || [],
      sourceId: r.source_id || "", genre: [], style: [], thumb: "", model: "",
    };
    if (r.preview && existsSync(r.preview)) {
      if (ref) card.thumb = `lib/${relPosix(libRoot, r.preview)}`;
      else { const dst = `${safeName(r.asset_id)}.png`; try { await cp(r.preview, join(mediaDir, dst)); card.thumb = `media/${dst}`; } catch { /* skip */ } }
    }
    if (r.kind === "model" && r.modelPath && existsSync(r.modelPath)) {
      if (ref) card.model = `lib/${relPosix(libRoot, r.modelPath)}`;
      else { const dst = `${safeName(r.asset_id)}.glb`; await cp(r.modelPath, join(mediaDir, dst)); card.model = `media/${dst}`; }
    }
    cards.push(card);
  }
  return cards;
}

async function buildReviewCards(assets, mediaDir) {
  const cards = [];
  for (const x of assets) {
    const card = {
      id: x.id, name: x.name, kind: x.kind, origin: x.origin, pack: "",
      source: x.source, license: x.license, tags: [], relpath: x.relpath, sourceId: "", thumb: "", model: "",
    };
    const ext = extname(x.abspath).toLowerCase();
    if (IMG_EXT.includes(ext)) {
      const dst = `${safeName(x.id)}.png`;
      try { await cp(x.abspath, join(mediaDir, dst)); card.thumb = `media/${dst}`; } catch { /* skip */ }
    } else if (GLB_EXT.includes(ext)) {
      const dst = `${safeName(x.id)}.glb`;
      try { await cp(x.abspath, join(mediaDir, dst)); card.model = `media/${dst}`; } catch { /* skip */ }
    }
    cards.push(card);
  }
  return cards;
}

// Scan a folder tree for asset files (no manifest/git needed) - "see all assets in
// this game/dir". Builds cards + a pick manifest (relpaths for promote).
async function buildScanCards(root, mediaDir, repo) {
  const files = (await walk(root)).filter((f) => kindForExt(extname(f).toLowerCase()));
  const dirCache = new Map();
  const cards = [];
  const assets = [];
  for (const abs of files) {
    const rel = relative(repo, abs).replace(/\\/g, "/");
    const ext = extname(abs).toLowerCase();
    const kind = libraryKind(ext, rel);
    const dir = dirname(abs);
    if (!dirCache.has(dir)) dirCache.set(dir, await walk(dir));
    const dirFiles = dirCache.get(dir);
    const origin = detectOrigin(rel, dirFiles);
    const licenseFile = findLicense(dirFiles);
    const source = VENDORS.find((v) => rel.toLowerCase().includes(v)) || "unknown";
    const id = rel.replace(/[\\/]/g, "__");
    const card = { id, name: basename(abs), kind, origin, pack: "", source, license: licenseFile ? "see license" : "unknown", tags: [], relpath: rel, sourceId: "", thumb: "", model: "" };
    if (IMG_EXT.includes(ext)) { const dst = `${safeName(id)}.png`; try { await cp(abs, join(mediaDir, dst)); card.thumb = `media/${dst}`; } catch { /* skip */ } }
    else if (GLB_EXT.includes(ext)) { const dst = `${safeName(id)}.glb`; try { await cp(abs, join(mediaDir, dst)); card.model = `media/${dst}`; } catch { /* skip */ } }
    cards.push(card);
    assets.push({ id, name: basename(abs), relpath: rel, kind, origin, source, licenseFile: licenseFile ? relative(repo, licenseFile).replace(/\\/g, "/") : "" });
  }
  return { cards, assets };
}

const ORIGIN_COLOR = { mine: "#7dd3fc", ai: "#f0abfc", sourced: "#86efac", unknown: "#9ca3af" };
const escHtml = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
// Safe to embed inside a <script>: neutralize </script>/<!-- breakout + JS line separators.
const safeJson = (v) => JSON.stringify(v).replace(/</g, "\\u003c").replace(new RegExp("[\\u2028\\u2029]", "g"), (c) => "\\u" + c.charCodeAt(0).toString(16));

// renderHtml emits a tiny shell; the SPA lives in viewer.js/viewer.css (copied
// to <out> by main), so the page is self-contained and works locally + over a
// tunnel. Untrusted data goes in via safeJson; the title is escaped.
function renderHtml({ mode, title, cards = [], packs = [] }) {
  const review = mode === "review" || mode === "scan";
  const payload = safeJson({ assets: cards, packs, opts: { mode, review, title } });
  return `<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${escHtml(title)}</title>
<link rel="stylesheet" href="viewer.css">
<script type="module" src="https://unpkg.com/@google/model-viewer/dist/model-viewer.min.js"></script>
</head><body>
<script>window.__VIEWER__=${payload};</script>
<script src="viewer.js"></script>
</body></html>`;
}

function parseArgs(argv) {
  const a = { mode: "library", game: "", base: "clean-seed", library: defaultLibrarySourceRoot(process.cwd()), repo: process.cwd(), out: "tmp/asset-review", path: "", ref: false };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--ref") { a.ref = true; continue; } // reference library files in place (no media copy) - for huge libraries
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--mode") a.mode = next;
    else if (arg === "--game") a.game = next;
    else if (arg === "--base") a.base = next;
    else if (arg === "--library") a.library = next;
    else if (arg === "--repo") a.repo = next;
    else if (arg === "--out") a.out = next;
    else if (arg === "--path") a.path = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  return a;
}

async function main() {
  const a = parseArgs(process.argv.slice(2));
  const outDir = resolve(a.out);
  const mediaDir = join(outDir, "media");
  await mkdir(mediaDir, { recursive: true });
  let cards, title, meta;
  let packs = [];
  if (a.mode === "review") {
    const assets = await discoverGameAssets({ base: a.base, repo: resolve(a.repo) });
    cards = await buildReviewCards(assets, mediaDir);
    title = `Asset review - ${a.game || "game"}`;
    const byOrigin = ORIGINS.map((o) => `${o}: ${cards.filter((x) => x.origin === o).length}`).filter((s) => !s.endsWith(": 0")).join(" В· ");
    meta = `${cards.length} new assets since ${a.base} - ${byOrigin}. Check keepers, send ids to the lead to export.`;
    await writeFile(join(outDir, "review-manifest.json"), JSON.stringify({ game: a.game, base: a.base, generated: new Date().toISOString(), assets }, null, 2), "utf8");
  } else if (a.mode === "scan") {
    const root = resolve(a.path || join(a.repo, "assets"));
    const { cards: c, assets } = await buildScanCards(root, mediaDir, resolve(a.repo));
    cards = c;
    title = `Game assets - ${a.game || basename(root)}`;
    const byKind = [...new Set(cards.map((x) => x.kind))].sort().map((k) => `${k}: ${cards.filter((x) => x.kind === k).length}`).join(" В· ");
    meta = `${cards.length} assets under ${root} - ${byKind}. Pick keepers to export to the library.`;
    await writeFile(join(outDir, "review-manifest.json"), JSON.stringify({ game: a.game, base: "scan", root, generated: new Date().toISOString(), assets }, null, 2), "utf8");
  } else {
    const { records, packs: packMeta } = await scanPackManifestSource(a.library);
    const libRootEarly = resolve(a.library);
    cards = await buildLibraryCards(records, { mediaDir, ref: a.ref, libRoot: libRootEarly });
    const pm = new Map(packMeta.map((p) => [p.pack, p]));
    for (const c of cards) { const p = pm.get(c.pack); c.genre = p ? p.genre : []; c.style = p ? p.style : []; }
    const libRoot = resolve(a.library);
    packs = [];
    for (const p of packMeta) {
      const members = cards.filter((c) => c.pack === p.pack);
      const thumbs = [];
      const isImg = (s) => /\.(png|jpg|jpeg|webp)$/i.test(s || "");
      const coverMember = p.cover && !isImg(p.cover) ? members.find((c) => c.id === p.cover) : null;
      if (coverMember && coverMember.thumb) thumbs.push(coverMember.thumb);
      for (const c of members) if (c.thumb && !thumbs.includes(c.thumb)) thumbs.push(c.thumb);
      // prefer a real vendor pack cover (the kit's "all assets" preview)
      let coverImg = "";
      const candidates = [];
      if (isImg(p.cover)) candidates.push(join(libRoot, p.cover));
      candidates.push(join(libRoot, "previews", p.pack, "cover.png"));
      for (const src of candidates) {
        if (existsSync(src)) {
          if (a.ref) coverImg = `lib/${relPosix(libRoot, src)}`;
          else { const dst = `pack__${safeName(p.pack)}.png`; await cp(src, join(mediaDir, dst)); coverImg = `media/${dst}`; }
          break;
        }
      }
      packs.push({ ...p, count: p.count || members.length, covers: thumbs.slice(0, 4), coverImg });
    }
    title = "Asset library";
    meta = `${cards.length} records В· ${packs.length} packs В· ${cards.filter((c) => c.model).length} 3D models`;
  }
  await cp(join(HERE, "viewer.js"), join(outDir, "viewer.js"));
  await cp(join(HERE, "viewer.css"), join(outDir, "viewer.css"));
  // shared studio HDR: model-viewer's environment-image, same source the PNG
  // thumbnails are baked with, so preview and live 3D share one light.
  const hdr = join(HERE, "..", "storage", "previews", "studio_env.hdr");
  if (existsSync(hdr)) await cp(hdr, join(outDir, "studio_env.hdr"));
  const indexPath = join(outDir, "index.html");
  await writeFile(indexPath, renderHtml({ mode: a.mode, title, cards, packs }), "utf8");
  const pickable = a.mode === "review" || a.mode === "scan";
  console.log(JSON.stringify({ mode: a.mode, assets: cards.length, html: indexPath, manifest: pickable ? join(outDir, "review-manifest.json") : null }, null, 2));
}

if (isMain(import.meta.url)) {
  main().catch((e) => { console.error(e.message); process.exit(1); });
}

export { discoverGameAssets, detectOrigin, kindForExt, libraryKind, buildLibraryCards, buildReviewCards, buildScanCards, renderHtml, escHtml, safeJson };
