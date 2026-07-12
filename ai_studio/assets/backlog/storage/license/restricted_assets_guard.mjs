#!/usr/bin/env node
// Single CLI adapter. The integrity gate is owned by ../manifests/integrity.mjs.
import { existsSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { ISSUE_CODES, REPORT_SCHEMA, loadAndAuditRepository } from "../manifests/integrity.mjs";

function repoRoot() {
  let dir = dirname(fileURLToPath(import.meta.url));
  for (let i = 0; i < 8; i += 1) {
    if (existsSync(join(dir, "AGENTS.md"))) return dir;
    const up = dirname(dir);
    if (up === dir) break;
    dir = up;
  }
  return process.cwd();
}

export function parseArgs(argv) {
  const value = { json: false, scope: "" };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--json") {
      if (value.json) return { ok: false, json: true, message: "duplicate --json" };
      value.json = true;
    } else if (arg === "--scope") {
      if (value.scope) return { ok: false, json: value.json, message: "duplicate --scope" };
      const next = argv[index + 1];
      if (!next || next.startsWith("--")) return { ok: false, json: value.json, message: "--scope requires a repository-relative path" };
      value.scope = next;
      index += 1;
    } else {
      return { ok: false, json: value.json, message: `unknown argument '${arg}'` };
    }
  }
  return { ok: true, ...value };
}

export function argumentError(message) {
  return {
    schema: REPORT_SCHEMA,
    ok: false,
    setup: true,
    exitCode: 2,
    summary: { trackedBinaryBlobs: 0, externalBoundaries: 0, inventoryEntries: 0, inventoryBoundaries: 0, metadataRecords: 0, verified: 0, scope: "all", issueCount: 1 },
    issues: [{ code: ISSUE_CODES.INVALID_ARGUMENT, path: "", message }],
  };
}

export function formatTextReport(result, { cap = 20 } = {}) {
  if (result.ok) return `ok: asset integrity - ${result.summary.verified}/${result.summary.trackedBinaryBlobs} tracked binary blobs verified; ${result.summary.externalBoundaries} external boundary; scope=${result.summary.scope}`;
  if (result.setup) return `error: asset integrity setup failed (${result.exitCode}): ${result.issues[0]?.code || "setup"} ${result.issues[0]?.path || "<setup>"}: ${result.issues[0]?.message || "unknown setup error"}`;
  const grouped = new Map();
  for (const entry of result.issues) {
    const path = entry.path || "<repository>";
    if (!grouped.has(path)) grouped.set(path, new Set());
    grouped.get(path).add(entry.code);
  }
  const paths = [...grouped.keys()].sort();
  const lines = [`error: asset integrity blocked ${paths.length} file(s) with ${result.issues.length} field issue(s); showing ${Math.min(cap, paths.length)} path(s); scope=${result.summary.scope}`];
  for (const path of paths.slice(0, cap)) lines.push(`  ${path}: ${[...grouped.get(path)].sort().join(", ")}`);
  if (paths.length > cap) lines.push(`  ... ${paths.length - cap} more path(s); narrow with --scope <repo-path> or use --json`);
  return lines.join("\n");
}

export async function main(argv = process.argv.slice(2), io = console) {
  const parsed = parseArgs(argv);
  const result = parsed.ok ? await loadAndAuditRepository(repoRoot(), { scope: parsed.scope }) : argumentError(parsed.message);
  const json = parsed.json || (!parsed.ok && parsed.json);
  if (json) io.log(JSON.stringify(result));
  else if (result.ok) io.log(formatTextReport(result));
  else io.error(formatTextReport(result));
  process.exitCode = result.exitCode;
  return result;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await main();
