#!/usr/bin/env node
import { brotliCompressSync, constants as zlibConstants } from "node:zlib";
import { existsSync, readFileSync } from "node:fs";
import { basename, dirname, join, parse, resolve } from "node:path";

import { isMain } from "../core_harness/tool_lib/cli.mjs";
import { entryDetail, parseNameHeader, parseNtpack } from "./ntpack.mjs";

function usage() {
  return `usage: node ai_studio/game_page/ntpack_cli.mjs <pack.ntpack> [options]

Options:
  --names <generated.h>  Asset-name header; inferred from the game build path when omitted.
  --quality <0..11>      Brotli quality (default: 11).
  --limit <n>            Maximum entries in the human table (default: 20).
  --all                  Print every entry.
  --json                 Print machine-readable JSON.`;
}

function numericOption(value, option) {
  if (value === undefined || value.startsWith("--")) throw new Error(`missing value for ${option}`);
  const parsed = Number.parseInt(value, 10);
  if (!Number.isInteger(parsed)) throw new Error(`invalid integer for ${option}: ${value}`);
  return parsed;
}

export function parseArgs(argv) {
  const args = { packPath: "", namesPath: "", quality: 11, limit: 20, json: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") throw new Error(usage());
    if (arg === "--json") { args.json = true; continue; }
    if (arg === "--all") { args.limit = Number.POSITIVE_INFINITY; continue; }
    if (!arg.startsWith("--")) {
      if (args.packPath) throw new Error(`unexpected positional argument: ${arg}`);
      args.packPath = arg;
      continue;
    }
    if (arg === "--names") {
      const value = argv[index + 1];
      if (value === undefined || value.startsWith("--")) throw new Error("missing value for --names");
      args.namesPath = value;
      index += 1;
      continue;
    }
    if (arg === "--quality") {
      args.quality = numericOption(argv[index + 1], arg);
      index += 1;
      if (args.quality < 0 || args.quality > 11) throw new Error("quality must be between 0 and 11");
      continue;
    }
    if (arg === "--limit") {
      args.limit = numericOption(argv[index + 1], arg);
      index += 1;
      if (args.limit < 1) throw new Error("limit must be at least 1");
      continue;
    }
    throw new Error(`unknown option: ${arg}`);
  }
  if (!args.packPath) throw new Error(usage());
  return args;
}

function brotli(bytes, quality) {
  return brotliCompressSync(bytes, {
    params: { [zlibConstants.BROTLI_PARAM_QUALITY]: quality },
  }).length;
}

function detailLabel(bytes, entry, names) {
  const detail = entryDetail(bytes, entry, names);
  if (detail?.texture) return `${detail.texture.width}x${detail.texture.height}`;
  return "";
}

export function analyzeNtpackBuffer(buffer, options = {}) {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  const quality = options.quality ?? 11;
  const names = options.names || new Map();
  const pack = parseNtpack(bytes, { names, gzip: false });
  const entries = pack.entries
    .filter((entry) => entry.dupOfIndex == null)
    .map((entry) => ({
      name: entry.name,
      type: entry.typeTag,
      rawBytes: entry.size,
      brotliBytes: brotli(bytes.subarray(entry.offset, entry.offset + entry.size), quality),
      detail: detailLabel(bytes, entry, names),
    }))
    .sort((left, right) => right.rawBytes - left.rawBytes || left.name.localeCompare(right.name));

  const byTypeMap = new Map();
  for (const entry of entries) {
    const row = byTypeMap.get(entry.type) || { type: entry.type, count: 0, rawBytes: 0, brotliBytes: 0 };
    row.count += 1;
    row.rawBytes += entry.rawBytes;
    row.brotliBytes += entry.brotliBytes;
    byTypeMap.set(entry.type, row);
  }
  const payloadBytes = entries.reduce((sum, entry) => sum + entry.rawBytes, 0);
  return {
    quality,
    fileBytes: bytes.length,
    packBrotliBytes: brotli(bytes, quality),
    payloadBytes,
    overheadBytes: bytes.length - payloadBytes,
    entryCount: pack.entries.length,
    uniqueEntryCount: entries.length,
    byType: [...byTypeMap.values()].sort((left, right) => right.rawBytes - left.rawBytes),
    entries,
  };
}

function formatBytes(value) {
  if (value >= 1024 * 1024) return `${(value / (1024 * 1024)).toFixed(2)} MiB`;
  if (value >= 1024) return `${(value / 1024).toFixed(1)} KiB`;
  return `${value} B`;
}

function row(columns, widths) {
  return columns.map((value, index) => String(value).padEnd(widths[index])).join("  ").trimEnd();
}

function table(headers, rows) {
  const widths = headers.map((header, index) => Math.max(
    header.length,
    ...rows.map((values) => String(values[index]).length),
  ));
  return [row(headers, widths), row(widths.map((width) => "-".repeat(width)), widths), ...rows.map((values) => row(values, widths))];
}

export function formatAnalysisText(result, options = {}) {
  const limit = options.limit ?? 20;
  const visible = Number.isFinite(limit) ? result.entries.slice(0, limit) : result.entries;
  const lines = [
    `${result.packPath || "ntpack"}: ${formatBytes(result.fileBytes)} raw, ${formatBytes(result.packBrotliBytes)} Brotli q${result.quality}`,
    `entries: ${result.entryCount} (${result.uniqueEntryCount} unique), payload ${formatBytes(result.payloadBytes)}, overhead ${formatBytes(result.overheadBytes)}`,
    `Brotli q${result.quality} per entry is isolated; entry values do not sum to the whole-pack result.`,
    "",
    "By type:",
    ...table(
      ["TYPE", "COUNT", "RAW", "BROTLI*"],
      result.byType.map((item) => [item.type, item.count, formatBytes(item.rawBytes), formatBytes(item.brotliBytes)]),
    ),
    "",
    `Largest entries${visible.length < result.entries.length ? ` (top ${visible.length})` : ""}:`,
    ...table(
      ["RAW", "BROTLI*", "TYPE", "DETAIL", "RESOURCE"],
      visible.map((item) => [formatBytes(item.rawBytes), formatBytes(item.brotliBytes), item.type, item.detail, item.name]),
    ),
  ];
  return lines.join("\n");
}

export function inferNamesPath(packPath) {
  const absolute = resolve(packPath);
  let cursor = dirname(absolute);
  const root = parse(absolute).root;
  while (cursor !== root && basename(cursor).toLowerCase() !== "build") cursor = dirname(cursor);
  if (basename(cursor).toLowerCase() !== "build") return "";
  const candidate = join(dirname(cursor), "src", "generated", `${basename(absolute, ".ntpack")}.h`);
  return existsSync(candidate) ? candidate : "";
}

export function main(argv = process.argv.slice(2)) {
  const options = parseArgs(argv);
  const packPath = resolve(options.packPath);
  if (!existsSync(packPath)) throw new Error(`pack not found: ${packPath}`);
  const namesPath = options.namesPath ? resolve(options.namesPath) : inferNamesPath(packPath);
  if (options.namesPath && !existsSync(namesPath)) throw new Error(`names header not found: ${namesPath}`);
  const names = namesPath ? parseNameHeader(readFileSync(namesPath, "utf8")) : new Map();
  const result = {
    packPath,
    namesPath,
    ...analyzeNtpackBuffer(readFileSync(packPath), { names, quality: options.quality }),
  };
  if (options.json) console.log(JSON.stringify(result, null, 2));
  else console.log(formatAnalysisText(result, { limit: options.limit }));
  return result;
}

if (isMain(import.meta.url)) {
  try {
    main();
  } catch (error) {
    console.error(`error: ${error.message}`);
    process.exitCode = 1;
  }
}
