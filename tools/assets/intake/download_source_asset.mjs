#!/usr/bin/env node
import { createHash } from "node:crypto";
import { existsSync } from "node:fs";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { basename, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { DEFAULT_LIBRARY } from "../source/find_assets.mjs";

function usage() {
  return `usage: node tools/assets/intake/download_source_asset.mjs --url <url-or-path> --source <source> --slug <asset-slug> [options]

Options:
  --library <path>         Asset library root. Defaults to ${DEFAULT_LIBRARY}
  --filename <name>        Output filename. Defaults to URL/path basename.
  --license-name <name>    Known license label, or unknown.
  --manual                 Manual/account-gated download (e.g. a paid CGTrader pack).
                           --url is read as a LOCAL file but is NOT stored; only the
                           filename + sha256 + product page are recorded as provenance.
  --source-page-url <url>  Public product/source page (recorded instead of a download link).
  --publish <true|false>   Publishability of the asset; paid packs use false. Flows to
                           the catalog so the asset routes to assets/restricted/.
  --overwrite              Replace an existing downloaded file.

Manual paid example (no download link is stored):
  node tools/assets/intake/download_source_asset.mjs --manual \\
    --url "C:/Users/.../NatureGradientPack.zip" --source cgtrader \\
    --slug nature-gradient-pack --license-name "CGTrader Royalty Free" \\
    --source-page-url https://www.cgtrader.com/... --publish false
`;
}

function parseArgs(argv) {
  const args = {
    library: DEFAULT_LIBRARY,
    licenseName: "unknown",
    manual: false,
    sourcePageUrl: "",
    publish: "",
    overwrite: false,
  };

  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--overwrite") {
      args.overwrite = true;
      continue;
    }
    if (arg === "--manual") {
      args.manual = true;
      continue;
    }
    const next = argv[index + 1];
    if (!next || next.startsWith("--")) {
      throw new Error(`missing value for ${arg}`);
    }
    index += 1;
    if (arg === "--url") args.url = next;
    else if (arg === "--source") args.source = next;
    else if (arg === "--slug") args.slug = next;
    else if (arg === "--library") args.library = next;
    else if (arg === "--filename") args.filename = next;
    else if (arg === "--license-name") args.licenseName = next;
    else if (arg === "--source-page-url") args.sourcePageUrl = next;
    else if (arg === "--publish") args.publish = next;
    else throw new Error(`unknown option: ${arg}`);
  }

  for (const required of ["url", "source", "slug"]) {
    if (!args[required]) throw new Error(`missing required --${required}`);
  }
  if (args.publish && !["true", "false"].includes(args.publish)) {
    throw new Error("--publish must be true or false");
  }
  return args;
}

function safeSegment(value, label) {
  const cleaned = value
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9._-]+/g, "-")
    .replace(/^-+|-+$/g, "");
  if (!cleaned) throw new Error(`${label} becomes empty after sanitizing`);
  return cleaned;
}

function inferFilename(urlOrPath) {
  try {
    const parsed = new URL(urlOrPath);
    const name = basename(decodeURIComponent(parsed.pathname));
    return name || "download.bin";
  } catch {
    return basename(urlOrPath) || "download.bin";
  }
}

async function readSource(urlOrPath) {
  if (urlOrPath.startsWith("http://") || urlOrPath.startsWith("https://")) {
    const response = await fetch(urlOrPath);
    if (!response.ok) {
      throw new Error(`download failed: HTTP ${response.status} ${response.statusText}`);
    }
    return Buffer.from(await response.arrayBuffer());
  }

  if (urlOrPath.startsWith("file://")) {
    return readFile(fileURLToPath(urlOrPath));
  }

  return readFile(urlOrPath);
}

function sha256(buffer) {
  return createHash("sha256").update(buffer).digest("hex");
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const source = safeSegment(args.source, "source");
  const slug = safeSegment(args.slug, "slug");
  const filename = args.filename || inferFilename(args.url);
  const targetDir = resolve(args.library, "_incoming", source, slug);
  const targetPath = join(targetDir, filename);

  if (existsSync(targetPath) && !args.overwrite) {
    throw new Error(`target already exists; pass --overwrite: ${targetPath}`);
  }

  const data = await readSource(args.url);
  const hash = sha256(data);
  await mkdir(targetDir, { recursive: true });
  await writeFile(targetPath, data);

  // Manual (paid/account-gated) intake: never store the local download path — only
  // the public product page, the original filename, sha256 and bytes for integrity.
  const record = {
    status: "incoming",
    source,
    slug,
    url: args.manual ? args.sourcePageUrl : args.url,
    source_page_url: args.sourcePageUrl,
    license_name: args.licenseName,
    downloaded_at: new Date().toISOString(),
    path: targetPath,
    bytes: data.length,
    sha256: hash,
    ...(args.manual ? { manual: true, original_filename: filename } : {}),
    ...(args.publish ? { publish: args.publish } : {}),
  };

  const log = `# Download Log

- Query / source site: ${source}
- Candidate URL: ${args.manual ? "(manual/account-gated download — link not stored)" : args.url}
- Product / source page: ${args.sourcePageUrl || "-"}
- Original filename: ${filename}
- Selected because:
- License checked before download: ${args.licenseName === "unknown" ? "no" : "yes"}
- License name: ${args.licenseName}
- Publishable: ${args.publish || "(unset)"}
- Download command/tool: tools/assets/intake/download_source_asset.mjs${args.manual ? " --manual" : ""}
- Result path: ${targetPath}
- Size/hash checked: yes
- Bytes: ${data.length}
- SHA256: ${hash}
- Next step: accept / quarantine / reject
`;
  await writeFile(join(targetDir, "download-log.md"), log);
  await writeFile(join(targetDir, "intake.json"), `${JSON.stringify(record, null, 2)}\n`);
  console.log(JSON.stringify(record, null, 2));
}

main().catch((error) => {
  console.error(error.message);
  console.error(usage());
  process.exit(1);
});
