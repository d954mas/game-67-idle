#!/usr/bin/env node
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

export const HTML_MINIFY_OPTIONS = Object.freeze({
  collapseBooleanAttributes: true,
  collapseWhitespace: true,
  minifyCSS: true,
  minifyJS: { compress: { booleans: false, sequences: false }, mangle: true },
  removeAttributeQuotes: true,
  removeComments: true,
  removeRedundantAttributes: true,
  removeScriptTypeAttributes: true,
  removeStyleLinkTypeAttributes: true,
  useShortDoctype: true,
});

export function resolveHtmlMinifierModule(environment = process.env, platform = process.platform) {
  const emsdk = environment.EMSDK || (platform === "win32" && existsSync("C:/develop/emsdk") ? "C:/develop/emsdk" : "");
  if (!emsdk) throw new Error("EMSDK is required for release HTML minification");
  const modulePath = resolve(emsdk, "upstream", "emscripten", "node_modules", "html-minifier-terser", "src", "htmlminifier.js");
  if (!existsSync(modulePath)) throw new Error(`html-minifier-terser is missing from Emscripten: ${modulePath}`);
  return modulePath;
}

export async function minifyWebRelease({ artifactDir, minify } = {}) {
  const root = resolve(artifactDir || "");
  const htmlPath = join(root, "index.html");
  if (!existsSync(htmlPath)) throw new Error(`release HTML is missing: ${htmlPath}`);
  const source = readFileSync(htmlPath, "utf8");
  let minifyHtml = minify;
  if (!minifyHtml) {
    const module = await import(pathToFileURL(resolveHtmlMinifierModule()).href);
    minifyHtml = module.minify;
  }
  const output = await minifyHtml(source, HTML_MINIFY_OPTIONS);
  if (typeof output !== "string" || output.length === 0) throw new Error("HTML minifier produced empty output");
  writeFileSync(htmlPath, output);
  return { beforeBytes: Buffer.byteLength(source), afterBytes: Buffer.byteLength(output) };
}

function parseArgs(argv) {
  if (argv.length !== 2 || argv[0] !== "--artifact" || !argv[1]) {
    throw new Error("usage: node tools/minify_web_release.mjs --artifact <build-bin>");
  }
  return { artifactDir: argv[1] };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const result = await minifyWebRelease(parseArgs(process.argv.slice(2)));
    console.log(`minified release HTML: ${result.beforeBytes} -> ${result.afterBytes} bytes`);
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
