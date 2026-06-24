#!/usr/bin/env node
// Promote picked assets from an asset-review manifest into the shared library.
//
// Dry-run by default; pass --apply to write. Unlike accept_incoming_asset.mjs
// (which needs an _incoming/intake.json from the download flow) this handles
// assets already vendored into a project, computes integrity, copies the file +
// license, writes the catalog record (with origin + legal flags), appends log.md.
//
//   node tools/asset_review/promote.mjs --manifest tmp/asset-review-ll/review-manifest.json \
//        --ids "a,b,c" --source kenney --license CC0-1.0 --apply
import { readFile, writeFile, mkdir, cp, appendFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve, basename, extname } from "node:path";
import { pathToFileURL } from "node:url";
import { createHash } from "node:crypto";
import { DEFAULT_LIBRARY, KIND_DIR } from "../assets/source/find_assets.mjs";
import { catalogFrontmatter } from "../lib/asset_catalog.mjs";

// review-kind -> library-kind, for legacy manifests; new manifests already carry
// a library kind, so this is only a fallback.
const KIND_FROM_REVIEW = { model: "model", font: "font", audio: "audio", image: "texture", ui: "ui", texture: "texture", material: "material" };
const FONT_EXT = [".ttf", ".otf", ".woff", ".woff2"];

const LICENSE_URLS = {
  "CC0-1.0": "https://creativecommons.org/publicdomain/zero/1.0/",
  "OFL-1.1": "https://openfontlicense.org/open-font-license-official-text/",
  "CC-BY-4.0": "https://creativecommons.org/licenses/by/4.0/",
  "CC-BY-SA-4.0": "https://creativecommons.org/licenses/by-sa/4.0/",
};

function kebab(s) {
  return String(s).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
}

function bool(v) {
  return v === "true" || v === true;
}

function parseArgs(argv) {
  const a = {
    manifest: "tmp/asset-review/review-manifest.json",
    ids: "", library: DEFAULT_LIBRARY, repo: process.cwd(),
    source: "", license: "", licenseUrl: "", origin: "sourced", kind: "",
    pack: "", packTitle: "", packUrl: "", packGenre: "", packStyle: "", packTags: "", packDesc: "", packCover: "",
    attributionRequired: "", commercialUse: "true", modificationAllowed: "true",
    redistributionAllowed: "true", shippingDecision: "allowed",
    apply: false, overwrite: false,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--apply") { a.apply = true; continue; }
    if (arg === "--overwrite") { a.overwrite = true; continue; }
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
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
    else if (arg === "--pack") a.pack = next;
    else if (arg === "--pack-title") a.packTitle = next;
    else if (arg === "--pack-url") a.packUrl = next;
    else if (arg === "--pack-genre") a.packGenre = next;
    else if (arg === "--pack-style") a.packStyle = next;
    else if (arg === "--pack-tags") a.packTags = next;
    else if (arg === "--pack-desc") a.packDesc = next;
    else if (arg === "--pack-cover") a.packCover = next;
    else if (arg === "--attribution-required") a.attributionRequired = next;
    else if (arg === "--commercial-use") a.commercialUse = next;
    else if (arg === "--modification-allowed") a.modificationAllowed = next;
    else if (arg === "--redistribution-allowed") a.redistributionAllowed = next;
    else if (arg === "--shipping-decision") a.shippingDecision = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  if (!a.ids) throw new Error("missing --ids (comma-separated ids from the review manifest)");
  if (!a.license) throw new Error("missing --license (e.g. CC0-1.0, OFL-1.1)");
  if (!["mine", "ai", "sourced"].includes(a.origin)) throw new Error("--origin must be mine|ai|sourced");
  if (!a.licenseUrl) {
    a.licenseUrl = LICENSE_URLS[a.license] || "";
    if (!a.licenseUrl) throw new Error(`no canonical URL for --license '${a.license}'; pass --license-url`);
  }
  // attribution default is license-aware (CC0 = no; OFL/CC-BY = yes) unless set.
  if (a.attributionRequired === "") a.attributionRequired = /ofl|cc-?by/i.test(a.license) ? "true" : "false";
  return a;
}

async function sha256(file) {
  return createHash("sha256").update(await readFile(file)).digest("hex");
}

function catalogMarkdown(rec) {
  const frontmatter = catalogFrontmatter({
    title: rec.title,
    description: rec.description,
    resource: rec.resource,
    tags: rec.tags,
    timestamp: rec.timestamp,
    assetId: rec.assetId,
    kind: rec.kind,
    status: "accepted",
    origin: rec.origin,
    license: rec.license,
    licenseUrl: rec.licenseUrl,
    attributionRequired: rec.attributionRequired,
    commercialUse: rec.commercialUse,
    modificationAllowed: rec.modificationAllowed,
    redistributionAllowed: rec.redistributionAllowed,
    publish: rec.publish,
    shippingDecision: rec.shippingDecision,
  }, rec.pack ? `pack: ${rec.pack}` : "");
  return `${frontmatter}

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
- Attribution required: ${rec.attributionRequired}
- Commercial use: ${rec.commercialUse}
- Modification allowed: ${rec.modificationAllowed}
- Redistribution allowed: ${rec.redistributionAllowed}
- Shipping decision: ${rec.shippingDecision}
- License file: licenses/${rec.licenseRef || rec.assetId}/license.md

## Runtime Notes

- Import boundary: copy selected files into project-local \`assets/source/...\` before runtime use.
- Notes: promoted via asset review.
`;
}

function packMarkdown(m) {
  const fmList = (csv) => `[${(csv || "").split(",").map((s) => s.trim()).filter(Boolean).join(", ")}]`;
  return `---
type: Asset Pack
title: ${m.title}
pack: ${m.pack}
source: ${m.source}
kind: ${m.kind}
license: ${m.license}
license_url: ${m.licenseUrl}
origin: ${m.origin}
count: ${m.count}
genre: ${fmList(m.genre)}
style: ${fmList(m.style)}
tags: ${fmList(m.tags)}
${m.cover ? `cover: ${m.cover}\n` : ""}${m.description ? `description: ${m.description}\n` : ""}timestamp: ${m.timestamp}
---

# ${m.title}

- Source/vendor: ${m.source}
- Pack: ${m.pack}
- Source page: ${m.url || "-"}
- License: ${m.license}
- Assets: ${m.count} (${m.kind})
- Origin: ${m.origin}
${m.genre ? `- Genre: ${m.genre}\n` : ""}${m.style ? `- Style: ${m.style}\n` : ""}- Prepared: ${m.timestamp}

${m.description || "Reusable pack — assets share an art style and combine."} Copy
individual assets into a project's \`assets/source/...\`; do not load from the
library directly.
`;
}

function licenseMarkdown(rec) {
  return `# License: ${rec.license}

- ${rec.pack ? "Pack" : "Asset id"}: ${rec.pack || rec.assetId}
- Origin: ${rec.origin}
- License URL: ${rec.licenseUrl}
- Source/vendor: ${rec.source}
- Attribution required: ${rec.attributionRequired}
- Commercial use: ${rec.commercialUse}
- Modification allowed: ${rec.modificationAllowed}
- Redistribution allowed: ${rec.redistributionAllowed}
- Shipping decision: ${rec.shippingDecision}
- Promoted from: ${rec.relpath}
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

  // M4: one --license cannot stamp both fonts and non-fonts correctly.
  const isFont = (p) => p.kind === "font" || FONT_EXT.includes(extname(p.relpath).toLowerCase());
  const fonts = picks.filter(isFont);
  const nonFonts = picks.filter((p) => !isFont(p));
  const licIsFont = /ofl|font/i.test(a.license);
  if (fonts.length && nonFonts.length) throw new Error("selection mixes font and non-font under one --license; run promote once per license group");
  if (fonts.length && !licIsFont) throw new Error(`font assets selected but --license '${a.license}' is not a font license (use OFL-1.1)`);
  if (nonFonts.length && licIsFont) throw new Error(`non-font assets selected but --license '${a.license}' looks font-only`);

  const library = resolve(a.library);
  const ts = new Date().toISOString();
  const licenseSlug = kebab(a.license);
  const packSlug = a.pack ? kebab(a.pack) : "";

  // build plan + resolve kinds
  const plan = [];
  for (const p of picks) {
    const source = a.source || (p.source && p.source !== "unknown" ? p.source : "");
    if (!source) throw new Error(`no source for ${p.id}; pass --source`);
    if (a.source && p.source && p.source !== "unknown" && kebab(a.source) !== kebab(p.source)) {
      console.warn(`warn: --source ${a.source} overrides manifest source ${p.source} for ${p.id}`);
    }
    let kind = a.kind || (KIND_DIR[p.kind] ? p.kind : KIND_FROM_REVIEW[p.kind]);
    if (kind === "image" || !kind) throw new Error(`ambiguous kind for ${p.id}; pass --kind (ui|texture|model|font|audio)`);
    if (!KIND_DIR[kind]) throw new Error(`unsupported kind '${kind}' for ${p.id}`);
    const slug = kebab(basename(p.relpath, extname(p.relpath)));
    const assetId = `${kebab(source)}__${slug}__${licenseSlug}`;
    const kindDir = packSlug ? `${KIND_DIR[kind]}/${packSlug}` : KIND_DIR[kind];
    plan.push({ pick: p, source, kind, kindDir, assetId });
  }

  // H3: reject duplicate asset_ids within the batch before any write.
  const seen = new Map();
  for (const it of plan) {
    if (seen.has(it.assetId)) throw new Error(`duplicate asset_id ${it.assetId}: ${seen.get(it.assetId)} vs ${it.pick.relpath}`);
    seen.set(it.assetId, it.pick.relpath);
  }

  // H2: refuse to clobber existing curated records unless --overwrite.
  for (const it of plan) {
    it.filesDir = join(library, "files", it.kindDir, it.assetId);
    it.catalogPath = join(library, "catalog", it.kindDir, `${it.assetId}.md`);
    it.licenseDir = packSlug ? join(library, "licenses", packSlug) : join(library, "licenses", it.assetId);
    it.exists = existsSync(it.catalogPath) || existsSync(it.filesDir);
  }
  const collisions = plan.filter((it) => it.exists);
  if (collisions.length && !a.overwrite) {
    throw new Error(`already in library (pass --overwrite to replace): ${collisions.map((c) => c.assetId).join(", ")}`);
  }

  console.log(`promote ${plan.length} asset(s) -> ${library}${a.pack ? ` [pack ${a.pack}]` : ""}  [${a.apply ? "APPLY" : "DRY-RUN"}]`);
  const written = [];
  const packsWritten = new Set();
  for (const it of plan) {
    const { pick, source, kind, kindDir, assetId, filesDir, catalogPath, licenseDir } = it;
    const abs = resolve(a.repo, pick.relpath);
    if (!existsSync(abs)) throw new Error(`source file missing: ${abs}`);
    console.log(`  ${assetId}  (${kind})${it.exists ? " [OVERWRITE]" : ""}  <- ${pick.relpath}`);
    if (!a.apply) continue;
    const bytes = (await readFile(abs)).length;
    const sha = await sha256(abs);
    await mkdir(filesDir, { recursive: true });
    await mkdir(join(library, "catalog", kindDir), { recursive: true });
    await mkdir(licenseDir, { recursive: true });
    await cp(abs, join(filesDir, basename(abs)), { force: a.overwrite });
    const rec = {
      assetId, title: pick.name, description: `${kind} promoted from ${source}`,
      resource: `files/${kindDir}/${assetId}/`, tags: [kind, source, licenseSlug, packSlug].filter(Boolean),
      timestamp: ts, kind, origin: a.origin, license: a.license, licenseUrl: a.licenseUrl,
      source, relpath: pick.relpath, sha, bytes, pack: a.pack || "", licenseRef: packSlug || assetId,
      attributionRequired: a.attributionRequired, commercialUse: a.commercialUse,
      modificationAllowed: a.modificationAllowed, redistributionAllowed: a.redistributionAllowed,
      publish: "true",
      shippingDecision: a.shippingDecision,
    };
    await writeFile(catalogPath, catalogMarkdown(rec), "utf8");
    // license: shared once per pack, else per-asset; copy the source license file too
    if (!packSlug || !packsWritten.has(kindDir)) {
      await writeFile(join(licenseDir, "license.md"), licenseMarkdown(rec), "utf8");
      if (pick.licenseFile) {
        const licAbs = resolve(a.repo, pick.licenseFile);
        if (existsSync(licAbs)) await cp(licAbs, join(licenseDir, basename(licAbs)), { force: true });
      }
    }
    // pack record (the unit) written once per pack folder
    if (packSlug && !packsWritten.has(kindDir)) {
      await writeFile(join(library, "catalog", kindDir, "_pack.md"), packMarkdown({
        title: a.packTitle || a.pack, pack: a.pack, source, kind, license: a.license,
        licenseUrl: a.licenseUrl, origin: a.origin, count: plan.length, timestamp: ts, url: a.packUrl,
        genre: a.packGenre, style: a.packStyle, tags: a.packTags, description: a.packDesc, cover: a.packCover,
      }), "utf8");
      packsWritten.add(kindDir);
    }
    // append to log immediately so a partial failure leaves an accurate log
    const logPath = join(library, "log.md");
    if (!existsSync(logPath)) await writeFile(logPath, "# Library Log\n", "utf8");
    await appendFile(logPath, `- ${ts} promoted ${assetId} (${originOrKind(rec)})${a.pack ? ` [pack ${a.pack}]` : ""} from ${pick.relpath}\n`, "utf8");
    written.push({ assetId, catalog: catalogPath });
  }
  console.log(a.apply ? `\nwrote ${written.length} catalog record(s).` : `\ndry-run only — pass --apply to write.`);
}

function originOrKind(rec) {
  return `${rec.origin}/${rec.kind}`;
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((e) => { console.error(e.message); process.exit(1); });
}

export { parseArgs, catalogMarkdown, kebab };
