import assert from "node:assert/strict";
import { execFile, spawn } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { createServer as createNetServer } from "node:net";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { promisify } from "node:util";
import test from "node:test";

const execFileAsync = promisify(execFile);
const testDir = fileURLToPath(new URL(".", import.meta.url));
const repoRoot = resolve(testDir, "../../..");
const launcher = join(repoRoot, "ai_studio", "studio_shell", "start_site_windows.ps1");
const powershell = join(process.env.SystemRoot || "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");
const stateDir = join(repoRoot, "tmp", "ai_studio");

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

function pathsFor(port) {
  return {
    pid: join(stateDir, `studio_shell_${port}.pid`),
    legacyRunner: join(stateDir, `studio_shell_${port}.runner.ps1`),
    out: join(stateDir, `studio_shell_${port}.out.log`),
    err: join(stateDir, `studio_shell_${port}.err.log`),
  };
}

function runnerFiles(port) {
  if (!existsSync(stateDir)) return [];
  const prefix = `studio_shell_${port}.runner_`;
  return readdirSync(stateDir)
    .filter((name) => name.startsWith(prefix) && name.endsWith(".ps1"))
    .map((name) => join(stateDir, name));
}

function readPowerShellLog(path) {
  const bytes = readFileSync(path);
  return bytes.length >= 2 && bytes[0] === 0xff && bytes[1] === 0xfe
    ? bytes.subarray(2).toString("utf16le")
    : bytes.toString("utf8");
}

async function clearState(port, { keepLogs = false } = {}) {
  const state = pathsFor(port);
  const paths = [state.pid, state.legacyRunner, ...runnerFiles(port), ...(keepLogs ? [] : [state.out, state.err])];
  for (const path of paths) {
    for (let attempt = 0; attempt < 200; attempt += 1) {
      try {
        rmSync(path, { force: true });
        break;
      } catch (error) {
        if (attempt === 199) throw error;
        await new Promise((resolveWait) => setTimeout(resolveWait, 50));
      }
    }
  }
}

async function runLauncher(port, ...args) {
  return execFileAsync(powershell, [
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", launcher,
    "-Port", String(port),
    ...args,
  ], { cwd: repoRoot, timeout: 20_000, windowsHide: true });
}

async function runLauncherFailure(port, ...args) {
  try {
    await runLauncher(port, ...args);
  } catch (error) {
    return error;
  }
  assert.fail("launcher unexpectedly succeeded");
}

function isAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch {
    return false;
  }
}

async function waitForExit(pid) {
  const deadline = Date.now() + 5_000;
  while (isAlive(pid) && Date.now() < deadline) {
    await new Promise((resolveWait) => setTimeout(resolveWait, 50));
  }
  assert.equal(isAlive(pid), false, `PID ${pid} did not exit`);
}

async function killTree(pid) {
  if (!pid || !isAlive(pid)) return;
  try {
    process.kill(pid);
  } catch {
    // A process can exit between the liveness check and termination.
  }
  await waitForExit(pid);
}

async function spawnNode(code, ...args) {
  const child = spawn(process.execPath, ["-e", code, ...args], {
    cwd: repoRoot,
    stdio: ["ignore", "pipe", "pipe"],
    windowsHide: true,
  });
  let stdout = "";
  let stderr = "";
  child.stdout.on("data", (chunk) => { stdout += chunk; });
  child.stderr.on("data", (chunk) => { stderr += chunk; });
  const deadline = Date.now() + 5_000;
  while (!stdout.includes("READY") && child.exitCode === null && Date.now() < deadline) {
    await new Promise((resolveWait) => setTimeout(resolveWait, 25));
  }
  assert.equal(child.exitCode, null, stderr);
  assert.match(stdout, /READY/, `child did not become ready\nstdout: ${stdout}\nstderr: ${stderr}`);
  return child;
}

test("Windows launcher rejects ports outside 1..65535 before creating state", { skip: process.platform !== "win32" }, async () => {
  mkdirSync(stateDir, { recursive: true });
  await clearState(0);
  const error = await runLauncherFailure(0);
  assert.match(`${error.stdout || ""}\n${error.stderr || ""}`, /between 1 and 65535/i);
  assert.equal(existsSync(pathsFor(0).legacyRunner), false);
  assert.deepEqual(runnerFiles(0), []);
  assert.equal(existsSync(pathsFor(0).pid), false);
});

test("Windows launcher smoke covers start, reuse, restart, foreign port, stale foreign PID, and cleanup", { skip: process.platform !== "win32", timeout: 60_000 }, async () => {
  const port = await reservePort();
  const state = pathsFor(port);
  const occupiedPort = await reservePort();
  const occupiedState = pathsFor(occupiedPort);
  const stalePort = await reservePort();
  const staleState = pathsFor(stalePort);
  let studioPid = null;
  let foreignPortProcess = null;
  let stalePidProcess = null;
  await clearState(port);
  await clearState(occupiedPort);
  await clearState(stalePort);

  try {
    const started = await runLauncher(port);
    assert.match(started.stdout, /AI Studio started:/);
    studioPid = Number(readFileSync(state.pid, "utf8").trim());
    assert.equal(isAlive(studioPid), true);
    assert.equal(existsSync(state.out), true);
    assert.equal(existsSync(state.err), true);
    assert.equal(existsSync(state.legacyRunner), false);
    assert.equal(runnerFiles(port).length, 1);
    assert.match(readPowerShellLog(state.out), /ai_studio: http:\/\/127\.0\.0\.1:/);
    assert.match(await (await fetch(`http://127.0.0.1:${port}/`)).text(), /AI Studio/);

    const reused = await runLauncher(port);
    assert.match(reused.stdout, /AI Studio already running:/);
    assert.equal(Number(readFileSync(state.pid, "utf8").trim()), studioPid);

    const restarted = await runLauncher(port, "-Restart");
    assert.match(restarted.stdout, /AI Studio started:/);
    const restartedPid = Number(readFileSync(state.pid, "utf8").trim());
    assert.notEqual(restartedPid, studioPid);
    await waitForExit(studioPid);
    studioPid = restartedPid;

    const firstRestartRunner = runnerFiles(port);
    assert.equal(firstRestartRunner.length, 1);
    const restartedAgain = await runLauncher(port, "-Restart");
    assert.match(restartedAgain.stdout, /AI Studio started:/);
    const restartedAgainPid = Number(readFileSync(state.pid, "utf8").trim());
    assert.notEqual(restartedAgainPid, studioPid);
    await waitForExit(studioPid);
    studioPid = restartedAgainPid;
    const secondRestartRunner = runnerFiles(port);
    assert.equal(secondRestartRunner.length, 1);
    assert.notEqual(secondRestartRunner[0], firstRestartRunner[0], "each launch must own a unique runner file");
    await new Promise((resolveWait) => setTimeout(resolveWait, 250));
    assert.equal(isAlive(studioPid), true, "an old runner cleanup must not break the newest launch");

    await killTree(studioPid);
    studioPid = null;
    await clearState(port);
    assert.equal(existsSync(state.out), false);
    assert.equal(existsSync(state.err), false);

    foreignPortProcess = await spawnNode(`
      const { createServer } = require("node:http");
      createServer((_req, res) => res.end("AI Studio foreign server")).listen(Number(process.argv[1]), "127.0.0.1", () => console.log("READY"));
    `, String(occupiedPort));
    writeFileSync(occupiedState.out, "preserved failure stdout\n", "utf8");
    writeFileSync(occupiedState.err, "preserved failure stderr\n", "utf8");
    const occupied = await runLauncherFailure(occupiedPort);
    assert.match(`${occupied.stdout || ""}\n${occupied.stderr || ""}`, /another process|already in use|occupied/i);
    assert.equal(isAlive(foreignPortProcess.pid), true, "launcher killed the process occupying the port");
    assert.equal(existsSync(occupiedState.pid), false);
    assert.equal(existsSync(occupiedState.legacyRunner), false);
    assert.deepEqual(runnerFiles(occupiedPort), []);
    assert.match(readFileSync(occupiedState.out, "utf8"), /preserved failure stdout/);
    assert.match(readFileSync(occupiedState.err, "utf8"), /preserved failure stderr/);
    await killTree(foreignPortProcess.pid);
    foreignPortProcess = null;

    stalePidProcess = await spawnNode("console.log('READY'); setInterval(() => {}, 1000)");
    writeFileSync(staleState.pid, `${stalePidProcess.pid}\n`, "utf8");
    writeFileSync(staleState.legacyRunner, "stale runner\n", "utf8");
    const orphanRunner = join(stateDir, `studio_shell_${stalePort}.runner_orphan.ps1`);
    writeFileSync(orphanRunner, "orphan runner\n", "utf8");
    writeFileSync(staleState.out, "stale stdout\n", "utf8");
    writeFileSync(staleState.err, "stale stderr\n", "utf8");

    const recovered = await runLauncher(stalePort, "-Restart");
    assert.match(recovered.stdout, /AI Studio started:/);
    assert.equal(isAlive(stalePidProcess.pid), true, "launcher killed a foreign process referenced by stale PID state");
    studioPid = Number(readFileSync(staleState.pid, "utf8").trim());
    assert.notEqual(studioPid, stalePidProcess.pid);
    assert.equal(existsSync(orphanRunner), false);
    assert.equal(runnerFiles(stalePort).length, 1);
    assert.doesNotMatch(readFileSync(staleState.out, "utf8"), /stale stdout/);
    assert.doesNotMatch(readFileSync(staleState.err, "utf8"), /stale stderr/);
  } finally {
    await killTree(studioPid);
    await killTree(foreignPortProcess?.pid);
    await killTree(stalePidProcess?.pid);
    await clearState(port);
    await clearState(occupiedPort);
    await clearState(stalePort);
    assert.equal(existsSync(state.out), false);
    assert.equal(existsSync(state.err), false);
    assert.equal(existsSync(staleState.out), false);
    assert.equal(existsSync(staleState.err), false);
  }
});
