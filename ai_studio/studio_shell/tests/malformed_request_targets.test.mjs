import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { existsSync, rmSync } from "node:fs";
import { createServer as createNetServer, createConnection } from "node:net";
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
  const address = server.address();
  assert(address && typeof address === "object");
  const { port } = address;
  await new Promise((resolveClose, reject) => server.close((error) => error ? reject(error) : resolveClose()));
  return port;
}

function waitForServer(child) {
  return new Promise((resolveReady, reject) => {
    let stdout = "";
    let stderr = "";
    const timeout = setTimeout(() => {
      reject(new Error(`studio shell startup timed out\nstdout: ${stdout}\nstderr: ${stderr}`));
    }, 10_000);

    const cleanup = () => {
      clearTimeout(timeout);
      child.stdout.off("data", onStdout);
      child.stderr.off("data", onStderr);
      child.off("exit", onExit);
    };
    const onStdout = (chunk) => {
      stdout += chunk.toString();
      if (stdout.includes("ai_studio: http://127.0.0.1:")) {
        cleanup();
        resolveReady();
      }
    };
    const onStderr = (chunk) => {
      stderr += chunk.toString();
    };
    const onExit = (code, signal) => {
      cleanup();
      reject(new Error(`studio shell exited during startup: code=${code} signal=${signal}\nstderr: ${stderr}`));
    };

    child.stdout.on("data", onStdout);
    child.stderr.on("data", onStderr);
    child.once("exit", onExit);
  });
}

function rawGet(port, target) {
  return new Promise((resolveResponse, reject) => {
    const socket = createConnection({ host: "127.0.0.1", port });
    let response = "";
    socket.setEncoding("utf8");
    socket.setTimeout(5_000, () => socket.destroy(new Error(`request timed out: ${target}`)));
    socket.on("connect", () => {
      socket.write(`GET ${target} HTTP/1.1\r\nHost: 127.0.0.1:${port}\r\nConnection: close\r\n\r\n`);
    });
    socket.on("data", (chunk) => {
      response += chunk;
    });
    socket.once("error", reject);
    socket.once("close", () => resolveResponse(response));
  });
}

function parseResponse(raw) {
  assert.equal(raw.match(/HTTP\/1\.1/g)?.length, 1, "expected exactly one HTTP response");
  const boundary = raw.indexOf("\r\n\r\n");
  assert.notEqual(boundary, -1, `expected an HTTP response, received: ${JSON.stringify(raw)}`);
  const head = raw.slice(0, boundary);
  const body = raw.slice(boundary + 4);
  const statusMatch = /^HTTP\/1\.1 (\d{3})\b/.exec(head);
  assert(statusMatch, `missing HTTP status line: ${JSON.stringify(head)}`);
  return { status: Number(statusMatch[1]), head, body };
}

async function stopServer(child, pidFile) {
  if (child.exitCode === null && child.signalCode === null) {
    const exited = new Promise((resolveExit) => child.once("exit", resolveExit));
    child.kill();
    await exited;
  }
  if (existsSync(pidFile)) rmSync(pidFile);
}

test("malformed percent-encoded request targets return 400 without killing studio shell", async () => {
  const port = await reservePort();
  const pidFile = join(repoRoot, "tmp", "ai_studio", `studio_shell_${port}.pid`);
  const child = spawn(process.execPath, [serverPath, String(port)], {
    cwd: repoRoot,
    stdio: ["ignore", "pipe", "pipe"],
    windowsHide: true,
  });

  try {
    await waitForServer(child);

    const bodies = [];
    for (const target of ["/%", "/%A", "/%C0%AF"]) {
      const response = parseResponse(await rawGet(port, target));
      assert.equal(response.status, 400, `unexpected status for ${target}`);
      assert.equal(response.body, "bad request");
      assert.match(response.head.toLowerCase(), /content-type: text\/plain; charset=utf-8/);
      assert(!response.body.includes(target), `error body echoed raw target ${target}`);
      bodies.push(response.body);
      assert.equal(child.exitCode, null, `studio shell exited after ${target}`);
    }
    assert.equal(new Set(bodies).size, 1, "malformed targets must return a deterministic body");

    const rootResponse = parseResponse(await rawGet(port, "/"));
    assert.equal(rootResponse.status, 200);
    const encodedStaticResponse = parseResponse(await rawGet(port, "/ai_studio/studio_shell/index%2Ehtml"));
    assert.equal(encodedStaticResponse.status, 200, "valid encoded static paths must keep working");
    for (const target of ["/asset_viewer/studio_env.hdr", "/viewer/studio_env.hdr"]) {
      const previewResponse = parseResponse(await rawGet(port, target));
      assert.equal(previewResponse.status, 200, `${target} must use the owned assets/previews store`);
      assert.ok(previewResponse.body.length > 0, `${target} must serve the committed HDR`);
    }
    const literalPercentResponse = parseResponse(await rawGet(port, "/%25"));
    assert.equal(literalPercentResponse.status, 404, "valid percent encoding must not be rejected as malformed");
    assert.equal(child.exitCode, null, "the same studio shell process must remain alive");
  } finally {
    await stopServer(child, pidFile);
  }
});
