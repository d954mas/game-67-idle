#!/usr/bin/env node
// Promote picked assets from an asset-review manifest into the shared library.
//
// Dry-run by default; pass --apply to write. Unlike accept_incoming_asset.mjs
// (which needs an _incoming/intake.json from the download flow) this handles
// assets already vendored into a project, computes integrity, copies the file +
// license, writes the catalog record (with origin), and appends to log.md.
//
//   node tools/asset_review/promote.mjs --manifest tmp/asset-review-ll/review-manifest.json \
//        --ids "assets__source__models__kenney__furniture_kit__desk.obj,..." \
//        --source kenney --license CC0-1.0 --license-url https://creativecommons.org/publicdomain/zero/1.0/ \
//        --apply
import { readFile, writeFile, mkdir, cp, appendFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve, basename, extname } from "node:path";
import { createHash } from "node:crypto";
import { DEFAULT_LIBRARY, KIND_DIR } from "../assets/source/find_assets.mjs";

const KIND_FROM_REVIEW = { model: "model", font: "font", audio: "audio", image: "texture", ui: "ui", texture: "texture" };

function kebab(s) {
  return String(s).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
}

function parseArgs(argv) {
  const a = {
    manifest: "tmp/asset-review/review-manifest.json",
    ids: "", library: DEFAULT_LIBRARY, repo: process.cwd(),
    source: "", license: "", licenseUrl: "", origin: "sourced", kind: "", apply: false,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--apply") { a.apply = true; continue; }
    const next = argv[i + 1];
    if (next === undefined) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--manifest") a.manifest = next;
    else if (arg === "--ids") a.ids = next;
    else if (arg === "--library") a.library = next;
    else if (arg === "--repo") a.repo = next;
    else if (arg === "--source") a.source = next;
    else if (arg === "--license") a.license = next;
    else if (arg === "--license-url") a.licenseUrl = next;
    else if (arg === "--origin") a.origin = next;
    else if (arg === "--kind") a.kind = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  if (!a.ids) throw new Error("missing --ids (comma-separated ids from the review manifest)");
  if (!a.license) throw new Error("missing --license (e.g. CC0-1.0, OFL-1.1)");
  if (!["mine", "ai", "sourced"].includes(a.origin)) throw new Error("--origin must be mine|ai|sourced");
  return a;
}

async function sha256(file) {
  return createHash("sha256").update(await readFile(file)).digest("hex");
}

function catalogMarkdown(rec) {
  return `---
type: Game Asset
title: ${rec.title}
description: ${rec.description}
resource: ${rec.resource}
tags: [${rec.tags.join(", ")}]
timestamp: ${rec.timestamp}
asset_id: ${rec.assetId}
kind: ${rec.kind}
status: accepted
origin: ${rec.origin}
license: ${rec.license}
license_url: ${rec.licenseUrl}
---

# ${rec.title}

## Provenance

- Source/vendor: ${rec.source}
- Origin: ${rec.origin}
- Promoted from project path: ${rec.relpath}
- Promoted at: ${rec.timestamp}
- SHA256: ${rec.sha}
- Bytes: ${rec.bytes}

## License Decision

- License: ${rec.license}
- License URL: ${rec.licenseUrl}
- License file: ${rec.licenseFile || "-"}

## Runtime Notes

- Import boundary: copy selected files into project-local \`assets/source/...\` before runtime use.
- Notes: promoted via asset review.
`;
}

async function main() {
  const a = parseArgs(process.argv.slice(2));
  const manifestPath = resolve(a.manifest);
  if (!existsSync(manifestPath)) throw new Error(`manifest not found: ${manifestPath}`);
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  const wantIds = new Set(a.ids.split(",").map((s) => s.trim()).filter(Boolean));
  const picks = (manifest.assets || []).filter((x) => wantIds.has(x.id));
  const missing = [...wantIds].filter((id) => !picks.some((p) => p.id === id));
  if (missing.length) throw new Error(`ids not in manifest: ${missing.join(", ")}`);
  if (!picks.length) throw new Error("no matching ids");

  const library = resolve(a.library);
  const ts = new Date().toISOString();
  const licenseSlug = kebab(a.license);
  const plan = [];
  for (const p of picks) {
    const source = a.source || (p.source && p.source !== "unknown" ? p.source : "");
    if (!source) throw new Error(`no source for ${p.id}; pass --source`);
    const kind = a.kind || KIND_FROM_REVIEW[p.kind] || p.kind;
    if (!KIND_DIR[kind]) throw new Error(`unsupported kind '${kind}' for ${p.id}; pass --kind`);
    const slug = kebab(basename(p.relpath, extname(p.relpath)));
    const assetId = `${kebab(source)}__${slug}__${licenseSlug}`;
    const kindDir = KIND_DIR[kind];
    plan.push({ pick: p, source, kind, kindDir, assetId });
  }

  console.log(`promote ${plan.length} asset(s) -> ${library}  [${a.apply ? "APPLY" : "DRY-RUN"}]`);
  const written = [];
  for (const item of plan) {
    const { pick, source, kind, kindDir, assetId } = item;
    const abs = resolve(a.repo, pick.relpath);
    if (!existsSync(abs)) throw new Error(`source file missing: ${abs}`);
    const filesDir = join(library, "files", kindDir, assetId);
    const catalogPath = join(library, "catalog", kindDir, `${assetId}.md`);
    const licenseDir = join(library, "licenses", assetId);
    console.log(`  ${assetId}  (${kind})  <- ${pick.relpath}`);
    if (!a.apply) continue;
    const bytes = (await readFile(abs)).length;
    const sha = await sha256(abs);
    await mkdir(filesDir, { recursive: true });
    await mkdir(join(library, "catalog", kindDir), { recursive: true });
    await mkdir(licenseDir, { recursive: true });
    await cp(abs, join(filesDir, basename(abs)), { force: true });
    let licenseFileRel = pick.licenseFile || "";
    if (pick.licenseFile) {
      const licAbs = resolve(a.repo, pick.licenseFile);
      if (existsSync(licAbs)) await cp(licAbs, join(licenseDir, basename(licAbs)), { force: true });
    }
    const rec = {
      assetId, title: pick.name, description: `${kind} promoted from ${source}`,
      resource: `files/${kindDir}/${assetId}/`, tags: [kind, source, a.origin, licenseSlug].filter(Boolean),
      timestamp: ts, kind, origin: a.origin, license: a.license, licenseUrl: a.licenseUrl,
      source, relpath: pick.relpath, sha, bytes, licenseFile: licenseFileRel,
    };
    await writeFile(catalogPath, catalogMarkdown(rec), "utf8");
    await writeFile(join(licenseDir, "license.md"), `# License: ${a.license}\n\n- Asset id: ${assetId}\n- Origin: ${a.origin}\n- License URL: ${a.licenseUrl}\n- Source/vendor: ${source}\n- Promoted from: ${pick.relpath}\n`, "utf8");
    written.push({ assetId, catalog: catalogPath });
  }

  if (a.apply && written.length) {
    await appendFile(join(library, "log.md"), `\n- ${ts} promoted ${written.length} asset(s) via asset review: ${written.map((w) => w.assetId).join(", ")}\n`, "utf8");
  }
  console.log(a.apply ? `\nwrote ${written.length} catalog record(s).` : `\ndry-run only — pass --apply to write.`);
}

main().catch((e) => { console.error(e.message); process.exit(1); });
