#!/usr/bin/env node

import {
  existsSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  realpathSync,
  renameSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { createHash, randomUUID } from "node:crypto";
import { basename, isAbsolute, join, relative, resolve, sep } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

import { createStoreZip, readStoreZip } from "./lib/zip_store.mjs";
import { findStudioRoot } from "./lib/studio_root.mjs";

const GAME_DIR = resolve(fileURLToPath(new URL("..", import.meta.url)));
const DEFAULT_STUDIO_ROOT = findStudioRoot(GAME_DIR);
const { inspectPlatformSdkArtifact } = await import(pathToFileURL(join(
  DEFAULT_STUDIO_ROOT,
  "features",
  "platform-sdk",
  "scripts",
  "artifact_tools.mjs",
)).href);

const TARGETS = new Set(["itch", "poki", "yandex", "playgama"]);
const SOURCE_EXTENSIONS = /\.(?:c|cc|cpp|cxx|h|hh|hpp|cmake|py|ts|map|pdb|obj|o)$/i;
const DEVAPI_MARKERS = ["window.__devapi", "--devapi", "wasm-devapi", "GAME_DEVAPI_ENABLED"];
const REVISION = /^[0-9a-f]{40}$/;
const SEMVER = /^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/;
const WASM_MAGIC = Buffer.from([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
const WASM_FORBIDDEN_MARKERS = ["nt_devapi", "__devapi", "debug_test", "sourceMappingURL", ".debug_"];

const slash = (value) => String(value || "").replaceAll("\\", "/");
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const jsonBytes = (value) => Buffer.from(`${JSON.stringify(value, null, 2)}\n`, "utf8");

function readJson(path, label) {
  let value;
  try { value = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, "")); }
  catch (error) { throw new Error(`${label} is not valid JSON: ${error.message}`); }
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value;
}

function exactKeys(value, keys, label) {
  if (JSON.stringify(Object.keys(value).sort()) !== JSON.stringify([...keys].sort())) throw new Error(`${label} has unexpected fields`);
}

function validateIdentity(identity) {
  exactKeys(identity, ["schema", "id", "title", "storageNamespace"], "game identity");
  if (identity.schema !== "ai_studio.game.v1" || !/^[a-z][a-z0-9-]*$/.test(identity.id)
      || !String(identity.title || "").trim() || !/^[a-z][a-z0-9-]*$/.test(identity.storageNamespace)) {
    throw new Error("game identity is invalid");
  }
  return identity;
}

function validateManifestGame(game, label) {
  exactKeys(game, ["id", "title", "storageNamespace"], label);
  if (!/^[a-z][a-z0-9-]*$/.test(game.id || "") || !String(game.title || "").trim()
      || !/^[a-z][a-z0-9-]*$/.test(game.storageNamespace || "")) throw new Error(`${label} is invalid`);
  return game;
}

function validateDependencyRow(row, label, feature = false) {
  exactKeys(row, feature ? ["id", "source", "version", "revision", "compatibility"] : ["source", "version", "revision", "compatibility"], label);
  if (feature && !/^[a-z][a-z0-9-]*$/.test(row.id || "")) throw new Error(`${label} id is invalid`);
  if (!String(row.source || "").trim() || !SEMVER.test(row.version || "") || !REVISION.test(row.revision || "")
      || !String(row.compatibility || "").trim()) throw new Error(`${label} version/revision record is invalid`);
}

export function validateDependencies(dependencies) {
  exactKeys(dependencies, ["schema", "engine", "features", "compatibility"], "dependencies");
  if (dependencies.schema !== "ai_studio.game.dependencies.v2" || !Array.isArray(dependencies.features)
      || !String(dependencies.compatibility || "").trim()) throw new Error("dependencies record is invalid");
  validateDependencyRow(dependencies.engine, "engine dependency");
  const ids = new Set();
  for (const feature of dependencies.features) {
    validateDependencyRow(feature, `feature dependency ${feature?.id || "<unknown>"}`, true);
    if (ids.has(feature.id)) throw new Error(`duplicate feature dependency: ${feature.id}`);
    ids.add(feature.id);
  }
  return dependencies;
}

function requirePlatformSdkDependency(dependencies) {
  const candidates = dependencies.features.filter((feature) => feature.id === "platform-sdk" || feature.source === "features/platform-sdk");
  if (candidates.length !== 1 || candidates[0].id !== "platform-sdk" || candidates[0].source !== "features/platform-sdk") {
    throw new Error("web packaging requires exactly one canonical platform-sdk dependency");
  }
}

function defaultGit(cwd, args) {
  const safe = resolve(cwd).replaceAll("\\", "/");
  return spawnSync("git", ["-c", `safe.directory=${safe}`, ...args], {
    cwd, encoding: "utf8", shell: false,
  });
}

function gitText(git, cwd, args, label) {
  const result = git(cwd, args);
  if (result?.error || result?.status !== 0) throw new Error(`${label}: ${result?.error?.message || result?.stderr || "git failed"}`);
  return String(result.stdout || "").trim();
}

function confinedSource(root, source, expected, label) {
  if (source !== expected || source.includes("\\") || isAbsolute(source) || source.split("/").some((part) => !part || part === "." || part === "..")) {
    throw new Error(`${label} dependency source must be exactly ${expected}`);
  }
  const candidate = resolve(root, ...source.split("/"));
  if (!existsSync(candidate)) throw new Error(`${label} dependency source is missing: ${source}`);
  if (lstatSync(candidate).isSymbolicLink()) throw new Error(`${label} dependency source cannot be a symlink: ${source}`);
  const realRoot = realpathSync(root);
  const realCandidate = realpathSync(candidate);
  const rel = relative(realRoot, realCandidate);
  if (!rel || rel === ".." || rel.startsWith(`..${sep}`) || isAbsolute(rel)) throw new Error(`${label} dependency source escapes the authoritative root`);
  return realCandidate;
}

function engineVersion(header) {
  const parts = ["MAJOR", "MINOR", "PATCH"].map((name) => {
    const match = new RegExp(`^\\s*#define\\s+NT_VERSION_${name}\\s+(\\d+)\\s*$`, "m").exec(header);
    if (!match) throw new Error(`engine nt_core.h is missing NT_VERSION_${name}`);
    return Number(match[1]);
  });
  return parts.join(".");
}

export function verifyDependencySources({ studioRoot, dependencies, git = defaultGit }) {
  const root = realpathSync(resolve(studioRoot));
  validateDependencies(dependencies);
  requirePlatformSdkDependency(dependencies);
  const studioRevision = gitText(git, root, ["rev-parse", "HEAD"], "Studio dependency revision is unavailable").toLowerCase();
  if (!REVISION.test(studioRevision)) throw new Error("Studio dependency revision is invalid");

  const engine = dependencies.engine;
  const engineRoot = confinedSource(root, engine.source, "external/neotolis-engine", "engine");
  const actualEngineVersion = engineVersion(readFileSync(join(engineRoot, "engine", "core", "nt_core.h"), "utf8"));
  if (actualEngineVersion !== engine.version) throw new Error(`engine dependency version mismatch: expected ${engine.version}, found ${actualEngineVersion}`);
  const engineRevision = gitText(git, engineRoot, ["rev-parse", "HEAD"], "engine dependency revision is unavailable").toLowerCase();
  if (engineRevision !== engine.revision) throw new Error(`engine dependency revision mismatch: expected ${engine.revision}, found ${engineRevision}`);
  const gitlink = gitText(git, root, ["ls-tree", "HEAD", "--", engine.source], "engine gitlink is unavailable");
  if (!new RegExp(`^160000 commit ${engine.revision}\\t${engine.source.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}$`).test(gitlink)) {
    throw new Error("engine dependency revision does not match the authoritative Studio gitlink");
  }
  if (gitText(git, engineRoot, ["status", "--porcelain=v1", "--untracked-files=all"], "engine cleanliness is unavailable")) {
    throw new Error("engine dependency source is dirty");
  }

  for (const feature of dependencies.features) {
    const featureRoot = confinedSource(root, feature.source, `features/${feature.id}`, `feature ${feature.id}`);
    const metadata = readJson(join(featureRoot, "feature.json"), `feature ${feature.id} metadata`);
    if (metadata.id !== feature.id) throw new Error(`feature dependency id mismatch: expected ${feature.id}, found ${metadata.id || "missing"}`);
    if (metadata.version !== feature.version) throw new Error(`feature ${feature.id} version mismatch: expected ${feature.version}, found ${metadata.version || "missing"}`);
    if (feature.revision !== studioRevision) throw new Error(`feature ${feature.id} revision mismatch: expected ${feature.revision}, found ${studioRevision}`);
    if (gitText(git, root, ["status", "--porcelain=v1", "--untracked-files=all", "--", feature.source], `feature ${feature.id} cleanliness is unavailable`)) {
      throw new Error(`feature ${feature.id} dependency source is dirty`);
    }
  }
  return { studioRevision, engineRevision };
}

function targetManifest(studioRoot, target) {
  if (!TARGETS.has(target)) throw new Error(`unknown package target: ${target} (use ${[...TARGETS].join("|")})`);
  const path = join(studioRoot, "features", "platform-sdk", "publish-targets", `${target}.json`);
  const value = readJson(path, `${target} publish manifest`);
  exactKeys(value, ["schema", "target", "platform_sdk", "required_files", "zip_layout", "metadata", "sdk_policy", "validation_command"], `${target} publish manifest`);
  if (value.schema !== "ai_studio.publish_target.v1" || value.target !== target || !/^[a-z][a-z0-9-]*$/.test(value.platform_sdk || "")
      || !Array.isArray(value.required_files) || value.required_files.length === 0) throw new Error(`${target} publish manifest is invalid`);
  if (value.required_files.some((path) => typeof path !== "string" || path.includes("\\"))) throw new Error(`${target} publish manifest required_files are invalid`);
  const required = value.required_files.map((path) => slash(path));
  if (new Set(required).size !== required.length || required.some((path) => !path || path.startsWith("/") || /^[A-Za-z]:/.test(path)
      || path.split("/").some((part) => !part || part === "." || part === ".."))) throw new Error(`${target} publish manifest required_files are invalid`);
  return { ...value, required_files: required };
}

function collectFiles(root) {
  const realRoot = realpathSync(root);
  const files = [];
  function visit(dir) {
    for (const name of readdirSync(dir).sort()) {
      const path = join(dir, name);
      const info = lstatSync(path);
      if (info.isSymbolicLink()) throw new Error(`artifact symlink is forbidden: ${slash(relative(root, path))}`);
      const real = realpathSync(path);
      const relReal = relative(realRoot, real);
      if (relReal === ".." || relReal.startsWith(`..${sep}`) || isAbsolute(relReal)) {
        throw new Error(`artifact path escapes its root: ${slash(relative(root, path))}`);
      }
      if (info.isDirectory()) visit(path);
      else if (info.isFile()) files.push(slash(relative(root, path)));
      else throw new Error(`unsupported artifact entry: ${slash(relative(root, path))}`);
    }
  }
  visit(root);
  return files;
}

function exactAllowlist(actual, required) {
  const actualSet = new Set(actual);
  const folded = new Map(actual.map((path) => [path.toLowerCase(), path]));
  for (const path of required) {
    if (actualSet.has(path)) continue;
    if (folded.has(path.toLowerCase())) throw new Error(`case mismatch for required file ${path}: found ${folded.get(path.toLowerCase())}`);
    throw new Error(`missing required file: ${path}`);
  }
  for (const path of actual) {
    if (SOURCE_EXTENSIONS.test(path)) throw new Error(`source-only file is forbidden in final artifact: ${path}`);
    if (!required.includes(path)) throw new Error(`unexpected file in final artifact: ${path}`);
  }
}

function scriptAttributes(input) {
  const attributes = new Map();
  let offset = 0;
  while (offset < input.length) {
    while (/\s/.test(input[offset] || "")) offset += 1;
    if (offset >= input.length || input[offset] === "/") break;
    const start = offset;
    while (offset < input.length && !/[\s=/>]/.test(input[offset])) offset += 1;
    const name = input.slice(start, offset).toLowerCase();
    while (/\s/.test(input[offset] || "")) offset += 1;
    let value = "";
    if (input[offset] === "=") {
      offset += 1;
      while (/\s/.test(input[offset] || "")) offset += 1;
      const quote = input[offset] === "'" || input[offset] === '"' ? input[offset++] : "";
      const valueStart = offset;
      if (quote) while (offset < input.length && input[offset] !== quote) offset += 1;
      else while (offset < input.length && !/[\s>]/.test(input[offset])) offset += 1;
      value = input.slice(valueStart, offset);
      if (quote && input[offset] === quote) offset += 1;
    }
    if (name) attributes.set(name, value);
  }
  return attributes;
}

function lexBootstrapScript(source) {
  const tokens = [];
  const regexPrefixPunctuators = new Set(["(", "[", "{", ",", ";", ":", "=", "!", "?", "+", "-", "*", "%", "&", "|", "^", "~", "<", ">"]);
  const regexPrefixKeywords = new Set(["return", "throw", "case", "delete", "typeof", "void", "new", "in", "of", "yield", "await"]);
  let offset = 0;
  while (offset < source.length) {
    if (/\s/.test(source[offset])) { offset += 1; continue; }
    if (source.startsWith("//", offset)) {
      offset = source.indexOf("\n", offset + 2);
      if (offset === -1) break;
      continue;
    }
    if (source.startsWith("/*", offset)) {
      const end = source.indexOf("*/", offset + 2);
      if (end === -1) throw new Error("release HTML has an unterminated bootstrap comment");
      offset = end + 2;
      continue;
    }
    if (source[offset] === "/") {
      const previous = tokens.at(-1);
      const startsRegex = previous === undefined
        || (previous.type === "punctuator" && regexPrefixPunctuators.has(previous.value))
        || (previous.type === "identifier" && regexPrefixKeywords.has(previous.value));
      if (startsRegex) {
        const start = offset++;
        let escaped = false;
        let characterClass = false;
        let closed = false;
        while (offset < source.length) {
          const character = source[offset++];
          if (character === "\n" || character === "\r") break;
          if (escaped) { escaped = false; continue; }
          if (character === "\\") { escaped = true; continue; }
          if (character === "[") { characterClass = true; continue; }
          if (character === "]") { characterClass = false; continue; }
          if (character === "/" && !characterClass) { closed = true; break; }
        }
        if (!closed) throw new Error("release HTML has an unterminated bootstrap regular expression");
        while (/[A-Za-z]/.test(source[offset] || "")) offset += 1;
        tokens.push({ type: "regex", value: source.slice(start, offset) });
        continue;
      }
    }
    const quote = source[offset];
    if (quote === "'" || quote === '"' || quote === "`") {
      offset += 1;
      let value = "";
      let escaped = false;
      while (offset < source.length) {
        const character = source[offset++];
        if (character === quote && !escaped) break;
        if (character === "\\" && !escaped) { escaped = true; value += character; continue; }
        escaped = false;
        value += character;
      }
      tokens.push({ type: "string", value, quote });
      continue;
    }
    if (/[A-Za-z_$]/.test(source[offset])) {
      const start = offset++;
      while (/[A-Za-z0-9_$]/.test(source[offset] || "")) offset += 1;
      tokens.push({ type: "identifier", value: source.slice(start, offset) });
      continue;
    }
    tokens.push({ type: "punctuator", value: source[offset++] });
  }
  return tokens;
}

function tokenValuesAt(tokens, offset, expected) {
  return expected.every((value, index) => tokens[offset + index]?.value === value);
}

function executableScript(attributes) {
  if (!attributes.has("type")) return true;
  const type = String(attributes.get("type") || "").trim().toLowerCase().split(";", 1)[0].trim();
  return type === "" || type === "module" || [
    "text/javascript",
    "application/javascript",
    "text/ecmascript",
    "application/ecmascript",
  ].includes(type);
}

function braceDepths(tokens) {
  const depths = [];
  let depth = 0;
  for (let offset = 0; offset < tokens.length; offset += 1) {
    depths.push(depth);
    if (tokens[offset].value === "{") depth += 1;
    else if (tokens[offset].value === "}") depth = Math.max(0, depth - 1);
  }
  return depths;
}

function topLevelSequenceOffset(tokens, expected, start = 0) {
  const depths = braceDepths(tokens);
  for (let offset = start; offset < tokens.length; offset += 1) {
    if (depths[offset] === 0 && tokenValuesAt(tokens, offset, expected)) return offset;
  }
  return -1;
}

function liveTopLevelSequenceOffset(tokens, expected, start = 0) {
  const depths = braceDepths(tokens);
  const dead = deadTokenOffsets(tokens);
  for (let offset = start; offset < tokens.length; offset += 1) {
    if (depths[offset] === 0 && !dead.has(offset) && tokenValuesAt(tokens, offset, expected)) return offset;
  }
  return -1;
}

function matchingDelimiter(tokens, start, open, close) {
  if (tokens[start]?.value !== open) return -1;
  let depth = 0;
  for (let offset = start; offset < tokens.length; offset += 1) {
    if (tokens[offset].value === open) depth += 1;
    else if (tokens[offset].value === close && --depth === 0) return offset;
  }
  return -1;
}

function argumentSlices(tokens, open, close) {
  if (open < 0 || close <= open + 1) return [];
  const result = [];
  let start = open + 1;
  let parens = 0;
  let brackets = 0;
  let braces = 0;
  for (let offset = start; offset < close; offset += 1) {
    const value = tokens[offset].value;
    if (value === "(") parens += 1;
    else if (value === ")") parens -= 1;
    else if (value === "[") brackets += 1;
    else if (value === "]") brackets -= 1;
    else if (value === "{") braces += 1;
    else if (value === "}") braces -= 1;
    else if (value === "," && parens === 0 && brackets === 0 && braces === 0) {
      result.push(tokens.slice(start, offset));
      start = offset + 1;
    }
  }
  result.push(tokens.slice(start, close));
  return result;
}

function topLevelFunctionDefinitions(tokens) {
  const depths = braceDepths(tokens);
  const definitions = new Map();
  for (let offset = 0; offset < tokens.length; offset += 1) {
    if (depths[offset] !== 0 || tokens[offset].value !== "function" || tokens[offset + 1]?.type !== "identifier") continue;
    const name = tokens[offset + 1].value;
    const paramsStart = offset + 2;
    const paramsEnd = matchingDelimiter(tokens, paramsStart, "(", ")");
    const bodyStart = paramsEnd + 1;
    const bodyEnd = matchingDelimiter(tokens, bodyStart, "{", "}");
    if (paramsEnd === -1 || bodyEnd === -1) continue;
    const params = argumentSlices(tokens, paramsStart, paramsEnd).map((part) => part.length === 1 && part[0].type === "identifier" ? part[0].value : null);
    definitions.set(name, { name, params, body: tokens.slice(bodyStart + 1, bodyEnd) });
    offset = bodyEnd;
  }
  return definitions;
}

function immediatelyInvokedFunctionBlocks(tokens) {
  const depths = braceDepths(tokens);
  const result = new Set();
  for (let offset = 0; offset < tokens.length; offset += 1) {
    if (depths[offset] !== 0 || tokens[offset].value !== "function" || tokens[offset + 1]?.value !== "(") continue;
    const paramsEnd = matchingDelimiter(tokens, offset + 1, "(", ")");
    const bodyStart = paramsEnd + 1;
    const bodyEnd = matchingDelimiter(tokens, bodyStart, "{", "}");
    if (paramsEnd === -1 || bodyEnd === -1) continue;
    const invokedBeforeClosingOuter = tokenValuesAt(tokens, bodyEnd + 1, ["(", ")", ")"]);
    const invokedAfterClosingOuter = tokenValuesAt(tokens, bodyEnd + 1, [")", "(", ")"]);
    if (invokedBeforeClosingOuter || invokedAfterClosingOuter) result.add(bodyStart);
    offset = bodyEnd;
  }
  return result;
}

function falseBlockStarts(tokens) {
  const result = new Set();
  for (let offset = 0; offset < tokens.length; offset += 1) {
    if (tokenValuesAt(tokens, offset, ["if", "(", "false", ")", "{"])) result.add(offset + 4);
  }
  return result;
}

function bracePaths(tokens) {
  const paths = [];
  const stack = [];
  for (let offset = 0; offset < tokens.length; offset += 1) {
    paths.push([...stack]);
    if (tokens[offset].value === "{") stack.push(offset);
    else if (tokens[offset].value === "}") stack.pop();
  }
  return paths;
}

function statementEnd(tokens, start) {
  let parens = 0;
  let brackets = 0;
  let braces = 0;
  for (let offset = start; offset < tokens.length; offset += 1) {
    const value = tokens[offset].value;
    if (value === "(") parens += 1;
    else if (value === ")") parens -= 1;
    else if (value === "[") brackets += 1;
    else if (value === "]") brackets -= 1;
    else if (value === "{") braces += 1;
    else if (value === "}") braces -= 1;
    else if (value === ";" && parens === 0 && brackets === 0 && braces === 0) return offset;
  }
  return tokens.length - 1;
}

function deadTokenOffsets(tokens) {
  const result = new Set();
  const paths = bracePaths(tokens);
  const deadBlocks = falseBlockStarts(tokens);
  for (let offset = 0; offset < tokens.length; offset += 1) {
    if (paths[offset].some((block) => deadBlocks.has(block))) result.add(offset);
    if (!tokenValuesAt(tokens, offset, ["if", "(", "false", ")"]) || tokens[offset + 4]?.value === "{") continue;
    const end = statementEnd(tokens, offset + 4);
    for (let dead = offset + 4; dead <= end; dead += 1) result.add(dead);
  }
  return result;
}

function callsIn(tokens) {
  const paths = bracePaths(tokens);
  const dead = deadTokenOffsets(tokens);
  const calls = [];
  for (let offset = 0; offset < tokens.length - 1; offset += 1) {
    if (tokens[offset].type !== "identifier" || tokens[offset + 1].value !== "(" || tokens[offset - 1]?.value === "function") continue;
    const close = matchingDelimiter(tokens, offset + 1, "(", ")");
    if (close === -1) continue;
    calls.push({
      name: tokens[offset].value,
      start: offset,
      args: argumentSlices(tokens, offset + 1, close),
      path: paths[offset],
      dead: dead.has(offset),
    });
  }
  return calls;
}

function dynamicallyLoadsGame(tokens) {
  const create = ["const", "let", "var"].map((declaration) => liveTopLevelSequenceOffset(
    tokens,
    [declaration, "gameScript", "=", "document", ".", "createElement", "(", "script", ")", ";"],
  )).find((offset) => offset !== -1) ?? -1;
  if (create === -1) return false;
  const source = liveTopLevelSequenceOffset(tokens, ["gameScript", ".", "src", "=", "game.js", ";"], create + 10);
  if (source === -1) return false;
  return liveTopLevelSequenceOffset(tokens, ["document", ".", "body", ".", "appendChild", "(", "gameScript", ")", ";"], source + 6) !== -1;
}

function helperInstantiatesPath(definition, pathParam, definitions, visited = new Set()) {
  const visitKey = `${definition.name}:${pathParam}`;
  if (visited.has(visitKey)) return false;
  visited.add(visitKey);
  const tokens = definition.body;
  const paths = bracePaths(tokens);
  const dead = deadTokenOffsets(tokens);
  for (let offset = 0; offset < tokens.length; offset += 1) {
    if (tokens[offset].type !== "identifier" || tokens[offset + 1]?.value !== "=") continue;
    const boundName = tokens[offset].value;
    let call = offset + 2;
    if (tokens[call]?.value === "await") call += 1;
    const source = tokens[call]?.value;
    if (!["fetch", "getWasmBinary"].includes(source) || tokens[call + 1]?.value !== "(" || tokens[call + 2]?.value !== pathParam) continue;
    const method = source === "fetch" ? "instantiateStreaming" : "instantiate";
    const bindingPath = paths[offset];
    if (dead.has(offset)) continue;
    for (let instantiate = call + 3; instantiate < tokens.length; instantiate += 1) {
      if (paths[instantiate].length !== bindingPath.length
          || paths[instantiate].some((block, index) => block !== bindingPath[index])) continue;
      if (dead.has(instantiate)) continue;
      if (!tokenValuesAt(tokens, instantiate, ["WebAssembly", ".", method, "(", boundName])) continue;
      const previous = tokens[instantiate - 1]?.value;
      const equals = previous === "await" ? instantiate - 2 : instantiate - 1;
      const returned = previous === "return";
      const assigned = tokens[equals]?.value === "="
        && tokens[equals - 1]?.type === "identifier"
        && ["const", "let", "var"].includes(tokens[equals - 2]?.value);
      if (returned || assigned) return true;
    }
  }
  for (const call of callsIn(tokens)) {
    if (call.dead) continue;
    const argument = call.args.findIndex((part) => part.length === 1 && part[0].value === pathParam);
    const child = definitions.get(call.name);
    if (argument !== -1 && child?.params[argument]
        && helperInstantiatesPath(child, child.params[argument], definitions, visited)) return true;
  }
  return false;
}

function validateGameLoader(input, label = "game.js") {
  const tokens = lexBootstrapScript(Buffer.isBuffer(input) ? input.toString("utf8") : String(input));
  const definitions = topLevelFunctionDefinitions(tokens);
  const finder = definitions.get("findWasmBinary");
  const creator = definitions.get("createWasm");
  const finderReturnsGame = finder
    && liveTopLevelSequenceOffset(finder.body, ["return", "locateFile", "(", "game.wasm", ")"]) !== -1;
  const initializesPath = creator && (
    liveTopLevelSequenceOffset(creator.body, ["wasmBinaryFile", "?", "?", "=", "findWasmBinary", "(", ")", ";"]) !== -1
    || liveTopLevelSequenceOffset(creator.body, ["wasmBinaryFile", "=", "findWasmBinary", "(", ")", ";"]) !== -1
  );
  const linkedInstantiation = creator && callsIn(creator.body).some((call) => {
    if (call.path.length !== 0 || call.dead) return false;
    const argument = call.args.findIndex((part) => part.length === 1 && part[0].value === "wasmBinaryFile");
    const helper = definitions.get(call.name);
    return argument !== -1 && helper?.params[argument]
      && helperInstantiatesPath(helper, helper.params[argument], definitions);
  });
  const topLevelInvocation = callsIn(tokens).some((call) => {
    if (call.name !== "createWasm" || call.args.length !== 0 || call.path.length !== 0 || call.dead) return false;
    const previous = tokens[call.start - 1]?.value;
    return previous === undefined || previous === ";" || previous === "}";
  });
  if (!finderReturnsGame || !initializesPath || !linkedInstantiation || !topLevelInvocation) {
    throw new Error(`${label} is not a linked executable Emscripten game.wasm loader`);
  }
}

function parseReleaseConfig(html) {
  const marker = "window.__PLATFORM_SDK_CONFIG__";
  const withoutComments = html.replace(/<!--[\s\S]*?-->/g, "");
  const configs = [];
  let loadsGame = false;
  for (const match of withoutComments.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script\s*>/gi)) {
    const attributes = scriptAttributes(match[1]);
    if (!executableScript(attributes)) continue;
    const src = attributes.get("src");
    if (src !== undefined) {
      if (src === "game.js") loadsGame = true;
      continue;
    }
    const tokens = lexBootstrapScript(match[2]);
    if (dynamicallyLoadsGame(tokens)) loadsGame = true;
    const paths = bracePaths(tokens);
    const dead = deadTokenOffsets(tokens);
    const invokedBlocks = immediatelyInvokedFunctionBlocks(tokens);
    for (let offset = 0; offset < tokens.length; offset += 1) {
      const path = paths[offset];
      if (dead.has(offset)
          || (path.length !== 0 && !(path.length === 1 && invokedBlocks.has(path[0])))) continue;
      const prefix = ["window", ".", "__PLATFORM_SDK_CONFIG__", "=", "Object", ".", "freeze", "(", "{", "target", ":"];
      if (!tokenValuesAt(tokens, offset, prefix)) continue;
      const target = tokens[offset + prefix.length];
      const middle = [",", "platformSdk", ":"];
      const middleOffset = offset + prefix.length + 1;
      const adapter = tokens[middleOffset + middle.length];
      const suffixOffset = middleOffset + middle.length + 1;
      const suffix = [",", "release", ":"];
      const release = tokens[suffixOffset + suffix.length];
      const endOffset = suffixOffset + suffix.length + 1;
      if (target?.type !== "string" || !tokenValuesAt(tokens, middleOffset, middle)
          || adapter?.type !== "string" || !tokenValuesAt(tokens, suffixOffset, suffix)
          || !["true", "false"].includes(release?.value)
          || !tokenValuesAt(tokens, endOffset, ["}", ")", ";"])) {
        throw new Error(`release HTML has an invalid executable ${marker} assignment`);
      }
      configs.push({ target: target.value, platformAdapter: adapter.value, release: release.value === "true" });
    }
  }
  if (configs.length !== 1) throw new Error(`release HTML must contain exactly one executable inline ${marker} assignment`);
  if (!loadsGame) throw new Error("release entrypoint index.html does not executably load game.js");
  return configs[0];
}

function readUleb(bytes, offset, label) {
  let value = 0;
  let shift = 0;
  for (let count = 0; count < 5; count += 1) {
    if (offset >= bytes.length) throw new Error(`truncated WASM ${label}`);
    const byte = bytes[offset++];
    value |= (byte & 0x7f) << shift;
    if ((byte & 0x80) === 0) return { value: value >>> 0, offset };
    shift += 7;
  }
  throw new Error(`invalid WASM ${label}`);
}

export function validateWasmRelease(input) {
  const bytes = Buffer.isBuffer(input) ? input : Buffer.from(input);
  if (bytes.length < WASM_MAGIC.length || !bytes.subarray(0, WASM_MAGIC.length).equals(WASM_MAGIC)) {
    throw new Error("invalid WebAssembly magic or version");
  }
  if (!WebAssembly.validate(bytes)) throw new Error("invalid WebAssembly structure");
  let offset = WASM_MAGIC.length;
  const sections = new Set();
  while (offset < bytes.length) {
    const id = bytes[offset++];
    if (id > 13) throw new Error(`invalid WASM section id ${id}`);
    const size = readUleb(bytes, offset, "section size");
    offset = size.offset;
    const end = offset + size.value;
    if (end > bytes.length) throw new Error("truncated WASM section payload");
    if (id === 0) {
      const nameLength = readUleb(bytes, offset, "custom section name");
      const nameEnd = nameLength.offset + nameLength.value;
      if (nameEnd > end) throw new Error("truncated WASM custom section name");
      const name = bytes.subarray(nameLength.offset, nameEnd).toString("utf8");
      const lowered = name.toLowerCase();
      if (lowered === "name" || lowered.includes("debug") || lowered.includes("source") || lowered === "external_debug_info") {
        throw new Error(`debug/name/source-map WASM custom section is forbidden: ${name}`);
      }
    } else sections.add(id);
    offset = end;
  }
  const markerText = bytes.toString("latin1").toLowerCase();
  const marker = WASM_FORBIDDEN_MARKERS.find((value) => markerText.includes(value.toLowerCase()));
  if (marker) throw new Error(`DevAPI/debug marker is forbidden in release WASM: ${marker}`);
  if (!sections.has(7) || ![2, 3, 5].some((id) => sections.has(id))) {
    throw new Error("release WASM is empty or lacks a stable game module shape");
  }
  const exports = WebAssembly.Module.exports(new WebAssembly.Module(bytes));
  if (!exports.some((entry) => entry.kind === "function") || !exports.some((entry) => entry.kind === "memory")) {
    throw new Error("release WASM lacks the required function and memory exports for the game module shape");
  }
  return true;
}

function expectedPlatformBytes(studioRoot, adapter) {
  const web = join(studioRoot, "features", "platform-sdk", "web");
  return new Map([
    ["platform-sdk.js", readFileSync(join(web, "platform-sdk.js"))],
    ["platform-sdk-core.js", readFileSync(join(web, "platform-sdk-core.js"))],
    ["platform-sdk-adapter.js", readFileSync(join(web, "adapters", `${adapter}.js`))],
  ]);
}

function fileBytes(artifactDir, paths) {
  return paths.map((path) => ({ path, bytes: readFileSync(join(artifactDir, ...path.split("/"))) }));
}

export function validateWebArtifact({ artifactDir, target, studioRoot = DEFAULT_STUDIO_ROOT }) {
  const root = resolve(artifactDir);
  if (!existsSync(root) || !statSync(root).isDirectory()) throw new Error(`web artifact directory is missing: ${root}`);
  const contract = targetManifest(resolve(studioRoot), target);
  const actual = collectFiles(root);
  exactAllowlist(actual, contract.required_files);

  const html = readFileSync(join(root, "index.html"), "utf8");
  const config = parseReleaseConfig(html);
  if (config.target !== target) throw new Error(`release target mismatch: expected ${target}, found ${config.target || "missing"}`);
  if (config.platformAdapter !== contract.platform_sdk) {
    throw new Error(`release adapter mismatch: expected ${contract.platform_sdk}, found ${config.platformAdapter || "missing"}`);
  }
  if (!config.release) throw new Error("release metadata is missing release=true");
  if (readFileSync(join(root, "game.wasm")).length === 0 || readFileSync(join(root, "assets", "game.ntpack")).length === 0) {
    throw new Error("required game payload is empty");
  }
  validateWasmRelease(readFileSync(join(root, "game.wasm")));
  validateGameLoader(readFileSync(join(root, "game.js")));
  for (const [path, expected] of expectedPlatformBytes(studioRoot, contract.platform_sdk)) {
    if (!readFileSync(join(root, path)).equals(expected)) {
      throw new Error(path === "platform-sdk-adapter.js" ? `platform adapter mismatch for ${target}` : `platform source mismatch for ${path}`);
    }
  }
  const inspection = inspectPlatformSdkArtifact({ target, artifactDir: root, production: true, requireFiles: true });
  if (!inspection.ok) throw new Error(`platform artifact inspection failed: ${inspection.violations.map((item) => `${item.reason}:${basename(item.file)}`).join(", ")}`);

  for (const path of actual.filter((entry) => /\.(?:html|js|json)$/i.test(entry))) {
    const text = readFileSync(join(root, ...path.split("/")), "utf8");
    const marker = DEVAPI_MARKERS.find((value) => text.includes(value));
    if (marker) throw new Error(`DevAPI payload is forbidden in final artifact: ${path} (${marker})`);
  }
  if (target === "playgama") {
    const configPath = join(root, "playgama-bridge-config.json");
    const bridge = readJson(configPath, "Playgama Bridge config");
    if (bridge.replace_before_submission === true || String(bridge.schema || "").includes("placeholder")) {
      throw new Error("Playgama placeholder configuration must be replaced before packaging");
    }
  }
  return { artifactDir: root, contract, files: fileBytes(root, contract.required_files) };
}

function compactDependencies(dependencies, bytes) {
  return {
    sha256: sha256(bytes),
    record: dependencies,
  };
}

function releaseMetadata(identity, dependenciesHash, target, adapter, proof = "game") {
  return {
    schema: "ai_studio.game.release.v1",
    game: { id: identity.id, title: identity.title, storageNamespace: identity.storageNamespace },
    target,
    platformAdapter: adapter,
    entrypoint: "index.html",
    dependenciesSha256: dependenciesHash,
    build: { preset: "wasm-release", devapi: false, debug: false },
    proof,
  };
}

function manifestFromZip({ zipBytes, zipPath, entries, release, dependencies }) {
  return {
    schema: "ai_studio.game.artifact_manifest.v1",
    game: release.game,
    target: release.target,
    platformAdapter: release.platformAdapter,
    artifact: { file: basename(zipPath), size: zipBytes.length, sha256: sha256(zipBytes) },
    entrypoint: release.entrypoint,
    releaseMetadataSha256: sha256(entries.get("release.json")),
    dependencies,
    entries: [...entries].map(([path, bytes]) => ({ path, size: bytes.length, sha256: sha256(bytes) })),
  };
}

function validateReopenedPayload(entries, target, studioRoot) {
  const contract = targetManifest(studioRoot, target);
  const actual = [...entries.keys()];
  const expected = [...contract.required_files, "release.json"].sort();
  if (JSON.stringify(actual) !== JSON.stringify(expected)) throw new Error("reopened ZIP exact allowlist mismatch");
  const html = entries.get("index.html")?.toString("utf8") || "";
  const config = parseReleaseConfig(html);
  if (config.target !== target || config.platformAdapter !== contract.platform_sdk || !config.release) {
    throw new Error("reopened ZIP target/adapter/release mismatch");
  }
  if (!entries.get("game.wasm")?.length || !entries.get("assets/game.ntpack")?.length) throw new Error("reopened ZIP required payload is empty");
  validateWasmRelease(entries.get("game.wasm"));
  validateGameLoader(entries.get("game.js"), "reopened ZIP game.js");
  for (const [path, expected] of expectedPlatformBytes(studioRoot, contract.platform_sdk)) {
    if (!entries.get(path)?.equals(expected)) {
      throw new Error(path === "platform-sdk-adapter.js" ? "reopened ZIP platform adapter mismatch" : `reopened ZIP platform source mismatch for ${path}`);
    }
  }
  const forbidden = [...new Set([...(contract.sdk_policy?.forbidden_markers || []), ...DEVAPI_MARKERS])];
  for (const [path, bytes] of entries) {
    if (!/\.(?:html|js|json)$/i.test(path)) continue;
    const text = bytes.toString("utf8");
    const marker = forbidden.find((value) => text.includes(value));
    if (marker) throw new Error(`reopened ZIP forbidden marker in ${path}: ${marker}`);
  }
  if (target === "playgama") {
    const bridge = JSON.parse(entries.get("playgama-bridge-config.json")?.toString("utf8") || "null");
    if (!bridge || bridge.replace_before_submission === true || String(bridge.schema || "").includes("placeholder")) {
      throw new Error("reopened ZIP contains Playgama placeholder configuration");
    }
  }
  return contract;
}

function publishVerified(tempPath, finalPath) {
  if (existsSync(finalPath)) {
    if (!readFileSync(finalPath).equals(readFileSync(tempPath))) throw new Error(`refusing to replace a different deterministic artifact: ${finalPath}`);
    rmSync(tempPath, { force: true });
    return false;
  }
  try {
    renameSync(tempPath, finalPath);
    return true;
  } catch (error) {
    if (existsSync(finalPath) && readFileSync(finalPath).equals(readFileSync(tempPath))) {
      rmSync(tempPath, { force: true });
      return false;
    }
    throw error;
  }
}

function preflightVerified(tempPath, finalPath) {
  if (existsSync(finalPath) && !readFileSync(finalPath).equals(readFileSync(tempPath))) {
    throw new Error(`refusing to replace a different deterministic artifact: ${finalPath}`);
  }
  return existsSync(finalPath);
}

export function packageWebArtifact(options) {
  const gameDir = resolve(options.gameDir);
  const target = options.target || "itch";
  const studioRoot = resolve(options.studioRoot || findStudioRoot(gameDir));
  const identity = validateIdentity(options.identity || readJson(join(gameDir, "game.json"), "game identity"));
  const dependencyPath = join(gameDir, "dependencies.json");
  const dependencies = validateDependencies(options.dependencies || readJson(dependencyPath, "dependencies"));
  requirePlatformSdkDependency(dependencies);
  (options.dependencyVerifier || verifyDependencySources)({ studioRoot, dependencies, ...(options.git ? { git: options.git } : {}) });
  const dependencyBytes = jsonBytes(dependencies);
  const validated = validateWebArtifact({ artifactDir: options.artifactDir, target, studioRoot });
  const compact = compactDependencies(dependencies, dependencyBytes);
  const release = releaseMetadata(identity, compact.sha256, target, validated.contract.platform_sdk, options.proof || "game");
  const entries = [...validated.files, { path: "release.json", bytes: jsonBytes(release) }];
  const zipBytes = createStoreZip(entries);
  const outDir = resolve(options.outDir || join(gameDir, "release", "artifacts"));
  mkdirSync(outDir, { recursive: true });
  const artifactId = sha256(zipBytes).slice(0, 16);
  const zipName = `${identity.id}-${target}-${artifactId}.zip`;
  const manifestName = `${identity.id}-${target}-${artifactId}.manifest.json`;
  const zipPath = join(outDir, zipName);
  const manifestPath = join(outDir, manifestName);
  const tempDir = join(outDir, `.package-${process.pid}-${randomUUID()}.tmp`);
  mkdirSync(tempDir);
  const tempZip = join(tempDir, zipName);
  const tempManifest = join(tempDir, manifestName);
  try {
    writeFileSync(tempZip, zipBytes);
    const reopened = readStoreZip(readFileSync(tempZip));
    const manifest = manifestFromZip({ zipBytes: readFileSync(tempZip), zipPath: tempZip, entries: reopened, release, dependencies: compact });
    writeFileSync(tempManifest, jsonBytes(manifest));
    verifyWebPackage({ zipPath: tempZip, manifestPath: tempManifest, expectedTarget: target, studioRoot });
    const publisher = options.publisher || publishVerified;
    preflightVerified(tempManifest, manifestPath);
    preflightVerified(tempZip, zipPath);
    publisher(tempManifest, manifestPath);
    publisher(tempZip, zipPath);
    return { zipPath, manifestPath, manifest };
  } finally {
    rmSync(tempDir, { recursive: true, force: true });
  }
}

export function verifyWebPackage({ zipPath, manifestPath, expectedTarget, studioRoot = DEFAULT_STUDIO_ROOT }) {
  const zipBytes = readFileSync(zipPath);
  const entries = readStoreZip(zipBytes);
  const canonical = createStoreZip([...entries].map(([path, bytes]) => ({ path, bytes })));
  if (!canonical.equals(zipBytes)) throw new Error("artifact is not a byte-canonical ZIP after reopen");
  const manifest = readJson(manifestPath, "artifact manifest");
  exactKeys(manifest, ["schema", "game", "target", "platformAdapter", "artifact", "entrypoint", "releaseMetadataSha256", "dependencies", "entries"], "artifact manifest");
  if (manifest.schema !== "ai_studio.game.artifact_manifest.v1") throw new Error("artifact manifest schema mismatch");
  validateManifestGame(manifest.game, "artifact manifest game");
  exactKeys(manifest.artifact, ["file", "size", "sha256"], "artifact manifest ZIP");
  exactKeys(manifest.dependencies, ["sha256", "record"], "artifact manifest dependencies");
  if (!Array.isArray(manifest.entries)) throw new Error("artifact manifest entries must be an array");
  if (expectedTarget && manifest.target !== expectedTarget) throw new Error(`artifact target mismatch: expected ${expectedTarget}, found ${manifest.target}`);
  if (manifest.artifact.file !== basename(zipPath) || manifest.artifact.size !== zipBytes.length || manifest.artifact.sha256 !== sha256(zipBytes)) {
    throw new Error("artifact ZIP hash/size/name mismatch");
  }
  const releaseBytes = entries.get("release.json");
  if (!releaseBytes) throw new Error("release metadata is missing from ZIP");
  const release = JSON.parse(releaseBytes.toString("utf8"));
  exactKeys(release, ["schema", "game", "target", "platformAdapter", "entrypoint", "dependenciesSha256", "build", "proof"], "ZIP release metadata");
  validateManifestGame(release.game, "ZIP release game");
  exactKeys(release.build, ["preset", "devapi", "debug"], "ZIP release build");
  if (release.schema !== "ai_studio.game.release.v1" || release.target !== manifest.target
      || release.platformAdapter !== manifest.platformAdapter || release.entrypoint !== "index.html"
      || release.dependenciesSha256 !== manifest.dependencies.sha256 || release.build?.preset !== "wasm-release"
      || release.build?.devapi !== false || release.build?.debug !== false
      || !["game", "reference-template"].includes(release.proof)
      || JSON.stringify(release.game) !== JSON.stringify(manifest.game)
      || manifest.entrypoint !== release.entrypoint) throw new Error("ZIP release metadata mismatch");
  if (manifest.releaseMetadataSha256 !== sha256(releaseBytes)) throw new Error("release metadata hash mismatch");
  const dependencies = validateDependencies(manifest.dependencies?.record);
  requirePlatformSdkDependency(dependencies);
  if (manifest.dependencies.sha256 !== sha256(jsonBytes(dependencies))) throw new Error("dependency record hash mismatch");
  validateReopenedPayload(entries, manifest.target, resolve(studioRoot));
  const rows = [...entries].map(([path, bytes]) => ({ path, size: bytes.length, sha256: sha256(bytes) }));
  if (JSON.stringify(manifest.entries) !== JSON.stringify(rows)) throw new Error("artifact entry hash/size/path manifest mismatch");
  if (!entries.has("index.html") || !entries.has(release.entrypoint)) throw new Error("artifact entrypoint is missing");
  return manifest;
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  console.error("package_web.mjs is owned by tools/game.mjs; use: node tools/game.mjs package --target itch");
  process.exitCode = 2;
}
