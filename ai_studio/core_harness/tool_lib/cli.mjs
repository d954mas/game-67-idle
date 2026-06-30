// Tiny CLI leaf — helpers shared by one-shot tool scripts.
//
// Dependency-free (node builtins only) and deliberately SEPARATE from the arg
// tokenizer in lib/args.mjs: importing fail() must never pull in the parser, so
// a tool that only needs fail/isMain stays minimal. The leak guard
// (ai_studio/assets/storage/license/restricted_assets_guard.mjs) intentionally keeps its own
// inline copies and never imports this module — its dependency surface stays at
// zero shared leaves.
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

// Print "error: <message>" to stderr and exit non-zero — the canonical
// one-shot-tool failure path, previously duplicated byte-for-byte across tools.
export function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

// True when the module at `metaUrl` (pass import.meta.url) is the process entry
// script — i.e. run as `node tool.mjs`, not imported. Lets a tool export its
// functions for tests/composition while still running main() when invoked
// directly.
export function isMain(metaUrl) {
  return Boolean(process.argv[1]) && metaUrl === pathToFileURL(resolve(process.argv[1])).href;
}
