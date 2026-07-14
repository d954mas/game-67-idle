import { readFileSync } from "node:fs";
import { isAbsolute, relative, resolve, sep } from "node:path";

function displayPath(value) {
  return String(value).replaceAll("\\", "/");
}

function assertSingleFile(value, location = "root") {
  if (Array.isArray(value)) {
    value.forEach((item, index) => assertSingleFile(item, `${location}[${index}]`));
    return;
  }
  if (!value || typeof value !== "object") return;
  for (const field of ["parts", "ref"]) {
    if (Object.hasOwn(value, field)) {
      throw new Error(`architecture tree is single-file; ${field} is not allowed at ${location}`);
    }
  }
  for (const [key, field] of Object.entries(value)) {
    assertSingleFile(field, `${location}.${key}`);
  }
}

export function loadArchitectureTree(repoRoot, mapPath) {
  const resolvedRepo = resolve(repoRoot);
  if (typeof mapPath !== "string" || !mapPath.trim() || isAbsolute(mapPath) || /^[a-zA-Z]:[\\/]/.test(mapPath)) {
    throw new Error(`architecture tree "${displayPath(mapPath)}" must be repository-relative`);
  }
  const mapAbs = resolve(resolvedRepo, mapPath);
  const confined = relative(resolvedRepo, mapAbs);
  if (confined === ".." || confined.startsWith(`..${sep}`) || isAbsolute(confined)) {
    throw new Error(`architecture tree "${displayPath(mapPath)}" escapes repository root`);
  }
  let tree;
  try {
    tree = JSON.parse(readFileSync(mapAbs, "utf8"));
  } catch (error) {
    if (error?.code === "ENOENT") throw new Error(`architecture tree "${displayPath(mapPath)}": not found`);
    if (error instanceof SyntaxError) throw new Error(`architecture tree "${displayPath(mapPath)}": malformed JSON: ${error.message}`);
    throw error;
  }
  if (!tree || typeof tree !== "object" || Array.isArray(tree) || !tree.root) {
    throw new Error(`architecture tree "${displayPath(mapPath)}": expected an object with root`);
  }
  assertSingleFile(tree);
  return tree;
}
