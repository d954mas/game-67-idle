import { spawn } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { basename, extname, join, resolve } from "node:path";
import { inflateSync } from "node:zlib";

import { readStoreZip } from "./lib/zip_store.mjs";

const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const CONTENT_TYPES = Object.freeze({
  ".css": "text/css; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".ntpack": "application/octet-stream",
  ".wasm": "application/wasm",
});

const delay = (milliseconds) => new Promise((resolveDelay) => setTimeout(resolveDelay, milliseconds));
const debug = (message) => {
  if (process.env.BROWSER_SMOKE_DEBUG !== "1") return;
  console.error(`[browser-smoke] ${message}`);
};

function paeth(left, above, upperLeft) {
  const estimate = left + above - upperLeft;
  const leftDistance = Math.abs(estimate - left);
  const aboveDistance = Math.abs(estimate - above);
  const cornerDistance = Math.abs(estimate - upperLeft);
  return leftDistance <= aboveDistance && leftDistance <= cornerDistance ? left : aboveDistance <= cornerDistance ? above : upperLeft;
}

export function analyzePngFrame(input) {
  const png = Buffer.isBuffer(input) ? input : Buffer.from(input);
  if (png.length < 33 || !png.subarray(0, 8).equals(PNG_SIGNATURE)) throw new Error("browser screenshot is not a PNG");
  let cursor = 8;
  let width = 0;
  let height = 0;
  let bitDepth = 0;
  let colorType = -1;
  let interlace = -1;
  const compressed = [];
  while (cursor + 12 <= png.length) {
    const size = png.readUInt32BE(cursor);
    if (cursor + 12 + size > png.length) throw new Error("browser screenshot PNG is truncated");
    const type = png.subarray(cursor + 4, cursor + 8).toString("ascii");
    const data = png.subarray(cursor + 8, cursor + 8 + size);
    if (type === "IHDR") {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      bitDepth = data[8];
      colorType = data[9];
      interlace = data[12];
    } else if (type === "IDAT") compressed.push(data);
    else if (type === "IEND") break;
    cursor += 12 + size;
  }
  const bytesPerPixel = colorType === 6 ? 4 : colorType === 2 ? 3 : 0;
  if (!width || !height || bitDepth !== 8 || !bytesPerPixel || interlace !== 0 || compressed.length === 0) {
    throw new Error(`unsupported browser screenshot PNG (${width}x${height}, depth=${bitDepth}, color=${colorType}, interlace=${interlace})`);
  }
  const stride = width * bytesPerPixel;
  const raw = inflateSync(Buffer.concat(compressed));
  if (raw.length !== height * (stride + 1)) throw new Error("browser screenshot PNG scanline size mismatch");
  let offset = 0;
  let previous = Buffer.alloc(stride);
  let minLuma = 255;
  let maxLuma = 0;
  let darkPixels = 0;
  for (let y = 0; y < height; y += 1) {
    const filter = raw[offset++];
    if (filter > 4) throw new Error(`unsupported browser screenshot PNG filter: ${filter}`);
    const row = Buffer.alloc(stride);
    for (let x = 0; x < stride; x += 1) {
      const source = raw[offset++];
      const left = x >= bytesPerPixel ? row[x - bytesPerPixel] : 0;
      const above = previous[x];
      const upperLeft = x >= bytesPerPixel ? previous[x - bytesPerPixel] : 0;
      const predictor = filter === 0 ? 0
        : filter === 1 ? left
          : filter === 2 ? above
            : filter === 3 ? Math.floor((left + above) / 2)
              : paeth(left, above, upperLeft);
      row[x] = (source + predictor) & 0xff;
    }
    for (let x = 0; x < stride; x += bytesPerPixel) {
      const luma = Math.round((row[x] * 2126 + row[x + 1] * 7152 + row[x + 2] * 722) / 10000);
      minLuma = Math.min(minLuma, luma);
      maxLuma = Math.max(maxLuma, luma);
      if (luma <= 12) darkPixels += 1;
    }
    previous = row;
  }
  const darkRatio = darkPixels / (width * height);
  const lumaRange = maxLuma - minLuma;
  return { width, height, darkRatio, lumaRange, blank: darkRatio >= 0.999 || lumaRange < 8 };
}

function compact(value, limit = 180) {
  const oneLine = String(value || "unknown error").replace(/\s+/g, " ").trim();
  return oneLine.length <= limit ? oneLine : `${oneLine.slice(0, limit - 1)}…`;
}

export function assertBrowserSmokeResult({ issues = [], readiness, frame }) {
  if (issues.length) {
    const unique = [...new Map(issues.map((issue) => [`${issue.kind}:${issue.message}`, issue])).values()];
    throw new Error(`browser smoke failed: ${unique.slice(0, 3).map((issue) => `${issue.kind}: ${compact(issue.message)}`).join("; ")}`);
  }
  if (!readiness?.ready) {
    throw new Error(`browser smoke failed: runtime readiness was not reached (canvas=${readiness?.canvasWidth || 0}x${readiness?.canvasHeight || 0})`);
  }
  if (!frame) throw new Error("browser smoke failed: first-frame screenshot is missing");
  if (frame.blank) {
    throw new Error(`browser smoke failed: blank/black first frame (dark=${(frame.darkRatio * 100).toFixed(1)}%, range=${frame.lumaRange})`);
  }
  return `runtime ready; canvas ${readiness.canvasWidth}x${readiness.canvasHeight}; frame range ${frame.lumaRange}`;
}

export function canvasScreenshotClip(readiness) {
  const clip = {
    x: Number(readiness?.canvasX),
    y: Number(readiness?.canvasY),
    width: Number(readiness?.displayWidth),
    height: Number(readiness?.displayHeight),
    scale: 1,
  };
  if (![clip.x, clip.y, clip.width, clip.height].every(Number.isFinite)
      || clip.x < 0 || clip.y < 0 || clip.width <= 0 || clip.height <= 0) {
    throw new Error("browser smoke canvas clip is invalid");
  }
  return clip;
}

export async function startZipServer(zipPath) {
  const entries = readStoreZip(readFileSync(resolve(zipPath)));
  const server = createServer((request, response) => {
    if (request.method !== "GET" && request.method !== "HEAD") {
      response.writeHead(405, { Allow: "GET, HEAD" });
      response.end();
      return;
    }
    let path;
    try {
      const pathname = new URL(request.url || "/", "http://127.0.0.1").pathname;
      path = decodeURIComponent(pathname === "/" ? "index.html" : pathname.slice(1));
    } catch {
      response.writeHead(400);
      response.end();
      return;
    }
    const bytes = entries.get(path);
    if (!bytes) {
      response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
      response.end("not found\n");
      return;
    }
    response.writeHead(200, {
      "Cache-Control": "no-store",
      "Content-Length": bytes.length,
      "Content-Type": CONTENT_TYPES[extname(path).toLowerCase()] || "application/octet-stream",
    });
    response.end(request.method === "HEAD" ? undefined : bytes);
  });
  await new Promise((resolveListen, rejectListen) => {
    server.once("error", rejectListen);
    server.listen(0, "127.0.0.1", resolveListen);
  });
  const address = server.address();
  return {
    url: new URL(`http://127.0.0.1:${address.port}/index.html`),
    close: () => new Promise((resolveClose, rejectClose) => {
      server.close((error) => error ? rejectClose(error) : resolveClose());
      server.closeAllConnections();
    }),
  };
}

export function findChrome(explicit = process.env.CHROME_PATH, platform = process.platform, env = process.env) {
  const candidates = explicit ? [explicit] : platform === "win32" ? [
    env.PROGRAMFILES && join(env.PROGRAMFILES, "Google", "Chrome", "Application", "chrome.exe"),
    env["PROGRAMFILES(X86)"] && join(env["PROGRAMFILES(X86)"], "Google", "Chrome", "Application", "chrome.exe"),
    env.LOCALAPPDATA && join(env.LOCALAPPDATA, "Google", "Chrome", "Application", "chrome.exe"),
    env.PROGRAMFILES && join(env.PROGRAMFILES, "Microsoft", "Edge", "Application", "msedge.exe"),
  ] : platform === "darwin" ? [
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    "/Applications/Chromium.app/Contents/MacOS/Chromium",
  ] : [
    "/usr/bin/google-chrome",
    "/usr/bin/google-chrome-stable",
    "/usr/bin/chromium",
    "/usr/bin/chromium-browser",
  ];
  const found = candidates.filter(Boolean).map((candidate) => resolve(candidate)).find(existsSync);
  if (!found) throw new Error(explicit ? `browser smoke Chrome path does not exist: ${explicit}` : "browser smoke requires Chrome, Chromium, or Edge (set CHROME_PATH)");
  return found;
}

class CdpClient {
  constructor(url, onEvent) {
    this.url = url;
    this.onEvent = onEvent;
    this.nextId = 0;
    this.pending = new Map();
  }

  async connect() {
    await new Promise((resolveOpen, rejectOpen) => {
      this.socket = new WebSocket(this.url);
      this.socket.addEventListener("open", resolveOpen, { once: true });
      this.socket.addEventListener("error", () => rejectOpen(new Error("browser smoke CDP WebSocket failed to open")), { once: true });
      this.socket.addEventListener("message", (event) => this.receive(event.data));
    });
  }

  receive(raw) {
    let message;
    try { message = JSON.parse(String(raw)); } catch { return; }
    if (!message.id) {
      this.onEvent?.(message);
      return;
    }
    const pending = this.pending.get(message.id);
    if (!pending) return;
    clearTimeout(pending.timer);
    this.pending.delete(message.id);
    if (message.error) pending.reject(new Error(`CDP ${pending.method} failed: ${message.error.message || "unknown error"}`));
    else pending.resolve(message.result || {});
  }

  call(method, params = {}, timeoutMs = 15000) {
    const id = ++this.nextId;
    return new Promise((resolveCall, rejectCall) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        rejectCall(new Error(`CDP ${method} timed out`));
      }, timeoutMs);
      this.pending.set(id, { method, resolve: resolveCall, reject: rejectCall, timer });
      this.socket.send(JSON.stringify({ id, method, params }));
    });
  }

  close() {
    for (const pending of this.pending.values()) {
      clearTimeout(pending.timer);
      pending.reject(new Error("browser smoke CDP connection closed"));
    }
    this.pending.clear();
    this.socket?.close();
  }
}

export function readDevToolsPort(profileDir) {
  const portFile = join(profileDir, "DevToolsActivePort");
  try {
    if (!existsSync(portFile)) return 0;
    const port = Number(readFileSync(portFile, "utf8").split(/\r?\n/, 1)[0]);
    return Number.isInteger(port) && port > 0 && port <= 65535 ? port : 0;
  } catch {
    return 0;
  }
}

async function waitForDebugTarget(profileDir, browser, launchState, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  let discoveredPort = 0;
  while (Date.now() < deadline) {
    if (launchState.error) throw launchState.error;
    if (browser.exitCode !== null || browser.signalCode !== null) {
      throw new Error(`browser smoke browser exited ${browser.exitCode ?? browser.signalCode}`);
    }
    if (!discoveredPort) discoveredPort = readDevToolsPort(profileDir);
    if (discoveredPort) {
      try {
        const targets = await (await fetch(`http://127.0.0.1:${discoveredPort}/json/list`, { signal: AbortSignal.timeout(1000) })).json();
        const page = targets.find((target) => target.type === "page" && target.webSocketDebuggerUrl);
        if (page) return page.webSocketDebuggerUrl;
      } catch {
        // Chrome writes DevToolsActivePort before the discovery endpoint is ready.
      }
    }
    await delay(100);
  }
  throw new Error(`browser smoke browser did not expose a CDP page (${discoveredPort ? `port ${discoveredPort}` : "DevToolsActivePort missing"})`);
}

export function chromeLaunchArgs(profileDir) {
  return [
    "--headless=new",
    "--disable-dev-shm-usage",
    "--enable-unsafe-swiftshader",
    "--no-first-run",
    "--no-default-browser-check",
    "--remote-debugging-port=0",
    `--user-data-dir=${profileDir}`,
    "--use-gl=angle",
    "--use-angle=swiftshader",
    "--window-size=960,540",
    "about:blank",
  ];
}

export function normalizeBrowserEvent(message) {
  const params = message.params || {};
  if (message.method === "Runtime.exceptionThrown") {
    return { kind: "page", message: params.exceptionDetails?.exception?.description || params.exceptionDetails?.text || "uncaught exception" };
  }
  if (message.method === "Runtime.consoleAPICalled" && params.type === "error") {
    const text = (params.args || []).map((arg) => arg.value ?? arg.description ?? arg.type).join(" ");
    return { kind: "console", message: text || "console.error" };
  }
  if (message.method === "Log.entryAdded" && params.entry?.level === "error") {
    return { kind: "console", message: params.entry.text || params.entry.url || "browser log error" };
  }
  if (message.method === "Network.responseReceived" && params.response?.status >= 400) {
    return { kind: "resource", message: `${params.response.status} ${params.response.url || "resource"}` };
  }
  if (message.method === "Network.loadingFailed" && !params.canceled) {
    return { kind: "resource", message: `${params.errorText || "load failed"} ${params.blockedReason || ""}`.trim() };
  }
  return null;
}

async function readReadiness(client) {
  const result = await client.call("Runtime.evaluate", {
    expression: `(() => {
      const overlay = document.getElementById("loading-overlay");
      const canvas = document.getElementById("canvas");
      const rect = canvas ? canvas.getBoundingClientRect() : { width: 0, height: 0 };
      return {
        ready: document.readyState === "complete" && !!overlay && getComputedStyle(overlay).display === "none"
          && !!canvas && canvas.width > 0 && canvas.height > 0 && rect.width > 0 && rect.height > 0,
        canvasWidth: canvas ? canvas.width : 0,
        canvasHeight: canvas ? canvas.height : 0,
        canvasX: rect.x + window.scrollX,
        canvasY: rect.y + window.scrollY,
        displayWidth: rect.width,
        displayHeight: rect.height
      };
    })()`,
    returnByValue: true,
  });
  return result.result?.value || { ready: false, canvasWidth: 0, canvasHeight: 0 };
}

export async function smokePackagedWeb({ zipPath, chromePath, timeoutMs = 45000 } = {}) {
  if (!zipPath) throw new Error("browser smoke requires a packaged ZIP path");
  const server = await startZipServer(zipPath);
  debug(`serving reopened ${basename(zipPath)} at ${server.url.href}`);
  let profileDir = "";
  let browser;
  let launchState;
  const issues = [];
  let client;
  try {
    profileDir = mkdtempSync(join(tmpdir(), "game-browser-smoke-"));
    debug("temporary browser profile created");
    const browserPath = findChrome(chromePath);
    debug(`launching ${basename(browserPath)}`);
    browser = spawn(browserPath, chromeLaunchArgs(profileDir), { stdio: "ignore", windowsHide: true });
    launchState = { error: null };
    browser.once("error", (error) => { launchState.error = error; });
    debug("browser process launched with Chrome-owned CDP port");
    const target = await waitForDebugTarget(profileDir, browser, launchState, Math.min(timeoutMs, 30000));
    debug("browser CDP target ready");
    client = new CdpClient(target, (message) => {
      const issue = normalizeBrowserEvent(message);
      if (issue) issues.push(issue);
    });
    await client.connect();
    for (const domain of ["Page", "Runtime", "Network", "Log"]) await client.call(`${domain}.enable`);
    debug("browser diagnostic domains enabled");
    const navigation = await client.call("Page.navigate", { url: server.url.href });
    debug("package entrypoint navigation started");
    if (navigation.errorText) issues.push({ kind: "page", message: navigation.errorText });

    const deadline = Date.now() + timeoutMs;
    let readiness = { ready: false, canvasWidth: 0, canvasHeight: 0 };
    while (Date.now() < deadline) {
      readiness = await readReadiness(client);
      if (readiness.ready) break;
      await delay(200);
    }
    debug(`readiness=${readiness.ready} canvas=${readiness.canvasWidth}x${readiness.canvasHeight} issues=${issues.length}`);
    let frame = null;
    if (readiness.ready) {
      await client.call("Runtime.evaluate", {
        expression: "new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))",
        awaitPromise: true,
      });
      const screenshot = await client.call("Page.captureScreenshot", {
        format: "png",
        fromSurface: true,
        captureBeyondViewport: false,
        clip: canvasScreenshotClip(readiness),
      });
      frame = analyzePngFrame(Buffer.from(screenshot.data || "", "base64"));
      debug(`frame dark=${(frame.darkRatio * 100).toFixed(1)}% range=${frame.lumaRange}`);
      await delay(200);
    }
    return { summary: assertBrowserSmokeResult({ issues, readiness, frame }), readiness, frame };
  } finally {
    debug("cleanup started");
    if (client) {
      try { await client.call("Browser.close", {}, 2000); } catch { /* process cleanup below */ }
      client.close();
    }
    debug("browser close requested");
    if (browser?.exitCode === null && browser?.signalCode === null) {
      await Promise.race([new Promise((resolveExit) => browser.once("exit", resolveExit)), delay(2000)]);
    }
    if (browser?.exitCode === null && browser?.signalCode === null) {
      browser.kill();
      await Promise.race([new Promise((resolveExit) => browser.once("exit", resolveExit)), delay(2000)]);
    }
    browser?.unref();
    await server.close();
    debug("ZIP server closed");
    if (profileDir) {
      rmSync(profileDir, { recursive: true, force: true });
      debug("temporary browser profile removed");
    }
  }
}
