#!/usr/bin/env node
// Asset viewer — one tool, two modes.
//
//   review : after a game, gallery of NEW assets the run brought in
//            (origin-tagged mine/ai/sourced) with checkboxes -> pick keepers
//            to promote into the shared library. Writes review-manifest.json.
//   library: browse the whole shared library to find/reuse assets, filter by
//            origin/kind/tags, search.
//
//   node tools/asset_review/build_review.mjs --mode review  --game little-lives --base clean-seed
//   node tools/asset_review/build_review.mjs --mode library
import { execFileSync } from "node:child_process";
import { readFile, readdir, writeFile, mkdir } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve, dirname, basename, extname, relative } from "node:path";
import { pathToFileURL } from "node:url";
import { scanLibrary, DEFAULT_LIBRARY, ORIGINS, KIND_DIR } from "../assets/source/find_assets.mjs";

const PRIMARY_EXT = {
  image: [".png", ".jpg", ".jpeg", ".webp", ".gif"],
  model: [".obj", ".glb", ".gltf", ".fbx"],
  font: [".ttf", ".otf", ".woff", ".woff2"],
  audio: [".wav", ".mp3", ".ogg"],
};
const VENDORS = ["kenney", "quaternius", "polyhaven", "poly-haven", "ambientcg", "poly-pizza", "opengameart"];
const UI_PATH = /[\\/](ui|icons?|hud|gui|sprites?|buttons?)[\\/]/i;

// broad type by extension (image|model|font|audio|null for sidecars)
function kindForExt(ext) {
  for (const [kind, exts] of Object.entries(PRIMARY_EXT)) if (exts.includes(ext)) return kind;
  return null;
}

// Map a discovered file to a real library kind (model|texture|ui|font|audio) so
// the manifest carries the kind promote.mjs files it under — no remap needed.
function libraryKind(ext, relPath) {
  const broad = kindForExt(ext);
  if (broad === "image") return UI_PATH.test(relPath) ? "ui" : "texture";
  return broad; // model | font | audio | null
}

function fileUrl(absPath) {
  return pathToFileURL(resolve(absPath)).href;
}

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

// origin is recorded at creation time elsewhere; here we infer conservatively
// from on-disk evidence and leave "unknown" rather than guessing mine-vs-ai.
// A license file ALONE is not enough for "sourced" (the user's own art may ship
// with a license); we require a vendor token or a /source/ path to corroborate.
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
    // core.quotepath=false keeps non-ASCII names literal; -z NUL-delimits them;
    // diff-filter=AR keeps added + renamed-in ("new") assets, drops pure edits.
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
    if (!kind) continue; // skip sidecars: .mtl/.bin/.txt/.glsl/.vert/.frag
    const abs = join(repo, rel);
    if (!existsSync(abs)) continue;
    const dir = dirname(abs);
    if (!dirCache.has(dir)) dirCache.set(dir, await walk(dir));
    const dirFiles = dirCache.get(dir);
    let origin = detectOrigin(rel, dirFiles);
    const sidecar = dirFiles.find((f) => /\.(origin|provenance)\.json$/i.test(f));
    if (sidecar) {
      try {
        const j = JSON.parse(await readFile(sidecar, "utf8"));
        if (ORIGINS.includes(j.origin)) origin = j.origin;
      } catch { /* ignore unreadable sidecar */ }
    }
    const licenseFile = findLicense(dirFiles);
    const vendor = VENDORS.find((v) => rel.toLowerCase().includes(v)) || (origin === "ai" ? "ai-generated" : "unknown");
    const broad = kindForExt(ext);
    assets.push({
      id: rel.replace(/[\\/]/g, "__"),
      name: basename(rel),
      relpath: rel,
      abspath: abs,
      kind,
      ext,
      origin,
      source: vendor,
      license: licenseFile ? "see license file" : "unknown",
      licenseFile: licenseFile ? relative(repo, licenseFile).replace(/\\/g, "/") : "",
      preview: broad === "image" ? fileUrl(abs) : "",
      fontUrl: broad === "font" ? fileUrl(abs) : "",
      audioUrl: broad === "audio" ? fileUrl(abs) : "",
    });
  }
  return assets;
}

const FONT_EXT = [".ttf", ".otf", ".woff", ".woff2"];

async function libraryToCards(records) {
  const cards = [];
  for (const r of records) {
    let fontUrl = "";
    if (r.kind === "font" && r.filesDir && existsSync(r.filesDir)) {
      try {
        const ff = (await readdir(r.filesDir)).find((n) => FONT_EXT.includes(extname(n).toLowerCase()));
        if (ff) fontUrl = fileUrl(join(r.filesDir, ff));
      } catch { /* no files dir */ }
    }
    cards.push({
      id: r.asset_id,
      name: r.title,
      relpath: r.resource,
      kind: r.kind || "asset",
      origin: r.origin,
      source: r.asset_id.split("__")[0] || "unknown",
      license: r.license || "unknown",
      tags: r.tags,
      description: r.description,
      preview: r.preview ? fileUrl(r.preview) : "",
      fontUrl,
      catalog: fileUrl(r.catalogPath),
    });
  }
  return cards;
}

const ORIGIN_COLOR = { mine: "#7dd3fc", ai: "#f0abfc", sourced: "#86efac", unknown: "#9ca3af" };

const escHtml = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
// Safe to embed inside a <script>: neutralize </script>/<!-- breakout + JS line separators.
const safeJson = (v) => JSON.stringify(v).replace(/</g, "\\u003c").replace(new RegExp("[\\u2028\\u2029]", "g"), (c) => "\\u" + c.charCodeAt(0).toString(16));

function renderHtml({ mode, title, cards, meta }) {
  const data = safeJson(cards);
  const origins = ORIGINS.filter((o) => cards.some((c) => c.origin === o));
  const kinds = [...new Set(cards.map((c) => c.kind))].sort();
  const review = mode === "review";
  return `<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${escHtml(title)}</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin:0; font:14px/1.5 system-ui,Segoe UI,sans-serif; background:#0f1115; color:#e7e9ee; }
  header { position:sticky; top:0; z-index:5; background:#161922; border-bottom:1px solid #262b36; padding:12px 18px; }
  h1 { font-size:16px; margin:0 0 4px; }
  .sub { color:#9ca3af; font-size:12px; }
  .bar { display:flex; flex-wrap:wrap; gap:8px; align-items:center; margin-top:10px; }
  .bar input, .bar select { background:#0f1115; color:#e7e9ee; border:1px solid #313845; border-radius:6px; padding:6px 8px; font-size:13px; }
  .bar input[type=search] { min-width:220px; }
  .chip { padding:2px 8px; border-radius:999px; font-size:11px; font-weight:600; color:#0f1115; }
  .count { color:#9ca3af; font-size:12px; margin-left:auto; }
  .grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(220px,1fr)); gap:14px; padding:18px; }
  .card { background:#161922; border:1px solid #262b36; border-radius:10px; overflow:hidden; display:flex; flex-direction:column; }
  .card.sel { outline:2px solid #7dd3fc; }
  .thumb { height:150px; display:flex; align-items:center; justify-content:center; background:#0b0d12; border-bottom:1px solid #262b36; overflow:hidden; }
  .thumb img { max-width:100%; max-height:100%; object-fit:contain; }
  .ph { color:#5b6470; font-size:34px; }
  .meta { padding:10px 12px; display:flex; flex-direction:column; gap:6px; }
  .name { font-weight:600; font-size:13px; word-break:break-word; }
  .row { display:flex; gap:6px; flex-wrap:wrap; align-items:center; }
  .k { color:#9ca3af; font-size:11px; }
  .path { color:#6b7280; font-size:11px; word-break:break-all; }
  .pick { display:flex; align-items:center; gap:6px; padding:8px 12px; border-top:1px solid #262b36; }
  .selbox { position:sticky; bottom:0; background:#161922; border-top:1px solid #262b36; padding:10px 18px; display:${review ? "flex" : "none"}; gap:10px; align-items:center; }
  textarea { flex:1; height:46px; background:#0f1115; color:#86efac; border:1px solid #313845; border-radius:6px; padding:6px; font-family:ui-monospace,monospace; font-size:12px; }
  button { background:#1f6feb; color:#fff; border:0; border-radius:6px; padding:8px 14px; font-weight:600; cursor:pointer; }
  a { color:#7dd3fc; }
  .fontsample { font-size:26px; padding:0 8px; text-align:center; }
</style></head><body>
<header>
  <h1>${escHtml(title)}</h1>
  <div class="sub">${escHtml(meta)}</div>
  <div class="bar">
    <input type="search" id="q" placeholder="search name / tags / path…">
    <select id="kind"><option value="">all kinds</option>${kinds.map((k) => `<option>${escHtml(k)}</option>`).join("")}</select>
    <select id="origin"><option value="">all origins</option>${origins.map((o) => `<option>${escHtml(o)}</option>`).join("")}</select>
    ${review ? '<button id="selall" style="background:#313845">select shown</button><button id="clr" style="background:#313845">clear</button>' : ""}
    <span class="count" id="count"></span>
  </div>
</header>
<div class="grid" id="grid"></div>
<div class="selbox">
  <span class="k">picked ids (copy &amp; send to lead to promote):</span>
  <textarea id="sel" readonly></textarea>
  <button id="copy">copy</button>
</div>
<script>
const DATA = ${data};
const REVIEW = ${review};
const OC = ${safeJson(ORIGIN_COLOR)};
const esc = s => String(s==null?'':s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fam = id => 'f_'+String(id).replace(/[^a-zA-Z0-9_]/g,'_');
const picked = new Set();
const grid = document.getElementById('grid');
function icon(k){ return k==='model'?'◫':k==='font'?'A':k==='audio'?'♪':'▦'; }
function card(c){
  const sel = picked.has(c.id) ? ' sel' : '';
  let thumb;
  if (c.preview) thumb = '<div class="thumb"><img loading="lazy" src="'+esc(c.preview)+'"></div>';
  else if (c.kind==='font' && c.fontUrl) thumb = '<div class="thumb"><div class="fontsample" style="font-family:'+fam(c.id)+'">Aa Bb 123</div></div>';
  else if (c.kind==='audio' && c.audioUrl) thumb = '<div class="thumb" style="height:auto;padding:10px"><audio controls preload="none" style="width:100%" src="'+esc(c.audioUrl)+'"></audio></div>';
  else thumb = '<div class="thumb"><span class="ph">'+icon(c.kind)+'</span></div>';
  const tags = (c.tags&&c.tags.length)?'<div class="k">'+esc(c.tags.join(', '))+'</div>':'';
  const lic = c.license&&c.license!=='unknown'?c.license:(c.licenseFile?'license file':'license?');
  let foot='';
  if (REVIEW) foot = '<label class="pick"><input type="checkbox" data-id="'+esc(c.id)+'" '+(picked.has(c.id)?'checked':'')+'> keep</label>';
  else if (c.catalog) foot = '<div class="pick"><a href="'+esc(c.catalog)+'">open record</a></div>';
  return '<div class="card'+sel+'" data-id="'+esc(c.id)+'">'+thumb+
    '<div class="meta"><div class="name">'+esc(c.name)+'</div>'+
    '<div class="row"><span class="chip" style="background:'+(OC[c.origin]||OC.unknown)+'">'+esc(c.origin)+'</span>'+
    '<span class="k">'+esc(c.kind)+'</span><span class="k">· '+esc(c.source)+'</span><span class="k">· '+esc(lic)+'</span></div>'+
    tags+'<div class="path">'+esc(c.relpath||'')+'</div></div>'+foot+'</div>';
}
function fontFaces(){
  const css = DATA.filter(c=>(c.kind==='font')&&c.fontUrl).map(c=>'@font-face{font-family:'+fam(c.id)+';src:url("'+c.fontUrl+'")}').join('');
  const s=document.createElement('style'); s.textContent=css; document.head.appendChild(s);
}
function render(){
  const q=document.getElementById('q').value.toLowerCase();
  const k=document.getElementById('kind').value, o=document.getElementById('origin').value;
  const shown=DATA.filter(c=>{
    if(k&&c.kind!==k) return false; if(o&&c.origin!==o) return false;
    if(q){ const hay=((c.name||'')+' '+(c.tags||[]).join(' ')+' '+(c.relpath||'')+' '+(c.description||'')+' '+(c.source||'')).toLowerCase(); if(!hay.includes(q)) return false; }
    return true;
  });
  grid.innerHTML=shown.map(card).join('');
  document.getElementById('count').textContent=shown.length+' / '+DATA.length+' assets';
  if(REVIEW) grid.querySelectorAll('input[type=checkbox]').forEach(cb=>cb.onchange=()=>{ cb.checked?picked.add(cb.dataset.id):picked.delete(cb.dataset.id); syncSel(); cb.closest('.card').classList.toggle('sel',cb.checked); });
  window._shown=shown;
}
function syncSel(){ document.getElementById('sel').value=[...picked].join('\\n'); }
document.getElementById('q').oninput=render;
document.getElementById('kind').onchange=render;
document.getElementById('origin').onchange=render;
if(REVIEW){
  document.getElementById('selall').onclick=()=>{ (window._shown||[]).forEach(c=>picked.add(c.id)); syncSel(); render(); };
  document.getElementById('clr').onclick=()=>{ picked.clear(); syncSel(); render(); };
  document.getElementById('copy').onclick=()=>{ const t=document.getElementById('sel'); t.select(); navigator.clipboard&&navigator.clipboard.writeText(t.value); };
}
fontFaces(); render();
</script>
</body></html>`;
}

function parseArgs(argv) {
  const a = { mode: "library", game: "", base: "clean-seed", library: DEFAULT_LIBRARY, repo: process.cwd(), out: "tmp/asset-review" };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--mode") a.mode = next;
    else if (arg === "--game") a.game = next;
    else if (arg === "--base") a.base = next;
    else if (arg === "--library") a.library = next;
    else if (arg === "--repo") a.repo = next;
    else if (arg === "--out") a.out = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  return a;
}

async function main() {
  const a = parseArgs(process.argv.slice(2));
  const outDir = resolve(a.out);
  await mkdir(outDir, { recursive: true });
  let cards, title, meta;
  if (a.mode === "review") {
    const assets = await discoverGameAssets({ base: a.base, repo: resolve(a.repo) });
    cards = assets;
    title = `Asset review — ${a.game || "game"}`;
    const byOrigin = ORIGINS.map((o) => `${o}: ${assets.filter((x) => x.origin === o).length}`).filter((s) => !s.endsWith(": 0")).join(" · ");
    meta = `${assets.length} new assets since ${a.base} — ${byOrigin}. Check keepers, send ids to the lead to promote into the library.`;
    await writeFile(
      join(outDir, "review-manifest.json"),
      JSON.stringify({ game: a.game, base: a.base, generated: new Date().toISOString(), assets }, null, 2),
      "utf8",
    );
  } else {
    const records = await scanLibrary(a.library);
    cards = await libraryToCards(records);
    title = "Asset library";
    const byOrigin = ORIGINS.map((o) => `${o}: ${cards.filter((x) => x.origin === o).length}`).filter((s) => !s.endsWith(": 0")).join(" · ");
    meta = `${cards.length} catalog records in ${a.library} — ${byOrigin}.`;
  }
  const html = renderHtml({ mode: a.mode, title, cards, meta });
  const indexPath = join(outDir, "index.html");
  await writeFile(indexPath, html, "utf8");
  console.log(JSON.stringify({ mode: a.mode, assets: cards.length, html: indexPath, manifest: a.mode === "review" ? join(outDir, "review-manifest.json") : null }, null, 2));
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((e) => { console.error(e.message); process.exit(1); });
}

export { discoverGameAssets, detectOrigin, kindForExt, libraryKind, libraryToCards, renderHtml, escHtml, safeJson };
