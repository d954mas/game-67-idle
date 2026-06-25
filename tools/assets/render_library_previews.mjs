#!/usr/bin/env node
// Render isometric preview thumbnails for library MODEL assets with the shared
// studio HDR (the same look across the whole library — Kenney, poly.pizza, etc.),
// then place one preview.<ext> per asset. Defaults to webp (small, keeps alpha)
// so thousands of previews stay storage-adequate.
//
//   node tools/assets/render_library_previews.mjs --source polypizza
//   node tools/assets/render_library_previews.mjs --pack survival-kit --png
//   node tools/assets/render_library_previews.mjs --all --limit 50
//
// Uses tools/assets/render_thumbs.py with a @manifest (avoids the Windows command
// line length limit) and glb::assetId stems (avoids basename collisions).
import { existsSync, readdirSync, mkdirSync, writeFileSync, copyFileSync, rmSync, statSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { scanLibrary, DEFAULT_LIBRARY } from "./source/find_assets.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const BLENDERS = [
  "C:\\Program Files\\Blender Foundation\\Blender 4.3\\blender.exe",
  "C:\\Program Files\\Blender Foundation\\Blender 4.2\\blender.exe",
  "C:\\Program Files\\Blender Foundation\\Blender 3.2\\blender.exe",
];

function parseArgs(argv) {
  const a = { library: DEFAULT_LIBRARY, source: "", pack: "", size: 512, webp: true, all: false, limit: 0, blender: "", force: false };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    if (k === "--all") a.all = true;
    else if (k === "--png") a.webp = false;
    else if (k === "--force") a.force = true;
    else if (k === "--source") a.source = argv[++i];
    else if (k === "--pack") a.pack = argv[++i];
    else if (k === "--size") a.size = Number(argv[++i]) || 512;
    else if (k === "--limit") a.limit = Number(argv[++i]) || 0;
    else if (k === "--blender") a.blender = argv[++i];
    else if (k === "--library") a.library = argv[++i];
  }
  return a;
}

const glbIn = (dir) => {
  if (!dir || !existsSync(dir)) return "";
  const f = readdirSync(dir).find((n) => n.toLowerCase().endsWith(".glb"));
  return f ? join(dir, f) : "";
};

async function main() {
  const a = parseArgs(process.argv.slice(2));
  const blender = a.blender || BLENDERS.find((p) => existsSync(p));
  if (!blender) { console.error("blender.exe not found; pass --blender <path>"); process.exit(1); }

  const ext = a.webp ? "webp" : "png";
  const records = (await scanLibrary(a.library)).filter((r) => r.kind === "model");
  let targets = records.filter((r) => {
    if (a.source && !r.asset_id.startsWith(a.source)) return false;
    if (a.pack && r.pack !== a.pack) return false;
    if (!a.source && !a.pack && !a.all) return false; // require a selector
    return true;
  });
  if (!targets.length) { console.error("no matching model assets (use --source/--pack/--all)"); process.exit(1); }

  // build manifest of glb::assetId
  const items = [];
  let missing = 0;
  for (const r of targets) {
    const glb = glbIn(r.filesDir);
    if (!glb) { missing += 1; continue; }
    const prevDir = join(a.library, "previews", r.asset_id);
    if (!a.force && existsSync(join(prevDir, "preview." + ext))) continue; // resumable
    items.push(glb.replace(/\\/g, "/") + "::" + r.asset_id);
  }
  if (a.limit) items.length = Math.min(items.length, a.limit);
  console.log(`targets: ${targets.length} | to render: ${items.length} | missing glb: ${missing} | ext: ${ext}`);
  if (!items.length) { console.log("nothing to render (all previews exist; use --force to redo)"); return; }

  // Render in chunks, each a FRESH Blender process: isolates a crash/leak to one
  // chunk (a single process over thousands of imports can OOM or die on a bad glb)
  // and lets us place results incrementally. The per-asset skip above makes the
  // whole run resumable across invocations too.
  const outDir = join(HERE, "..", "..", "tmp", "lib_previews_out");
  const manifest = join(HERE, "..", "..", "tmp", "lib_previews_manifest.txt");
  const CHUNK = 200;
  const chunks = Math.ceil(items.length / CHUNK);
  let placed = 0;
  for (let c = 0; c < items.length; c += CHUNK) {
    const chunk = items.slice(c, c + CHUNK);
    rmSync(outDir, { recursive: true, force: true });
    mkdirSync(outDir, { recursive: true });
    writeFileSync(manifest, chunk.join("\n"), "utf8");
    const args = ["--background", "--python", join(HERE, "render_thumbs.py"), "--", outDir, String(a.size)];
    if (a.webp) args.push("--webp");
    args.push("@" + manifest);
    process.stdout.write(`chunk ${c / CHUNK + 1}/${chunks} (${chunk.length}) ... `);
    const res = spawnSync(blender, args, { stdio: ["ignore", "ignore", "inherit"] });
    let cp = 0;
    for (const it of chunk) {
      const id = it.split("::")[1];
      const src = join(outDir, id + "." + ext);
      if (!existsSync(src)) continue;
      const prevDir = join(a.library, "previews", id);
      mkdirSync(prevDir, { recursive: true });
      for (const old of (existsSync(prevDir) ? readdirSync(prevDir) : [])) {
        if (/^preview\.(png|jpg|jpeg|webp|gif)$/i.test(old) && old !== "preview." + ext) rmSync(join(prevDir, old), { force: true });
      }
      copyFileSync(src, join(prevDir, "preview." + ext));
      cp += 1; placed += 1;
    }
    process.stdout.write(`placed ${cp}${res.status !== 0 ? " (blender rc=" + res.status + ")" : ""}\n`);
  }
  console.log(`placed previews: ${placed}/${items.length}`);
}

main().catch((e) => { console.error(e.message); process.exit(1); });
