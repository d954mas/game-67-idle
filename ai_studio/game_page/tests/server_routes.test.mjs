// HTTP seam: the game-page routes served by the real Studio Shell server.
// Assertions stay generic (schemas, statuses) so no private game token is
// hard-coded in this public test file.
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { existsSync, rmSync } from "node:fs";
import { createServer as createNetServer } from "node:net";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const testDir = fileURLToPath(new URL(".", import.meta.url));
const repoRoot = resolve(testDir, "../../..");
const serverPath = join(repoRoot, "ai_studio", "studio_shell", "server.mjs");

async function reservePort() {
  const server = createNetServer();
  await new Promise((resolveListen, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolveListen);
  });
  const { port } = server.address();
  await new Promise((resolveClose, reject) => server.close((error) => error ? reject(error) : resolveClose()));
  return port;
}

function waitForServer(child) {
  return new Promise((resolveReady, reject) => {
    let output = "";
    const timeout = setTimeout(() => reject(new Error(`studio shell startup timed out: ${output}`)), 10_000);
    const onData = (chunk) => {
      output += chunk.toString();
      if (output.includes("ai_studio: http://127.0.0.1:")) {
        clearTimeout(timeout);
        resolveReady();
      }
    };
    child.stdout.on("data", onData);
    child.stderr.on("data", onData);
    child.once("exit", (code) => {
      clearTimeout(timeout);
      reject(new Error(`studio shell exited during startup: code=${code}\n${output}`));
    });
  });
}

test("game page routes serve the games list, page html, and confined game files", async () => {
  const port = await reservePort();
  const pidFile = join(repoRoot, "tmp", "ai_studio", `studio_shell_${port}.pid`);
  const child = spawn(process.execPath, [serverPath, String(port)], {
    cwd: repoRoot,
    stdio: ["ignore", "pipe", "pipe"],
    windowsHide: true,
  });
  const base = `http://127.0.0.1:${port}`;

  try {
    await waitForServer(child);

    const gamesResponse = await fetch(`${base}/api/game-page/games`);
    assert.equal(gamesResponse.status, 200);
    const games = await gamesResponse.json();
    assert.equal(games.schema, "ai_studio.game_page.games.v1");
    assert.ok(Array.isArray(games.games) && games.games.length > 0, "repo must expose at least one game");

    const pickerResponse = await fetch(`${base}/games/`);
    assert.equal(pickerResponse.status, 200);
    assert.match(await pickerResponse.text(), /id="gamesList"/);

    const gameId = games.games[0].id;
    const pageResponse = await fetch(`${base}/game/${encodeURIComponent(gameId)}`);
    assert.equal(pageResponse.status, 200);
    assert.match(await pageResponse.text(), /game_page\/site\/game\.js/);

    const overviewResponse = await fetch(`${base}/api/game-page/overview?game=${encodeURIComponent(gameId)}`);
    assert.equal(overviewResponse.status, 200);
    const overview = await overviewResponse.json();
    assert.equal(overview.schema, "ai_studio.game_page.overview.v1");
    assert.equal(overview.game.id, gameId);

    const missingOverview = await fetch(`${base}/api/game-page/overview?game=definitely-not-a-game`);
    assert.equal(missingOverview.status, 404);

    const buildsResponse = await fetch(`${base}/api/game-page/builds?game=${encodeURIComponent(gameId)}`);
    assert.equal(buildsResponse.status, 200);
    const builds = await buildsResponse.json();
    assert.equal(builds.schema, "ai_studio.game_page.builds.v1");
    assert.ok(Array.isArray(builds.configs) && Array.isArray(builds.release));

    const identityResponse = await fetch(`${base}/game-file/${encodeURIComponent(gameId)}/game.json`);
    assert.equal(identityResponse.status, 200);
    assert.equal((await identityResponse.json()).id, gameId);

    const traversal = await fetch(`${base}/game-file/${encodeURIComponent(gameId)}/${encodeURIComponent("..")}/README.md`);
    assert.equal(traversal.status, 404, "game-file must stay confined to the game root");

    const unknownGameFile = await fetch(`${base}/game-file/definitely-not-a-game/game.json`);
    assert.equal(unknownGameFile.status, 404);

    // Executable types from inside a game folder must download, not render on
    // the studio origin.
    const htmlProbe = await fetch(`${base}/game-file/${encodeURIComponent(gameId)}/web/index.html.in`).then(async (r) => r.status === 404
      ? null
      : r.headers.get("content-type"));
    if (htmlProbe != null) assert.equal(htmlProbe, "application/octet-stream");
  } finally {
    if (child.exitCode === null && child.signalCode === null) {
      const exited = new Promise((resolveExit) => child.once("exit", resolveExit));
      child.kill();
      await exited;
    }
    if (existsSync(pidFile)) rmSync(pidFile);
  }
});
