#!/usr/bin/env node
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const WEB_DIR = join(ROOT, "web");
const RELEASE_DIR = join(WEB_DIR, "release");

export const REAL_SDK_MARKERS = Object.freeze({
  poki: ["https://game-cdn.poki.com/scripts/v2/poki-sdk.js", "PokiSDK"],
  yandex: ["YaGames.init", "/sdk.js"],
  playgama: ["https://bridge.playgama.com/v1/stable/playgama-bridge.js", "bridge.initialize"],
});

export const DEBUG_MARKERS = Object.freeze(["Show interstitial ad", "Show rewarded ad", "debug_test"]);
const STAGED_WEB_MODULES = Object.freeze([
  "platform-sdk.js",
  "platform-sdk-adapter.js",
]);

function classicPlatformModule(input, label) {
  const output = [];
  const expectedImports = label === "platform-sdk.js" ? new Set([
    'import { createPlatformSdkAdapter } from "./platform-sdk-adapter.js";',
  ]) : new Set();
  for (const line of String(input).split(/\r?\n/)) {
    if (/^\s*import\s/.test(line)) {
      if (!expectedImports.delete(line.trim())) throw new Error(`unsupported ES module import in ${label}`);
      continue;
    }
    const classic = line.replace(
      /^(\s*)export\s+((?:async\s+)?function|const|let|var|class)\b/,
      "$1$2",
    );
    if (/^\s*(?:import|export)\b/.test(classic)) throw new Error(`unsupported ES module syntax in ${label}`);
    output.push(classic);
  }
  if (expectedImports.size !== 0) throw new Error(`required ES module import is missing from ${label}`);
  return output.join("\n").trimEnd();
}

export function platformSdkSourceBundle(sdk) {
  const sources = [
    ["platform-sdk-adapter.js", join(WEB_DIR, "adapters", `${sdk}.js`)],
    ["platform-sdk.js", join(WEB_DIR, "platform-sdk.js")],
  ];
  const body = sources.map(([label, path]) => (
    classicPlatformModule(readFileSync(path, "utf8"), label)
  )).join("\n\n");
  return Buffer.from(`(function () {\n${body}\n}());\n`, "utf8");
}

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex");
}

export function platformSdkBundlePrefix(sdk) {
  const manifest = JSON.parse(readFileSync(join(RELEASE_DIR, "manifest.json"), "utf8"));
  const record = manifest && manifest.schema === "ai_studio.platform_sdk.release_bundles.v1"
    ? manifest.adapters && manifest.adapters[sdk]
    : null;
  if (!record) throw new Error(`missing release SDK bundle manifest entry: ${sdk}`);
  const source = platformSdkSourceBundle(sdk);
  if (record.sourceSha256 !== sha256(source)) throw new Error(`stale release SDK bundle source: ${sdk}`);
  const bundle = readFileSync(join(RELEASE_DIR, `${sdk}.min.js`));
  if (record.bundleSha256 !== sha256(bundle)) throw new Error(`invalid release SDK bundle bytes: ${sdk}`);
  return bundle;
}

export function sdkForTarget(target) {
  if (target === "local" || target === "itch") return "mock";
  if (target === "poki" || target === "yandex" || target === "playgama") return target;
  throw new Error(`unknown publish target: ${target}`);
}

function copyFile(src, dst) {
  mkdirSync(dirname(dst), { recursive: true });
  writeFileSync(dst, readFileSync(src));
}

export function stagePlatformSdkWebAssets({ target, outDir, debugUi = false }) {
  void debugUi;
  const sdk = sdkForTarget(target);
  const resolvedOut = resolve(outDir);
  mkdirSync(resolvedOut, { recursive: true });
  copyFile(join(WEB_DIR, "platform-sdk.js"), join(resolvedOut, "platform-sdk.js"));
  copyFile(join(WEB_DIR, "adapters", `${sdk}.js`), join(resolvedOut, "platform-sdk-adapter.js"));
  rmSync(join(resolvedOut, "platform-sdk-core.js"), { force: true });
  if (target === "playgama") {
    copyFile(
      join(WEB_DIR, "portal", "playgama-bridge-config.json"),
      join(resolvedOut, "playgama-bridge-config.json"),
    );
  }

  const debugPath = join(resolvedOut, "platform-sdk-debug-ui.js");
  rmSync(debugPath, { force: true });

  return { outDir: resolvedOut, platformSdk: sdk, target };
}

function collectFiles(dir) {
  if (!existsSync(dir)) return [];
  const out = [];
  for (const entry of readdirSync(dir)) {
    const full = join(dir, entry);
    const st = statSync(full);
    if (st.isDirectory()) out.push(...collectFiles(full));
    else if (/\.(html|js|mjs|json)$/i.test(entry)) out.push(full);
  }
  return out;
}

export function inspectPlatformSdkArtifact({ target, artifactDir, production = true, requireFiles = false }) {
  const sdk = sdkForTarget(target);
  const files = collectFiles(artifactDir);
  const violations = [];

  if (requireFiles && target !== "local") {
    const manifestPath = join(ROOT, "publish-targets", `${target}.json`);
    if (!existsSync(manifestPath)) {
      violations.push({ file: manifestPath, marker: target, reason: "missing-manifest" });
    } else {
      const manifest = JSON.parse(readFileSync(manifestPath, "utf8"));
      const stagedModuleCount = STAGED_WEB_MODULES.filter((rel) => existsSync(join(artifactDir, rel))).length;
      const staged = stagedModuleCount === STAGED_WEB_MODULES.length;
      if (stagedModuleCount !== 0 && !staged) {
        violations.push({
          file: artifactDir,
          marker: STAGED_WEB_MODULES.join(","),
          reason: "partial-platform-sdk-layout",
        });
      }
      const required = staged ? manifest.required_files : manifest.packaged_required_files;
      if (!Array.isArray(required)) {
        violations.push({ file: manifestPath, marker: "packaged_required_files", reason: "invalid-manifest" });
      }
      for (const rel of required || []) {
        if (!existsSync(join(artifactDir, rel))) {
          violations.push({ file: join(artifactDir, rel), marker: rel, reason: "missing-required-file" });
        }
      }
      const gamePath = join(artifactDir, "game.js");
      const gameSource = existsSync(gamePath) ? readFileSync(gamePath) : Buffer.alloc(0);
      const expected = platformSdkBundlePrefix(sdk);
      const hasBundledSdk = gameSource.subarray(0, expected.length).equals(expected);
      if (staged && hasBundledSdk) {
        violations.push({ file: gamePath, marker: sdk, reason: "hybrid-platform-sdk-layout" });
      }
      if (!staged) {
        if (!hasBundledSdk) {
          violations.push({ file: gamePath, marker: sdk, reason: "invalid-selected-sdk-bundle" });
        }
      }
    }
  }

  for (const file of files) {
    const text = readFileSync(file, "utf8");
    if (production) {
      for (const marker of DEBUG_MARKERS) {
        if (text.includes(marker)) violations.push({ file, marker, reason: "debug-ui-marker" });
      }
    }
    for (const [markerSdk, markers] of Object.entries(REAL_SDK_MARKERS)) {
      if (markerSdk === sdk) continue;
      for (const marker of markers) {
        if (text.includes(marker)) violations.push({ file, marker, reason: `unused-${markerSdk}-sdk` });
      }
    }
  }

  return { ok: violations.length === 0, violations };
}

function parseCli(argv) {
  const args = {
    artifactDir: "",
    command: "",
    debugUi: false,
    outDir: "",
    production: true,
    requireFiles: true,
    target: "local",
  };
  args.command = argv[0] || "";
  for (let i = 1; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--target") args.target = argv[++i];
    else if (arg === "--out") args.outDir = argv[++i];
    else if (arg === "--artifact") args.artifactDir = argv[++i];
    else if (arg === "--debug-ui") args.debugUi = true;
    else if (arg === "--no-production") args.production = false;
    else if (arg === "--skip-required-files") args.requireFiles = false;
    else throw new Error(`unknown option: ${arg}`);
  }
  return args;
}

if (process.argv[1] && resolve(fileURLToPath(import.meta.url)) === resolve(process.argv[1])) {
  try {
    const args = parseCli(process.argv.slice(2));
    if (args.command === "stage") {
      if (!args.outDir) throw new Error("--out is required");
      console.log(JSON.stringify(stagePlatformSdkWebAssets(args), null, 2));
    } else if (args.command === "inspect") {
      if (!args.artifactDir) throw new Error("--artifact is required");
      const result = inspectPlatformSdkArtifact(args);
      console.log(JSON.stringify(result, null, 2));
      process.exitCode = result.ok ? 0 : 1;
    } else {
      throw new Error("usage: artifact_tools.mjs <stage|inspect> --target <target> --out/--artifact <dir>");
    }
  } catch (error) {
    console.error(`platform-sdk artifact: ${error.message}`);
    process.exitCode = 2;
  }
}
