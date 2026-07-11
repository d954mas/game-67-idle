#!/usr/bin/env node
import { existsSync, mkdirSync, statSync, writeFileSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { listGameMounts } from "../workspace/games.mjs";
import { loadArchitectureTree } from "./tree_loader.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const defaultMapPath = "ai_studio/tree.json";
const defaultReportPath = "ai_studio/architecture_map/validation-report.json";

const defaultScanRoots = [
  "ai_studio",
  "AGENTS.md",
  "CLAUDE.md",
  ".codex/agents",
  ".codex/skills",
  ".claude/agents",
  { path: "templates", mode: "root-files-and-child-directories" },
  { path: "features", mode: "root-files-and-child-directories" },
  { path: "games", mode: "root-files-and-child-directories" },
];

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

function excludedByPrefix(rel, prefixes = []) {
  const posix = normalizeMapPath(rel);
  return prefixes.some((prefix) => posix === prefix || posix.startsWith(`${prefix}/`));
}

function privateMountScanExclusions(repoRoot) {
  try {
    return listGameMounts(repoRoot, { includePrivate: true, skipPreflight: true })
      .filter((mount) => mount.visibility !== "public")
      .map((mount) => normalizeMapPath(mount.root))
      .filter(Boolean);
  } catch {
    return [];
  }
}

function normalizeScanRoot(scanRoot) {
  if (typeof scanRoot === "string") return { path: scanRoot, mode: "tracked-files" };
  return {
    path: scanRoot.path,
    mode: scanRoot.mode || "tracked-files",
  };
}

function listGitPaths(repoRoot, args, label) {
  let output;
  try {
    output = execFileSync("git", ["-C", repoRoot, "ls-files", "-z", ...args], {
      encoding: "utf8",
      windowsHide: true,
      stdio: ["ignore", "pipe", "pipe"],
    });
  } catch (error) {
    throw new Error(`architecture map requires git ${label}: ${error && error.message ? error.message : error}`);
  }
  return output.split("\0").map(normalizeMapPath).filter(Boolean).sort((a, b) => a.localeCompare(b));
}

function listTrackedPaths(repoRoot) {
  return listGitPaths(repoRoot, [], "ls-files");
}

function listUntrackedPaths(repoRoot) {
  return listGitPaths(repoRoot, ["--others", "--exclude-standard"], "ls-files --others --exclude-standard");
}

function trackedPathsForRoot(paths, spec) {
  const root = normalizeMapPath(spec.path);
  if (spec.mode === "tracked-files") {
    return paths.filter((path) => path === root || path.startsWith(`${root}/`));
  }
  const found = new Set();
  for (const path of paths) {
    if (path === root) {
      found.add(path);
      continue;
    }
    if (!path.startsWith(`${root}/`)) continue;
    const rest = path.slice(root.length + 1);
    const slash = rest.indexOf("/");
    if (slash === -1) {
      if (spec.mode === "root-files-and-child-directories") found.add(path);
    } else {
      found.add(`${root}/${rest.slice(0, slash)}`);
    }
  }
  if (spec.mode !== "child-directories" && spec.mode !== "root-files-and-child-directories") {
    throw new Error(`Unknown architecture map scan mode: ${spec.mode}`);
  }
  return [...found];
}

function collectScannedPaths(repoRoot, scanRoots = defaultScanRoots, options = {}) {
  const scanOptions = { excludedPrefixes: (options.excludedPrefixes || []).map(normalizeMapPath).filter(Boolean) };
  const trackedPaths = (options.trackedPaths || listTrackedPaths(repoRoot))
    .map(normalizeMapPath)
    .filter(Boolean);
  return [...new Set(scanRoots.flatMap((root) => {
    const spec = normalizeScanRoot(root);
    if (excludedByPrefix(spec.path, scanOptions.excludedPrefixes)) return [];
    return trackedPathsForRoot(trackedPaths, spec)
      .filter((path) => !excludedByPrefix(path, scanOptions.excludedPrefixes));
  }))].sort((a, b) => a.localeCompare(b));
}

function collectHygienePaths(repoRoot, scanRoots, options) {
  return collectScannedPaths(repoRoot, scanRoots, {
    excludedPrefixes: options.excludedPrefixes,
    trackedPaths: listUntrackedPaths(repoRoot),
  });
}

function descriptionPolicyViolations(description) {
  const text = String(description || "").trim();
  const violations = [];
  if ([...text].length > 240) violations.push("too-long");
  if (/(?:^|\s)--?[a-z][a-z0-9-]*|\b(?:node|powershell(?:\.exe)?|python(?:3)?|npm|pnpm|git)\s+(?:\.?\.?[/\\]|[a-z0-9_.-]+\.(?:mjs|js|py|ps1|cmd)|run\b|exec\b|ls-files\b|status\b|add\b|commit\b|diff\b)/i.test(text)) {
    violations.push("command-or-flag");
  }
  if (/\b(?:GET|POST|PATCH|DELETE|PUT)\s+\/|\/api\/|(?:^|\s)\/[a-z0-9]/i.test(text)) violations.push("route");
  if (/\b(?:regression tests?|tests? for|test cases?|smoke tests?)\b/i.test(text)) violations.push("test-case-detail");
  if (/\b(?:localStorage|double-click|right-click|hover|click-away|keyboard shortcut|toggle)\b/i.test(text)) {
    violations.push("ui-micro-behavior");
  }
  return violations;
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
  const invalidDescriptions = [];
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
    const descriptionViolations = descriptionPolicyViolations(node.description);
    if (node.id && descriptionViolations.length) {
      invalidDescriptions.push({
        id: node.id,
        title: node.title || "",
        path: pathValue,
        violations: descriptionViolations,
      });
    }
  });
  return { entries, missingDescriptions, invalidDescriptions };
}

function isCoveredByMappedDirectory(file, mappedDirectories) {
  return mappedDirectories.some((dir) => file.startsWith(dir));
}

function createValidationReport(options = {}) {
  const repoRoot = options.repoRoot || defaultRepoRoot;
  const mapPath = options.mapPath || defaultMapPath;
  const scanRoots = options.scanRoots || defaultScanRoots;
  const scanExclusions = options.excludedPrefixes || privateMountScanExclusions(repoRoot);
  const map = loadArchitectureTree(repoRoot, mapPath);
  const { entries, missingDescriptions, invalidDescriptions } = collectMapEntries(map);
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

  const trackedPaths = (options.trackedPaths || listTrackedPaths(repoRoot)).map(normalizeMapPath).filter(Boolean);
  const scanned = collectScannedPaths(repoRoot, scanRoots, {
    excludedPrefixes: scanExclusions,
    trackedPaths,
  });
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
      invalidDescriptions: invalidDescriptions.length,
    },
    issues: {
      missingInRepo,
      duplicateMappings,
      unmappedInAiStudio: unmappedInAiStudio.map((path) => ({ path })),
      unmappedOutsideAiStudio: unmappedOutsideAiStudio.map((path) => ({ path })),
      missingDescriptions,
      invalidDescriptions,
    },
    ...(options.includeHygiene ? {
      hygiene: {
        untrackedPaths: collectHygienePaths(repoRoot, scanRoots, { excludedPrefixes: scanExclusions }),
      },
    } : {}),
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
    } else if (arg === "--hygiene") {
      options.includeHygiene = true;
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
  node ai_studio/architecture_map/validate_map.mjs [--strict] [--hygiene] [--map <path>] [--report <path>] [--repo <path>]

Writes ai_studio/architecture_map/validation-report.json.
Default mode exits 0 so the site can show open architecture work.
--hygiene adds a separate non-gating report of generated/untracked paths.
--strict exits non-zero when mapped paths are missing or duplicated, tracked paths are unmapped,
or descriptions are missing or violate the architectural description policy.`);
}

function hasStrictFailures(report) {
  return report.summary.missingInRepo > 0
    || report.summary.duplicateMappings > 0
    || report.summary.unmappedInAiStudio > 0
    || report.summary.unmappedOutsideAiStudio > 0
    || report.summary.missingDescriptions > 0
    || report.summary.invalidDescriptions > 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  try {
    const options = parseArgs(process.argv.slice(2));
    if (options.help) {
      printUsage();
      process.exit(0);
    }
    const repoRoot = options.repoRoot || defaultRepoRoot;
    const report = createValidationReport({
      repoRoot,
      mapPath: options.mapPath || defaultMapPath,
      includeHygiene: options.includeHygiene,
    });
    writeReport(report, repoRoot, options.reportPath || defaultReportPath);
    console.log(`wrote ${options.reportPath || defaultReportPath}`);
    console.log(`mapped=${report.summary.mappedPaths} scanned=${report.summary.scannedPaths} unmapped_ai_studio=${report.summary.unmappedInAiStudio} unmapped_outside_ai_studio=${report.summary.unmappedOutsideAiStudio} missing=${report.summary.missingInRepo} duplicates=${report.summary.duplicateMappings} invalid_descriptions=${report.summary.invalidDescriptions}`);
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
  descriptionPolicyViolations,
  hasStrictFailures,
  listTrackedPaths,
};
