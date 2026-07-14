import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { createServer as createNetServer } from "node:net";
import { join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import test from "node:test";

const testDir = fileURLToPath(new URL(".", import.meta.url));
const repoRoot = resolve(testDir, "../../..");
const serverUrl = pathToFileURL(join(repoRoot, "ai_studio", "studio_shell", "server.mjs")).href;

async function reservePort() {
  const server = createNetServer();
  await new Promise((resolveListen, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolveListen);
  });
  const address = server.address();
  assert(address && typeof address === "object");
  await new Promise((resolveClose, reject) => server.close((error) => error ? reject(error) : resolveClose()));
  return address.port;
}

async function runCleanShutdown({ replacementPid = null } = {}) {
  const port = await reservePort();
  const pidFile = join(repoRoot, "tmp", "ai_studio", `studio_shell_${port}.pid`);
  rmSync(pidFile, { force: true });
  const code = `
    import { existsSync, writeFileSync } from "node:fs";
    await import(process.argv[1]);
    const pidFile = process.argv[3];
    const replacementPid = process.argv[4];
    const deadline = Date.now() + 5000;
    while (!existsSync(pidFile) && Date.now() < deadline) {
      await new Promise((resolve) => setTimeout(resolve, 20));
    }
    if (replacementPid) writeFileSync(pidFile, replacementPid + "\\n", "utf8");
    process.emit("SIGTERM", "SIGTERM");
  `;
  const child = spawn(process.execPath, ["-e", code, serverUrl, String(port), pidFile, replacementPid === null ? "" : String(replacementPid)], {
    cwd: repoRoot,
    stdio: ["ignore", "pipe", "pipe"],
    windowsHide: true,
  });
  let stdout = "";
  let stderr = "";
  child.stdout.on("data", (chunk) => { stdout += chunk; });
  child.stderr.on("data", (chunk) => { stderr += chunk; });
  const exitCode = await new Promise((resolveExit, reject) => {
    const timer = setTimeout(() => {
      child.kill();
      reject(new Error(`server shutdown timed out\nstdout: ${stdout}\nstderr: ${stderr}`));
    }, 10_000);
    child.once("exit", (codeValue) => {
      clearTimeout(timer);
      resolveExit(codeValue);
    });
  });
  return { exitCode, pidFile, stdout, stderr };
}

test("foreground server removes its own PID file during clean shutdown", async () => {
  const result = await runCleanShutdown();
  assert.equal(result.exitCode, 0, result.stderr);
  assert.equal(existsSync(result.pidFile), false);
});

test("foreground server leaves a PID file that no longer belongs to it", async () => {
  const replacementPid = 424242;
  const result = await runCleanShutdown({ replacementPid });
  try {
    assert.equal(result.exitCode, 0, result.stderr);
    assert.equal(readFileSync(result.pidFile, "utf8").trim(), String(replacementPid));
  } finally {
    rmSync(result.pidFile, { force: true });
  }
});
