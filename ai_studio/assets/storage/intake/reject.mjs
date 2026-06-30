#!/usr/bin/env node
import { mkdir, rm, rename } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { isMain } from "../../../../tools/lib/cli.mjs";
import { safeSegment } from "./stage.mjs";
import { defaultLibrarySourceRoot } from "../sources/libraries.mjs";

function usage() {
  return `usage: node ai_studio/assets/storage/intake/reject.mjs --source <source> --slug <slug> [options]

Options:
  --source-root <path>  Asset source root. Defaults to the shared library.
  --reason <text>
  --delete              Delete instead of moving to _rejected.
  --overwrite           Replace existing rejected folder.`;
}

function parseArgs(argv) {
  const args = { sourceRoot: defaultLibrarySourceRoot(process.cwd()), reason: "", delete: false, overwrite: false };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--delete") { args.delete = true; continue; }
    if (arg === "--overwrite") { args.overwrite = true; continue; }
    if (arg === "--help" || arg === "-h") { console.log(usage()); process.exit(0); }
    const next = argv[i + 1];
    if (!next || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--source-root" || arg === "--library") args.sourceRoot = next;
    else if (arg === "--source") args.source = next;
    else if (arg === "--slug") args.slug = next;
    else if (arg === "--reason") args.reason = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  for (const required of ["source", "slug"]) {
    if (!args[required]) throw new Error(`missing required --${required}`);
  }
  return args;
}

export async function rejectStagedAsset(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  const sourceRoot = resolve(args.sourceRoot);
  const source = safeSegment(args.source, "source");
  const slug = safeSegment(args.slug, "slug");
  const incoming = join(sourceRoot, "_incoming", source, slug);
  if (!existsSync(incoming)) throw new Error(`incoming item not found: ${incoming}`);
  if (args.delete) {
    await rm(incoming, { recursive: true, force: true });
    return { source, slug, deleted: true };
  }
  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const target = join(sourceRoot, "_rejected", source, `${slug}-${stamp}`);
  if (existsSync(target) && !args.overwrite) throw new Error(`rejected target exists; pass --overwrite: ${target}`);
  if (existsSync(target)) await rm(target, { recursive: true, force: true });
  await mkdir(join(sourceRoot, "_rejected", source), { recursive: true });
  await rename(incoming, target);
  return { source, slug, rejected: target, reason: args.reason || "" };
}

if (isMain(import.meta.url)) {
  rejectStagedAsset().then((result) => {
    console.log(JSON.stringify(result, null, 2));
  }).catch((error) => {
    console.error(error.message);
    console.error(usage());
    process.exit(1);
  });
}
