#!/usr/bin/env node
// Pull (reuse) shared-library assets into a game/template asset source.
//
// Copies the asset file, optional preview, and license evidence into a local
// Pack Manifest. The library remains canonical through source_id/source_page.
//
// Dry-run by default; pass --apply to write.
//
// Assets are per-game: --to is the target game's assets dir (e.g. template/assets
// or <game>/assets), NOT the repo root.
//   node ai_studio/assets/viewer/pull.mjs --ids kenney__desk__cc0-1-0 --to mygame/assets --apply
import { readFile, writeFile, mkdir, cp } from "node:fs/promises";
import { existsSync } from "node:fs";
import { basename, dirname, join, relative, resolve } from "node:path";
import { scanPackManifestSource } from "../storage/manifests/manifest.mjs";
import { decideLicense, isPublishable } from "../storage/license/restricted.mjs";
import { defaultLibrarySourceRoot } from "../storage/sources/libraries.mjs";
import { isMain } from "../../core_harness/tool_lib/cli.mjs";

const PULL_PACK = "library-pulls";

function relPosix(root, abs) {
  return relative(root, abs).replace(/\\/g, "/");
}

function parseArgs(argv) {
  const a = { ids: "", library: defaultLibrarySourceRoot(process.cwd()), to: "", apply: false, overwrite: false };
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
  if (!a.to) throw new Error("missing --to (the game's assets dir, e.g. template/assets or <game>/assets)");
  return a;
}

async function readJsonl(path) {
  if (!existsSync(path)) return [];
  const text = await readFile(path, "utf8");
  return text.split(/\r?\n/).map((line) => line.trim()).filter(Boolean).map((line) => JSON.parse(line));
}

async function writeJsonl(path, records) {
  await writeFile(path, records.map((record) => JSON.stringify(record)).join("\n") + "\n", "utf8");
}

async function ensurePack(packDir, restricted) {
  await mkdir(join(packDir, "files"), { recursive: true });
  await mkdir(join(packDir, "previews"), { recursive: true });
  await mkdir(join(packDir, "licenses"), { recursive: true });
  const packPath = join(packDir, "pack.json");
  if (!existsSync(packPath)) {
    await writeFile(packPath, JSON.stringify({
      pack: PULL_PACK,
      title: "Library Pulls",
      description: "Assets copied from the shared AI Studio asset library for local game/template use.",
      origin: "sourced",
      restricted: Boolean(restricted),
    }, null, 2), "utf8");
  }
}

function localRecord(record, resource, preview, ts) {
  const decision = decideLicense(record);
  return {
    asset_id: record.asset_id,
    title: record.title || record.asset_id,
    description: record.description || "",
    kind: record.kind || "",
    status: "accepted",
    origin: record.origin || "sourced",
    source_id: record.source_id || record.asset_id,
    source_page: record.source_page || "",
    author_vendor: record.author_vendor || record.author || "",
    license: record.license || "",
    license_url: record.license_url || decision.licenseUrl || "",
    license_kind: record.license_kind || decision.licenseKind || "",
    attribution_required: record.attribution_required || (decision.attributionRequired ? "true" : "false"),
    notice_required: record.notice_required || (decision.noticeRequired ? "true" : "false"),
    credit_text: record.credit_text || "",
    commercial_use: record.commercial_use || "",
    modification_allowed: record.modification_allowed || "",
    redistribution_allowed: record.redistribution_allowed || "",
    publish: record.publish || "",
    resource,
    preview,
    tags: record.tags || [],
    genre: record.genre || [],
    style: record.style || [],
    pulled_at: ts,
  };
}

async function main() {
  const a = parseArgs(process.argv.slice(2));
  const library = resolve(a.library);
  const to = resolve(a.to);
  const want = new Set(a.ids.split(",").map((s) => s.trim()).filter(Boolean));
  const { records } = await scanPackManifestSource(library);
  const picks = records.filter((r) => want.has(r.asset_id));
  const missing = [...want].filter((id) => !picks.some((p) => p.asset_id === id));
  if (missing.length) throw new Error(`not in library: ${missing.join(", ")}`);

  const ts = new Date().toISOString();
  console.log(`pull ${picks.length} asset(s) -> ${to}  [${a.apply ? "APPLY" : "DRY-RUN"}]`);
  const written = [];
  for (const r of picks) {
    const publish = isPublishable(r);
    const packRoot = publish ? join(to, "packs") : join(to, "restricted", "packs");
    const packDir = join(packRoot, PULL_PACK);
    const assetsPath = join(packDir, "assets.jsonl");
    const fileName = basename(r.modelPath || r.resource || r.asset_id);
    const resource = `files/${r.asset_id}/${fileName}`;
    const preview = r.preview ? `previews/${r.asset_id}/${basename(r.preview)}` : "";
    const filesDst = join(packDir, "files", r.asset_id, fileName);
    const exists = existsSync(filesDst);
    console.log(`  ${r.asset_id}  (${r.kind})${publish ? "" : " [RESTRICTED]"}${exists ? " [exists]" : ""} -> ${relPosix(to, filesDst)}`);
    if (exists && !a.overwrite) throw new Error(`already in game (pass --overwrite): ${r.asset_id}`);
    if (!a.apply) continue;
    await ensurePack(packDir, !publish);
    if (r.modelPath && existsSync(r.modelPath)) {
      await mkdir(dirname(filesDst), { recursive: true });
      await cp(r.modelPath, filesDst, { force: true });
    }
    if (r.preview && existsSync(r.preview)) {
      const pdst = join(packDir, "previews", r.asset_id);
      await mkdir(pdst, { recursive: true });
      await cp(r.preview, join(pdst, basename(r.preview)), { force: true });
    }
    const existing = await readJsonl(assetsPath);
    const next = localRecord(r, resource, preview, ts);
    const filtered = existing.filter((record) => record.asset_id !== next.asset_id);
    await writeJsonl(assetsPath, [...filtered, next]);
    written.push(r.asset_id);
  }
  console.log(a.apply ? `\nlinked ${written.length} asset(s) into ${to} (source_id set).` : `\ndry-run only - pass --apply to write.`);
}

if (isMain(import.meta.url)) {
  main().catch((e) => { console.error(e.message); process.exit(1); });
}

export { parseArgs, localRecord };
