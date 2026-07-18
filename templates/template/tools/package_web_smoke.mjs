import { spawn, spawnSync } from "node:child_process";
import { createServer } from "node:http";
import {
  existsSync,
  mkdtempSync,
  readFileSync,
  rmSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { basename, join, resolve } from "node:path";
import { inflateSync } from "node:zlib";

import { readStoreZip } from "./lib/zip_store.mjs";

const SHA256 = /^[0-9a-f]{64}$/;
const LOCAL_HOSTS = new Set(["127.0.0.1", "localhost", "::1", "[::1]"]);
const CONTENT_TYPES = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".wasm", "application/wasm"],
  [".ntpack", "application/octet-stream"],
  [".png", "image/png"],
]);

function localUrl(raw, label) {
  const url = new URL(raw);
  if (url.protocol !== "http:" || !LOCAL_HOSTS.has(url.hostname.toLowerCase()) || url.username || url.password) {
    throw new Error(`${label} must stay on loopback HTTP`);
  }
  return url;
}

function remaining(deadline) {
  const value = deadline - Date.now();
  if (value <= 0) throw new Error("packaged web smoke timed out");
  return value;
}

function issueLabel(kind) {
  if (kind === "page.exception") return "page error";
  if (kind === "console.error" || kind === "log.error") return "console error";
  return "resource error";
}

export function assessPackagedWebObservation(observation, expectedRuntimeBuildFingerprint) {
  const failures = [];
  try { localUrl(observation?.finalUrl, "final page URL"); }
  catch { failures.push("final page URL left loopback"); }
  if (!Array.isArray(observation?.issues)) failures.push("browser error transcript is missing");
  else {
    for (const issue of observation.issues) {
      failures.push(`${issueLabel(issue?.kind)}: ${String(issue?.text || "unknown browser failure")}`);
    }
  }
  if (observation?.ready !== true) failures.push("runtime readiness was not reached");
  if (observation?.runtimeBuildFingerprint !== expectedRuntimeBuildFingerprint) {
    failures.push("page runtime build fingerprint does not match the package");
  }
  if (observation?.compiledRuntimeBuildFingerprint !== expectedRuntimeBuildFingerprint) {
    failures.push("executed WASM runtime build marker does not match the package");
  }
  if (!(observation?.canvas?.width > 0) || !(observation?.canvas?.height > 0)) {
    failures.push("render canvas has no visible area");
  }
  const frame = observation?.frame;
  if (!frame || !(frame.width > 0) || !(frame.height > 0)
      || !Number.isFinite(frame.minLuma) || !Number.isFinite(frame.maxLuma)
      || !Number.isFinite(frame.variance)) {
    failures.push("first frame evidence is missing");
  } else if (frame.maxLuma <= 12 && frame.variance <= 4) {
    failures.push("first frame is black");
  } else if (frame.maxLuma - frame.minLuma < 8 || frame.variance < 4) {
    failures.push("first frame is blank");
  }
  return failures;
}

function extension(path) {
  const index = path.lastIndexOf(".");
  return index >= 0 ? path.slice(index).toLowerCase() : "";
}

async function serveEntries(entries, entrypoint) {
  const server = createServer((request, response) => {
    let path = "";
    try {
      const url = new URL(request.url || "/", "http://127.0.0.1");
      path = decodeURIComponent(url.pathname).replace(/^\/+/, "");
    } catch {
      response.writeHead(400).end("bad request\n");
      return;
    }
    if (!path) path = entrypoint;
    const bytes = entries.get(path);
    if (!bytes || !["GET", "HEAD"].includes(request.method || "")) {
      response.writeHead(bytes ? 405 : 404, {
        "content-type": "text/plain; charset=utf-8",
        "cache-control": "no-store",
        "x-content-type-options": "nosniff",
      }).end(bytes ? "method not allowed\n" : "not found\n");
      return;
    }
    response.writeHead(200, {
      "content-type": CONTENT_TYPES.get(extension(path)) || "application/octet-stream",
      "content-length": bytes.length,
      "cache-control": "no-store",
      "x-content-type-options": "nosniff",
      "cross-origin-opener-policy": "same-origin",
      "cross-origin-resource-policy": "same-origin",
    });
    response.end(request.method === "HEAD" ? undefined : bytes);
  });
  await new Promise((resolveListen, rejectListen) => {
    server.once("error", rejectListen);
    server.listen(0, "127.0.0.1", resolveListen);
  });
  const address = server.address();
  return {
    url: `http://127.0.0.1:${address.port}/${entrypoint}`,
    async close() {
      server.closeAllConnections?.();
      await new Promise((resolveClose) => server.close(resolveClose));
    },
  };
}

function browserCandidates(env = process.env, platform = process.platform) {
  const candidates = [env.CHROME_PATH];
  if (platform === "win32") {
    for (const root of [env.PROGRAMFILES, env["PROGRAMFILES(X86)"], env.LOCALAPPDATA]) {
      if (root) {
        candidates.push(join(root, "Google", "Chrome", "Application", "chrome.exe"));
        candidates.push(join(root, "Chromium", "Application", "chrome.exe"));
      }
    }
  } else if (platform === "darwin") {
    candidates.push("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome");
    candidates.push("/Applications/Chromium.app/Contents/MacOS/Chromium");
  } else {
    candidates.push("/usr/bin/google-chrome", "/usr/bin/google-chrome-stable", "/usr/bin/chromium", "/usr/bin/chromium-browser");
  }
  return [...new Set(candidates.filter(Boolean).map((path) => resolve(path)))];
}

export function findSupportedBrowser(options = {}) {
  const candidates = options.chromePath ? [resolve(options.chromePath)] : browserCandidates(options.env, options.platform);
  const found = candidates.find((path) => existsSync(path));
  if (!found) throw new Error("supported Chrome/Chromium browser was not found (set CHROME_PATH)");
  return found;
}

export class CdpClient {
  constructor(socket) {
    this.socket = socket;
    this.nextId = 0;
    this.pending = new Map();
    this.listeners = new Map();
    this.sessionId = "";
    socket.addEventListener("message", (event) => this.message(event));
    socket.addEventListener("close", () => {
      for (const pending of this.pending.values()) pending.reject(new Error(`Chrome CDP connection closed during ${pending.method}`));
      this.pending.clear();
    });
  }

  async message(event) {
    let raw = event.data;
    if (typeof raw !== "string") raw = Buffer.from(await raw.arrayBuffer()).toString("utf8");
    const message = JSON.parse(raw);
    if (message.id) {
      const pending = this.pending.get(message.id);
      if (!pending) return;
      this.pending.delete(message.id);
      if (message.error) pending.reject(new Error(message.error.message));
      else pending.resolve(message.result);
      return;
    }
    for (const listener of this.listeners.get(message.method) || []) listener(message.params || {});
  }

  on(method, listener) {
    this.listeners.set(method, [...(this.listeners.get(method) || []), listener]);
  }

  call(method, params, deadline) {
    return new Promise((resolveCall, rejectCall) => {
      const id = ++this.nextId;
      const timer = setTimeout(() => {
        this.pending.delete(id);
        rejectCall(new Error(`${method} timed out`));
      }, remaining(deadline));
      this.pending.set(id, {
        method,
        resolve: (value) => { clearTimeout(timer); resolveCall(value); },
        reject: (error) => { clearTimeout(timer); rejectCall(error); },
      });
      try {
        this.socket.send(JSON.stringify({
          id, method, params: params || {}, ...(this.sessionId ? { sessionId: this.sessionId } : {}),
        }));
      } catch (error) {
        this.pending.delete(id);
        clearTimeout(timer);
        rejectCall(error);
      }
    });
  }

  close() { this.socket.close(); }
}

export class PipeTransport {
  constructor(input, output) {
    this.input = input;
    this.output = output;
    this.buffer = Buffer.alloc(0);
    this.listeners = new Map();
    this.closed = false;
    output.on("data", (bytes) => this.feed(bytes));
    input.on("error", () => this.fail());
    input.on("close", () => this.fail());
    output.on("error", () => this.fail());
    output.on("close", () => this.fail());
  }

  addEventListener(type, callback, options = {}) {
    this.listeners.set(type, [...(this.listeners.get(type) || []), { callback, once: options.once === true }]);
  }

  emit(type, event) {
    const listeners = this.listeners.get(type) || [];
    this.listeners.set(type, listeners.filter((listener) => !listener.once));
    for (const listener of listeners) listener.callback(event);
  }

  feed(bytes) {
    this.buffer = Buffer.concat([this.buffer, bytes]);
    for (let end = this.buffer.indexOf(0); end >= 0; end = this.buffer.indexOf(0)) {
      const message = this.buffer.subarray(0, end).toString("utf8");
      this.buffer = this.buffer.subarray(end + 1);
      if (message) this.emit("message", { data: message });
    }
  }

  fail() {
    if (this.closed) return;
    this.closed = true;
    this.emit("close", {});
  }

  send(message) {
    if (this.closed) throw new Error("Chrome CDP pipe is closed");
    this.input.write(`${message}\0`, (error) => { if (error) this.fail(); });
  }

  close() {
    if (this.closed) return;
    this.closed = true;
    this.input.end();
    this.output.destroy();
  }
}

function addIssue(issues, kind, text) {
  const issue = { kind, text: String(text || "unknown browser failure").slice(0, 500) };
  if (!issues.some((entry) => entry.kind === issue.kind && entry.text === issue.text)) issues.push(issue);
}

export function registerBrowserIssueCapture(client, issues) {
  client.on("Runtime.exceptionThrown", ({ exceptionDetails }) => addIssue(
    issues, "page.exception", exceptionDetails?.exception?.description || exceptionDetails?.text,
  ));
  client.on("Runtime.consoleAPICalled", ({ type, args }) => {
    if (type === "error") addIssue(issues, "console.error", (args || []).map((arg) => arg?.value ?? arg?.description ?? "error").join(" "));
  });
  client.on("Log.entryAdded", ({ entry }) => {
    if (entry?.level === "error") addIssue(issues, entry.source === "network" ? "resource.http" : "log.error", entry.text || entry.url);
  });
  client.on("Network.responseReceived", ({ response: resource }) => {
    if (resource?.status >= 400) addIssue(issues, "resource.http", `${resource.status} ${resource.url}`);
    if (remoteNetworkUrl(resource?.url)) addIssue(issues, "resource.remote", resource?.url);
  });
  client.on("Network.loadingFailed", ({ errorText, blockedReason, canceled }) => {
    if (canceled && !blockedReason && errorText === "net::ERR_ABORTED") return;
    addIssue(issues, "resource.load", blockedReason || errorText);
  });
  client.on("Network.requestWillBeSent", ({ request }) => {
    if (remoteNetworkUrl(request?.url)) addIssue(issues, "resource.remote", request?.url);
  });
  client.on("Network.webSocketCreated", ({ url }) => {
    if (remoteNetworkUrl(url)) addIssue(issues, "resource.remote", url);
  });
  client.on("Network.webTransportCreated", ({ url }) => {
    if (remoteNetworkUrl(url)) addIssue(issues, "resource.remote", url);
  });
  client.on("Network.directTCPSocketCreated", ({ remoteAddr }) => addIssue(
    issues, "resource.direct", `direct TCP socket: ${remoteAddr || "unknown"}`,
  ));
  client.on("Network.directUDPSocketCreated", ({ options }) => addIssue(
    issues, "resource.direct", `direct UDP socket: ${options?.remoteAddr || "unconnected"}`,
  ));
}

function remoteNetworkUrl(raw) {
  try {
    const url = new URL(raw);
    return ["http:", "https:", "ws:", "wss:"].includes(url.protocol) && !LOCAL_HOSTS.has(url.hostname.toLowerCase());
  } catch {
    return true;
  }
}

const PAGE_STATE = `(() => {
  const canvas = document.querySelector("canvas");
  const rect = canvas?.getBoundingClientRect();
  const overlay = document.getElementById("loading-overlay");
  const overlayHidden = !overlay || getComputedStyle(overlay).display === "none";
  const compiled = globalThis.__AI_STUDIO_RUNTIME_BUILD_FINGERPRINT__ || null;
  const configured = globalThis.__PLATFORM_SDK_CONFIG__?.runtimeBuildFingerprint || null;
  return {
    finalUrl: location.href,
    ready: document.readyState === "complete" && overlayHidden && Boolean(compiled) && compiled === configured,
    runtimeBuildFingerprint: configured,
    compiledRuntimeBuildFingerprint: compiled,
    canvas: { x: rect?.x || 0, y: rect?.y || 0, width: rect?.width || 0, height: rect?.height || 0 }
  };
})()`;

async function closeChild(child, deadline) {
  const exited = () => !child || child.exitCode !== null || child.signalCode !== null;
  if (exited()) return;
  const waitForExit = async (until) => new Promise((resolveExit) => {
    if (exited()) { resolveExit(); return; }
    const timer = setTimeout(resolveExit, Math.max(1, until - Date.now()));
    child.once("exit", () => { clearTimeout(timer); resolveExit(); });
  });
  await waitForExit(deadline);
  if (!exited()) {
    if (process.platform === "win32" && child.pid) {
      spawnSync("taskkill", ["/PID", String(child.pid), "/T", "/F"], {
        stdio: "ignore", windowsHide: true, timeout: 3000,
      });
    } else {
      try { process.kill(-child.pid, "SIGKILL"); }
      catch { child.kill("SIGKILL"); }
    }
    await waitForExit(Date.now() + 1000);
  }
}

async function removeBrowserProfile(profileDir) {
  let lastError = null;
  let stableAbsentChecks = 0;
  for (let attempt = 0; attempt < 50; attempt += 1) {
    if (existsSync(profileDir)) {
      stableAbsentChecks = 0;
      try {
        rmSync(profileDir, { recursive: true, force: true });
      } catch (error) {
        lastError = error;
        if (!["EBUSY", "ENOTEMPTY", "EPERM"].includes(error?.code)) throw error;
        if (process.platform === "win32") {
          spawnSync("powershell.exe", [
            "-NoProfile", "-NonInteractive", "-Command",
            "Remove-Item -LiteralPath $args[0] -Recurse -Force", profileDir,
          ], { stdio: "ignore", windowsHide: true, timeout: 5000 });
        }
      }
    } else {
      stableAbsentChecks += 1;
      if (stableAbsentChecks >= 10) return;
    }
    await new Promise((resolveWait) => setTimeout(resolveWait, 100));
  }
  throw lastError || new Error(`browser profile kept reappearing after cleanup: ${profileDir}`);
}

function paeth(a, b, c) {
  const p = a + b - c;
  const pa = Math.abs(p - a);
  const pb = Math.abs(p - b);
  const pc = Math.abs(p - c);
  return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

export function analyzePngFrame(bytes) {
  const png = Buffer.isBuffer(bytes) ? bytes : Buffer.from(bytes);
  if (!png.subarray(0, 8).equals(Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]))) throw new Error("first-frame screenshot is not PNG");
  let width = 0;
  let height = 0;
  let colorType = -1;
  const idat = [];
  for (let offset = 8; offset + 12 <= png.length;) {
    const length = png.readUInt32BE(offset);
    const type = png.subarray(offset + 4, offset + 8).toString("ascii");
    if (offset + 12 + length > png.length) throw new Error("first-frame PNG is truncated");
    const data = png.subarray(offset + 8, offset + 8 + length);
    if (type === "IHDR") {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      if (data[8] !== 8 || data[10] !== 0 || data[11] !== 0 || data[12] !== 0) throw new Error("first-frame PNG format is unsupported");
      colorType = data[9];
    } else if (type === "IDAT") idat.push(data);
    offset += 12 + length;
    if (type === "IEND") break;
  }
  const channels = colorType === 6 ? 4 : colorType === 2 ? 3 : colorType === 0 ? 1 : 0;
  if (!channels || width < 1 || height < 1 || width * height > 20_000_000 || idat.length === 0) {
    throw new Error("first-frame PNG dimensions or color type are unsupported");
  }
  const stride = width * channels;
  const raw = inflateSync(Buffer.concat(idat));
  if (raw.length !== (stride + 1) * height) throw new Error("first-frame PNG scanlines are malformed");
  let previous = Buffer.alloc(stride);
  let cursor = 0;
  let minLuma = 255;
  let maxLuma = 0;
  let sum = 0;
  let sumSquares = 0;
  for (let y = 0; y < height; y += 1) {
    const filter = raw[cursor++];
    const row = Buffer.allocUnsafe(stride);
    for (let x = 0; x < stride; x += 1) {
      const source = raw[cursor++];
      const left = x >= channels ? row[x - channels] : 0;
      const up = previous[x] || 0;
      const upperLeft = x >= channels ? previous[x - channels] : 0;
      const predictor = filter === 0 ? 0 : filter === 1 ? left : filter === 2 ? up
        : filter === 3 ? Math.floor((left + up) / 2) : filter === 4 ? paeth(left, up, upperLeft) : -1;
      if (predictor < 0) throw new Error("first-frame PNG filter is unsupported");
      row[x] = (source + predictor) & 0xff;
    }
    for (let x = 0; x < stride; x += channels) {
      const red = row[x];
      const green = channels === 1 ? red : row[x + 1];
      const blue = channels === 1 ? red : row[x + 2];
      const luma = Math.round((red * 299 + green * 587 + blue * 114) / 1000);
      minLuma = Math.min(minLuma, luma);
      maxLuma = Math.max(maxLuma, luma);
      sum += luma;
      sumSquares += luma * luma;
    }
    previous = row;
  }
  const pixels = width * height;
  const mean = sum / pixels;
  return { width, height, minLuma, maxLuma, variance: Math.max(0, sumSquares / pixels - mean * mean) };
}

async function defaultBrowserProbe({ url, expectedRuntimeBuildFingerprint, chromePath, timeoutMs = 60000 }) {
  const deadline = Date.now() + timeoutMs;
  const browser = findSupportedBrowser({ chromePath });
  let profileDir = "";
  let child = null;
  let rootClient = null;
  let client = null;
  let targetId = "";
  let operationError = null;
  const issues = [];
  try {
    profileDir = mkdtempSync(join(tmpdir(), "ai-studio-package-smoke-"));
    child = spawn(browser, [
      "--headless=new",
      "--remote-debugging-pipe",
      `--user-data-dir=${profileDir}`,
      "--no-first-run",
      "--no-default-browser-check",
      "--disable-extensions",
      "--disable-component-extensions-with-background-pages",
      "--disable-default-apps",
      "--disable-breakpad",
      "--disable-background-networking",
      "--disable-component-update",
      "--disable-sync",
      "--no-sandbox",
      "--disable-gpu-sandbox",
      "--use-angle=swiftshader",
      "--metrics-recording-only",
      "--mute-audio",
      "about:blank",
    ], {
      stdio: ["ignore", "ignore", "ignore", "pipe", "pipe"],
      windowsHide: true,
      detached: process.platform !== "win32",
    });
    await new Promise((resolveSpawn, rejectSpawn) => {
      child.once("spawn", resolveSpawn);
      child.once("error", rejectSpawn);
    });
    if (!child.stdio[3] || !child.stdio[4]) throw new Error("headless browser CDP pipes are unavailable");
    rootClient = new CdpClient(new PipeTransport(child.stdio[3], child.stdio[4]));
    await rootClient.call("Target.setDiscoverTargets", { discover: true }, deadline);
    const created = await rootClient.call("Target.createTarget", { url: "about:blank" }, deadline);
    targetId = created.targetId || "";
    if (!targetId) throw new Error("Chrome did not create a page target");
    rootClient.on("Target.targetCrashed", ({ targetId: crashedId, status, errorCode }) => {
      if (crashedId === targetId) addIssue(issues, "page.exception", `browser target crashed: ${status || "unknown"} (${errorCode ?? "unknown"})`);
    });
    const attached = await rootClient.call("Target.attachToTarget", { targetId, flatten: true }, deadline);
    if (!attached.sessionId) throw new Error("Chrome did not attach the package smoke target");
    rootClient.sessionId = attached.sessionId;
    client = rootClient;
    registerBrowserIssueCapture(client, issues);
    await Promise.all([
      client.call("Page.enable", {}, deadline),
      client.call("Runtime.enable", {}, deadline),
      client.call("Log.enable", {}, deadline),
      client.call("Network.enable", {}, deadline),
      client.call("Emulation.setDeviceMetricsOverride", { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false }, deadline),
    ]);
    const navigation = await client.call("Page.navigate", { url: localUrl(url, "package page URL").href }, deadline);
    if (navigation.errorText) addIssue(issues, "resource.load", navigation.errorText);
    let state = null;
    while (Date.now() < deadline) {
      const evaluated = await client.call("Runtime.evaluate", { expression: PAGE_STATE, returnByValue: true }, deadline);
      state = evaluated.result?.value || null;
      if (issues.length > 0) break;
      if (state?.ready && state.compiledRuntimeBuildFingerprint === expectedRuntimeBuildFingerprint
          && state.canvas?.width > 0 && state.canvas?.height > 0) break;
      await new Promise((resolveWait) => setTimeout(resolveWait, Math.min(100, remaining(deadline))));
    }
    if (state?.ready && issues.length === 0) {
      await client.call("Runtime.evaluate", {
        expression: "new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)))",
        awaitPromise: true,
      }, deadline);
    }
    let frame = null;
    if (state?.canvas?.width > 0 && state?.canvas?.height > 0) {
      const screenshot = await client.call("Page.captureScreenshot", {
        format: "png",
        fromSurface: true,
        captureBeyondViewport: false,
        clip: {
          x: Math.max(0, state.canvas.x), y: Math.max(0, state.canvas.y),
          width: Math.min(1280, state.canvas.width), height: Math.min(720, state.canvas.height), scale: 1,
        },
      }, deadline);
      frame = analyzePngFrame(Buffer.from(screenshot.data, "base64"));
    }
    return { ...state, frame, issues: [...issues] };
  } catch (error) {
    operationError = error;
    throw error;
  } finally {
    if (targetId && rootClient && Date.now() < deadline) {
      try {
        rootClient.sessionId = "";
        await rootClient.call("Target.closeTarget", { targetId }, deadline);
      } catch {}
    }
    if (rootClient) {
      try { await rootClient.call("Browser.close", {}, Date.now() + 2000); } catch {}
    }
    rootClient?.close();
    if (child) await closeChild(child, Date.now() + 3000);
    if (profileDir) {
      try {
        await removeBrowserProfile(profileDir);
      } catch (error) {
        if (!operationError) throw error;
      }
    }
  }
}

export async function smokePackagedWebArtifact({
  zipPath,
  expectedTarget,
  chromePath,
  timeoutMs = 60000,
  probe = defaultBrowserProbe,
}) {
  const packagePath = resolve(zipPath);
  const entries = readStoreZip(readFileSync(packagePath));
  let release;
  try { release = JSON.parse(entries.get("release.json")?.toString("utf8") || "null"); }
  catch (error) { throw new Error(`reopened package release.json is invalid: ${error.message}`); }
  if (!release || release.schema !== "ai_studio.game.release.v2" || release.target !== expectedTarget
      || release.entrypoint !== "index.html" || !SHA256.test(release.runtimeBuildFingerprint || "")
      || !entries.has(release.entrypoint)) {
    throw new Error(`reopened package release contract is invalid: ${basename(packagePath)}`);
  }
  const server = await serveEntries(entries, release.entrypoint);
  try {
    const observation = await probe({
      url: server.url,
      expectedRuntimeBuildFingerprint: release.runtimeBuildFingerprint,
      chromePath,
      timeoutMs,
    });
    const failures = assessPackagedWebObservation(observation, release.runtimeBuildFingerprint);
    if (failures.length > 0) throw new Error(`packaged web smoke failed: ${failures.join("; ")}`);
    return observation;
  } finally {
    await server.close();
  }
}
