#!/usr/bin/env node
// Promote picked assets from an asset-review manifest into Pack Manifest storage.
//
// Dry-run by default; pass --apply to write. This handles assets already
// vendored into a project, computes integrity, copies the file + license, writes
// pack.json/assets.jsonl metadata, and appends intake-log.md.
//
//   node ai_studio/assets/viewer/promote.mjs --manifest tmp/asset-review-ll/review-manifest.json \
//        --ids "a,b,c" --source kenney --license CC0-1.0 --apply
import { readFile, writeFile, mkdir, cp, appendFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve, basename, extname } from "node:path";
import { createHash } from "node:crypto";
import { DEFAULT_ASSET_SOURCE_ROOT } from "../storage/defaults.mjs";
import { KIND_DIR } from "../storage/kinds.mjs";
import { isMain } from "../../../tools/lib/cli.mjs";
import { LICENSE_URLS } from "../../../tools/lib/licenses.mjs";
import { boolText, decideLicense, validateLicenseRecord } from "../storage/license/registry.mjs";

// review-kind -> library-kind, for legacy manifests; new manifests already carry
// a library kind, so this is only a fallback.
const KIND_FROM_REVIEW = { model: "model", font: "font", audio: "audio", image: "texture", ui: "ui", texture: "texture", material: "material" };
const FONT_EXT = [".ttf", ".otf", ".woff", ".woff2"];

function kebab(s) {
  return String(s).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
}

function parseArgs(argv) {
  const a = {
    manifest: "tmp/asset-review/review-manifest.json",
    ids: "", library: DEFAULT_ASSET_SOURCE_ROOT, repo: process.cwd(),
    source: "", license: "", licenseUrl: "", origin: "sourced", kind: "",
    pack: "", packTitle: "", packUrl: "", packGenre: "", packStyle: "", packTags: "", packDesc: "", packCover: "",
    sourcePageUrl: "", authorVendor: "",
    licenseKind: "", attributionRequired: "", noticeRequired: "", creditText: "",
    commercialUse: "", modificationAllowed: "", redistributionAllowed: "", publish: "",
    shippingDecision: "allowed",
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
    else if (arg === "--source-page-url") a.sourcePageUrl = next;
    else if (arg === "--author-vendor") a.authorVendor = next;
    else if (arg === "--license") a.license = next;
    else if (arg === "--license-url") a.licenseUrl = next;
    else if (arg === "--license-kind") a.licenseKind = next;
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
    else if (arg === "--notice-required") a.noticeRequired = next;
    else if (arg === "--credit-text") a.creditText = next;
    else if (arg === "--commercial-use") a.commercialUse = next;
    else if (arg === "--modification-allowed") a.modificationAllowed = next;
    else if (arg === "--redistribution-allowed") a.redistributionAllowed = next;
    else if (arg === "--publish") a.publish = next;
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
  for (const [name, value] of [
    ["attribution-required", a.attributionRequired],
    ["notice-required", a.noticeRequired],
    ["commercial-use", a.commercialUse],
    ["modification-allowed", a.modificationAllowed],
    ["redistribution-allowed", a.redistributionAllowed],
    ["publish", a.publish],
  ]) {
    if (value && !["true", "false"].includes(value)) throw new Error(`--${name} must be true or false`);
  }
  const decisionInput = {
    license: a.license,
    license_url: a.licenseUrl,
    license_kind: a.licenseKind,
    attribution_required: a.attributionRequired,
    notice_required: a.noticeRequired,
    commercial_use: a.commercialUse,
    modification_allowed: a.modificationAllowed,
    redistribution_allowed: a.redistributionAllowed,
    publish: a.publish,
    author_vendor: a.authorVendor || a.source,
    source_page: a.sourcePageUrl,
    credit_text: a.creditText,
  };
  const decision = decideLicense(decisionInput);
  a.licenseKind = a.licenseKind || decision.licenseKind;
  a.licenseUrl = a.licenseUrl || decision.licenseUrl;
  a.attributionRequired = a.attributionRequired || boolText(decision.attributionRequired, "false");
  a.noticeRequired = a.noticeRequired || boolText(decision.noticeRequired, "false");
  a.redistributionAllowed = a.redistributionAllowed || boolText(decision.redistributionAllowed, "false");
  a.commercialUse = a.commercialUse || boolText(decision.commercialUse);
  a.modificationAllowed = a.modificationAllowed || boolText(decision.modificationAllowed);
  a.publish = a.publish || boolText(decision.publishable, "false");
  if (!a.creditText && a.attributionRequired === "true" && a.authorVendor && a.sourcePageUrl) {
    a.creditText = `${a.authorVendor} - ${a.sourcePageUrl}`;
  }
  const validation = validateLicenseRecord({
    ...decisionInput,
    license_kind: a.licenseKind,
    license_url: a.licenseUrl,
    attribution_required: a.attributionRequired,
    notice_required: a.noticeRequired,
    commercial_use: a.commercialUse,
    modification_allowed: a.modificationAllowed,
    redistribution_allowed: a.redistributionAllowed,
    publish: a.publish,
    credit_text: a.creditText,
  });
  if (!validation.ok) throw new Error(`invalid license decision: ${validation.issues.join("; ")}`);
  return a;
}

async function sha256(file) {
  return createHash("sha256").update(await readFile(file)).digest("hex");
}

function licenseMarkdown(rec) {
  return `# License: ${rec.license}

- ${rec.pack ? "Pack" : "Asset id"}: ${rec.pack || rec.assetId}
- Origin: ${rec.origin}
- License URL: ${rec.licenseUrl}
- License kind: ${rec.licenseKind}
- Source/vendor: ${rec.source}
- Author/vendor: ${rec.authorVendor || rec.source}
- Attribution required: ${rec.attributionRequired}
- Notice required: ${rec.noticeRequired}
- Credit text: ${rec.creditText || "-"}
- Commercial use: ${rec.commercialUse}
- Modification allowed: ${rec.modificationAllowed}
- Redistribution allowed: ${rec.redistributionAllowed}
- Shipping decision: ${rec.shippingDecision}
- Promoted from: ${rec.relpath}
- Source page: ${rec.sourcePage || "-"}
`;
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
  const packSlug = kebab(a.pack || a.source || "promoted-assets");

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
    plan.push({ pick: p, source, kind, assetId });
  }

  // H3: reject duplicate asset_ids within the batch before any write.
  const seen = new Map();
  for (const it of plan) {
    if (seen.has(it.assetId)) throw new Error(`duplicate asset_id ${it.assetId}: ${seen.get(it.assetId)} vs ${it.pick.relpath}`);
    seen.set(it.assetId, it.pick.relpath);
  }

  // H2: refuse to clobber existing curated records unless --overwrite.
  for (const it of plan) {
    const packRootName = a.publish === "true" ? "packs" : "restricted/packs";
    it.packDir = join(library, packRootName, packSlug);
    it.filesDir = join(it.packDir, "files", it.assetId);
    it.assetsPath = join(it.packDir, "assets.jsonl");
    it.licenseDir = join(it.packDir, "licenses");
    it.exists = existsSync(join(it.filesDir, basename(it.pick.relpath)));
  }
  const collisions = plan.filter((it) => it.exists);
  if (collisions.length && !a.overwrite) {
    throw new Error(`already in library (pass --overwrite to replace): ${collisions.map((c) => c.assetId).join(", ")}`);
  }

  console.log(`promote ${plan.length} asset(s) -> ${library} [pack ${packSlug}]  [${a.apply ? "APPLY" : "DRY-RUN"}]`);
  const written = [];
  const packRootName = a.publish === "true" ? "packs" : "restricted/packs";
  const packDir = join(library, packRootName, packSlug);
  const assetsPath = join(packDir, "assets.jsonl");
  const rows = a.apply ? await readJsonl(assetsPath) : [];
  for (const it of plan) {
    const { pick, source, kind, assetId, filesDir, licenseDir } = it;
    const abs = resolve(a.repo, pick.relpath);
    if (!existsSync(abs)) throw new Error(`source file missing: ${abs}`);
    console.log(`  ${assetId}  (${kind})${it.exists ? " [OVERWRITE]" : ""}  <- ${pick.relpath}`);
    if (!a.apply) continue;
    const bytes = (await readFile(abs)).length;
    const sha = await sha256(abs);
    await mkdir(filesDir, { recursive: true });
    await mkdir(licenseDir, { recursive: true });
    const targetFile = join(filesDir, basename(abs));
    await cp(abs, targetFile, { force: a.overwrite });
    const rec = {
      assetId, title: pick.name, description: `${kind} promoted from ${source}`,
      resource: `files/${assetId}/${basename(abs)}`, tags: [kind, source, licenseSlug, packSlug].filter(Boolean),
      timestamp: ts, kind, origin: a.origin, license: a.license, licenseUrl: a.licenseUrl,
      source, sourcePage: a.sourcePageUrl, authorVendor: a.authorVendor || source,
      relpath: pick.relpath, sha, bytes, pack: a.pack || "", licenseRef: packSlug || assetId,
      licenseKind: a.licenseKind, attributionRequired: a.attributionRequired,
      noticeRequired: a.noticeRequired, creditText: a.creditText,
      commercialUse: a.commercialUse,
      modificationAllowed: a.modificationAllowed, redistributionAllowed: a.redistributionAllowed,
      publish: a.publish,
      shippingDecision: a.shippingDecision,
    };
    const assetRow = {
      asset_id: assetId,
      title: rec.title,
      description: rec.description,
      kind,
      resource: rec.resource,
      model: [".glb", ".gltf", ".obj", ".fbx"].includes(extname(abs).toLowerCase()) ? rec.resource : "",
      tags: rec.tags,
      origin: rec.origin,
      license: rec.license,
      license_url: rec.licenseUrl,
      license_kind: rec.licenseKind,
      attribution_required: rec.attributionRequired,
      notice_required: rec.noticeRequired,
      credit_text: rec.creditText,
      commercial_use: rec.commercialUse,
      modification_allowed: rec.modificationAllowed,
      redistribution_allowed: rec.redistributionAllowed,
      publish: rec.publish,
      source_page: rec.sourcePage,
      author_vendor: rec.authorVendor,
      sha256: sha,
      bytes,
    };
    const existingRow = rows.findIndex((row) => row.asset_id === assetId);
    if (existingRow >= 0 && !a.overwrite) throw new Error(`asset row exists; pass --overwrite: ${assetId}`);
    if (existingRow >= 0) rows[existingRow] = assetRow;
    else rows.push(assetRow);
    await writeJsonl(assetsPath, rows);
    const packJson = await readJson(join(packDir, "pack.json"), {
      pack: packSlug,
      title: a.packTitle || a.pack || source,
      source,
      kind,
      origin: a.origin,
      license: a.license,
      license_url: a.licenseUrl,
      license_kind: a.licenseKind,
      attribution_required: a.attributionRequired,
      notice_required: a.noticeRequired,
      source_page: a.packUrl || a.sourcePageUrl,
      author_vendor: a.authorVendor || source,
      genre: list(a.packGenre),
      style: list(a.packStyle),
      tags: list(a.packTags),
      description: a.packDesc,
      cover: a.packCover,
    });
    packJson.count = undefined;
    await writeFile(join(packDir, "pack.json"), `${JSON.stringify(packJson, null, 2)}\n`, "utf8");
    await writeFile(join(licenseDir, `${assetId}.md`), licenseMarkdown(rec), "utf8");
    if (pick.licenseFile) {
      const licAbs = resolve(a.repo, pick.licenseFile);
      if (existsSync(licAbs)) await cp(licAbs, join(licenseDir, `${assetId}-${basename(licAbs)}`), { force: true });
    }
    await appendFile(join(library, "intake-log.md"), `- ${ts} promoted ${assetId} (${originOrKind(rec)}) [pack ${packSlug}] from ${pick.relpath}\n`, "utf8");
    written.push({ assetId, pack: packDir, resource: rec.resource });
  }
  console.log(a.apply ? `\nwrote ${written.length} manifest record(s).` : `\ndry-run only - pass --apply to write.`);
}

function originOrKind(rec) {
  return `${rec.origin}/${rec.kind}`;
}

if (isMain(import.meta.url)) {
  main().catch((e) => { console.error(e.message); process.exit(1); });
}

export { parseArgs, kebab };
