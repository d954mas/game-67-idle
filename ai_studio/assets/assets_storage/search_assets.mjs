#!/usr/bin/env node
import { existsSync } from "node:fs";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { queryIndexedAssets, refreshAssetIndex } from "./asset_index/asset_index.mjs";
import { DEFAULT_LIBRARY } from "./okf_catalog/find_assets.mjs";

function parseCsv(value = "") {
  return String(value || "").split(",").map((item) => item.trim()).filter(Boolean);
}

function parseArgs(argv) {
  const args = {
    sourceId: "global-library",
    sourcePath: DEFAULT_LIBRARY,
    mode: "library",
    query: "",
    limit: 24,
    offset: 0,
    sort: "name",
    filters: {},
    json: false,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--json") {
      args.json = true;
      continue;
    }
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--source-id") args.sourceId = next;
    else if (arg === "--source-path") {
      args.sourcePath = resolve(next);
      args.mode = "scan";
    } else if (arg === "--mode") args.mode = next;
    else if (arg === "--query" || arg === "--q") args.query = next;
    else if (arg === "--kind") args.filters.kind = parseCsv(next);
    else if (arg === "--tags") args.filters.tags = parseCsv(next);
    else if (arg === "--origin") args.filters.origin = parseCsv(next);
    else if (arg === "--license") args.filters.license = parseCsv(next);
    else if (arg === "--pack") args.pack = next;
    else if (arg === "--limit") args.limit = Number.parseInt(next, 10) || 24;
    else if (arg === "--offset") args.offset = Number.parseInt(next, 10) || 0;
    else if (arg === "--sort") args.sort = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  return args;
}

function sourceFromOptions(options) {
  const sourcePath = resolve(options.sourcePath || DEFAULT_LIBRARY);
  return {
    id: options.sourceId || "global-library",
    type: options.mode === "library" ? "library" : "local",
    label: options.sourceId || "Assets",
    path: sourcePath,
    mode: options.mode || "library",
    available: existsSync(sourcePath),
  };
}

export async function searchAssets(root, options = {}) {
  const source = sourceFromOptions(options);
  if (!source.available) throw new Error(`asset source is not available: ${source.path}`);
  await refreshAssetIndex(root, source);
  return queryIndexedAssets(root, source, {
    query: options.query || "",
    pack: options.pack || "",
    sort: options.sort || "name",
    offset: options.offset || 0,
    limit: options.limit || 24,
    filters: options.filters || {},
  });
}

function printText(result) {
  console.log(`assets: ${result.total} match(es)`);
  for (const asset of result.assets) {
    const tags = asset.tags.length ? ` tags=${asset.tags.join(",")}` : "";
    console.log(`${asset.id} [${asset.kind || "asset"} | ${asset.origin || "origin?"} | ${asset.license || "license?"}]${tags}`);
    console.log(`  ${asset.name}`);
    if (asset.thumb) console.log(`  preview: ${asset.thumb}`);
  }
}

export async function main(argv = process.argv.slice(2), root = process.cwd()) {
  const options = parseArgs(argv);
  const result = await searchAssets(root, options);
  if (options.json) console.log(JSON.stringify(result, null, 2));
  else printText(result);
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((error) => {
    console.error(error.message);
    process.exit(1);
  });
}
