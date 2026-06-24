// Load the per-game art contract JSON: resolve, exists-check (fail via onError
// if required), parse (fail via onError on invalid). Returns the parsed contract,
// or null when absent and not required.
//
// Scope note: the path convention (gamedesign/projects/<id>/art/art_contract.json)
// and its token sanitization stay at each call site — the default token differs
// per tool ("gate" vs "game"), so they are not a shared contract; only this
// load-and-validate core (with identical error messages) is. IO leaf, kept apart
// from visual_axes (data) and llm-json (parse).
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";

export function loadArtContract(contractPath, { required = false, onError } = {}) {
  const fullPath = resolve(contractPath);
  if (!existsSync(fullPath)) {
    if (required && onError) onError(`art contract does not exist: ${contractPath}`);
    return null;
  }
  try {
    return JSON.parse(readFileSync(fullPath, "utf8"));
  } catch (error) {
    if (onError) onError(`art contract is not valid JSON: ${contractPath}: ${error.message}`);
    return null;
  }
}
