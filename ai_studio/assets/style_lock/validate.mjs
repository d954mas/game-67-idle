#!/usr/bin/env node

import { existsSync, readFileSync, realpathSync, statSync } from "node:fs";
import { dirname, isAbsolute, relative, resolve, sep } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const DEFAULT_WORKSPACE_ROOT = resolve(fileURLToPath(new URL("../../../", import.meta.url)));

const SLUG = /^[a-z][a-z0-9-]*$/;
const COLOR = /^#[0-9A-F]{6}$/;
const CANVAS_GROUP = /^canvas:\/\/(?:(?:game\/[a-z][a-z0-9-]*\/)?[A-Za-z0-9_-]+)\/group\/[A-Za-z0-9_-]+$/;
const CANVAS_ELEMENT = /^canvas:\/\/(?:(?:game\/[a-z][a-z0-9-]*\/)?[A-Za-z0-9_-]+)\/element\/[A-Za-z0-9_-]+$/;
const ART_CONTRACT = /^design\/(?:[A-Za-z0-9_-]+\/)*art_contract\.json$/;
const TOP_LEVEL_KEYS = [
  "schema",
  "id",
  "game_id",
  "status",
  "canvas_ref",
  "art_contract_ref",
  "prompt_preamble",
  "negative_prompt",
  "palette",
  "bg_rule",
  "exemplar_refs",
  "asset_size",
  "model_checkpoint",
];

function object(value, label) {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value;
}

function exactKeys(value, keys, label) {
  object(value, label);
  const actual = Object.keys(value).sort();
  const expected = [...keys].sort();
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    const extras = actual.filter((key) => !expected.includes(key));
    const missing = expected.filter((key) => !actual.includes(key));
    throw new Error(`${label} has unexpected fields or missing required fields (unexpected: ${extras.join(", ") || "none"}; missing: ${missing.join(", ") || "none"})`);
  }
}

function nonEmptyString(value, label) {
  if (typeof value !== "string" || !value.trim()) throw new Error(`${label} must be a non-empty string`);
}

function boundedInteger(value, label, minimum = 1, maximum = 4096) {
  if (!Number.isInteger(value) || value < minimum || value > maximum) throw new Error(`${label} must be an integer in ${minimum}..${maximum}`);
}

function canvasScope(ref) {
  return String(ref).replace(/\/(?:group|element)\/[A-Za-z0-9_-]+$/, "");
}

function isInside(root, candidate) {
  const fromRoot = relative(root, candidate);
  return fromRoot === "" || (!isAbsolute(fromRoot) && fromRoot !== ".." && !fromRoot.startsWith(`..${sep}`));
}

export function validateStyleLock(value) {
  exactKeys(value, TOP_LEVEL_KEYS, "style lock");
  if (value.schema !== "ai_studio.game.style_lock.v1") throw new Error("style lock schema must be ai_studio.game.style_lock.v1");
  if (!SLUG.test(value.id || "")) throw new Error("style lock id must be a lowercase slug");
  if (!SLUG.test(value.game_id || "")) throw new Error("style lock game_id must be a lowercase slug");
  if (!["draft", "accepted"].includes(value.status)) throw new Error("style lock status must be draft or accepted");
  if (!CANVAS_GROUP.test(value.canvas_ref || "")) throw new Error("style lock canvas_ref must identify a Canvas group");
  const privateGame = /^canvas:\/\/game\/([^/]+)\//.exec(value.canvas_ref)?.[1];
  if (privateGame && privateGame !== value.game_id) throw new Error("style lock private canvas_ref game id must match game_id");
  if (!ART_CONTRACT.test(value.art_contract_ref || "") || value.art_contract_ref.includes("..")) {
    throw new Error("style lock art_contract_ref must be a confined design/.../art_contract.json path");
  }
  nonEmptyString(value.prompt_preamble, "style lock prompt_preamble");
  nonEmptyString(value.negative_prompt, "style lock negative_prompt");

  if (!Array.isArray(value.palette) || value.palette.length < 2 || value.palette.length > 12
      || value.palette.some((entry) => !COLOR.test(entry))) {
    throw new Error("style lock palette must contain 2..12 canonical uppercase #RRGGBB colors");
  }
  if (new Set(value.palette.map((entry) => entry.toLowerCase())).size !== value.palette.length) {
    throw new Error("style lock palette colors must be unique");
  }

  exactKeys(value.bg_rule, ["mode", "key_color", "description"], "style lock bg_rule");
  if (!["chroma", "transparent"].includes(value.bg_rule.mode)) throw new Error("style lock bg_rule mode must be chroma or transparent");
  nonEmptyString(value.bg_rule.description, "style lock bg_rule description");
  if (value.bg_rule.mode === "chroma" && !["#FF00FF", "#00FF00"].includes(value.bg_rule.key_color)) {
    throw new Error("style lock chroma bg_rule key_color must be magenta or green");
  }
  if (value.bg_rule.mode === "transparent" && value.bg_rule.key_color !== null) {
    throw new Error("style lock transparent bg_rule key_color must be null");
  }

  if (!Array.isArray(value.exemplar_refs) || value.exemplar_refs.length < 2 || value.exemplar_refs.length > 3) {
    throw new Error("style lock requires 2-3 owned exemplar_refs");
  }
  const domains = new Set();
  const exemplarRefs = new Set();
  for (const [index, exemplar] of value.exemplar_refs.entries()) {
    exactKeys(exemplar, ["ref", "origin", "domain"], `style lock exemplar_refs[${index}]`);
    if (!CANVAS_ELEMENT.test(exemplar.ref || "")) throw new Error(`style lock exemplar_refs[${index}].ref must identify a Canvas element`);
    if (canvasScope(exemplar.ref) !== canvasScope(value.canvas_ref)) throw new Error("style lock exemplar_refs must use the same Canvas store/project as canvas_ref");
    if (exemplar.origin !== "owned") throw new Error(`style lock exemplar_refs[${index}].origin must be owned`);
    if (!["world", "gui"].includes(exemplar.domain)) throw new Error(`style lock exemplar_refs[${index}].domain must be world or gui`);
    exemplarRefs.add(exemplar.ref);
    domains.add(exemplar.domain);
  }
  if (exemplarRefs.size !== value.exemplar_refs.length) throw new Error("style lock exemplar_refs must identify unique Canvas elements");
  if (!domains.has("world") || !domains.has("gui")) throw new Error("style lock exemplar_refs must cover both world and gui domains");

  exactKeys(value.asset_size, ["width", "height"], "style lock asset_size");
  boundedInteger(value.asset_size.width, "style lock asset_size.width", 64);
  boundedInteger(value.asset_size.height, "style lock asset_size.height", 64);

  if (value.model_checkpoint !== null) throw new Error("style lock model_checkpoint is parked and must remain null in v1");
  return value;
}

export function validateStyleLockFile(filePath, options = {}) {
  const workspaceRoot = realpathSync(resolve(options.workspaceRoot || DEFAULT_WORKSPACE_ROOT));
  const requestedPath = resolve(filePath);
  if (!existsSync(requestedPath)) throw new Error("style lock file does not exist");
  const path = realpathSync(requestedPath);
  if (!isInside(workspaceRoot, path)) throw new Error("style lock file must remain inside the workspace");

  const workspacePath = relative(workspaceRoot, path).replaceAll("\\", "/");
  const privateOwnership = /^games\/private\/([a-z][a-z0-9-]*)\/design\/style_lock\.json$/.exec(workspacePath);
  const publicOwnership = /^games\/([a-z][a-z0-9-]*)\/design\/style_lock\.json$/.exec(workspacePath);
  const ownership = privateOwnership || publicOwnership;
  if (!ownership) {
    throw new Error("style lock file must use a public or private workspace games path: games/<id>/design/style_lock.json or games/private/<id>/design/style_lock.json");
  }

  const value = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
  validateStyleLock(value);
  if (ownership[1] !== value.game_id) throw new Error("style lock path game id must match game_id");
  const usesPrivateCanvas = value.canvas_ref.startsWith("canvas://game/");
  if (privateOwnership && !usesPrivateCanvas) throw new Error("private game lock must use private Canvas refs");
  if (!privateOwnership && usesPrivateCanvas) throw new Error("public game lock must use public Canvas refs");

  const gameRoot = dirname(dirname(path));
  const designRoot = realpathSync(resolve(gameRoot, "design"));
  const requestedArtContract = resolve(gameRoot, value.art_contract_ref);
  if (!existsSync(requestedArtContract)) throw new Error("style lock art_contract_ref does not exist");
  const artContractPath = realpathSync(requestedArtContract);
  if (!statSync(artContractPath).isFile()) throw new Error("style lock art_contract_ref must identify a regular file");
  if (!isInside(designRoot, artContractPath)) {
    throw new Error("style lock art_contract_ref must remain inside the physical game design directory");
  }
  return value;
}

export function main(argv = process.argv.slice(2)) {
  if (argv.length !== 1) {
    console.error("usage: node ai_studio/assets/style_lock/validate.mjs <games/<id>/design/style_lock.json>");
    return 2;
  }
  try {
    const path = resolve(argv[0]);
    validateStyleLockFile(path);
    console.log(`style lock valid: ${path}`);
    return 0;
  } catch (error) {
    console.error(error?.message || String(error));
    return 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  process.exitCode = main();
}
