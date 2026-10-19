import { existsSync, realpathSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

// The Studio this module lives in. The CLI compiles, fingerprints and proves
// against one Studio, so every root it uses must be this one.
export const OWN_STUDIO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");

const hasEngine = (root) => {
  const engine = join(root, "external", "neotolis-engine");
  return existsSync(engine) && statSync(engine).isDirectory();
};
const same = (left, right) => realpathSync(left).toLowerCase() === realpathSync(right).toLowerCase();

// A game checked out outside the Studio names it with GAME_REPO_ROOT, as CMake
// does; otherwise the game sits two or three levels down. Either way the answer
// must be the Studio this CLI was loaded from, or the bytes built and the bytes
// proven would come from two different checkouts.
export function findStudioRoot(gameDir, { env = process.env, ownRoot = OWN_STUDIO_ROOT } = {}) {
  const explicit = env.GAME_REPO_ROOT;
  let found = "";
  if (explicit) {
    found = resolve(explicit);
    if (!hasEngine(found)) throw new Error(`GAME_REPO_ROOT '${found}' has no external/neotolis-engine`);
  } else {
    const gameRoot = resolve(gameDir);
    found = [resolve(gameRoot, "..", ".."), resolve(gameRoot, "..", "..", "..")].find(hasEngine) || "";
    if (!found) throw new Error(`cannot find Studio root for game directory: ${gameRoot}`);
  }
  if (!same(found, ownRoot)) {
    throw new Error(`Studio root ${found} is not the Studio this game CLI runs from (${ownRoot}); run the game's own tools/game.mjs`);
  }
  return ownRoot;
}
