#!/usr/bin/env node
// The loading bar needs the DECOMPRESSED size of the two files it waits on.
// A CDN that compresses on the fly (Poki serves through Cloudflare) answers with
// Content-Encoding: br and chunked transfer -- no Content-Length at all -- while
// the byte stream the page reads is already decompressed. Only the build knows
// the real sizes, so it writes them into the shell.
import { existsSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const PLACEHOLDER = /window\.__gameAssetBytes\s*=\s*\{[^}]*\}/;

export function assetBytes({ artifactDir, wasmName = "game.wasm", packName = "assets/game.ntpack" } = {}) {
  const root = resolve(artifactDir || "");
  const sizeOf = (relative) => {
    const path = join(root, relative);
    if (!existsSync(path)) throw new Error(`artifact file is missing: ${path}`);
    return statSync(path).size;
  };
  return { wasm: sizeOf(wasmName), pack: sizeOf(packName) };
}

export function injectAssetBytes({ artifactDir, bytes } = {}) {
  const htmlPath = join(resolve(artifactDir || ""), "index.html");
  if (!existsSync(htmlPath)) throw new Error(`shell HTML is missing: ${htmlPath}`);
  const source = readFileSync(htmlPath, "utf8");
  if (!PLACEHOLDER.test(source)) throw new Error(`shell HTML has no __gameAssetBytes placeholder: ${htmlPath}`);
  const output = source.replace(
    PLACEHOLDER,
    `window.__gameAssetBytes = { wasm: ${bytes.wasm}, pack: ${bytes.pack} }`,
  );
  writeFileSync(htmlPath, output);
  return bytes;
}

export function main(argv = process.argv.slice(2)) {
  const index = argv.indexOf("--artifact");
  if (index < 0 || !argv[index + 1]) throw new Error("usage: inject_asset_bytes.mjs --artifact <dir>");
  const artifactDir = argv[index + 1];
  const bytes = injectAssetBytes({ artifactDir, bytes: assetBytes({ artifactDir }) });
  return `loading sizes: game.wasm ${bytes.wasm} B, game.ntpack ${bytes.pack} B`;
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(fileURLToPath(import.meta.url))) {
  try {
    console.log(main());
  } catch (error) {
    console.error(error.message);
    process.exit(1);
  }
}
