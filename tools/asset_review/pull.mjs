#!/usr/bin/env node
// Pull (reuse) a shared-library asset into a game as a LINKED copy.
//
// Copies the asset's files + preview + license into the game's OKF bundle and
// writes a game catalog record whose `source_id` points back to the library —
// so the library stays the single source of truth (re-pull to update). The game
// record's presence of `source_id` marks it "linked" (vs game-local "new").
//
// Dry-run by default; pass --apply to write.
//
//   node tools/asset_review/pull.mjs --ids kenney__desk__cc0-1-0,kenney__loungesofa__cc0-1-0 --to assets --apply
import { readFile, writeFile, mkdir, cp, readdir } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { scanLibrary, parseFrontmatter, DEFAULT_LIBRARY, KIND_DIR } from "../assets/source/find_assets.mjs";

function parseArgs(argv) {
  const a = { ids: "", library: DEFAULT_LIBRARY, to: "assets", apply: false, overwrite: false };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--apply") { a.apply = true; continue; }
    if (arg === "--overwrite") { a.overwrite = true; continue; }
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--ids") a.ids = next;
    else if (arg === "--library") a.library = next;
    else if (arg === "--to") a.to = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  if (!a.ids) throw new Error("missing --ids (comma-separated library asset_id list)");
  return a;
}

function gameRecord(fm, resource, sourceId, ts) {
  return `---
type: Game Asset
title: ${fm.title || fm.asset_id}
description: ${fm.description || ""}
resource: ${resource}
tags: [${(Array.isArray(fm.tags) ? fm.tags : []).join(", ")}]
timestamp: ${ts}
asset_id: ${fm.asset_id}
kind: ${fm.kind}
status: accepted
origin: ${fm.origin || "sourced"}
source_id: ${sourceId}
license: ${fm.license || ""}
license_url: ${fm.license_url || ""}
${fm.pack ? `pack: ${fm.pack}\n` : ""}---

# ${fm.title || fm.asset_id}

## Provenance

- Linked from shared library: ${sourceId}
- Pulled at: ${ts}
- The library is canonical; re-pull to update this copy.
- Runtime uses project-local copies; the builder packs \`${resource}\`.
`;
}

async function copyDir(src, dst) {
  await mkdir(dst, { recursive: true });
  for (const n of await readdir(src)) {
    const s = join(src, n);
    try { await cp(s, join(dst, n), { recursive: true, force: true }); } catch { /* skip */ }
  }
}

async function main() {
  const a = parseArgs(process.argv.slice(2));
  const library = resolve(a.library);
  const to = resolve(a.to);
  const want = new Set(a.ids.split(",").map((s) => s.trim()).filter(Boolean));
  const records = await scanLibrary(library);
  const picks = records.filter((r) => want.has(r.asset_id));
  const missing = [...want].filter((id) => !picks.some((p) => p.asset_id === id));
  if (missing.length) throw new Error(`not in library: ${missing.join(", ")}`);

  const ts = new Date().toISOString();
  console.log(`pull ${picks.length} asset(s) -> ${to}  [${a.apply ? "APPLY" : "DRY-RUN"}]`);
  const written = [];
  for (const r of picks) {
    const fm = parseFrontmatter(await readFile(r.catalogPath, "utf8"));
    const kindDir = KIND_DIR[fm.kind] || fm.kind;
    const resource = `source/${kindDir}/${fm.asset_id}/`;
    const filesSrc = join(library, fm.resource);
    const filesDst = join(to, resource);
    const catalogPath = join(to, "catalog", kindDir, `${fm.asset_id}.md`);
    const exists = existsSync(catalogPath) || existsSync(filesDst);
    console.log(`  ${fm.asset_id}  (${fm.kind})${exists ? " [exists]" : ""} -> ${resource}`);
    if (exists && !a.overwrite) throw new Error(`already in game (pass --overwrite): ${fm.asset_id}`);
    if (!a.apply) continue;
    if (existsSync(filesSrc)) await copyDir(filesSrc, filesDst);
    if (r.preview && existsSync(r.preview)) {
      const pdst = join(to, "previews", fm.asset_id);
      await mkdir(pdst, { recursive: true });
      await cp(r.preview, join(pdst, "preview.png"), { force: true });
    }
    const licSrc = join(library, "licenses", fm.pack || fm.asset_id);
    if (existsSync(licSrc)) await copyDir(licSrc, join(to, "licenses", fm.asset_id));
    await mkdir(join(to, "catalog", kindDir), { recursive: true });
    await writeFile(catalogPath, gameRecord(fm, resource, fm.asset_id, ts), "utf8");
    written.push(fm.asset_id);
  }
  console.log(a.apply ? `\nlinked ${written.length} asset(s) into ${to} (source_id set).` : `\ndry-run only — pass --apply to write.`);
}

main().catch((e) => { console.error(e.message); process.exit(1); });

export { parseArgs, gameRecord };
