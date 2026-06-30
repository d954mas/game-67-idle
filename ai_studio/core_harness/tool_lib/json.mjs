// Tiny JSON I/O leaf for one-shot tools. Pure (node builtins only). The per-tool
// copies closed over a module-level root/fail; here root + onError are
// PARAMETERS so the lib stays dependency-free. The stdout "print JSON" helper
// (taskboard) and the trivial test-file readers are intentionally NOT folded in
// — they are a different contract, not this one.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

// Parse a JSON file. On read/parse error, call onError(message) if provided
// (e.g. a tool's fail(), which exits the process); otherwise rethrow. The caller
// resolves the path first if it needs cwd/root resolution.
export function readJson(path, onError) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch (error) {
    if (onError) return onError(`cannot read JSON ${path}: ${error.message}`);
    throw error;
  }
}

// Write data as pretty JSON (+ trailing newline) to root/path, refusing to
// overwrite an existing file (calls onError, or throws). dryRun prints the
// intended write instead of touching disk. Creates parent dirs. This is the
// scaffolder file writer.
export function writeJsonFile(path, data, { root = process.cwd(), onError, dryRun = false } = {}) {
  const fullPath = join(root, path);
  if (existsSync(fullPath)) {
    const message = `refusing to overwrite existing file: ${path}`;
    if (onError) return onError(message);
    throw new Error(message);
  }
  const text = `${JSON.stringify(data, null, 2)}\n`;
  if (dryRun) {
    console.log(`would write ${path}`);
    console.log(text);
    return;
  }
  mkdirSync(dirname(fullPath), { recursive: true });
  writeFileSync(fullPath, text, "utf8");
  console.log(`wrote ${path}`);
}
