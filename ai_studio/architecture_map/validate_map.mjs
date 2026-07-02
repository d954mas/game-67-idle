#!/usr/bin/env node
import { existsSync, mkdirSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { dirname, extname, join, relative, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { loadArchitectureTree } from "./tree_loader.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const defaultMapPath = "ai_studio/tree.json";
const defaultReportPath = "ai_studio/architecture_map/validation-report.json";

const defaultScanRoots = [
  "ai_studio",
  "AGENTS.md",
  "CLAUDE.md",
  ".codex/skills",
  { path: "templates", mode: "root-files-and-child-directories" },
  { path: "features", mode: "root-files-and-child-directories" },
  { path: "games", mode: "root-files-and-child-directories" },
];

const ignoredPrefixes = [
  ".git/",
  "tmp/",
  "build/",
  "node_modules/",
  ".pytest_cache/",
];

const ignoredBasenames = new Set([
  ".DS_Store",
]);

const trackedExtensions = new Set([
  ".md",
  ".json",
  ".html",
  ".css",
  ".js",
  ".mjs",
  ".py",
  ".ps1",
  ".cmd",
  ".c",
  ".h",
]);

function toPosix(value) {
  return value.replaceAll("\\", "/");
}

function repoPath(repoRoot, rel) {
  return join(repoRoot, rel);
}

function normalizeMapPath(pathValue) {
  if (!pathValue) return "";
  return toPosix(pathValue).replace(/^\/+/, "");
}

function shouldTrack(rel) {
  const posix = normalizeMapPath(rel);
  if (!posix) return false;
  if (ignoredPrefixes.some((prefix) => posix.startsWith(prefix))) return false;
  if (ignoredBasenames.has(posix.split("/").at(-1))) return false;
  return trackedExtensions.has(extname(posix));
}

function walkRepo(repoRoot, rel, out = []) {
  const abs = repoPath(repoRoot, rel);
  if (!existsSync(abs)) return out;
  const stat = statSync(abs);
  if (stat.isFile()) {
    const posix = normalizeMapPath(rel);
    if (shouldTrack(posix)) out.push(posix);
    return out;
  }
  if (!stat.isDirectory()) return out;
  for (const entry of readdirSync(abs, { withFileTypes: true })) {
    if (entry.name === ".git" || entry.name === "node_modules" || entry.name === "__pycache__") continue;
    walkRepo(repoRoot, join(rel, entry.name), out);
  }
  return out;
}

function listChildDirectories(repoRoot, rel) {
  const abs = repoPath(repoRoot, rel);
  if (!existsSync(abs)) return [];
  const stat = statSync(abs);
  if (!stat.isDirectory()) return [];
  return readdirSync(abs, { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && !entry.name.startsWith(".") && entry.name !== "node_modules" && entry.name !== "__pycache__")
    .map((entry) => normalizeMapPath(join(rel, entry.name)));
}

function listRootFilesAndChildDirectories(repoRoot, rel) {
  const abs = repoPath(repoRoot, rel);
  if (!existsSync(abs)) return [];
  const stat = statSync(abs);
  if (!stat.isDirectory()) return [];
  return readdirSync(abs, { withFileTypes: true })
    .filter((entry) => {
      if (entry.name.startsWith(".") || entry.name === "node_modules" || entry.name === "__pycache__") return false;
      return entry.isDirectory() || entry.isFile();
    })
    .map((entry) => normalizeMapPath(join(rel, entry.name)))
    .filter((path) => {
      const entry = statSync(repoPath(repoRoot, path));
      return entry.isDirectory() || shouldTrack(path);
    });
}

function normalizeScanRoot(scanRoot) {
  if (typeof scanRoot === "string") return { path: scanRoot, mode: "tracked-files" };
  return {
    path: scanRoot.path,
    mode: scanRoot.mode || "tracked-files",
  };
}

function collectScannedPaths(repoRoot, scanRoots = defaultScanRoots) {
  return [...new Set(scanRoots.flatMap((root) => {
    const spec = normalizeScanRoot(root);
    if (spec.mode === "tracked-files") return walkRepo(repoRoot, spec.path);
    if (spec.mode === "child-directories") return listChildDirectories(repoRoot, spec.path);
    if (spec.mode === "root-files-and-child-directories") return listRootFilesAndChildDirectories(repoRoot, spec.path);
    throw new Error(`Unknown architecture map scan mode: ${spec.mode}`);
  }))].sort((a, b) => a.localeCompare(b));
}

function visitNodes(node, visitor, ancestry = []) {
  visitor(node, ancestry);
  for (const child of node.children || []) {
    visitNodes(child, visitor, [...ancestry, node]);
  }
}

function collectMapEntries(map) {
  const entries = [];
  const missingDescriptions = [];
  visitNodes(map.root, (node, ancestry) => {
    const pathValue = normalizeMapPath(node.path || "");
    if (pathValue) {
      entries.push({
        path: pathValue,
        id: node.id || "",
        title: node.title || pathValue.split("/").at(-1),
        kind: node.kind || "",
        coverage: node.coverage || "children",
        ancestry: ancestry.map((item) => item.title || item.id || "").filter(Boolean),
      });
    }
    const generatedOnly = Array.isArray(node.generatedChildren) && node.generatedChildren.length > 0;
    if (!generatedOnly && node.id && node.id !== "studio" && !String(node.description || "").trim()) {
      missingDescriptions.push({
        id: node.id,
        title: node.title || "",
        path: pathValue,
      });
    }
  });
  return { entries, missingDescriptions };
}

function isCoveredByMappedDirectory(file, mappedDirectories) {
  return mappedDirectories.some((dir) => file.startsWith(dir));
}

function createValidationReport(options = {}) {
  const repoRoot = options.repoRoot || defaultRepoRoot;
  const mapPath = options.mapPath || defaultMapPath;
  const scanRoots = options.scanRoots || defaultScanRoots;
  const map = loadArchitectureTree(repoRoot, mapPath);
  const { entries, missingDescriptions } = collectMapEntries(map);
  const pathToEntries = new Map();
  for (const entry of entries) {
    if (!pathToEntries.has(entry.path)) pathToEntries.set(entry.path, []);
    pathToEntries.get(entry.path).push(entry);
  }

  const mappedExactPaths = new Set();
  const mappedDirectories = [];
  const missingInRepo = [];
  for (const entry of entries) {
    const abs = repoPath(repoRoot, entry.path);
    if (!existsSync(abs)) {
      missingInRepo.push(entry);
      continue;
    }
    const stat = statSync(abs);
    mappedExactPaths.add(entry.path);
    if (stat.isDirectory()) {
      if (entry.coverage !== "self") {
        mappedDirectories.push(entry.path.endsWith("/") ? entry.path : `${entry.path}/`);
      }
    }
  }

  const scanned = collectScannedPaths(repoRoot, scanRoots);
  const unmapped = scanned.filter((file) => !mappedExactPaths.has(file) && !isCoveredByMappedDirectory(file, mappedDirectories));
  const unmappedInAiStudio = unmapped.filter((file) => file.startsWith("ai_studio/"));
  const unmappedOutsideAiStudio = unmapped.filter((file) => !file.startsWith("ai_studio/"));
  const duplicateMappings = [...pathToEntries.entries()]
    .filter(([, items]) => items.length > 1)
    .map(([path, items]) => ({ path, nodes: items.map((item) => ({ id: item.id, title: item.title })) }));

  return {
    schema: 1,
    generatedAt: new Date().toISOString(),
    mapPath,
    scanRoots,
    summary: {
      mappedPaths: entries.length,
      scannedPaths: scanned.length,
      missingInRepo: missingInRepo.length,
      duplicateMappings: duplicateMappings.length,
      unmappedInAiStudio: unmappedInAiStudio.length,
      unmappedOutsideAiStudio: unmappedOutsideAiStudio.length,
      missingDescriptions: missingDescriptions.length,
    },
    issues: {
      missingInRepo,
      duplicateMappings,
      unmappedInAiStudio: unmappedInAiStudio.map((path) => ({ path })),
      unmappedOutsideAiStudio: unmappedOutsideAiStudio.map((path) => ({ path })),
      missingDescriptions,
    },
  };
}

function writeReport(report, repoRoot, reportPath = defaultReportPath) {
  const abs = repoPath(repoRoot, reportPath);
  mkdirSync(dirname(abs), { recursive: true });
  writeFileSync(abs, `${JSON.stringify(report, null, 2)}\n`, "utf8");
}

function parseArgs(argv) {
  const args = [...argv];
  const options = {};
  while (args.length) {
    const arg = args.shift();
    if (arg === "--strict") {
      options.strict = true;
    } else if (arg === "--map") {
      options.mapPath = args.shift();
    } else if (arg === "--report") {
      options.reportPath = args.shift();
    } else if (arg === "--repo") {
      options.repoRoot = resolve(args.shift());
    } else if (arg === "--help" || arg === "-h") {
      options.help = true;
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }
  return options;
}

function printUsage() {
  console.log(`usage:
  node ai_studio/architecture_map/validate_map.mjs [--strict] [--map <path>] [--report <path>] [--repo <path>]

Writes ai_studio/architecture_map/validation-report.json.
Default mode exits 0 so the site can show open architecture work.
--strict exits non-zero when missing, duplicate, unmapped ai_studio files, or missing descriptions exist.`);
}

function hasStrictFailures(report) {
  return report.summary.missingInRepo > 0
    || report.summary.duplicateMappings > 0
    || report.summary.unmappedInAiStudio > 0
    || report.summary.unmappedOutsideAiStudio > 0
    || report.summary.missingDescriptions > 0;
}

if (import.meta.url === pathToFileURL(process.argv[1]).href) {
  try {
    const options = parseArgs(process.argv.slice(2));
    if (options.help) {
      printUsage();
      process.exit(0);
    }
    const repoRoot = options.repoRoot || defaultRepoRoot;
    const report = createValidationReport({ repoRoot, mapPath: options.mapPath || defaultMapPath });
    writeReport(report, repoRoot, options.reportPath || defaultReportPath);
    console.log(`wrote ${options.reportPath || defaultReportPath}`);
    console.log(`mapped=${report.summary.mappedPaths} scanned=${report.summary.scannedPaths} unmapped_ai_studio=${report.summary.unmappedInAiStudio} unmapped_outside_ai_studio=${report.summary.unmappedOutsideAiStudio} missing=${report.summary.missingInRepo} duplicates=${report.summary.duplicateMappings}`);
    if (options.strict && hasStrictFailures(report)) process.exit(1);
  } catch (error) {
    console.error(error && error.message ? error.message : String(error));
    process.exit(2);
  }
}

export {
  collectMapEntries,
  collectScannedPaths,
  createValidationReport,
  hasStrictFailures,
};
