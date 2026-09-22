import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { existsSync, mkdtempSync, readFileSync } from "node:fs";
import { createServer } from "node:http";
import { spawn, spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const feature = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const emcc = process.env.EMCC || "C:/develop/emsdk/upstream/emscripten/emcc.py";
const chrome = process.env.CHROME_PATH || "C:/Program Files/Google/Chrome/Application/chrome.exe";

function frame(payload) { return Buffer.concat([Buffer.from([0x82, payload.length]), payload]); }
function build(dir) {
  const args = [emcc,
    join(feature, "tests/web_order_fixture.c"), join(feature, "src/net_ws_client_web.c"),
    join(feature, "src/net_codec.c"), "-I" + join(feature, "include"),
    "-I" + join(feature, "src"), "-sEXPORTED_FUNCTIONS=['_fixture_start','_fixture_service','_fixture_done','_fixture_passed']",
    "-sEXPORTED_RUNTIME_METHODS=['ccall']", "--no-entry", "-lwebsocket.js", "-o", join(dir, "web_order.js")];
  const result = spawnSync(process.env.PYTHON || "python", args, { encoding: "utf8", windowsHide: true });
  assert.equal(result.status, 0, result.stderr || result.stdout);
  assert.ok(existsSync(join(dir, "web_order.wasm")), result.stderr || result.stdout);
}

test("browser client drains 64 records before slow close", async () => {
  const dir = mkdtempSync(join(tmpdir(), "nt-web-order-"));
  build(dir);
  const server = createServer((request, response) => {
    const pathname = new URL(request.url, "http://127.0.0.1").pathname;
    const file = pathname === "/web_order.html" ? "tests/web_order.html" : pathname.slice(1);
    try { response.setHeader("Content-Type", file.endsWith(".wasm") ? "application/wasm" : file.endsWith(".js") ? "text/javascript" : "text/html"); response.end(readFileSync(file.startsWith("tests/") ? join(feature, file) : join(dir, file))); }
    catch { response.writeHead(404).end(); }
  });
  const sockets = new Set();
  server.on("upgrade", (request, socket) => {
    sockets.add(socket); socket.on("data", () => {}); socket.on("error", () => {}); socket.on("close", () => sockets.delete(socket));
    const accept = createHash("sha1").update(request.headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest("base64");
    socket.write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n");
    for (let index = 0; index < 65; index++) socket.write(frame(Buffer.from([1, index])));
    setTimeout(() => socket.end(Buffer.from([0x88, 2, 0x0f, 0xa6])), 100);
  });
  await new Promise(resolveListen => server.listen(0, "127.0.0.1", resolveListen));
  const port = server.address().port;
  const url = `http://127.0.0.1:${port}/web_order.html?ws=${encodeURIComponent(`ws://127.0.0.1:${port}/room`)}`;
  const profile = mkdtempSync(join(tmpdir(), "nt-web-order-profile-"));
  const browser = spawn(chrome, ["--headless=new", "--disable-gpu", "--no-first-run",
    "--disable-background-networking", "--remote-debugging-address=127.0.0.1", "--remote-debugging-port=0",
    `--user-data-dir=${profile}`, "about:blank"], { windowsHide: true, stdio: "ignore" });
  let ws;
  const pending = new Map();
  let nextId = 1;
  const pause = milliseconds => new Promise(resolvePause => setTimeout(resolvePause, milliseconds));
  try {
    let cdpPort;
    for (let attempt = 0; attempt < 100; ++attempt) {
      const active = join(profile, "DevToolsActivePort");
      if (existsSync(active)) { cdpPort = Number(readFileSync(active, "utf8").split(/\r?\n/)[0]); break; }
      await pause(100);
    }
    assert.ok(cdpPort > 0, "Chrome did not publish DevToolsActivePort");
    let target;
    for (let attempt = 0; attempt < 100; ++attempt) {
      const targets = await (await fetch(`http://127.0.0.1:${cdpPort}/json`)).json();
      target = targets.find(item => item.type === "page");
      if (target) break;
      await pause(100);
    }
    assert.ok(target, "Chrome did not expose a page target");
    ws = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((resolveOpen, rejectOpen) => { ws.onopen = resolveOpen; ws.onerror = rejectOpen; });
    ws.onmessage = event => {
      const message = JSON.parse(event.data);
      const request = pending.get(message.id);
      if (!request) return;
      pending.delete(message.id);
      message.error ? request.reject(new Error(JSON.stringify(message.error))) : request.resolve(message.result);
    };
    const call = (method, params = {}) => new Promise((resolveCall, rejectCall) => {
      const id = nextId++;
      pending.set(id, {resolve: resolveCall, reject: rejectCall});
      ws.send(JSON.stringify({id, method, params}));
    });
    const diagnostics=[];
    ws.addEventListener("message", event=>{const m=JSON.parse(event.data);if(m.method==="Runtime.exceptionThrown" || m.method==="Runtime.consoleAPICalled")diagnostics.push(m.params);});
    await call("Runtime.enable");
    await call("Page.navigate", {url});
    let result;
    for (let attempt = 0; attempt < 300; ++attempt) {
      result = (await call("Runtime.evaluate", {expression: "document.body.dataset.result", returnByValue: true})).result?.value;
      if (result === "pass" || result === "fail") break;
      await pause(100);
    }
    assert.equal(result, "pass", JSON.stringify(diagnostics).slice(0,3000));
    await call("Browser.close").catch(() => {});
  } finally {
    if (ws) ws.close();
    for (const request of pending.values()) request.reject(new Error("CDP closed"));
    if (browser.exitCode === null && browser.signalCode === null) {
      await new Promise(resolveExit => {browser.once("exit", resolveExit);browser.kill();});
    }
    for (const socket of sockets) socket.destroy();
    await new Promise(resolveClose => server.close(resolveClose));
  }
});
