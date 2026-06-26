// Stable launcher for the AI Studio browser site.
//
// Usage:
//   node ai_studio/studio_shell/start_site.mjs
//   node ai_studio/studio_shell/start_site.mjs --open
//   node ai_studio/studio_shell/start_site.mjs --port 8780
//   node ai_studio/studio_shell/start_site.mjs --restart

import { spawn } from "node:child_process";
import { closeSync, existsSync, mkdirSync, openSync, readFileSync, writeFileSync } from "node:fs";
import http from "node:http";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const scriptDir = dirname(fileURLToPath(import.meta.url));
const serverPath = join(scriptDir, "server.mjs");
const stateDir = join(repoRoot, ".tmp", "ai_studio");

const args = process.argv.slice(2);
const port = readPort(args);
const restart = args.includes("--restart");
const openBrowser = args.includes("--open");
const url = `http://127.0.0.1:${port}/`;
const pidFile = join(stateDir, `studio_shell_${port}.pid`);
const outLog = join(stateDir, `studio_shell_${port}.out.log`);
const errLog = join(stateDir, `studio_shell_${port}.err.log`);

mkdirSync(stateDir, { recursive: true });

if (restart) {
  stopRecordedProcess(pidFile);
}

const alreadyRunning = await readUrl(url);
if (alreadyRunning.ok) {
  if (!alreadyRunning.body.includes("AI Studio")) {
    fail(`Port ${port} is already used by another HTTP server. Choose another port with --port.`);
  }
  maybeOpen(url, openBrowser);
  console.log(`AI Studio already running: ${url}`);
  process.exit(0);
}

const outFd = openSync(outLog, "a");
const errFd = openSync(errLog, "a");
const child = spawn(process.execPath, [serverPath, String(port)], {
  cwd: repoRoot,
  detached: true,
  stdio: ["ignore", outFd, errFd],
  windowsHide: true,
});
child.unref();
closeSync(outFd);
closeSync(errFd);
writeFileSync(pidFile, `${child.pid}\n`, "utf8");

const started = await waitForServer(url, 5000);
if (!started.ok) {
  fail(
    [
      `AI Studio failed to start at ${url}.`,
      `PID file: ${pidFile}`,
      `stdout: ${outLog}`,
      `stderr: ${errLog}`,
      "",
      "stderr tail:",
      tail(errLog),
      "",
      "stdout tail:",
      tail(outLog),
    ].join("\n"),
  );
}

maybeOpen(url, openBrowser);
console.log(`AI Studio started: ${url}`);
console.log(`pid: ${child.pid}`);
console.log(`logs: ${outLog} | ${errLog}`);

function readPort(values) {
  const portIndex = values.indexOf("--port");
  const value = portIndex >= 0 ? values[portIndex + 1] : process.env.AI_STUDIO_PORT || "8765";
  const parsed = Number.parseInt(value, 10);
  if (!Number.isInteger(parsed) || parsed < 1 || parsed > 65535) {
    fail(`Invalid port: ${value}`);
  }
  return parsed;
}

function stopRecordedProcess(path) {
  if (!existsSync(path)) return;
  const pid = Number.parseInt(readFileSync(path, "utf8"), 10);
  if (!Number.isInteger(pid) || pid <= 0) return;
  try {
    process.kill(pid);
  } catch {
    // The recorded process is already gone. The new start attempt will verify the port.
  }
}

function readUrl(targetUrl, timeoutMs = 1000) {
  return new Promise((resolveResult) => {
    const req = http.get(targetUrl, { timeout: timeoutMs }, (res) => {
      let body = "";
      res.setEncoding("utf8");
      res.on("data", (chunk) => {
        body += chunk;
      });
      res.on("end", () => {
        resolveResult({ ok: res.statusCode >= 200 && res.statusCode < 500, status: res.statusCode, body });
      });
    });
    req.on("timeout", () => {
      req.destroy(new Error("timeout"));
    });
    req.on("error", (error) => {
      resolveResult({ ok: false, error });
    });
  });
}

async function waitForServer(targetUrl, timeoutMs) {
  const startedAt = Date.now();
  while (Date.now() - startedAt < timeoutMs) {
    const result = await readUrl(targetUrl, 600);
    if (result.ok && result.body.includes("AI Studio")) return result;
    await new Promise((resolveDelay) => setTimeout(resolveDelay, 200));
  }
  return { ok: false };
}

function maybeOpen(targetUrl, shouldOpen) {
  if (!shouldOpen) return;
  if (process.platform === "win32") {
    spawn("cmd", ["/c", "start", "", targetUrl], { detached: true, stdio: "ignore", windowsHide: true }).unref();
    return;
  }
  const opener = process.platform === "darwin" ? "open" : "xdg-open";
  spawn(opener, [targetUrl], { detached: true, stdio: "ignore" }).unref();
}

function tail(path, maxLines = 30) {
  if (!existsSync(path)) return "(missing)";
  const lines = readFileSync(path, "utf8").split(/\r?\n/).filter(Boolean);
  return lines.slice(-maxLines).join("\n") || "(empty)";
}

function fail(message) {
  console.error(message);
  process.exit(1);
}
