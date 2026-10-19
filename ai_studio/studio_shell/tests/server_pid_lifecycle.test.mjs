import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { EventEmitter } from "node:events";
import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { createServer as createNetServer } from "node:net";
import { join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import test from "node:test";

const testDir = fileURLToPath(new URL(".", import.meta.url));
const repoRoot = resolve(testDir, "../../..");
const serverUrl = pathToFileURL(join(repoRoot, "ai_studio", "studio_shell", "server.mjs")).href;
const configuredBudget = Number.parseInt(process.env.AI_STUDIO_SHELL_TEST_BUDGET_MS || "", 10);
const STARTUP_BUDGET_MS = Number.isSafeInteger(configuredBudget) && configuredBudget >= 5_000 && configuredBudget <= 30_000
  ? configuredBudget : 30_000;
const SHUTDOWN_BUDGET_MS = 10_000;

function waitForChildExit(child, {
  startupBudgetMs,
  shutdownBudgetMs,
  diagnostics,
}) {
  return new Promise((resolveExit, reject) => {
    let phase = "startup";
    let timer;
    const cleanup = () => {
      clearTimeout(timer);
      child.off("message", onMessage);
      child.off("exit", onExit);
      child.off("error", onError);
    };
    const rejectTimeout = () => {
      child.kill();
      cleanup();
      reject(new Error(`server ${phase} timed out\n${diagnostics()}`));
    };
    const armTimer = (timeoutMs) => {
      clearTimeout(timer);
      timer = setTimeout(rejectTimeout, timeoutMs);
    };
    const onMessage = (message) => {
      if (phase === "startup" && message === "shutdown-requested") {
        phase = "shutdown";
        armTimer(shutdownBudgetMs);
      }
    };
    const onExit = (codeValue) => {
      cleanup();
      if (phase === "startup") {
        reject(new Error(`server exited before requesting shutdown: code=${codeValue}\n${diagnostics()}`));
      } else {
        resolveExit(codeValue);
      }
    };
    const onError = (error) => {
      cleanup();
      reject(error);
    };
    child.once("message", onMessage);
    child.once("exit", onExit);
    child.once("error", onError);
    armTimer(startupBudgetMs);
  });
}

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

async function runCleanShutdown({
  replacementPid = null,
  startupBudgetMs = STARTUP_BUDGET_MS,
  shutdownBudgetMs = SHUTDOWN_BUDGET_MS,
} = {}) {
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
    if (!existsSync(pidFile)) throw new Error("studio shell did not create its PID file");
    if (replacementPid) writeFileSync(pidFile, replacementPid + "\\n", "utf8");
    process.send?.("shutdown-requested");
    process.emit("SIGTERM", "SIGTERM");
  `;
  const child = spawn(process.execPath, ["-e", code, serverUrl, String(port), pidFile, replacementPid === null ? "" : String(replacementPid)], {
    cwd: repoRoot,
    stdio: ["ignore", "pipe", "pipe", "ipc"],
    windowsHide: true,
  });
  let stdout = "";
  let stderr = "";
  child.stderr.on("data", (chunk) => { stderr += chunk; });
  child.stdout.on("data", (chunk) => { stdout += chunk; });
  const exitCode = await waitForChildExit(child, {
    startupBudgetMs,
    shutdownBudgetMs,
    diagnostics: () => `stdout: ${stdout}\nstderr: ${stderr}`,
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

test("shutdown deadline starts after cold server startup", async (t) => {
  t.mock.timers.enable({ apis: ["setTimeout"] });
  const child = new EventEmitter();
  let kills = 0;
  child.kill = () => { kills += 1; };
  const outcome = waitForChildExit(child, {
    startupBudgetMs: 3_000,
    shutdownBudgetMs: 250,
    diagnostics: () => "test diagnostics",
  }).then(
    () => "resolved",
    (error) => error,
  );

  t.mock.timers.tick(1_000);
  await Promise.resolve();
  assert.equal(kills, 0, "cold startup must not consume the shutdown deadline");

  child.emit("message", "shutdown-requested");
  t.mock.timers.tick(250);
  const error = await outcome;
  assert(error instanceof Error);
  assert.match(error.message, /server shutdown timed out/);
  assert.equal(kills, 1);
});
