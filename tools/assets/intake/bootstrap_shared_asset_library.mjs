#!/usr/bin/env node
import { existsSync } from "node:fs";
import { mkdir, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";

const DEFAULT_LIBRARY = "C:\\Users\\ROG\\YandexDisk\\gamedev\\assets\\ai_pipeline_assets";

function usage() {
  return `usage: node tools/assets/intake/bootstrap_shared_asset_library.mjs [--library <path>] [--force]

Creates the shared OKF-style source asset library folders and Markdown templates.
Default library: ${DEFAULT_LIBRARY}
`;
}

function parseArgs(argv) {
  const args = { library: DEFAULT_LIBRARY, force: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--force") {
      args.force = true;
      continue;
    }
    if (arg === "--library") {
      const next = argv[index + 1];
      if (!next || next.startsWith("--")) throw new Error("missing value for --library");
      args.library = next;
      index += 1;
      continue;
    }
    if (arg === "--help" || arg === "-h") {
      console.log(usage());
      process.exit(0);
    }
    throw new Error(`unknown option: ${arg}`);
  }
  return args;
}

const directories = [
  "_incoming",
  "_quarantine",
  "_templates",
  "catalog/models",
  "catalog/textures",
  "catalog/materials",
  "catalog/audio",
  "catalog/ui",
  "catalog/fonts",
  "catalog/references",
  "files/models",
  "files/textures",
  "files/materials",
  "files/audio",
  "files/ui",
  "files/fonts",
  "files/references",
  "licenses",
  "previews",
  "tools-notes",
];

const templates = {
  "_templates/asset-record.md": `---
type: Game Asset
title: Short Human Title
description: One sentence for search and selection.
resource: files/<kind>/<asset-id>/
tags: [asset, source, license]
timestamp: 2026-06-20T00:00:00Z
asset_id: <source>__<asset-slug>__<license>
kind: model
status: incoming
license: unknown
license_url:
attribution_required: unknown
commercial_use: unknown
modification_allowed: unknown
redistribution_allowed: unknown
shipping_decision: reference-only
---

# Short Human Title

## Provenance

- Source page:
- Direct download:
- Author/vendor:
- Downloaded at:
- Local source:
- SHA256:

## License Decision

- Decision: accepted / reference-only / blocked
- Reason:

## Runtime Notes

- Intended use:
- Formats:
- Scale/axis/material notes:
- Import boundary:
`,
  "_templates/model-record.md": `---
type: Game Asset
kind: model
title:
description:
resource: files/models/<asset-id>/
tags: [model]
asset_id:
status: incoming
license: unknown
has_animation: unknown
formats: []
---

# Model Record

## Provenance

- Source page:
- Author/vendor:
- License:
- Attribution:

## Technical Notes

- Formats:
- Animation clips:
- Axis/up:
- Units:
- Materials/textures:
- Preview:
`,
  "_templates/texture-record.md": `---
type: Game Asset
kind: texture
title:
description:
resource: files/textures/<asset-id>/
tags: [texture]
asset_id:
status: incoming
license: unknown
tileable: unknown
wrap_mode: repeat
maps: [albedo]
preview_2x2:
---

# Texture Record

## Usage Contract

- Usage class: tileable material / asset material / decal / material map set
- Tiling: repeat / clamp / repeat-x / repeat-y / unique
- UV assumption:
- Gameplay scale:

## Provenance

- Source page or generation prompt:
- Author/generator:
- License or no-seed reason:

## Checks

- Seam audit:
- 2x2 preview:
- Runtime path:
`,
  "_templates/material-record.md": `---
type: Game Asset
kind: material
title:
description:
resource: files/materials/<asset-id>/
tags: [material]
asset_id:
status: incoming
license: unknown
maps: [albedo, normal, roughness]
---

# Material Record

## Maps

- Albedo:
- Normal:
- Roughness:
- Metallic:
- Emissive:

## Contract

- Tiling:
- UV assumption:
- Runtime shader/material path:
`,
};

const readme = `# AI Pipeline Source Asset Library

Shared OKF-style catalog for legal downloaded or generated source assets.

- Search \`catalog/**/*.md\` first by tags, license, description, and technical
  fields.
- Store binaries under \`files/<kind>/<asset-id>/\`.
- Keep unreviewed downloads in \`_incoming/\`; unclear or unsafe assets go to
  \`_quarantine/\`.
- Game projects copy selected files into project-local asset folders before
  runtime use.
- Every accepted asset needs license, provenance, integrity, and import notes.
`;

async function writeIfMissing(path, text, force) {
  if (existsSync(path) && !force) return false;
  await writeFile(path, text, "utf8");
  return true;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const library = resolve(args.library);
  for (const dir of directories) {
    await mkdir(join(library, dir), { recursive: true });
  }

  const written = [];
  for (const [rel, text] of Object.entries(templates)) {
    if (await writeIfMissing(join(library, rel), text, args.force)) written.push(rel);
  }
  if (await writeIfMissing(join(library, "README.md"), readme, args.force)) written.push("README.md");

  console.log(JSON.stringify({ library, directories: directories.length, templates_written: written }, null, 2));
}

main().catch((error) => {
  console.error(error.message);
  console.error(usage());
  process.exit(1);
});
