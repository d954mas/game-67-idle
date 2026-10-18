import { existsSync, statSync } from "node:fs";
import { isAbsolute, relative, resolve, sep } from "node:path";
import { pathToFileURL } from "node:url";

function fail(message) {
  process.stderr.write(`${message}\n`);
  process.exitCode = 2;
}

const [gameArgument, studioArgument] = process.argv.slice(2);
if (!gameArgument || !studioArgument || process.argv.length !== 4) {
  fail("usage: native_capture_fingerprint.mjs <game-root> <studio-root>");
} else {
  const gameRoot = resolve(gameArgument);
  const studioRoot = resolve(studioArgument);
  const confined = relative(studioRoot, gameRoot);
  if (confined === "" || confined === ".." || confined.startsWith(`..${sep}`) || isAbsolute(confined)) {
    fail("game root must stay inside the Studio root");
  } else {
    const modulePath = resolve(gameRoot, "tools", "lib", "runtime_build.mjs");
    if (!existsSync(modulePath) || !statSync(modulePath).isFile()) {
      fail(`game-owned runtime build module is missing: ${modulePath}`);
    } else {
      const { createRuntimeBuildRecord } = await import(pathToFileURL(modulePath).href);
      const record = createRuntimeBuildRecord({ gameDir: gameRoot, studioRoot });
      process.stdout.write(`${JSON.stringify(record)}\n`);
    }
  }
}
