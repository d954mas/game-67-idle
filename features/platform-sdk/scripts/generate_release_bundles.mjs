#!/usr/bin/env node
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { platformSdkSourceBundle } from "./artifact_tools.mjs";

const FEATURE_DIR = dirname(dirname(fileURLToPath(import.meta.url)));
const RELEASE_DIR = join(FEATURE_DIR, "web", "release");
const ADAPTERS = Object.freeze(["mock", "poki", "yandex", "playgama", "crazygames", "wavedash", "pikabu", "gamepush"]);
// The crash reporter a release page inlines; its top-level installPlatformDiagnostics keeps its name.
const DIAGNOSTICS_SOURCE = join(FEATURE_DIR, "web", "runtime-diagnostics.js");
export const DIAGNOSTICS_BUNDLE = "runtime-diagnostics.min.js";

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex");
}

export function resolveEmscriptenRoot(environment = process.env, platform = process.platform) {
  const emsdk = environment.EMSDK || (platform === "win32" && existsSync("C:/develop/emsdk") ? "C:/develop/emsdk" : "");
  if (!emsdk) throw new Error("EMSDK is required to generate release SDK bundles");
  const root = resolve(emsdk, "upstream", "emscripten");
  if (!existsSync(join(root, "node_modules", "terser", "main.js"))) {
    throw new Error(`Emscripten Terser is missing: ${root}`);
  }
  return root;
}

export async function generateReleaseBundles({ emscriptenRoot, write = true } = {}) {
  const root = resolve(emscriptenRoot || resolveEmscriptenRoot());
  const terserRoot = join(root, "node_modules", "terser");
  const terser = await import(pathToFileURL(join(terserRoot, "main.js")).href);
  const terserPackage = JSON.parse(readFileSync(join(terserRoot, "package.json"), "utf8"));
  const minify = async (source, label) => {
    const result = await terser.minify(source.toString("utf8"), {
      compress: { passes: 2 },
      ecma: 2020,
      format: { ascii_only: true, comments: false },
      mangle: true,
      module: false,
    });
    if (!result.code) throw new Error(`Terser produced an empty bundle: ${label}`);
    const bundle = Buffer.from(`${result.code}\n`, "utf8");
    return [bundle, {
      sourceSha256: sha256(source),
      bundleSha256: sha256(bundle),
      sourceBytes: source.length,
      bundleBytes: bundle.length,
    }];
  };
  const outputs = new Map();
  const adapters = {};
  for (const adapter of ADAPTERS) {
    const [bundle, record] = await minify(platformSdkSourceBundle(adapter), adapter);
    outputs.set(`${adapter}.min.js`, bundle);
    adapters[adapter] = record;
  }
  const [diagnosticsBundle, diagnostics] = await minify(readFileSync(DIAGNOSTICS_SOURCE), DIAGNOSTICS_BUNDLE);
  outputs.set(DIAGNOSTICS_BUNDLE, diagnosticsBundle);
  const manifest = {
    schema: "ai_studio.platform_sdk.release_bundles.v1",
    minifier: { name: "terser", version: terserPackage.version, ecma: 2020, compressPasses: 2, mangle: true },
    adapters,
    diagnostics,
  };
  if (write) {
    mkdirSync(RELEASE_DIR, { recursive: true });
    for (const [name, bundle] of outputs) writeFileSync(join(RELEASE_DIR, name), bundle);
    writeFileSync(join(RELEASE_DIR, "manifest.json"), `${JSON.stringify(manifest, null, 2)}\n`);
  }
  return { manifest, outputs };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  await generateReleaseBundles({ emscriptenRoot: resolveEmscriptenRoot() });
  console.log(`generated ${ADAPTERS.length} release SDK bundles and ${DIAGNOSTICS_BUNDLE}`);
}
