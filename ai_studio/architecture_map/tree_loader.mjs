// Recursively materializes the split Architecture Map storage into the exact
// single-tree model consumed by validation, API, and browser rendering.

import { existsSync, readFileSync } from "node:fs";
import { dirname, isAbsolute, join, normalize, relative, resolve, sep } from "node:path";

function displayPath(value) {
  return String(value).replaceAll("\\", "/");
}

function resolvePartPath(referrerPath, requestedPath) {
  if (typeof requestedPath !== "string" || !requestedPath.trim()) {
    throw new Error(`architecture tree ref from "${displayPath(referrerPath)}" must be a non-empty string`);
  }
  const requested = requestedPath.trim();
  if (isAbsolute(requested) || /^[a-zA-Z]:[\\/]/.test(requested)) {
    throw new Error(`architecture tree ref "${displayPath(requested)}" from "${displayPath(referrerPath)}" must be relative`);
  }
  return normalize(join(dirname(referrerPath), requested));
}

// `loadPart(resolvedPath, requestedPath, referrerPath)` returns parsed JSON.
// Paths passed to it use the same namespace as `indexPath` (repo-relative for
// loadArchitectureTree, URL-relative for the browser twin).
export function mergeArchitectureTree(index, loadPart, { indexPath = "tree.json" } = {}) {
  if (!index || typeof index !== "object" || Array.isArray(index) || !index.root) return index;
  const hadRootParts = Object.hasOwn(index.root, "parts");

  function loadReferenced(requestedPath, referrerPath, stack) {
    const resolvedPath = resolvePartPath(referrerPath, requestedPath);
    const cycleAt = stack.indexOf(resolvedPath);
    if (cycleAt >= 0) {
      const chain = [...stack.slice(cycleAt), resolvedPath].map(displayPath).join(" -> ");
      throw new Error(`architecture tree ref cycle: ${chain}`);
    }
    let parsed;
    try {
      parsed = loadPart(resolvedPath, requestedPath, referrerPath);
    } catch (error) {
      if (String(error?.message || error).startsWith("architecture tree ref ")) throw error;
      const prefix = `architecture tree ref "${displayPath(requestedPath)}" from "${displayPath(referrerPath)}" resolved to "${displayPath(resolvedPath)}"`;
      if (error?.code === "ENOENT") throw new Error(`${prefix}: not found`);
      if (error instanceof SyntaxError) throw new Error(`${prefix}: malformed JSON: ${error.message}`);
      throw new Error(`${prefix}: ${error?.message || error}`);
    }
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
      throw new Error(
        `architecture tree ref "${displayPath(requestedPath)}" from "${displayPath(referrerPath)}" resolved to "${displayPath(resolvedPath)}": expected a JSON object`,
      );
    }
    return materialize(parsed, resolvedPath, [...stack, resolvedPath]);
  }

  function materialize(value, referrerPath, stack) {
    if (Array.isArray(value)) return value.map((item) => materialize(item, referrerPath, stack));
    if (!value || typeof value !== "object") return value;
    if (Object.hasOwn(value, "parts")) {
      if (!Array.isArray(value.parts)) {
        throw new Error(`architecture tree parts in "${displayPath(referrerPath)}" must be an array`);
      }
      if (Object.hasOwn(value, "children")) {
        throw new Error(`architecture tree node in "${displayPath(referrerPath)}" cannot contain both parts and children`);
      }
      const children = value.parts.map((partPath) => loadReferenced(partPath, referrerPath, stack));
      const materialized = {};
      for (const [key, field] of Object.entries(value)) {
        if (key === "parts") materialized.children = children;
        else materialized[key] = field;
      }
      return materialized;
    }
    if (!Array.isArray(value.children)) return value;
    return { ...value, children: value.children.map((child) => materialize(child, referrerPath, stack)) };
  }

  const materializedRoot = materialize(index.root, normalize(indexPath), []);
  if (!hadRootParts) return { ...index, root: materializedRoot };
  const { index: _indexFlag, ...rest } = index;
  return { ...rest, root: materializedRoot };
}

export function loadArchitectureTree(repoRoot, mapPath) {
  const resolvedRepo = resolve(repoRoot);
  const indexAbs = resolve(resolvedRepo, mapPath);
  let index;
  try {
    index = JSON.parse(readFileSync(indexAbs, "utf8"));
  } catch (error) {
    if (error?.code === "ENOENT") throw new Error(`architecture tree index "${displayPath(mapPath)}": not found`);
    if (error instanceof SyntaxError) throw new Error(`architecture tree index "${displayPath(mapPath)}": malformed JSON: ${error.message}`);
    throw error;
  }
  return mergeArchitectureTree(
    index,
    (resolvedPath) => {
      const absolute = resolve(resolvedRepo, resolvedPath);
      const outside = relative(resolvedRepo, absolute);
      if (outside === ".." || outside.startsWith(`..${sep}`) || isAbsolute(outside)) {
        throw new Error(`resolved path "${displayPath(resolvedPath)}" escapes repository root`);
      }
      if (!existsSync(absolute)) {
        const error = new Error("not found");
        error.code = "ENOENT";
        throw error;
      }
      return JSON.parse(readFileSync(absolute, "utf8"));
    },
    { indexPath: normalize(mapPath) },
  );
}
