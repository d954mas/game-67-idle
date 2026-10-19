import { existsSync, realpathSync, statSync } from "node:fs";
import { isAbsolute, relative, resolve, sep } from "node:path";
import { pathToFileURL } from "node:url";

function fail(message) {
  process.stderr.write(`${message}\n`);
  process.exitCode = 2;
}

function inside(root, path) {
  const confined = relative(root, path);
  return confined !== "" && confined !== ".." && !confined.startsWith(`..${sep}`) && !isAbsolute(confined);
}

const [gameArgument, studioArgument] = process.argv.slice(2);
if (!gameArgument || !studioArgument || process.argv.length !== 4) {
  fail("usage: native_capture_fingerprint.mjs <game-root> <studio-root>");
} else {
  const gameRoot = realpathSync(resolve(gameArgument));
  const studioRoot = realpathSync(resolve(studioArgument));
  if (!inside(studioRoot, gameRoot)) {
    fail("game root must stay inside the Studio root");
  } else {
    // Games with copied tooling keep their own module; current games use the Studio's.
    const gameModule = resolve(gameRoot, "tools", "lib", "runtime_build.mjs");
    const ownModule = existsSync(gameModule);
    const modulePath = ownModule ? gameModule : resolve(studioRoot, "ai_studio", "game_tools", "lib", "runtime_build.mjs");
    if (!existsSync(modulePath) || !statSync(modulePath).isFile()) {
      fail(`runtime build module is missing: ${modulePath}`);
    } else if (ownModule && !inside(gameRoot, realpathSync(modulePath))) {
      fail("game-owned runtime build module escapes its game root");
    } else {
      const module = await import(pathToFileURL(modulePath).href);
      const createRecord = Object.hasOwn(module, "createRuntimeSourceRecord")
        ? module.createRuntimeSourceRecord : module.createRuntimeBuildRecord;
      if (typeof createRecord !== "function") {
        fail("game-owned runtime source record API is missing");
      } else {
        const record = createRecord({ gameDir: gameRoot, studioRoot });
        process.stdout.write(`${JSON.stringify(record)}\n`);
      }
    }
  }
}
