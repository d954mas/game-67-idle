#!/usr/bin/env node
import { cp, mkdir, readFile, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { basename, join, resolve } from "node:path";
import { DEFAULT_LIBRARY, KIND_DIR } from "../source/find_assets.mjs";
import { catalogFrontmatter, licenseMarkdown } from "../../lib/asset_catalog.mjs";

function usage() {
  return `usage: node tools/assets/intake/accept_incoming_asset.mjs --source <source> --slug <slug> --asset-id <id> --kind <kind> --title <title> --description <text> --license-name <name> --license-url <url> --tags <a,b,c> [options]

Options:
  --library <path>                 Defaults to ${DEFAULT_LIBRARY}
  --origin <mine|ai|sourced>       who made it; default sourced (downloaded free/CC0)
  --source-page-url <url>          human/audit source page; defaults to intake URL
  --author-vendor <name>           author/vendor; defaults to intake source
  --attribution-required <bool>    default false
  --commercial-use <bool>          default true
  --modification-allowed <bool>    default true
  --redistribution-allowed <bool>  default follows --publish (true unless paid)
  --publish <true|false>           may this asset be committed to an open repo?
                                   default reads intake.json, else true. Paid packs
                                   use false -> pull routes the binary to assets/restricted/
  --shipping-decision <value>      default allowed
  --tileable <bool|unknown>        texture metadata
  --wrap-mode <mode>               texture metadata
  --preview-2x2 <path>             texture proof path, usually previews/<asset-id>/tile_2x2.png
  --seam-audit <path>              texture seam audit path, usually previews/<asset-id>/tile_audit.json
  --uv-assumption <text>           model/texture metadata
  --notes <text>                   runtime/import notes
  --overwrite                      replace existing catalog/files/license outputs
`;
}

function parseArgs(argv) {
  const args = {
    library: DEFAULT_LIBRARY,
    origin: "sourced",
    attributionRequired: "false",
    commercialUse: "true",
    modificationAllowed: "true",
    redistributionAllowed: "",
    publish: "",
    shippingDecision: "allowed",
    sourcePageUrl: "",
    authorVendor: "",
    tileable: "",
    wrapMode: "",
    preview2x2: "",
    seamAudit: "",
    uvAssumption: "",
    notes: "",
    overwrite: false,
  };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--overwrite") {
      args.overwrite = true;
      continue;
    }
    const next = argv[index + 1];
    if (!next || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    index += 1;
    if (arg === "--library") args.library = next;
    else if (arg === "--origin") args.origin = next;
    else if (arg === "--source") args.source = next;
    else if (arg === "--slug") args.slug = next;
    else if (arg === "--asset-id") args.assetId = next;
    else if (arg === "--kind") args.kind = next;
    else if (arg === "--title") args.title = next;
    else if (arg === "--description") args.description = next;
    else if (arg === "--license-name") args.licenseName = next;
    else if (arg === "--license-url") args.licenseUrl = next;
    else if (arg === "--tags") args.tags = next;
    else if (arg === "--source-page-url") args.sourcePageUrl = next;
    else if (arg === "--author-vendor") args.authorVendor = next;
    else if (arg === "--attribution-required") args.attributionRequired = next;
    else if (arg === "--commercial-use") args.commercialUse = next;
    else if (arg === "--modification-allowed") args.modificationAllowed = next;
    else if (arg === "--redistribution-allowed") args.redistributionAllowed = next;
    else if (arg === "--publish") args.publish = next;
    else if (arg === "--shipping-decision") args.shippingDecision = next;
    else if (arg === "--tileable") args.tileable = next;
    else if (arg === "--wrap-mode") args.wrapMode = next;
    else if (arg === "--preview-2x2") args.preview2x2 = next;
    else if (arg === "--seam-audit") args.seamAudit = next;
    else if (arg === "--uv-assumption") args.uvAssumption = next;
    else if (arg === "--notes") args.notes = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  for (const required of ["source", "slug", "assetId", "kind", "title", "description", "licenseName", "licenseUrl", "tags"]) {
    if (!args[required]) throw new Error(`missing required --${required.replace(/[A-Z]/g, (c) => `-${c.toLowerCase()}`)}`);
  }
  if (!KIND_DIR[args.kind]) throw new Error(`unsupported --kind ${args.kind}`);
  if (!["mine", "ai", "sourced"].includes(args.origin)) throw new Error(`--origin must be mine|ai|sourced`);
  for (const [name, value] of [["publish", args.publish], ["redistribution-allowed", args.redistributionAllowed]]) {
    if (value && !["true", "false"].includes(value)) throw new Error(`--${name} must be true or false`);
  }
  return args;
}

function extraFrontmatter(args) {
  const lines = [];
  if (args.kind === "texture") {
    if (args.tileable) lines.push(`tileable: ${args.tileable}`);
    if (args.wrapMode) lines.push(`wrap_mode: ${args.wrapMode}`);
    if (args.preview2x2) lines.push(`preview_2x2: ${args.preview2x2}`);
    if (args.seamAudit) lines.push(`seam_audit: ${args.seamAudit}`);
  }
  if (args.uvAssumption) lines.push(`uv_assumption: ${JSON.stringify(args.uvAssumption)}`);
  return lines.join("\n");
}

function catalogMarkdown(args, intake, resource) {
  const extra = extraFrontmatter(args);
  const sourcePage = args.sourcePageUrl || intake.source_page_url || intake.url || "-";
  const authorVendor = args.authorVendor || intake.source;
  const directDownload = intake.manual ? "(manual/account-gated — not stored)" : (intake.url || "-");
  const frontmatter = catalogFrontmatter({
    title: args.title,
    description: args.description,
    resource,
    tags: args.tags,
    timestamp: new Date().toISOString(),
    assetId: args.assetId,
    kind: args.kind,
    status: "accepted",
    origin: args.origin,
    license: args.licenseName,
    licenseUrl: args.licenseUrl,
    attributionRequired: args.attributionRequired,
    commercialUse: args.commercialUse,
    modificationAllowed: args.modificationAllowed,
    redistributionAllowed: args.redistributionAllowed,
    publish: args.publish,
    shippingDecision: args.shippingDecision,
  }, extra);
  return `${frontmatter}

# ${args.title}

## Provenance

- Source page: ${sourcePage}
- Direct download: ${directDownload}
- Author/vendor: ${authorVendor}
- Downloaded at: ${intake.downloaded_at}
- Original filename: ${intake.original_filename || "-"}
- SHA256: ${intake.sha256}
- Bytes: ${intake.bytes}

## License Decision

- License: ${args.licenseName}
- License URL: ${args.licenseUrl}
- Attribution required: ${args.attributionRequired}
- Commercial use: ${args.commercialUse}
- Modification allowed: ${args.modificationAllowed}
- Redistribution allowed: ${args.redistributionAllowed}
- Publishable (commit to open repo): ${args.publish}
- Shipping decision: ${args.shippingDecision}

## Runtime Notes

- Import boundary: copy selected files into project-local \`assets/source/...\` before runtime use.
- Texture proof: ${args.preview2x2 || args.seamAudit ? [args.preview2x2, args.seamAudit].filter(Boolean).join(", ") : "-"}
- Notes: ${args.notes || "-"}
`;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const library = resolve(args.library);
  const kindDir = KIND_DIR[args.kind];
  const incomingDir = join(library, "_incoming", args.source, args.slug);
  const intakePath = join(incomingDir, "intake.json");
  if (!existsSync(intakePath)) throw new Error(`missing incoming intake: ${intakePath}`);
  const intake = JSON.parse(await readFile(intakePath, "utf8"));
  // Resolve publishability: explicit flag > intake.json > default true (free/CC0).
  args.publish = args.publish || intake.publish || "true";
  // Redistribution defaults to follow publishability unless the lead set it explicitly.
  args.redistributionAllowed = args.redistributionAllowed || (args.publish === "true" ? "true" : "false");

  const filesDir = join(library, "files", kindDir, args.assetId);
  const catalogPath = join(library, "catalog", kindDir, `${args.assetId}.md`);
  const licenseDir = join(library, "licenses", args.assetId);
  const licensePath = join(licenseDir, "license.md");
  if (!args.overwrite) {
    for (const path of [filesDir, catalogPath, licensePath]) {
      if (existsSync(path)) throw new Error(`target exists; pass --overwrite: ${path}`);
    }
  }

  await mkdir(filesDir, { recursive: true });
  await mkdir(join(library, "catalog", kindDir), { recursive: true });
  await mkdir(licenseDir, { recursive: true });
  await cp(intake.path, join(filesDir, basename(intake.path)), { force: args.overwrite });
  await cp(join(incomingDir, "download-log.md"), join(filesDir, "download-log.md"), { force: args.overwrite });
  await cp(intakePath, join(filesDir, "intake.json"), { force: args.overwrite });

  const resource = `files/${kindDir}/${args.assetId}/`;
  await writeFile(catalogPath, catalogMarkdown(args, intake, resource), "utf8");
  await writeFile(licensePath, licenseMarkdown({
    license: args.licenseName,
    assetId: args.assetId,
    origin: args.origin,
    licenseUrl: args.licenseUrl,
    attributionRequired: args.attributionRequired,
    commercialUse: args.commercialUse,
    modificationAllowed: args.modificationAllowed,
    redistributionAllowed: args.redistributionAllowed,
    publish: args.publish,
    shippingDecision: args.shippingDecision,
    directDownload: intake.manual ? "(manual/account-gated — not stored)" : (intake.url || "-"),
    sourcePage: args.sourcePageUrl || intake.source_page_url || intake.url || "-",
    authorVendor: args.authorVendor || intake.source,
  }), "utf8");

  console.log(JSON.stringify({ asset_id: args.assetId, kind: args.kind, catalog: catalogPath, resource, license: licensePath }, null, 2));
}

main().catch((error) => {
  console.error(error.message);
  console.error(usage());
  process.exit(1);
});
