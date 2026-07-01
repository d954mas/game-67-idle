#!/usr/bin/env node
import { mkdir, readFile, readdir, writeFile, cp, rm } from "node:fs/promises";
import { existsSync, statSync } from "node:fs";
import { basename, dirname, extname, join, relative, resolve, sep } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { sha256File, sha256Hex } from "../../../../core_harness/tool_lib/hash.mjs";
import { isMain } from "../../../../core_harness/tool_lib/cli.mjs";
import { defaultLibrarySourceRoot } from "../sources/libraries.mjs";

const ASSET_EXTS = new Set([
  ".glb", ".gltf", ".obj", ".fbx", ".bin", ".mtl",
  ".png", ".jpg", ".jpeg", ".webp", ".gif", ".tga", ".ktx2", ".dds", ".hdr", ".exr", ".svg",
  ".wav", ".ogg", ".mp3", ".flac",
  ".ttf", ".otf", ".woff", ".woff2",
]);

const SKIP = /(^|[\\/])(__MACOSX|\.DS_Store|Thumbs\.db|desktop\.ini)([\\/]|$)/i;

function usage() {
  return `usage: node ai_studio/assets/backlog/storage/intake/stage.mjs --input <file|folder|zip|url> --source <source> --slug <slug> [options]

Options:
  --source-root <path>       Asset source root. Defaults to the shared library.
  --filename <name>          Output filename for single files/URLs.
  --source-page-url <url>    Public source/product page.
  --license <name>           License label or unknown. Default unknown.
  --manual                   Account-gated/private local file; source link is not stored as direct download.
  --publish <true|false>     Optional publish hint carried into intake.json.
  --all-ext                  Stage every file from folders/zips.
  --overwrite                Replace existing _incoming/<source>/<slug>.`;
}

export function safeSegment(value, label = "segment") {
  const cleaned = String(value || "").trim().toLowerCase()
    .replace(/[^a-z0-9._-]+/g, "-")
    .replace(/^-+|-+$/g, "");
  if (!cleaned) throw new Error(`${label} becomes empty after sanitizing`);
  return cleaned;
}

function parseArgs(argv) {
  const args = {
    sourceRoot: defaultLibrarySourceRoot(process.cwd()),
    license: "unknown",
    sourcePageUrl: "",
    manual: false,
    publish: "",
    allExt: false,
    overwrite: false,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--manual") { args.manual = true; continue; }
    if (arg === "--all-ext") { args.allExt = true; continue; }
    if (arg === "--overwrite") { args.overwrite = true; continue; }
    if (arg === "--help" || arg === "-h") { console.log(usage()); process.exit(0); }
    const next = argv[i + 1];
    if (!next || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--input" || arg === "--url" || arg === "--archive") args.input = next;
    else if (arg === "--source-root" || arg === "--library") args.sourceRoot = next;
    else if (arg === "--source") args.source = next;
    else if (arg === "--slug") args.slug = next;
    else if (arg === "--filename") args.filename = next;
    else if (arg === "--source-page-url") args.sourcePageUrl = next;
    else if (arg === "--license" || arg === "--license-name") args.license = next;
    else if (arg === "--publish") args.publish = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  for (const required of ["input", "source", "slug"]) {
    if (!args[required]) throw new Error(`missing required --${required}`);
  }
  if (args.publish && !["true", "false"].includes(args.publish)) throw new Error("--publish must be true or false");
  return args;
}

function inferFilename(input) {
  try {
    const parsed = new URL(input);
    return basename(decodeURIComponent(parsed.pathname)) || "download.bin";
  } catch {
    return basename(input) || "download.bin";
  }
}

async function readInput(input) {
  if (input.startsWith("http://") || input.startsWith("https://")) {
    const response = await fetch(input);
    if (!response.ok) throw new Error(`download failed: HTTP ${response.status} ${response.statusText}`);
    return Buffer.from(await response.arrayBuffer());
  }
  if (input.startsWith("file://")) return readFile(fileURLToPath(input));
  return readFile(input);
}

async function walk(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const p = join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await walk(p));
    else out.push(p);
  }
  return out;
}

function extractZip(zipPath, destDir) {
  let r = spawnSync("unzip", ["-o", "-q", zipPath, "-d", destDir], { encoding: "utf8" });
  if (r.error || r.status !== 0) {
    r = spawnSync("powershell", [
      "-NoProfile",
      "-Command",
      `Expand-Archive -LiteralPath ${JSON.stringify(zipPath)} -DestinationPath ${JSON.stringify(destDir)} -Force`,
    ], { encoding: "utf8" });
  }
  if (r.status !== 0) throw new Error(`failed to extract ${zipPath}: ${(r.stderr || r.stdout || "").trim()}`);
}

function isZip(path) {
  return extname(path).toLowerCase() === ".zip";
}

async function collectFiles(input, allExt) {
  const resolved = resolve(input);
  if (!existsSync(resolved)) return null;
  if (statSync(resolved).isDirectory()) {
    const files = (await walk(resolved)).filter((f) => !SKIP.test(f) && (allExt || ASSET_EXTS.has(extname(f).toLowerCase())));
    return { root: resolved, files, cleanup: async () => {} };
  }
  if (isZip(resolved)) {
    const tempRoot = join(tmpdir(), `asset-intake-${process.pid}-${basename(resolved, extname(resolved))}`);
    await mkdir(tempRoot, { recursive: true });
    extractZip(resolved, tempRoot);
    const files = (await walk(tempRoot)).filter((f) => !SKIP.test(f) && (allExt || ASSET_EXTS.has(extname(f).toLowerCase())));
    return { root: tempRoot, files, cleanup: async () => rm(tempRoot, { recursive: true, force: true }) };
  }
  return { root: dirname(resolved), files: [resolved], cleanup: async () => {} };
}

export async function stageAsset(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  const source = safeSegment(args.source, "source");
  const slug = safeSegment(args.slug, "slug");
  const sourceRoot = resolve(args.sourceRoot);
  const targetDir = join(sourceRoot, "_incoming", source, slug);
  if (existsSync(targetDir) && !args.overwrite) throw new Error(`target already exists; pass --overwrite: ${targetDir}`);
  if (existsSync(targetDir) && args.overwrite) await rm(targetDir, { recursive: true, force: true });
  await mkdir(targetDir, { recursive: true });

  const staged = [];
  const collected = await collectFiles(args.input, args.allExt);
  if (collected) {
    try {
      if (!collected.files.length) throw new Error(`no asset files found in ${args.input} (use --all-ext to stage every file)`);
      for (const file of collected.files) {
        const rel = collected.files.length === 1 && args.filename
          ? args.filename
          : relative(collected.root, file).split(sep).join("/");
        const dest = join(targetDir, rel);
        await mkdir(dirname(dest), { recursive: true });
        await cp(file, dest);
        staged.push({ path: rel, bytes: statSync(dest).size, sha256: await sha256File(dest) });
      }
    } finally {
      await collected.cleanup();
    }
  } else {
    const data = await readInput(args.input);
    const filename = args.filename || inferFilename(args.input);
    await writeFile(join(targetDir, filename), data);
    staged.push({ path: filename, bytes: data.length, sha256: sha256Hex(data) });
  }

  const record = {
    status: "incoming",
    source,
    slug,
    input: args.manual ? "(manual/account-gated input not stored)" : args.input,
    source_page_url: args.sourcePageUrl,
    license: args.license,
    staged_at: new Date().toISOString(),
    file_count: staged.length,
    files: staged,
    ...(args.manual ? { manual: true } : {}),
    ...(args.publish ? { publish: args.publish } : {}),
  };
  const log = `# Intake Stage

- Source: ${source}
- Slug: ${slug}
- Input: ${args.manual ? "(manual/account-gated input not stored)" : args.input}
- Source page: ${args.sourcePageUrl || "-"}
- License: ${args.license}
- Publish hint: ${args.publish || "(unset)"}
- Files: ${staged.length}
- Target: ${targetDir}
- Next step: review and accept/reject.
`;
  await writeFile(join(targetDir, "intake.json"), `${JSON.stringify(record, null, 2)}\n`, "utf8");
  await writeFile(join(targetDir, "ingest-log.md"), log, "utf8");
  return { sourceRoot, source, slug, target: targetDir, files: staged.length };
}

if (isMain(import.meta.url)) {
  stageAsset().then((result) => {
    console.log(JSON.stringify(result, null, 2));
  }).catch((error) => {
    console.error(error.message);
    console.error(usage());
    process.exit(1);
  });
}
