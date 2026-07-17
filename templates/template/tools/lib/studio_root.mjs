import { existsSync } from "node:fs";
import { join, resolve } from "node:path";

// Public games and templates sit at depth 2; private games sit at depth 3.
// Probe the engine checkout marker so copied tools work in either layout.
export function findStudioRoot(gameDir) {
  for (const levels of [["..", ".."], ["..", "..", ".."]]) {
    const candidate = resolve(gameDir, ...levels);
    if (existsSync(join(candidate, "external", "neotolis-engine"))) return candidate;
  }
  throw new Error(`cannot locate the studio repo root (external/neotolis-engine) above ${resolve(gameDir)}`);
}
