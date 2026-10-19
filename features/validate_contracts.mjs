#!/usr/bin/env node
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const SEMVER = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/;
const ROUTER_SECTIONS = ["Purpose", "Public surface", "Validation", "Compatibility", "Extension points"];
const SKILL_ROUTES = {
  ".codex/skills/nt-game-feature/SKILL.md": ["features/README.md", "ai_studio/feature_testing/consumer/src/features/README.md", "README.md", "INSTALL.md", "feature.json"],
  ".codex/skills/nt-game-state-management/SKILL.md": ["features/game-state/README.md", "features/game-state/INSTALL.md", "features/game-state/feature.json"],
  ".codex/skills/nt-game-items/SKILL.md": ["features/items-core/README.md", "features/items-core/INSTALL.md", "features/items-core/feature.json"],
};
const DEFAULT_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

function readJson(root, rel) {
  let value;
  try { value = JSON.parse(readFileSync(join(root, rel), "utf8").replace(/^\uFEFF/, "")); }
  catch (error) { throw new Error(`${rel}: invalid JSON (${error.message})`); }
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error(`${rel}: expected an object`);
  return value;
}

function exactKeys(value, keys, label) {
  const actual = Object.keys(value).sort();
  const expected = [...keys].sort();
  if (JSON.stringify(actual) !== JSON.stringify(expected)) throw new Error(`${label}: unexpected fields`);
}

function requireText(root, rel, label) {
  if (!existsSync(join(root, rel))) throw new Error(`${label}: missing ${rel}`);
  const text = readFileSync(join(root, rel), "utf8");
  if (!text.trim()) throw new Error(`${label}: ${rel} must not be empty`);
  return text;
}

function validateManifest(root, id, base) {
  const label = `${id} feature contract`;
  const manifestRel = `${base}/feature.json`;
  const manifest = readJson(root, manifestRel);
  if (manifest.schema !== "ai_studio.feature.v1") throw new Error(`${label}: expected schema ai_studio.feature.v1`);
  if (manifest.id !== id) throw new Error(`${label}: feature.json id must be '${id}'`);
  if (!SEMVER.test(String(manifest.version || ""))) throw new Error(`${label}: version must be exact SemVer x.y.z`);
  const installRel = manifest.manuals?.install;
  if (installRel !== `${base}/INSTALL.md`) throw new Error(`${label}: manuals.install must own ${base}/INSTALL.md`);
  requireText(root, installRel, label);
  const readme = requireText(root, `${base}/README.md`, label);
  const headings = [...readme.matchAll(/^## (.+)$/gm)];
  for (const section of ROUTER_SECTIONS) {
    const headingIndex = headings.findIndex((match) => match[1] === section);
    if (headingIndex < 0) throw new Error(`${label}: README router is missing '## ${section}'`);
    const heading = headings[headingIndex];
    const next = headings[headingIndex + 1];
    const body = readme.slice(heading.index + heading[0].length, next?.index ?? readme.length).trim();
    if (!body) throw new Error(`${label}: README router section '${section}' must not be empty`);
    if (section === "Compatibility" && !["PATCH", "MINOR", "MAJOR"].every((token) => new RegExp(`\\b${token}\\b`, "i").test(body))) {
      throw new Error(`${label}: README Compatibility must define PATCH, MINOR, and MAJOR rules`);
    }
  }
  // Maturity and default composition are independent: the seed names actual consumers.
  const defaultStatus = base.startsWith("features/") ? "reusable" : "template-local";
  const allowedStatuses = base.startsWith("features/")
    ? new Set(["reusable", "opt-in", "specified"])
    : new Set(["template-local"]);
  const status = manifest.status ?? defaultStatus;
  if (typeof status !== "string" || !allowedStatuses.has(status)) {
    throw new Error(
      `${label}: status must be one of ${[...allowedStatuses].join(", ")}`,
    );
  }
  return {
    id,
    version: manifest.version,
    source: base,
    status,
  };
}

function discoverFeatureDirectories(root, rel, { declaredOnly = false } = {}) {
  const base = join(root, rel);
  if (!existsSync(base)) throw new Error(`${rel}: feature contract root is missing`);
  return readdirSync(base, { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && (!declaredOnly || existsSync(join(base, entry.name, "feature.json"))))
    .map((entry) => entry.name)
    .sort();
}

function validateSeed(root, rootContracts, template) {
  const rel = `${template}/game-dependencies.json`;
  const seed = readJson(root, rel);
  exactKeys(seed, ["schema", "engine", "features", "compatibility"], rel);
  if (typeof seed.compatibility !== "string" || !seed.compatibility.trim()) {
    throw new Error(`${rel}: compatibility must be nonempty text`);
  }
  if (seed.schema !== "ai_studio.game.dependencies.seed.v3") throw new Error(`${rel}: unsupported schema`);
  if (!seed.engine || typeof seed.engine !== "object" || Array.isArray(seed.engine)) throw new Error(`${rel}: invalid engine contract`);
  // No engine version: the gitlink pins the engine, and a game records its revision prefix at creation.
  exactKeys(seed.engine, ["source", "compatibility"], `${rel}.engine`);
  if (seed.engine.source !== "external/neotolis-engine"
      || typeof seed.engine.compatibility !== "string" || !seed.engine.compatibility.trim()) {
    throw new Error(`${rel}: invalid engine contract`);
  }
  if (!Array.isArray(seed.features)) throw new Error(`${rel}: features must be an array`);
  const declared = new Map();
  for (const entry of seed.features) {
    if (!entry || typeof entry !== "object" || Array.isArray(entry)) throw new Error(`${rel}: feature entries must be objects`);
    exactKeys(entry, ["id", "source", "version", "compatibility"], `${rel}.features[]`);
    if (declared.has(entry.id)) throw new Error(`${rel}: duplicate feature '${entry.id}'`);
    declared.set(entry.id, entry);
  }
  const contracts = new Map(rootContracts.map((contract) => [contract.id, contract]));
  for (const entry of declared.values()) {
    const contract = contracts.get(entry.id);
    if (!contract) throw new Error(`${rel}: unknown feature '${entry.id}'`);
    if (contract.status === "specified") throw new Error(`${entry.id}: specified-only feature cannot be a runtime dependency`);
    if (entry.source !== contract.source) throw new Error(`${contract.id}: seed source must be '${contract.source}'`);
    if (entry.version !== contract.version) {
      throw new Error(`${contract.id}: seed version ${entry.version || "<missing>"} does not match feature.json ${contract.version}`);
    }
    if (typeof entry.compatibility !== "string" || !entry.compatibility.trim()) {
      throw new Error(`${contract.id}: seed compatibility must be nonempty text`);
    }
  }
  return seed;
}

function validateSkillRouters(root) {
  for (const [rel, references] of Object.entries(SKILL_ROUTES)) {
    const text = requireText(root, rel, `${rel} router`);
    for (const reference of references) {
      if (!text.includes(reference)) throw new Error(`${rel}: missing owning contract route ${reference}`);
    }
  }
}

export function validateFeatureContracts(root = DEFAULT_ROOT) {
  const repoRoot = resolve(root);
  const rootContracts = discoverFeatureDirectories(repoRoot, "features")
    .map((id) => validateManifest(repoRoot, id, `features/${id}`));
  const templates = readdirSync(join(repoRoot, "templates"), { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && existsSync(join(repoRoot, "templates", entry.name, "game-dependencies.json")))
    .map((entry) => `templates/${entry.name}`);
  const consumer = "ai_studio/feature_testing/consumer";
  if (existsSync(join(repoRoot, consumer, "game-dependencies.json"))) templates.push(consumer);
  const pointerContracts = templates.flatMap((template) => {
    const base = `${template}/src/features`;
    if (!existsSync(join(repoRoot, base))) return [];
    return discoverFeatureDirectories(repoRoot, base, { declaredOnly: true })
      .map((id) => validateManifest(repoRoot, id, `${base}/${id}`));
  });
  for (const template of templates) validateSeed(repoRoot, rootContracts, template);
  validateSkillRouters(repoRoot);
  return {
    rootFeatures: rootContracts.map(({ id }) => id),
    pointerFeatures: pointerContracts.map(({ id }) => id),
  };
}

function parseArgs(argv) {
  let root = DEFAULT_ROOT;
  for (let index = 0; index < argv.length; index += 1) {
    if (argv[index] === "--root" && argv[index + 1]) root = resolve(argv[++index]);
    else if (argv[index] === "--help" || argv[index] === "-h") return { help: true, root };
    else throw new Error(`unknown argument: ${argv[index]}`);
  }
  return { help: false, root };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const args = parseArgs(process.argv.slice(2));
    if (args.help) console.log("usage: node features/validate_contracts.mjs [--root <repo>]");
    else {
      const result = validateFeatureContracts(args.root);
      console.log(`feature contracts ok: ${result.rootFeatures.length} modules, ${result.pointerFeatures.length} pointers`);
    }
  } catch (error) {
    console.error(`feature contracts invalid: ${error.message}`);
    process.exitCode = 1;
  }
}
