import { existsSync, statSync } from "node:fs";
import { join, resolve } from "node:path";

export function findStudioRoot(gameDir) {
  const gameRoot = resolve(gameDir);
  for (const candidate of [
    resolve(gameRoot, "..", ".."),
    resolve(gameRoot, "..", "..", ".."),
  ]) {
    const engine = join(candidate, "external", "neotolis-engine");
    if (existsSync(engine) && statSync(engine).isDirectory()) return candidate;
  }
  throw new Error(`cannot find Studio root for game directory: ${gameRoot}`);
}
