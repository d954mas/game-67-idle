#!/usr/bin/env node
import { appendFile, cp, mkdir, readdir, readFile, rename, stat, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { basename, dirname, extname, join, relative, resolve, sep } from "node:path";
import { boolText, decideLicense, validateLicenseRecord } from "../licenses/ops.mjs";
import { isMain } from "../../core_harness/tool_lib/cli.mjs";
import { sha256File } from "../../core_harness/tool_lib/hash.mjs";
import { safeSegment } from "./stage.mjs";
import { defaultLibrarySourceRoot } from "../sources/ops.mjs";

// A bought pack ships each model in several formats (GLB and FBX of the
// same mesh); one record per model keeps search free of duplicates.
const MODEL_FORMATS = [".glb", ".gltf", ".fbx", ".obj"];
const TEXTURE_FORMATS = new Set([".png", ".jpg", ".jpeg", ".tga", ".webp"]);

function usage() {
  return `usage: node ai_studio/assets/intake/accept_pack.mjs --folder <extracted-pack> --source <source> --pack <pack-id> --license <license> [options]

Accepts a whole extracted pack in one pass: one record per model (the first
available format of --prefer), one per texture, the folder structure kept
under files/.

Options:
  --source-root <path>              Asset source root. Defaults to the shared library.
  --title <title>                   Pack title. Defaults to the pack id.
  --description <text>              Pack description.
  --tags <a,b,c>                    Tags for the pack and every record.
  --prefer <.glb,.gltf,.fbx,.obj>   Model format priority.
  --move                            Move files into the library instead of copying.
  --origin <mine|ai|sourced>        Default sourced.
  --source-page-url <url>
  --author-vendor <name>
  --license-url <url>
  --license-kind <kind>
  --attribution-required <bool>
  --notice-required <bool>
  --credit-text <text>
  --commercial-use <bool>
  --modification-allowed <bool>
  --redistribution-allowed <bool>
  --publish <true|false>
  --dry-run                         Report what would be accepted; write nothing.`;
}

const BOOL_FLAGS = {
  "--attribution-required": "attributionRequired",
  "--notice-required": "noticeRequired",
  "--commercial-use": "commercialUse",
  "--modification-allowed": "modificationAllowed",
  "--redistribution-allowed": "redistributionAllowed",
  "--publish": "publish",
};
const VALUE_FLAGS = {
  "--source-root": "sourceRoot",
  "--folder": "folder",
  "--source": "source",
  "--pack": "pack",
  "--license": "license",
  "--title": "title",
  "--description": "description",
  "--tags": "tags",
  "--prefer": "prefer",
  "--origin": "origin",
  "--source-page-url": "sourcePageUrl",
  "--author-vendor": "authorVendor",
  "--license-url": "licenseUrl",
  "--license-kind": "licenseKind",
  "--credit-text": "creditText",
  ...BOOL_FLAGS,
};

function parseArgs(argv) {
  const args = { sourceRoot: defaultLibrarySourceRoot(process.cwd()), origin: "sourced", move: false, dryRun: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") throw new Error(usage());
    if (arg === "--move") { args.move = true; continue; }
    if (arg === "--dry-run") { args.dryRun = true; continue; }
    const key = VALUE_FLAGS[arg];
    if (!key) throw new Error(`unknown argument: ${arg}\n${usage()}`);
    const next = argv[index + 1];
    if (next === undefined) throw new Error(`missing value for ${arg}`);
    args[key] = next;
    index += 1;
  }
  for (const required of ["folder", "source", "pack", "license"]) {
    if (!args[required]) throw new Error(`missing required --${required}\n${usage()}`);
  }
  if (!["mine", "ai", "sourced"].includes(args.origin)) throw new Error("--origin must be mine|ai|sourced");
  for (const [flag, key] of Object.entries(BOOL_FLAGS)) {
    if (args[key] && !["true", "false"].includes(args[key])) throw new Error(`${flag} must be true or false`);
  }
  args.prefer = args.prefer ? list(args.prefer).map((ext) => (ext.startsWith(".") ? ext : `.${ext}`).toLowerCase()) : MODEL_FORMATS;
  return args;
}

function list(value) {
  return String(value || "").split(",").map((item) => item.trim()).filter(Boolean);
}

function posix(path) {
  return path.split(sep).join("/");
}

async function walk(dir) {
  const files = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) files.push(...await walk(path));
    else if (entry.isFile()) files.push(path);
  }
  return files;
}

/* Picks, per model name, every file of the highest-priority format that
   name exists in; textures are all kept. */
export function selectPackFiles(relativePaths, prefer = MODEL_FORMATS) {
  const byStem = new Map();
  const textures = [];
  for (const path of relativePaths) {
    const ext = extname(path).toLowerCase();
    if (TEXTURE_FORMATS.has(ext)) { textures.push(path); continue; }
    if (!prefer.includes(ext)) continue;
    const stem = basename(path, extname(path)).toLowerCase();
    if (!byStem.has(stem)) byStem.set(stem, []);
    byStem.get(stem).push(path);
  }
  const models = [];
  for (const paths of byStem.values()) {
    const best = prefer.find((ext) => paths.some((path) => extname(path).toLowerCase() === ext));
    models.push(...paths.filter((path) => extname(path).toLowerCase() === best));
  }
  return { models: models.sort(), textures: textures.sort() };
}

/* A .gltf keeps its geometry and maybe its images in sibling files; they
   travel with it, or the model loads empty. */
async function gltfCompanions(folder, relativePath) {
  const json = JSON.parse(await readFile(join(folder, ...relativePath.split("/")), "utf8"));
  const uris = [...(json.buffers || []), ...(json.images || [])].map((item) => item.uri)
    .filter((uri) => uri && !uri.startsWith("data:") && !/^[a-z]+:/i.test(uri));
  return uris.map((uri) => posix(join(dirname(relativePath), decodeURIComponent(uri))));
}

function assetIdFor(packId, relativePath) {
  const withoutExt = relativePath.slice(0, relativePath.length - extname(relativePath).length);
  return `${packId}__${safeSegment(withoutExt.replaceAll("/", "__"), "asset-id")}`;
}

function pathTags(relativePath) {
  return posix(dirname(relativePath)).split("/").filter((part) => part && part !== ".")
    .map((part) => safeSegment(part, "tag")).filter((tag) => tag.length > 1);
}

export async function acceptPack(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  const sourceRoot = resolve(args.sourceRoot);
  const folder = resolve(args.folder);
  if (!existsSync(folder) || !(await stat(folder)).isDirectory()) throw new Error(`--folder is not a directory: ${folder}`);
  const source = safeSegment(args.source, "source");
  const packId = safeSegment(args.pack, "pack");

  const decisionInput = {
    license: args.license,
    license_url: args.licenseUrl,
    license_kind: args.licenseKind,
    attribution_required: args.attributionRequired,
    notice_required: args.noticeRequired,
    commercial_use: args.commercialUse,
    modification_allowed: args.modificationAllowed,
    redistribution_allowed: args.redistributionAllowed,
    publish: args.publish || "",
    author_vendor: args.authorVendor || source,
    source_page: args.sourcePageUrl || "",
    credit_text: args.creditText,
  };
  const decision = decideLicense(decisionInput);
  const publish = args.publish || boolText(decision.publishable, "false");
  const licenseKind = args.licenseKind || decision.licenseKind;
  const attributionRequired = args.attributionRequired || boolText(decision.attributionRequired, "false");
  const noticeRequired = args.noticeRequired || boolText(decision.noticeRequired, "false");
  const authorVendor = args.authorVendor || source;
  const sourcePage = args.sourcePageUrl || "";
  const creditText = args.creditText || (attributionRequired === "true" && sourcePage ? `${authorVendor} - ${sourcePage}` : "");
  const rights = {
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
  };
  const validation = validateLicenseRecord({ ...decisionInput, ...rights });
  if (!validation.ok) throw new Error(`invalid license decision: ${validation.issues.join("; ")}`);

  const relativePaths = (await walk(folder)).map((path) => posix(relative(folder, path)));
  const { models, textures } = selectPackFiles(relativePaths, args.prefer);
  const present = new Set(relativePaths);
  const textureSet = new Set(textures);
  const companions = new Set();
  for (const model of models.filter((path) => extname(path).toLowerCase() === ".gltf")) {
    for (const companion of await gltfCompanions(folder, model)) {
      if (!present.has(companion)) throw new Error(`${model} references a missing file: ${companion}`);
      if (!textureSet.has(companion)) companions.add(companion);
    }
  }
  const packRootName = publish === "true" ? "packs" : join("restricted", "packs");
  const packDir = join(sourceRoot, packRootName, packId);
  const summary = { pack: packId, publish, pack_dir: packDir, models: models.length, textures: textures.length, companions: companions.size, skipped: relativePaths.length - models.length - textures.length - companions.size };
  if (args.dryRun) return { ...summary, dry_run: true, sample: models.slice(0, 5) };
  if (existsSync(join(packDir, "assets.jsonl"))) throw new Error(`pack already has records; remove it or pick another --pack: ${packDir}`);

  const place = async (relativePath) => {
    const target = join(packDir, "files", ...relativePath.split("/"));
    await mkdir(dirname(target), { recursive: true });
    const from = join(folder, ...relativePath.split("/"));
    if (args.move) await rename(from, target);
    else await cp(from, target);
    return target;
  };
  for (const companion of companions) await place(companion);
  const packTags = list(args.tags);
  const rows = [];
  // Packs ship the same texture as .png and .webp; the extension tells them apart.
  const taken = new Set();
  const uniqueId = (id, relativePath) => {
    let unique = id;
    if (taken.has(unique)) unique = `${id}-${extname(relativePath).slice(1).toLowerCase()}`;
    for (let n = 2; taken.has(unique); n += 1) unique = `${id}-${n}`;
    taken.add(unique);
    return unique;
  };
  for (const [kind, paths] of [["model", models], ["texture", textures]]) {
    for (const relativePath of paths) {
      const target = await place(relativePath);
      const resource = `files/${relativePath}`;
      const row = {
        asset_id: uniqueId(assetIdFor(packId, relativePath), relativePath),
        title: basename(relativePath, extname(relativePath)).replace(/[_-]+/g, " "),
        description: "",
        kind,
        resource,
        tags: [...new Set([...packTags, ...pathTags(relativePath)])],
        origin: args.origin,
        ...rights,
        source_page: sourcePage,
        author_vendor: authorVendor,
        sha256: await sha256File(target),
        bytes: (await stat(target)).size,
      };
      if ([".glb", ".gltf"].includes(extname(relativePath).toLowerCase())) row.model = resource;
      rows.push(row);
    }
  }

  await mkdir(join(packDir, "licenses"), { recursive: true });
  const pack = {
    pack: packId,
    title: args.title || packId,
    source,
    kind: "model",
    origin: args.origin,
    ...rights,
    source_page: sourcePage,
    author_vendor: authorVendor,
    tags: packTags,
    description: args.description || "",
  };
  await writeFile(join(packDir, "pack.json"), `${JSON.stringify(pack, null, 2)}\n`, "utf8");
  await writeFile(join(packDir, "assets.jsonl"), rows.map((row) => JSON.stringify(row)).join("\n") + (rows.length ? "\n" : ""), "utf8");
  await writeFile(join(packDir, "licenses", "pack.md"), `# License: ${pack.license}

- Pack: ${packId}
- License URL: ${pack.license_url || "-"}
- License kind: ${pack.license_kind}
- Commercial use: ${pack.commercial_use}
- Modification allowed: ${pack.modification_allowed}
- Redistribution allowed: ${pack.redistribution_allowed}
- Publishable: ${pack.publish}
- Source page: ${pack.source_page || "-"}
- Author/vendor: ${pack.author_vendor || "-"}
`, "utf8");
  await appendFile(join(sourceRoot, "intake-log.md"), `- ${new Date().toISOString()} accepted pack ${packId}: ${models.length} models, ${textures.length} textures from ${folder} -> ${packRootName}/${packId}\n`, "utf8");
  return summary;
}

if (isMain(import.meta.url)) {
  try {
    console.log(JSON.stringify(await acceptPack(), null, 2));
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
