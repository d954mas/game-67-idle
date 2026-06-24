#!/usr/bin/env node
// Single-purpose: stage a downloaded ZIP or FOLDER of assets into the library's
// _incoming/ area with per-file provenance (sha256), for later review + accept.
// It does NOT write the catalog — that is accept_incoming_asset's job. One leaf
// per job; the game-asset skill composes fetch/ingest -> accept -> preview -> pull.
//
//   node tools/assets/intake/ingest_archive.mjs --archive <pack.zip|folder> --source <name> [options]
//
// Options:
//   --slug <base>          _incoming sub-id. Defaults to the archive/folder name.
//   --library <path>       Asset library root. Defaults to the shared library.
//   --license-name <name>  Known license label (CC0-1.0, CC-BY-4.0, ...) or unknown.
//   --source-page-url <u>  Public product/source page recorded as provenance.
//   --publish <true|false> Publishability; paid packs use false (routes to restricted later).
//   --all-ext              Stage every file, not just known asset extensions.
//   --overwrite            Replace an existing _incoming/<source>/<slug>/ staging dir.
//
// Output: _incoming/<source>/<slug>/ with the extracted files (original tree),
// intake.json (manifest: archive provenance + per-file sha256/bytes), ingest-log.md.
import { cp, mkdir, readFile, writeFile, readdir, rm } from "node:fs/promises";
import { existsSync, statSync } from "node:fs";
import { basename, dirname, extname, join, relative, resolve, sep } from "node:path";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { DEFAULT_LIBRARY } from "../source/find_assets.mjs";
import { sha256File } from "../../lib/hash.mjs";

const ASSET_EXTS = new Set([
  ".glb", ".gltf", ".obj", ".fbx", ".png", ".jpg", ".jpeg", ".webp", ".tga",
  ".ktx2", ".dds", ".wav", ".ogg", ".mp3", ".ttf", ".otf", ".hdr", ".exr", ".svg",
]);
const SKIP = /(^|[\\/])(__MACOSX|\.DS_Store|Thumbs\.db|desktop\.ini)([\\/]|$)/i;

function usage() {
  return `usage: node tools/assets/intake/ingest_archive.mjs --archive <pack.zip|folder> --source <name> [options]

Stages a downloaded zip OR folder of assets into <library>/_incoming/<source>/<slug>/
with per-file sha256 provenance. Does not write the catalog (run accept next).
Default library: ${DEFAULT_LIBRARY}`;
}

// Local sanitizer (matches download_source_asset's _incoming naming). When the
// shared lib/text leaf lands (review step 4) both migrate to it.
function safeSegment(value, label) {
  const cleaned = String(value).trim().toLowerCase()
    .replace(/[^a-z0-9._-]+/g, "-")
    .replace(/^-+|-+$/g, "");
  if (!cleaned) throw new Error(`${label} becomes empty after sanitizing`);
  return cleaned;
}

function parseArgs(argv) {
  const args = { library: DEFAULT_LIBRARY, licenseName: "unknown", sourcePageUrl: "", publish: "", allExt: false, overwrite: false };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--all-ext") { args.allExt = true; continue; }
    if (arg === "--overwrite") { args.overwrite = true; continue; }
    if (arg === "--help" || arg === "-h") { console.log(usage()); process.exit(0); }
    const next = argv[i + 1];
    if (!next || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--archive") args.archive = next;
    else if (arg === "--source") args.source = next;
    else if (arg === "--slug") args.slug = next;
    else if (arg === "--library") args.library = next;
    else if (arg === "--license-name") args.licenseName = next;
    else if (arg === "--source-page-url") args.sourcePageUrl = next;
    else if (arg === "--publish") args.publish = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  for (const required of ["archive", "source"]) {
    if (!args[required]) throw new Error(`missing required --${required}`);
  }
  if (!existsSync(args.archive)) throw new Error(`--archive not found: ${args.archive}`);
  if (args.publish && !["true", "false"].includes(args.publish)) throw new Error("--publish must be true or false");
  return args;
}

async function walk(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const p = join(dir, entry.name);
    if (entry.isDirectory()) out.push(...(await walk(p)));
    else out.push(p);
  }
  return out;
}

function extractZip(zipPath, destDir) {
  let r = spawnSync("unzip", ["-o", "-q", zipPath, "-d", destDir], { encoding: "utf8" });
  if (r.error || r.status !== 0) {
    r = spawnSync("powershell", ["-NoProfile", "-Command",
      `Expand-Archive -LiteralPath ${JSON.stringify(zipPath)} -DestinationPath ${JSON.stringify(destDir)} -Force`],
      { encoding: "utf8" });
  }
  if (r.status !== 0) throw new Error(`failed to extract ${zipPath}: ${(r.stderr || r.stdout || "").trim()}`);
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const archivePath = resolve(args.archive);
  const isDir = statSync(archivePath).isDirectory();

  let sourceRoot = archivePath;
  let tempRoot = null;
  if (!isDir) {
    tempRoot = join(tmpdir(), `ingest-${process.pid}-${basename(archivePath, extname(archivePath))}`);
    await mkdir(tempRoot, { recursive: true });
    extractZip(archivePath, tempRoot);
    sourceRoot = tempRoot;
  }

  const assetFiles = (await walk(sourceRoot)).filter(
    (f) => !SKIP.test(f) && (args.allExt || ASSET_EXTS.has(extname(f).toLowerCase())),
  );
  if (assetFiles.length === 0) {
    if (tempRoot) await rm(tempRoot, { recursive: true, force: true });
    throw new Error(`no asset files found in ${args.archive} (use --all-ext to stage every file)`);
  }

  const source = safeSegment(args.source, "source");
  const slug = safeSegment(args.slug || basename(archivePath, isDir ? "" : extname(archivePath)), "slug");
  const targetDir = resolve(args.library, "_incoming", source, slug);
  if (existsSync(targetDir) && !args.overwrite) {
    if (tempRoot) await rm(tempRoot, { recursive: true, force: true });
    throw new Error(`target already exists; pass --overwrite: ${targetDir}`);
  }

  const files = [];
  for (const f of assetFiles) {
    const rel = relative(sourceRoot, f).split(sep).join("/");
    const dest = join(targetDir, rel);
    await mkdir(dirname(dest), { recursive: true });
    await cp(f, dest);
    files.push({ path: rel, bytes: statSync(dest).size, sha256: await sha256File(dest) });
  }

  const record = {
    status: "incoming",
    source,
    slug,
    from_archive: basename(archivePath),
    archive_kind: isDir ? "folder" : "zip",
    ...(isDir ? {} : { archive_sha256: await sha256File(archivePath) }),
    source_page_url: args.sourcePageUrl,
    license_name: args.licenseName,
    ingested_at: new Date().toISOString(),
    file_count: files.length,
    ...(args.publish ? { publish: args.publish } : {}),
    files,
  };

  const log = `# Ingest Log

- Source site: ${source}
- Archive / folder: ${basename(archivePath)} (${record.archive_kind})
- Product / source page: ${args.sourcePageUrl || "-"}
- License checked before ingest: ${args.licenseName === "unknown" ? "no" : "yes"}
- License name: ${args.licenseName}
- Publishable: ${args.publish || "(unset)"}
- Ingest command/tool: tools/assets/intake/ingest_archive.mjs
- Staged files: ${files.length}
- Result dir: ${targetDir}
- Next step: review, then accept each asset (accept_incoming_asset.mjs) or reject.
`;

  await mkdir(targetDir, { recursive: true });
  await writeFile(join(targetDir, "intake.json"), `${JSON.stringify(record, null, 2)}\n`);
  await writeFile(join(targetDir, "ingest-log.md"), log);
  if (tempRoot) await rm(tempRoot, { recursive: true, force: true });

  console.log(JSON.stringify({ source, slug, file_count: files.length, target: targetDir }, null, 2));
}

main().catch((error) => {
  console.error(error.message);
  console.error(usage());
  process.exit(1);
});
