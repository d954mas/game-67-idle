#!/usr/bin/env node
import { existsSync } from "node:fs";
import { cp, mkdir, readFile, readdir, writeFile } from "node:fs/promises";
import { basename, dirname, extname, join, relative, resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { DEFAULT_LIBRARY, parseFrontmatter } from "../okf_catalog/find_assets.mjs";

const modelExt = new Set([".glb", ".gltf", ".obj", ".fbx"]);
const previewExt = new Set([".png", ".jpg", ".jpeg", ".webp", ".gif"]);

async function walk(dir) {
  const out = [];
  let entries;
  try {
    entries = await readdir(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const entry of entries) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) out.push(...(await walk(path)));
    else out.push(path);
  }
  return out;
}

function list(value) {
  return Array.isArray(value) ? value.filter(Boolean) : value ? [value] : [];
}

function uniqueList(...values) {
  return [...new Set(values.flatMap(list).map(String).filter(Boolean))];
}

function sourcePage(text, fm) {
  for (const key of ["source_page", "source_url", "url", "origin_url"]) {
    if (fm[key]) return fm[key];
  }
  return text.match(/Source page:\s*(https?:\/\/[^\s)]+)/i)?.[1]
    || text.match(/https?:\/\/[^\s)]+/i)?.[0]
    || "";
}

function memberships(fm) {
  return uniqueList(fm.pack, fm.packs, fm.member_of, fm.bundles);
}

function relPosix(root, abs) {
  return relative(root, abs).replace(/\\/g, "/");
}

async function readOkfCatalog(libraryPath) {
  const catalogDir = join(libraryPath, "catalog");
  const files = (await walk(catalogDir)).filter((file) => file.endsWith(".md"));
  const packs = [];
  const assets = [];
  for (const file of files) {
    const text = await readFile(file, "utf8").catch(() => "");
    const fm = parseFrontmatter(text);
    if (/[\\/]_pack\.md$/i.test(file)) {
      packs.push({ fm, text, file });
    } else if (fm.asset_id || fm.title) {
      assets.push({ fm, text, file });
    }
  }
  return { packs, assets };
}

async function firstMatchingFile(dir, exts) {
  if (!dir || !existsSync(dir)) return "";
  const files = (await walk(dir)).sort();
  return files.find((file) => exts.has(extname(file).toLowerCase())) || "";
}

async function copyIfExists(src, dst) {
  if (!src || !existsSync(src)) return false;
  await mkdir(dirname(dst), { recursive: true });
  await cp(src, dst, { recursive: true });
  return true;
}

export async function exportOkfPackToManifest(libraryPath, packId, outRoot) {
  if (!packId) throw new Error("missing pack id");
  const { packs, assets } = await readOkfCatalog(libraryPath);
  const packSource = packs.find((pack) => (pack.fm.pack || basename(dirname(pack.file))) === packId);
  if (!packSource) throw new Error(`OKF pack not found: ${packId}`);

  const packDir = join(outRoot, "packs", packId);
  const packFm = packSource.fm;
  const selectedAssets = assets.filter((asset) => memberships(asset.fm).includes(packId));
  await mkdir(packDir, { recursive: true });

  const packJson = {
    pack: packId,
    title: packFm.title || packId,
    source: packFm.source || "",
    kind: packFm.kind || "",
    origin: packFm.origin || "unknown",
    license: packFm.license || "",
    license_url: packFm.license_url || "",
    tags: list(packFm.tags),
    genre: list(packFm.genre),
    style: list(packFm.style),
    description: packFm.description || "",
    source_page: sourcePage(packSource.text, packFm),
    legacy_okf_catalog: relPosix(libraryPath, packSource.file),
  };

  const rows = [];
  for (const asset of selectedAssets) {
    const fm = asset.fm;
    const assetId = fm.asset_id || basename(asset.file, ".md");
    const srcResource = fm.resource ? resolve(libraryPath, fm.resource) : "";
    const dstResourceDir = join(packDir, "files", assetId);
    await copyIfExists(srcResource, dstResourceDir);
    const copiedModel = await firstMatchingFile(dstResourceDir, modelExt);

    const srcPreviewDir = join(libraryPath, "previews", assetId);
    const dstPreviewDir = join(packDir, "previews", assetId);
    await copyIfExists(srcPreviewDir, dstPreviewDir);
    const copiedPreview = await firstMatchingFile(dstPreviewDir, previewExt);

    rows.push({
      asset_id: assetId,
      title: fm.title || assetId,
      description: fm.description || "",
      kind: fm.kind || packFm.kind || "",
      resource: copiedModel ? relPosix(packDir, copiedModel) : "",
      preview: copiedPreview ? relPosix(packDir, copiedPreview) : "",
      origin: fm.origin || packFm.origin || "unknown",
      license: fm.license || packFm.license || "",
      license_url: fm.license_url || packFm.license_url || "",
      author: fm.author || "",
      source_page: sourcePage(asset.text, fm),
      tags: list(fm.tags),
      packs: memberships(fm),
      legacy_okf_catalog: relPosix(libraryPath, asset.file),
    });
  }

  await writeFile(join(packDir, "pack.json"), JSON.stringify(packJson, null, 2), "utf8");
  await writeFile(join(packDir, "assets.jsonl"), rows.map((row) => JSON.stringify(row)).join("\n") + "\n", "utf8");
  await writeFile(join(packDir, "README.md"), `# ${packJson.title}\n\nExported from OKF catalog pack \`${packId}\`.\n`, "utf8");

  return {
    pack: packId,
    assets: rows.length,
    out: packDir,
    missing_resources: rows.filter((row) => !row.resource).map((row) => row.asset_id),
    missing_previews: rows.filter((row) => !row.preview).map((row) => row.asset_id),
  };
}

function parseArgs(argv) {
  const args = { library: DEFAULT_LIBRARY, pack: "", out: "" };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--library") args.library = next;
    else if (arg === "--pack") args.pack = next;
    else if (arg === "--out") args.out = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  if (!args.pack) throw new Error("--pack is required");
  if (!args.out) throw new Error("--out is required");
  return args;
}

export async function main(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  const result = await exportOkfPackToManifest(args.library, args.pack, resolve(args.out));
  console.log(JSON.stringify(result, null, 2));
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((error) => {
    console.error(error.message);
    process.exit(1);
  });
}
