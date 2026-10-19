import { existsSync } from "node:fs";
import { join } from "node:path";

// Games created before ai_studio/game_tools carry a full copy of the CLI in
// tools/; their packager is the marker. Such a game keeps using its own copy
// until it is ported to the shim.
export function hasCopiedGameTooling(gameDir) {
  return existsSync(join(gameDir, "tools", "package_web.mjs"));
}
