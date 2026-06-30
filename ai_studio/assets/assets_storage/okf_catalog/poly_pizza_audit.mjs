#!/usr/bin/env node
import { readFile, readdir, writeFile, mkdir } from "node:fs/promises";
import { basename, dirname, join, relative, resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { DEFAULT_LIBRARY, parseFrontmatter } from "./find_assets.mjs";

function list(value) {
  return Array.isArray(value) ? value.filter(Boolean) : value ? [value] : [];
}

function uniqueList(...values) {
  return [...new Set(values.flatMap(list).map(String).filter(Boolean))];
}

async function walk(dir) {
  const out = [];
  let entries;
  try {
    entries = await readdir(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const entry of entries) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) out.push(...(await walk(path)));
    else out.push(path);
  }
  return out;
}

function bodyWithoutFrontmatter(text) {
  const match = String(text).match(/^п»ї?---\r?\n[\s\S]*?\r?\n---\r?\n([\s\S]*)$/);
  return match ? match[1].trim() : String(text || "");
}

function sourcePage(text, fm) {
  for (const key of ["source_page", "source_url", "url", "origin_url"]) {
    if (fm[key] && /poly\.pizza/i.test(fm[key])) return fm[key];
  }
  return text.match(/Source page:\s*(https?:\/\/poly\.pizza\/[^\s)]+)/i)?.[1]
    || text.match(/https?:\/\/poly\.pizza\/[^\s)]+/i)?.[0]
    || "";
}

function memberships(fm) {
  return uniqueList(fm.pack, fm.packs, fm.member_of, fm.bundles);
}

function duplicateRows(rows, keyFn) {
  const grouped = new Map();
  for (const row of rows) {
    const key = keyFn(row);
    if (!key) continue;
    if (!grouped.has(key)) grouped.set(key, []);
    grouped.get(key).push(row);
  }
  return [...grouped.entries()]
    .filter(([, list]) => list.length > 1)
    .map(([key, list]) => ({
      key,
      count: list.length,
      rows: list.map((row) => ({
        asset_id: row.asset_id,
        title: row.title,
        pack: row.pack,
        packs: row.packs,
        catalog_path: row.catalog_path,
      })),
    }));
}

export async function auditPolyPizzaCatalog(libraryPath = DEFAULT_LIBRARY) {
  const catalogDir = join(libraryPath, "catalog");
  const files = (await walk(catalogDir)).filter((file) => file.endsWith(".md"));
  const assets = [];
  const packs = [];

  for (const file of files) {
    const text = await readFile(file, "utf8").catch(() => "");
    if (!/poly\.pizza/i.test(text)) continue;
    const fm = parseFrontmatter(text);
    const rel = relative(libraryPath, file).replace(/\\/g, "/");
    const page = sourcePage(text, fm);
    if (/[\\/]_pack\.md$/i.test(file)) {
      packs.push({
        pack: fm.pack || basename(dirname(file)),
        title: fm.title || fm.pack || basename(dirname(file)),
        declared_count: Number(fm.count) || 0,
        source: fm.source || "",
        license: fm.license || "",
        source_page: page,
        is_bundle: /\/bundle\//i.test(page),
        catalog_path: rel,
        body: bodyWithoutFrontmatter(text),
      });
    } else if (fm.asset_id || fm.title) {
      assets.push({
        asset_id: fm.asset_id || "",
        title: fm.title || fm.asset_id || basename(file, ".md"),
        pack: fm.pack || "",
        packs: memberships(fm),
        source_page: page,
        license: fm.license || "",
        origin: fm.origin || "",
        author: fm.author || "",
        catalog_path: rel,
      });
    }
  }

  const membershipCounts = new Map();
  const primaryCounts = new Map();
  for (const asset of assets) {
    if (asset.pack) primaryCounts.set(asset.pack, (primaryCounts.get(asset.pack) || 0) + 1);
    for (const pack of asset.packs) membershipCounts.set(pack, (membershipCounts.get(pack) || 0) + 1);
  }

  const packRows = packs
    .map((pack) => ({
      ...pack,
      primary_count: primaryCounts.get(pack.pack) || 0,
      membership_count: membershipCounts.get(pack.pack) || 0,
      declared_mismatch: Boolean(pack.declared_count && pack.declared_count !== (membershipCounts.get(pack.pack) || 0)),
    }))
    .sort((a, b) => a.pack.localeCompare(b.pack));

  const multiPackAssets = assets
    .filter((asset) => asset.packs.length > 1)
    .map((asset) => ({
      asset_id: asset.asset_id,
      title: asset.title,
      primary_pack: asset.pack,
      packs: asset.packs,
      source_page: asset.source_page,
      catalog_path: asset.catalog_path,
    }))
    .sort((a, b) => a.asset_id.localeCompare(b.asset_id));

  const report = {
    library: libraryPath,
    generated_at: new Date().toISOString(),
    summary: {
      poly_pizza_assets: assets.length,
      poly_pizza_pack_docs: packs.length,
      bundle_pack_docs: packs.filter((pack) => pack.is_bundle).length,
      declared_count_mismatches: packRows.filter((pack) => pack.declared_mismatch).length,
      multi_pack_assets: multiPackAssets.length,
      duplicate_asset_ids: duplicateRows(assets, (asset) => asset.asset_id).length,
      duplicate_source_pages: duplicateRows(assets, (asset) => asset.source_page).length,
      assets_without_source_page: assets.filter((asset) => !asset.source_page).length,
      assets_without_license: assets.filter((asset) => !asset.license).length,
    },
    pack_count_mismatches: packRows.filter((pack) => pack.declared_mismatch),
    bundle_packs: packRows.filter((pack) => pack.is_bundle),
    multi_pack_assets: multiPackAssets,
    duplicate_asset_ids: duplicateRows(assets, (asset) => asset.asset_id),
    duplicate_source_pages: duplicateRows(assets, (asset) => asset.source_page),
    assets_without_source_page: assets.filter((asset) => !asset.source_page),
    assets_without_license: assets.filter((asset) => !asset.license),
  };
  return report;
}

function parseArgs(argv) {
  const args = { library: DEFAULT_LIBRARY, json: false, out: "" };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--json") {
      args.json = true;
      continue;
    }
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--library") args.library = next;
    else if (arg === "--out") args.out = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  return args;
}

function printText(report) {
  console.log(`Poly Pizza assets: ${report.summary.poly_pizza_assets}`);
  console.log(`Pack docs: ${report.summary.poly_pizza_pack_docs} (${report.summary.bundle_pack_docs} bundles)`);
  console.log(`Declared count mismatches: ${report.summary.declared_count_mismatches}`);
  console.log(`Multi-pack assets: ${report.summary.multi_pack_assets}`);
  console.log(`Duplicate asset ids: ${report.summary.duplicate_asset_ids}`);
  console.log(`Duplicate source pages: ${report.summary.duplicate_source_pages}`);
  if (report.pack_count_mismatches.length) {
    console.log("\nCount mismatches:");
    for (const pack of report.pack_count_mismatches.slice(0, 40)) {
      console.log(`  ${pack.pack}: declared=${pack.declared_count} membership=${pack.membership_count} primary=${pack.primary_count}`);
    }
  }
  if (report.multi_pack_assets.length) {
    console.log("\nMulti-pack assets:");
    for (const asset of report.multi_pack_assets.slice(0, 40)) {
      console.log(`  ${asset.asset_id}: ${asset.packs.join(", ")}`);
    }
  }
}

export async function main(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  const report = await auditPolyPizzaCatalog(args.library);
  if (args.out) {
    await mkdir(dirname(resolve(args.out)), { recursive: true });
    await writeFile(args.out, JSON.stringify(report, null, 2), "utf8");
  }
  if (args.json) console.log(JSON.stringify(report, null, 2));
  else printText(report);
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((error) => {
    console.error(error.message);
    process.exit(1);
  });
}
