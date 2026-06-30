#!/usr/bin/env node
import { appendFile, cp, mkdir, readFile, rename, rm, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { basename, extname, join, resolve } from "node:path";
import { boolText, decideLicense, validateLicenseRecord } from "../license/registry.mjs";
import { isMain } from "../../../core_harness/tool_lib/cli.mjs";
import { sha256File } from "../../../core_harness/tool_lib/hash.mjs";
import { safeSegment } from "./stage.mjs";
import { KIND_DIR } from "../kinds.mjs";
import { defaultLibrarySourceRoot } from "../sources/libraries.mjs";

function usage() {
  return `usage: node ai_studio/assets/storage/intake/accept.mjs --source <source> --slug <slug> --file <relative-file> --pack <pack-id> --asset-id <id> --kind <kind> --license <license> [options]

Options:
  --source-root <path>              Asset source root. Defaults to the shared library.
  --title <title>                   Defaults to asset id.
  --description <text>
  --tags <a,b,c>
  --origin <mine|ai|sourced>        Default sourced.
  --source-page-url <url>           Public source/product page when known.
  --author-vendor <name>            Author, vendor, or source owner when known.
  --license-url <url>               License or terms URL when known.
  --license-kind <kind>             cc|spdx|custom|private|unknown when known.
  --attribution-required <bool>     Record release credit debt for CC-BY style assets.
  --notice-required <bool>          Record third-party notice debt for OFL/SPDX style assets.
  --credit-text <text>
  --commercial-use <bool>           Required before custom assets can be publishable.
  --modification-allowed <bool>     Required before custom assets can be publishable.
  --redistribution-allowed <bool>   Required before custom assets can be publishable.
  --publish <true|false>            Override only when license evidence supports it.
  --overwrite                       Replace existing target file and asset row.`;
}

function parseArgs(argv) {
  const args = {
    sourceRoot: defaultLibrarySourceRoot(process.cwd()),
    origin: "sourced",
    title: "",
    description: "",
    tags: "",
    sourcePageUrl: "",
    authorVendor: "",
    licenseUrl: "",
    licenseKind: "",
    attributionRequired: "",
    noticeRequired: "",
    creditText: "",
    commercialUse: "",
    modificationAllowed: "",
    redistributionAllowed: "",
    publish: "",
    overwrite: false,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--overwrite") { args.overwrite = true; continue; }
    if (arg === "--help" || arg === "-h") { console.log(usage()); process.exit(0); }
    const next = argv[i + 1];
    if (!next || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--source-root" || arg === "--library") args.sourceRoot = next;
    else if (arg === "--source") args.source = next;
    else if (arg === "--slug") args.slug = next;
    else if (arg === "--file") args.file = next;
    else if (arg === "--pack") args.pack = next;
    else if (arg === "--asset-id") args.assetId = next;
    else if (arg === "--kind") args.kind = next;
    else if (arg === "--title") args.title = next;
    else if (arg === "--description") args.description = next;
    else if (arg === "--tags") args.tags = next;
    else if (arg === "--origin") args.origin = next;
    else if (arg === "--source-page-url") args.sourcePageUrl = next;
    else if (arg === "--author-vendor") args.authorVendor = next;
    else if (arg === "--license") args.license = next;
    else if (arg === "--license-url") args.licenseUrl = next;
    else if (arg === "--license-kind") args.licenseKind = next;
    else if (arg === "--attribution-required") args.attributionRequired = next;
    else if (arg === "--notice-required") args.noticeRequired = next;
    else if (arg === "--credit-text") args.creditText = next;
    else if (arg === "--commercial-use") args.commercialUse = next;
    else if (arg === "--modification-allowed") args.modificationAllowed = next;
    else if (arg === "--redistribution-allowed") args.redistributionAllowed = next;
    else if (arg === "--publish") args.publish = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  for (const required of ["source", "slug", "file", "pack", "assetId", "kind", "license"]) {
    if (!args[required]) throw new Error(`missing required --${required.replace(/[A-Z]/g, (c) => `-${c.toLowerCase()}`)}`);
  }
  if (!KIND_DIR[args.kind]) throw new Error(`unsupported --kind ${args.kind}`);
  if (!["mine", "ai", "sourced"].includes(args.origin)) throw new Error("--origin must be mine|ai|sourced");
  for (const [name, value] of [
    ["attribution-required", args.attributionRequired],
    ["notice-required", args.noticeRequired],
    ["commercial-use", args.commercialUse],
    ["modification-allowed", args.modificationAllowed],
    ["redistribution-allowed", args.redistributionAllowed],
    ["publish", args.publish],
  ]) {
    if (value && !["true", "false"].includes(value)) throw new Error(`--${name} must be true or false`);
  }
  return args;
}

function list(value) {
  return String(value || "").split(",").map((item) => item.trim()).filter(Boolean);
}

async function readJson(path, fallback) {
  if (!existsSync(path)) return fallback;
  return JSON.parse(await readFile(path, "utf8"));
}

async function readJsonl(path) {
  if (!existsSync(path)) return [];
  const text = await readFile(path, "utf8");
  return text.split(/\r?\n/).map((line) => line.trim()).filter(Boolean).map((line, index) => {
    try { return JSON.parse(line); } catch (error) {
      throw new Error(`${path}:${index + 1}: invalid JSONL row: ${error.message}`);
    }
  });
}

async function writeJsonl(path, rows) {
  await writeFile(path, rows.map((row) => JSON.stringify(row)).join("\n") + (rows.length ? "\n" : ""), "utf8");
}

async function archiveAcceptedCandidate(sourceRoot, source, slug, stagedDir, assetId, overwrite = false) {
  const acceptedRoot = join(sourceRoot, "_accepted", source);
  let acceptedDir = join(acceptedRoot, slug);
  if (existsSync(acceptedDir)) {
    if (overwrite) await rm(acceptedDir, { recursive: true, force: true });
    else {
      const fallbackDir = join(acceptedRoot, `${slug}-${safeSegment(assetId, "asset-id")}`);
      acceptedDir = existsSync(fallbackDir) ? `${fallbackDir}-${Date.now()}` : fallbackDir;
    }
  }
  await mkdir(acceptedRoot, { recursive: true });
  await rename(stagedDir, acceptedDir);
  return acceptedDir;
}

function licenseMarkdown(record) {
  return `# License: ${record.license}

- Asset id: ${record.asset_id}
- License URL: ${record.license_url || "-"}
- License kind: ${record.license_kind}
- Attribution required: ${record.attribution_required}
- Notice required: ${record.notice_required}
- Credit text: ${record.credit_text || "-"}
- Commercial use: ${record.commercial_use}
- Modification allowed: ${record.modification_allowed}
- Redistribution allowed: ${record.redistribution_allowed}
- Publishable: ${record.publish}
- Source page: ${record.source_page || "-"}
- Author/vendor: ${record.author_vendor || "-"}
`;
}

export async function acceptStagedAsset(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  const sourceRoot = resolve(args.sourceRoot);
  const source = safeSegment(args.source, "source");
  const slug = safeSegment(args.slug, "slug");
  const packId = safeSegment(args.pack, "pack");
  const stagedDir = join(sourceRoot, "_incoming", source, slug);
  const intakePath = join(stagedDir, "intake.json");
  if (!existsSync(intakePath)) throw new Error(`missing staged intake: ${intakePath}`);
  const stagedFile = resolve(stagedDir, args.file);
  if (!stagedFile.startsWith(resolve(stagedDir))) throw new Error(`--file escapes staged directory: ${args.file}`);
  if (!existsSync(stagedFile)) throw new Error(`staged file not found: ${args.file}`);
  const intake = await readJson(intakePath, {});

  const decisionInput = {
    license: args.license,
    license_url: args.licenseUrl,
    license_kind: args.licenseKind,
    attribution_required: args.attributionRequired,
    notice_required: args.noticeRequired,
    commercial_use: args.commercialUse,
    modification_allowed: args.modificationAllowed,
    redistribution_allowed: args.redistributionAllowed,
    publish: args.publish || intake.publish || "",
    author_vendor: args.authorVendor || intake.source,
    source_page: args.sourcePageUrl || intake.source_page_url || intake.input,
    credit_text: args.creditText,
  };
  const decision = decideLicense(decisionInput);
  const publish = args.publish || intake.publish || boolText(decision.publishable, "false");
  const licenseKind = args.licenseKind || decision.licenseKind;
  const attributionRequired = args.attributionRequired || boolText(decision.attributionRequired, "false");
  const noticeRequired = args.noticeRequired || boolText(decision.noticeRequired, "false");
  const sourcePage = args.sourcePageUrl || intake.source_page_url || "";
  const authorVendor = args.authorVendor || intake.source || source;
  const creditText = args.creditText || (attributionRequired === "true" && authorVendor && sourcePage ? `${authorVendor} - ${sourcePage}` : "");
  const recordForValidation = {
    ...decisionInput,
    license_kind: licenseKind,
    attribution_required: attributionRequired,
    notice_required: noticeRequired,
    commercial_use: args.commercialUse || boolText(decision.commercialUse),
    modification_allowed: args.modificationAllowed || boolText(decision.modificationAllowed),
    redistribution_allowed: args.redistributionAllowed || boolText(decision.redistributionAllowed, "false"),
    publish,
    credit_text: creditText,
  };
  const validation = validateLicenseRecord(recordForValidation);
  if (!validation.ok) throw new Error(`invalid license decision: ${validation.issues.join("; ")}`);

  const packRootName = publish === "true" ? "packs" : join("restricted", "packs");
  const packDir = join(sourceRoot, packRootName, packId);
  const filesDir = join(packDir, "files");
  const licensesDir = join(packDir, "licenses");
  const targetName = basename(args.file);
  const targetFile = join(filesDir, targetName);
  if (existsSync(targetFile) && !args.overwrite) throw new Error(`target file exists; pass --overwrite: ${targetFile}`);

  await mkdir(filesDir, { recursive: true });
  await mkdir(licensesDir, { recursive: true });
  await cp(stagedFile, targetFile, { force: args.overwrite });

  const packPath = join(packDir, "pack.json");
  const pack = await readJson(packPath, {
    pack: packId,
    title: packId,
    source,
    kind: args.kind,
    origin: args.origin,
    license: args.license,
    license_url: args.licenseUrl || decision.licenseUrl || "",
    license_kind: licenseKind,
    attribution_required: attributionRequired,
    notice_required: noticeRequired,
    source_page: sourcePage,
    author_vendor: authorVendor,
    tags: list(args.tags),
    description: "",
  });
  pack.count = undefined;
  await writeFile(packPath, `${JSON.stringify(pack, null, 2)}\n`, "utf8");

  const asset = {
    asset_id: args.assetId,
    title: args.title || args.assetId,
    description: args.description || "",
    kind: args.kind,
    resource: `files/${targetName}`,
    tags: list(args.tags),
    origin: args.origin,
    license: args.license,
    license_url: args.licenseUrl || decision.licenseUrl || "",
    license_kind: licenseKind,
    attribution_required: attributionRequired,
    notice_required: noticeRequired,
    credit_text: creditText,
    commercial_use: args.commercialUse || boolText(decision.commercialUse),
    modification_allowed: args.modificationAllowed || boolText(decision.modificationAllowed),
    redistribution_allowed: args.redistributionAllowed || boolText(decision.redistributionAllowed, "false"),
    publish,
    source_page: sourcePage,
    author_vendor: authorVendor,
    sha256: await sha256File(targetFile),
    bytes: (await readFile(targetFile)).length,
  };
  if ([".glb", ".gltf"].includes(extname(targetName).toLowerCase())) asset.model = `files/${targetName}`;

  const assetsPath = join(packDir, "assets.jsonl");
  const rows = await readJsonl(assetsPath);
  const existing = rows.findIndex((row) => row.asset_id === args.assetId);
  if (existing >= 0 && !args.overwrite) throw new Error(`asset row exists; pass --overwrite: ${args.assetId}`);
  if (existing >= 0) rows[existing] = asset;
  else rows.push(asset);
  await writeJsonl(assetsPath, rows);
  await writeFile(join(licensesDir, `${args.assetId}.md`), licenseMarkdown(asset), "utf8");
  await appendFile(join(sourceRoot, "intake-log.md"), `- ${new Date().toISOString()} accepted ${args.assetId} from _incoming/${source}/${slug}/${args.file} -> ${packRootName}/${packId}\n`, "utf8");
  const acceptedDir = await archiveAcceptedCandidate(sourceRoot, source, slug, stagedDir, args.assetId, args.overwrite);

  return { asset_id: args.assetId, pack: packId, publish, pack_dir: packDir, resource: asset.resource, accepted_dir: acceptedDir };
}

if (isMain(import.meta.url)) {
  acceptStagedAsset().then((result) => {
    console.log(JSON.stringify(result, null, 2));
  }).catch((error) => {
    console.error(error.message);
    console.error(usage());
    process.exit(1);
  });
}
