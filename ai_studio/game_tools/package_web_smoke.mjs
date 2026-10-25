import { spawn, spawnSync } from "node:child_process";
import { createServer } from "node:http";
import {
  existsSync,
  readFileSync,
  rmSync,
} from "node:fs";
import { basename, isAbsolute, join, relative, resolve } from "node:path";
import { inflateSync } from "node:zlib";

import { readStoreZip } from "../core_harness/tool_lib/zip_store.mjs";
import { PROFILE_ROOT, createChromeProfile } from "../runtime_automation/chrome_profile.mjs";

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

export function smokeProbePlan(mode = "full") {
  if (mode === "quick") return { captureFrame: false, recoveryCycles: 0 };
  if (mode === "full") return { captureFrame: true, recoveryCycles: 2 };
  throw new Error(`unknown smoke mode: ${mode}`);
}

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

function validateFrameExpectation(value) {
  if (!["rendered", "blank"].includes(value)) throw new Error(`unknown frame expectation: ${value}`);
}

function assessRenderedFrame(failures, frame, label, frameExpectation) {
  if (!frame || !(frame.width > 0) || !(frame.height > 0)
      || !Number.isFinite(frame.minLuma) || !Number.isFinite(frame.maxLuma)
      || !Number.isFinite(frame.variance)) {
    failures.push(`${label} evidence is missing`);
  } else if (frameExpectation === "blank") {
    if (frame.maxLuma - frame.minLuma >= 8 || frame.variance >= 4) {
      failures.push(`${label} contains content in a blank foundation`);
    }
  } else if (frame.maxLuma <= 12 && frame.variance <= 4) {
    failures.push(`${label} is black`);
  } else if (frame.maxLuma - frame.minLuma < 8 || frame.variance < 4) {
    failures.push(`${label} is blank`);
  }
}

export function assessPackagedWebObservation(observation, expectedRuntimeBuildFingerprint, { mode = "full", frameExpectation = "rendered" } = {}) {
  validateFrameExpectation(frameExpectation);
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
  if (mode === "quick") return failures;
  assessRenderedFrame(failures, observation?.frame, "first frame", frameExpectation);
  const viewport = observation?.viewport;
  if (!viewport) {
    failures.push("viewport recovery evidence is missing");
  } else {
    if (viewport.collapsed?.width !== 1 || viewport.collapsed?.height !== 1) {
      failures.push("viewport did not collapse to 1x1");
    }
    if (viewport.collapsed?.ready !== true) failures.push("runtime was not ready in collapsed viewport");
    if (viewport.restored?.width !== 1280 || viewport.restored?.height !== 720) {
      failures.push("viewport was not restored to 1280x720");
    }
    if (viewport.restored?.ready !== true) failures.push("runtime was not ready after viewport restoration");
    if (!(viewport.restored?.canvas?.width > 0) || !(viewport.restored?.canvas?.height > 0)) {
      failures.push("restored viewport canvas has no visible area");
    }
    assessRenderedFrame(failures, viewport.frame, "restored viewport frame", frameExpectation);
  }
  const recovery = observation?.recovery;
  if (!recovery) {
    failures.push("WebGL recovery evidence is missing");
  } else {
    if (!(recovery.completedCycles >= 2)) failures.push("two consecutive WebGL recovery cycles were not completed");
    if (recovery.extensionAvailable !== true) failures.push("WEBGL_lose_context is unavailable");
    if (recovery.lostEvent !== true) failures.push("webglcontextlost was not observed");
    if (recovery.contextLostDuringLoss !== true) failures.push("WebGL context did not enter the lost state");
    if (recovery.overlayVisibleDuringLoss !== true) failures.push("recovery overlay was not shown during context loss");
    if (recovery.restoredEvent !== true) failures.push("webglcontextrestored was not observed");
    if (recovery.overlayVisibleAtRestore !== true) failures.push("recovery overlay cleared before restoration");
    if (recovery.contextRestored !== true) failures.push("WebGL context remained lost after restoration");
    if (recovery.overlayClearedAfterReadyFrame !== true) failures.push("runtime did not confirm a recovered frame");
    const cycles = Array.isArray(recovery.cycles) ? recovery.cycles : [];
    for (let index = 0; index < Math.min(2, cycles.length); index += 1) {
      if (cycles[index]?.runtimePresentedFrameAck !== true) {
        failures.push(`runtime did not acknowledge a presented frame after recovery cycle ${index + 1}`);
      }
      assessRenderedFrame(failures, cycles[index]?.frame, `recovered frame after cycle ${index + 1}`, frameExpectation);
    }
    if (recovery.failure) failures.push(`WebGL recovery probe failed: ${recovery.failure}`);
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

export function browserSandboxArgs(env = process.env) {
  return env.AI_STUDIO_CHROME_NO_SANDBOX === "1"
    ? ["--no-sandbox", "--disable-gpu-sandbox"]
    : [];
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
    return this.callInSession(method, params, deadline, this.sessionId);
  }

  callBrowser(method, params, deadline) {
    return this.callInSession(method, params, deadline, "");
  }

  callInSession(method, params, deadline, sessionId) {
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
          id, method, params: params || {}, ...(sessionId ? { sessionId } : {}),
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

// The cap fits an 8K UHD RGBA screenshot as base64 (~169 MiB) and bounds an
// unterminated pipe response.
const PIPE_MESSAGE_LIMIT = 256 * 1024 * 1024;

export class PipeTransport {
  constructor(input, output, { maxMessageBytes = PIPE_MESSAGE_LIMIT } = {}) {
    if (!Number.isSafeInteger(maxMessageBytes) || maxMessageBytes <= 0 || maxMessageBytes > PIPE_MESSAGE_LIMIT) {
      throw new Error(`Chrome CDP pipe message limit must be 1..${PIPE_MESSAGE_LIMIT} bytes`);
    }
    this.input = input;
    this.output = output;
    this.maxMessageBytes = maxMessageBytes;
    this.buffer = Buffer.allocUnsafe(Math.min(4096, maxMessageBytes));
    this.used = 0;
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

  append(bytes, start, end) {
    const required = this.used + end - start;
    if (required > this.maxMessageBytes) {
      this.fail();
      return false;
    }
    // Geometric growth keeps prefix copying linear across many pipe fragments.
    if (required > this.buffer.length) {
      let capacity = this.buffer.length;
      while (capacity < required) capacity = Math.min(capacity * 2, this.maxMessageBytes);
      const grown = Buffer.allocUnsafe(capacity);
      this.buffer.copy(grown, 0, 0, this.used);
      this.buffer = grown;
    }
    bytes.copy(this.buffer, this.used, start, end);
    this.used = required;
    return true;
  }

  feed(bytes) {
    for (let start = 0; start < bytes.length && !this.closed;) {
      const end = bytes.indexOf(0, start);
      if (!this.append(bytes, start, end < 0 ? bytes.length : end) || end < 0) return;
      const message = this.buffer.toString("utf8", 0, this.used);
      this.used = 0;
      start = end + 1;
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

// Site directory i is served at /i/, so arms built apart share one origin.
export async function serveSites(dirs, port = 0) {
  const roots = dirs.map((dir) => resolve(dir));
  const server = createServer((request, response) => {
    let bytes = null;
    let file = "";
    try {
      const [, index, ...rest] = decodeURIComponent(new URL(request.url || "/", "http://127.0.0.1").pathname).split("/");
      const root = /^\d+$/.test(index) ? roots[Number(index)] : undefined;
      const path = root && resolve(root, ...rest);
      const inside = path && relative(root, path);
      if (root && !inside.startsWith("..") && !isAbsolute(inside)) {
        file = path === root ? join(root, "index.html") : path;
        bytes = readFileSync(file);
      }
    } catch {
      bytes = null;
    }
    if (!bytes) {
      response.writeHead(404, { "cache-control": "no-store" }).end();
      return;
    }
    response.writeHead(200, {
      "content-type": CONTENT_TYPES.get(extension(file)) || "application/octet-stream",
      "content-length": bytes.length,
      "cache-control": "no-store",
    });
    response.end(bytes);
  });
  await new Promise((resolveListen, rejectListen) => {
    server.once("error", rejectListen);
    server.listen(port, "127.0.0.1", resolveListen);
  });
  return {
    origin: `http://127.0.0.1:${server.address().port}/`,
    async close() {
      server.closeAllConnections?.();
      await new Promise((resolveClose) => server.close(resolveClose));
    },
  };
}

// Headless Chrome with one blank page attached over a CDP pipe; `args` go
// before the page URL.
export async function launchHeadlessChrome({ args = [], deadline, profilePrefix = "ai-studio-chrome" }) {
  const browser = findSupportedBrowser();
  const profileDir = createChromeProfile(profilePrefix).dir;
  let child = null;
  let client = null;
  const close = async () => {
    try { client?.close(); } catch { /* the pipe may already be gone */ }
    if (child) await closeChild(child, Date.now() + 3000);
    await removeBrowserProfile(profileDir).catch(() => {});
  };
  try {
    child = spawn(browser, [
      "--headless=new",
      "--remote-debugging-pipe",
      `--user-data-dir=${profileDir}`,
      "--no-first-run",
      "--no-default-browser-check",
      "--disable-extensions",
      "--disable-background-networking",
      "--mute-audio",
      ...browserSandboxArgs(),
      ...args,
      "about:blank",
    ], {
      stdio: ["ignore", "ignore", "ignore", "pipe", "pipe"],
      windowsHide: true,
      detached: process.platform !== "win32",
    });
    client = new CdpClient(new PipeTransport(child.stdio[3], child.stdio[4]));
    await new Promise((resolveSpawn, rejectSpawn) => {
      child.once("spawn", resolveSpawn);
      child.once("error", rejectSpawn);
    });
    const { targetId } = await client.callBrowser("Target.createTarget", { url: "about:blank" }, deadline);
    client.sessionId = (await client.callBrowser("Target.attachToTarget", { targetId, flatten: true }, deadline)).sessionId;
  } catch (error) {
    await close();
    throw error;
  }
  return { client, close };
}

function addIssue(issues, kind, text) {
  const issue = { kind, text: String(text || "unknown browser failure").slice(0, 500) };
  if (!issues.some((entry) => entry.kind === issue.kind && entry.text === issue.text)) issues.push(issue);
}

function cdpLocation(url, lineNumber, columnNumber) {
  const line = Number.isInteger(lineNumber) ? lineNumber + 1 : "?";
  const column = Number.isInteger(columnNumber) ? columnNumber + 1 : "?";
  return `${url || "unknown URL"}:${line}:${column}`;
}

function compactStackFrames(callFrames, limit = 2) {
  return (callFrames || []).slice(0, limit).map((frame) => (
    `${frame?.functionName || "<anonymous>"}@${cdpLocation(frame?.url, frame?.lineNumber, frame?.columnNumber)}`
  )).join(" <- ");
}

function runtimeExceptionText(exceptionDetails) {
  const frames = exceptionDetails?.stackTrace?.callFrames || [];
  const firstFrame = frames[0];
  const message = String(
    exceptionDetails?.exception?.description || exceptionDetails?.text || "unknown browser failure",
  ).split(/\r?\n/, 1)[0];
  const location = cdpLocation(
    exceptionDetails?.url || firstFrame?.url,
    exceptionDetails?.lineNumber ?? firstFrame?.lineNumber,
    exceptionDetails?.columnNumber ?? firstFrame?.columnNumber,
  );
  const stack = compactStackFrames(frames);
  return `${message} at ${location}${stack ? `; stack=${stack}` : ""}`;
}

function initiatorProvenance(initiator) {
  if (!initiator) return "";
  const type = initiator.type || "other";
  const stack = compactStackFrames(initiator.stack?.callFrames);
  if (stack) return `${type} ${stack}`;
  if (initiator.url || Number.isInteger(initiator.lineNumber) || Number.isInteger(initiator.columnNumber)) {
    return `${type} ${cdpLocation(initiator.url, initiator.lineNumber, initiator.columnNumber)}`;
  }
  return type;
}

function loadingFailureText(failure, request) {
  const type = failure?.type || request?.type || "Other";
  const url = request?.url || "unknown URL";
  const details = [`${type} ${url} failed: ${failure?.errorText || "unknown network error"}`];
  if (failure?.blockedReason) details.push(`blocked=${failure.blockedReason}`);
  if (failure?.canceled) details.push("canceled=true");
  const initiator = initiatorProvenance(request?.initiator);
  if (initiator) details.push(`initiator=${initiator}`);
  return details.join("; ");
}

/* The engine's web HTTP backend logs every non-abort fetch failure without the URL. */
const ENGINE_HTTP_FETCH_FAILED = /^ERROR \[http\] fetch failed slot=\d+(\s|$)/;

/* An endpoint the build compiled in. Each base URL admits itself and the paths
   below it on the same origin, nothing else. */
export function remoteEndpointMatcher(allowedRemoteUrls = []) {
  const bases = allowedRemoteUrls.map((raw) => {
    let url;
    try { url = new URL(raw); } catch { throw new Error(`allowed remote endpoint is not a URL: ${raw}`); }
    if (!["http:", "https:"].includes(url.protocol) || LOCAL_HOSTS.has(url.hostname.toLowerCase())
        || url.username || url.password || url.search || url.hash) {
      throw new Error(`allowed remote endpoint must be a plain remote HTTP(S) base URL: ${raw}`);
    }
    return { origin: url.origin, path: url.pathname.replace(/\/+$/, "") };
  });
  const matches = (raw) => {
    let url;
    try { url = new URL(raw); } catch { return false; }
    return bases.some((base) => url.origin === base.origin && !url.username && !url.password
      && (url.pathname === base.path || url.pathname.startsWith(`${base.path}/`)));
  };
  // The base itself, the base with a query, and every path below it.
  matches.fetchPatterns = bases.flatMap((base) => {
    const escaped = `${base.origin}${base.path}`.replace(/[\\*?]/g, "\\$&");
    return [escaped, `${escaped}\\?*`, `${escaped}/*`];
  });
  return matches;
}

// policy.offlineRemote: a remoteEndpointMatcher whose requests the probe fails as
// if the player were offline. An engine fetch failure is excused only by a failed
// request to one of them; settle() reports the ones left unpaired.
export function registerBrowserIssueCapture(client, issues, allowedRemote = [], toleratedRemote = [], policy = {}) {
  const allowed = (url) => allowedRemote.some((pattern) => pattern.test(url || ""));
  const offline = policy.offlineRemote || null;
  const offlineRequestIds = new Set();
  let unexplainedOfflineFailures = 0;
  const unexplainedHttpErrors = [];
  // tolerated = the ad stack the platform SDK pulls: the smoke BLOCKS those
  // requests (adblock simulation) and their failures are expected, not issues
  const tolerated = (text) => toleratedRemote.some((pattern) => pattern.test(text || ""));
  const toleratedRequestIds = new Set();
  const requestMetadata = new Map();
  client.on("Runtime.exceptionThrown", ({ exceptionDetails }) => addIssue(
    issues, "page.exception", runtimeExceptionText(exceptionDetails),
  ));
  client.on("Runtime.consoleAPICalled", ({ type, args }) => {
    if (type !== "error") return;
    const text = (args || []).map((arg) => arg?.value ?? arg?.description ?? "error").join(" ");
    if (policy.consumeConsoleError?.(text) === true) return;
    if (tolerated(text)) return;
    if (offline && ENGINE_HTTP_FETCH_FAILED.test(text)) {
      if (unexplainedOfflineFailures > 0) unexplainedOfflineFailures -= 1;
      else unexplainedHttpErrors.push(text);
      return;
    }
    addIssue(issues, "console.error", text);
  });
  client.on("Log.entryAdded", ({ entry }) => {
    if (entry?.level !== "error") return;
    if (tolerated(entry?.text) || tolerated(entry?.url)) return;
    if (offline && entry.source === "network" && offline(entry.url)) return;
    addIssue(issues, entry.source === "network" ? "resource.http" : "log.error", entry.text || entry.url);
  });
  client.on("Network.responseReceived", ({ requestId, response: resource }) => {
    if (requestId && toleratedRequestIds.has(requestId)) return;
    // An endpoint the probe fails offline must never answer: if it did, it went
    // past the interception and reached the live network.
    if (requestId && offlineRequestIds.has(requestId)) {
      addIssue(issues, "resource.remote", `offline endpoint answered: ${resource?.url}`);
      return;
    }
    if (resource?.status >= 400) addIssue(issues, "resource.http", `${resource.status} ${resource.url}`);
    if (!allowed(resource?.url) && remoteNetworkUrl(resource?.url)) {
      addIssue(issues, "resource.remote", resource?.url);
    }
  });
  client.on("Network.loadingFailed", (failure) => {
    const request = failure?.requestId ? requestMetadata.get(failure.requestId) : undefined;
    const requestWasTolerated = failure?.requestId && toleratedRequestIds.has(failure.requestId);
    if (failure?.requestId && offlineRequestIds.delete(failure.requestId)) {
      requestMetadata.delete(failure.requestId);
      if (unexplainedHttpErrors.length > 0) unexplainedHttpErrors.shift();
      else unexplainedOfflineFailures += 1;
      return;
    }
    if (failure?.requestId) {
      requestMetadata.delete(failure.requestId);
      toleratedRequestIds.delete(failure.requestId);
    }
    if (requestWasTolerated) return;
    if (failure?.canceled && !failure?.blockedReason && failure?.errorText === "net::ERR_ABORTED") return;
    addIssue(issues, "resource.load", loadingFailureText(failure, request));
  });
  client.on("Network.loadingFinished", ({ requestId }) => {
    if (requestId) {
      requestMetadata.delete(requestId);
      toleratedRequestIds.delete(requestId);
      offlineRequestIds.delete(requestId);
    }
  });
  client.on("Network.requestWillBeSent", ({ requestId, request, type, initiator }) => {
    if (requestId) {
      requestMetadata.set(requestId, {
        url: request?.url,
        type,
        initiator,
      });
      toleratedRequestIds.delete(requestId);
    }
    // tolerated outranks allowed: an origin may be reachable (the platform CDN)
    // and still serve ad plumbing whose failure the game must survive
    if (tolerated(request?.url)) {
      if (requestId) toleratedRequestIds.add(requestId);
      return;
    }
    if (allowed(request?.url)) {
      return;
    }
    if (offline?.(request?.url)) {
      if (requestId) offlineRequestIds.add(requestId);
      return;
    }
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
  return {
    settle() {
      for (const text of unexplainedHttpErrors.splice(0)) addIssue(issues, "console.error", text);
    },
  };
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
  const overlay = document.getElementById("game-loading-overlay");
  const overlayHidden = !overlay || getComputedStyle(overlay).display === "none";
  const compiled = globalThis.__AI_STUDIO_RUNTIME_BUILD_FINGERPRINT__ || null;
  const configured = globalThis.__PLATFORM_SDK_CONFIG__?.runtimeBuildFingerprint || null;
  return {
    finalUrl: location.href,
    ready: document.readyState === "complete" && overlayHidden && Boolean(compiled) && compiled === configured,
    runtimeBuildFingerprint: configured,
    compiledRuntimeBuildFingerprint: compiled,
    viewport: { width: innerWidth, height: innerHeight },
    canvas: { x: rect?.x || 0, y: rect?.y || 0, width: rect?.width || 0, height: rect?.height || 0 }
  };
})()`;

const webglRecoveryScript = (timeoutMs) => `new Promise((resolve) => {
  const canvas = document.getElementById("canvas");
  const overlay = document.getElementById("runtime-overlay");
  const gl = canvas?.getContext("webgl2");
  const extension = gl?.getExtension("WEBGL_lose_context");
  const evidence = {
    extensionAvailable: Boolean(extension),
    lostEvent: false,
    contextLostDuringLoss: false,
    overlayVisibleDuringLoss: false,
    restoredEvent: false,
    overlayVisibleAtRestore: false,
    contextRestored: false,
    overlayClearedAfterReadyFrame: false,
    runtimePresentedFrameAck: false,
    failure: ""
  };
  const clearRuntimeRecovery = globalThis.__clearRuntimeRecovery;
  let wrappedClearRuntimeRecovery = null;
  let finished = false;
  let timeout = 0;
  const finish = () => {
    if (finished) return;
    finished = true;
    clearTimeout(timeout);
    if (wrappedClearRuntimeRecovery
        && globalThis.__clearRuntimeRecovery === wrappedClearRuntimeRecovery) {
      globalThis.__clearRuntimeRecovery = clearRuntimeRecovery;
    }
    resolve(evidence);
  };
  const fail = (error) => {
    evidence.failure = String(error?.message || error || "unknown recovery failure");
    finish();
  };
  if (!canvas || !overlay || !gl || !extension || typeof clearRuntimeRecovery !== "function") {
    if (typeof clearRuntimeRecovery !== "function") evidence.failure = "runtime recovery bridge is unavailable";
    finish();
    return;
  }
  wrappedClearRuntimeRecovery = (...args) => {
    evidence.runtimePresentedFrameAck = true;
    return Reflect.apply(clearRuntimeRecovery, globalThis, args);
  };
  globalThis.__clearRuntimeRecovery = wrappedClearRuntimeRecovery;
  timeout = setTimeout(() => fail("recovery timed out"), ${timeoutMs});
  canvas.addEventListener("webglcontextlost", () => {
    evidence.lostEvent = true;
    evidence.contextLostDuringLoss = gl.isContextLost();
    evidence.overlayVisibleDuringLoss = !overlay.hidden;
    setTimeout(() => {
      try { extension.restoreContext(); }
      catch (error) { fail(error); }
    }, 100);
  }, { once: true });
  canvas.addEventListener("webglcontextrestored", () => {
    evidence.restoredEvent = true;
    evidence.overlayVisibleAtRestore = !overlay.hidden;
    const waitForReadyFrame = () => {
      if (!overlay.hidden) {
        requestAnimationFrame(waitForReadyFrame);
        return;
      }
      requestAnimationFrame(() => requestAnimationFrame(() => {
        evidence.contextRestored = !gl.isContextLost();
        evidence.overlayClearedAfterReadyFrame = overlay.hidden;
        finish();
      }));
    };
    requestAnimationFrame(waitForReadyFrame);
  }, { once: true });
  // The loss is the probe's own: without the mark the page reports it to the
  // platform as a real GPU failure.
  try { window.__platformDiagnostics?.intentionalLoss?.(); extension.loseContext(); }
  catch (error) { fail(error); }
})`;

async function evaluateByValue(client, expression, deadline) {
  const evaluated = await client.call("Runtime.evaluate", {
    expression,
    awaitPromise: true,
    returnByValue: true,
  }, deadline);
  if (evaluated.exceptionDetails) throw new Error(runtimeExceptionText(evaluated.exceptionDetails));
  return evaluated.result?.value;
}

async function captureCanvasFrame(client, canvas, deadline) {
  if (!(canvas?.width > 0) || !(canvas?.height > 0)) return { bytes: null, frame: null };
  const screenshot = await client.call("Page.captureScreenshot", {
    format: "png",
    fromSurface: true,
    captureBeyondViewport: false,
    clip: {
      x: Math.max(0, canvas.x),
      y: Math.max(0, canvas.y),
      width: Math.min(1280, canvas.width),
      height: Math.min(720, canvas.height),
      scale: 1,
    },
  }, deadline);
  const bytes = Buffer.from(screenshot.data, "base64");
  const frame = analyzePngFrame(bytes);
  // The analysis blocks the event loop; yield so output a live service printed
  // meanwhile is read before the next checkpoint takes its baseline.
  await new Promise((resolveTick) => setTimeout(resolveTick, 0));
  return { bytes, frame };
}

function recoveryCyclePassed(evidence) {
  return evidence?.extensionAvailable === true
    && evidence.lostEvent === true
    && evidence.contextLostDuringLoss === true
    && evidence.overlayVisibleDuringLoss === true
    && evidence.restoredEvent === true
    && evidence.overlayVisibleAtRestore === true
    && evidence.contextRestored === true
    && evidence.overlayClearedAfterReadyFrame === true
    && evidence.runtimePresentedFrameAck === true
    && !evidence.failure;
}

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

async function defaultBrowserProbe({ url, expectedRuntimeBuildFingerprint, chromePath, timeoutMs = 60000, webglRecoveryTimeoutMs = 10000, chromeArgs = [], live = null, allowedRemote = [], toleratedRemote = [], blockedUrlPatterns = [], allowedRemoteUrls = [], profileRoot = PROFILE_ROOT, mode = "full" }) {
  const offlineRemote = remoteEndpointMatcher(allowedRemoteUrls);
  const probePlan = smokeProbePlan(mode);
  const startedAt = Date.now();
  const deadline = startedAt + timeoutMs;
  let phase = "browser startup";
  let state = null;
  const browser = findSupportedBrowser({ chromePath });
  let profileDir = "";
  let child = null;
  let rootClient = null;
  let client = null;
  let targetId = "";
  let operationError = null;
  const issues = [];
  let contextLossConsoleAllowance = 0;
  let expectedLossConsoleErrors = 0;
  const issuePolicy = {
    offlineRemote: allowedRemoteUrls.length > 0 ? offlineRemote : null,
    consumeConsoleError(text) {
      if (text.trim() !== "ERROR [gfx] WebGL context lost" || contextLossConsoleAllowance <= 0) return false;
      contextLossConsoleAllowance -= 1;
      expectedLossConsoleErrors += 1;
      return true;
    },
  };
  try {
    profileDir = createChromeProfile("ai-studio-package-smoke", { root: profileRoot }).dir;
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
      ...browserSandboxArgs(),
      ...chromeArgs,
      "--use-angle=swiftshader",
      // Chrome 151+ refuses software WebGL without this opt-in; the smoke
      // renders trusted local content only
      "--enable-unsafe-swiftshader",
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
    const capture = registerBrowserIssueCapture(client, issues, allowedRemote, toleratedRemote, issuePolicy);
    live?.attach(client);
    // Compiled-in endpoints fail as if the player were offline: the smoke stays
    // hermetic and proves the build reaches readiness without them.
    client.on("Fetch.requestPaused", ({ requestId }) => {
      client.call("Fetch.failRequest", { requestId, errorReason: "InternetDisconnected" }, deadline).catch(() => {});
    });
    if (offlineRemote.fetchPatterns.length > 0) {
      await client.call("Fetch.enable", {
        patterns: offlineRemote.fetchPatterns.map((urlPattern) => ({ urlPattern, requestStage: "Request" })),
      }, deadline);
    }
    await Promise.all([
      client.call("Page.enable", {}, deadline),
      client.call("Runtime.enable", {}, deadline),
      client.call("Log.enable", {}, deadline),
      client.call("Network.enable", {}, deadline),
      client.call("Emulation.setDeviceMetricsOverride", { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false }, deadline),
    ]);
    if (blockedUrlPatterns.length > 0) {
      // adblock simulation: the tolerated ad stack must FAIL, and the game
      // must still reach readiness (Poki requires adblocked players playable)
      await client.call("Network.setBlockedURLs", { urls: blockedUrlPatterns }, deadline);
    }
    // A background target may throttle its frames, which a live connection would
    // not survive.
    phase = "target activation";
    await client.call("Page.bringToFront", {}, deadline);
    phase = "page navigation";
    const navigation = await client.call("Page.navigate", { url: localUrl(url, "package page URL").href }, deadline);
    if (navigation.errorText) addIssue(issues, "resource.load", navigation.errorText);
    // half the budget waits for readiness, half is reserved for the evidence
    // capture, so a game that never comes up is still diagnosed as not ready
    // instead of exhausting the deadline and reporting a bare timeout
    const readinessDeadline = Math.min(deadline, Date.now() + Math.floor(timeoutMs / 2));
    phase = "runtime readiness";
    while (Date.now() < readinessDeadline) {
      const evaluated = await client.call("Runtime.evaluate", { expression: PAGE_STATE, returnByValue: true }, readinessDeadline);
      state = evaluated.result?.value || null;
      // a recorded issue still fails the smoke, but it must not end the wait:
      // the platform SDK degrades on a deadline, so abandoning readiness at the
      // first failed request reports "not ready" for a game that does come up
      if (state?.ready && state.compiledRuntimeBuildFingerprint === expectedRuntimeBuildFingerprint
          && state.canvas?.width > 0 && state.canvas?.height > 0) break;
      await new Promise((resolveWait) => setTimeout(resolveWait, Math.min(100, remaining(readinessDeadline))));
    }
    if (state?.ready) {
      phase = "first presented frames";
      await client.call("Runtime.evaluate", {
        expression: "new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)))",
        awaitPromise: true,
      }, deadline);
    }
    capture.settle();
    let frame = null;
    let viewport = null;
    let recovery = null;
    // Each capture is bracketed by live checkpoints: the connection must survive
    // the screenshot stall, not merely exist before it.
    const checkpoint = async (name) => {
      phase = `live checkpoint ${name}`;
      await live?.checkpoint(name, deadline);
    };
    if (probePlan.captureFrame && state?.canvas?.width > 0 && state?.canvas?.height > 0) {
      await checkpoint("initial-before");
      phase = "initial canvas capture";
      const initialCapture = await captureCanvasFrame(client, state.canvas, deadline);
      frame = initialCapture.frame;
      await checkpoint("initial");
      if (state.ready) {
        phase = "collapsed viewport";
        await client.call("Emulation.setDeviceMetricsOverride", { width: 1, height: 1, deviceScaleFactor: 1, mobile: false }, deadline);
        await evaluateByValue(client, "new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(() => requestAnimationFrame(resolve))))", deadline);
        const collapsed = await evaluateByValue(client, PAGE_STATE, deadline);
        await checkpoint("viewport-collapsed");
        phase = "restored viewport";
        await client.call("Emulation.setDeviceMetricsOverride", { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false }, deadline);
        await evaluateByValue(client, "new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(() => requestAnimationFrame(resolve))))", deadline);
        const restored = await evaluateByValue(client, PAGE_STATE, deadline);
        await checkpoint("viewport-restored-before");
        phase = "restored viewport capture";
        const restoredCapture = await captureCanvasFrame(client, restored?.canvas, deadline);
        await checkpoint("viewport-restored");
        viewport = {
          collapsed: { width: collapsed?.viewport?.width, height: collapsed?.viewport?.height, ready: collapsed?.ready },
          restored: { width: restored?.viewport?.width, height: restored?.viewport?.height, ready: restored?.ready, canvas: restored?.canvas },
          frame: restoredCapture.frame,
        };
        const cycles = [];
        for (let cycle = 0; cycle < probePlan.recoveryCycles; cycle += 1) {
          contextLossConsoleAllowance += 1;
          phase = `WebGL recovery ${cycle + 1}`;
          const cycleEvidence = await evaluateByValue(client, webglRecoveryScript(webglRecoveryTimeoutMs), deadline);
          await checkpoint(`recovery-${cycle + 1}-before`);
          phase = `WebGL recovery ${cycle + 1} capture`;
          const cycleCapture = await captureCanvasFrame(client, state.canvas, deadline);
          await checkpoint(`recovery-${cycle + 1}`);
          const evidenceWithFrame = {
            ...(cycleEvidence || { failure: "recovery result is missing" }),
            frame: cycleCapture.frame,
          };
          cycles.push(evidenceWithFrame);
          if (!recoveryCyclePassed(evidenceWithFrame)) break;
        }
        await evaluateByValue(client, "0", deadline);
        contextLossConsoleAllowance = 0;
        const evidence = cycles.at(-1);
        recovery = {
          ...(evidence || { failure: "recovery result is missing" }),
          cycles,
          completedCycles: cycles.filter(recoveryCyclePassed).length,
          expectedLossConsoleErrors,
        };
      }
    }
    if (!probePlan.captureFrame) await checkpoint("ready");
    // Released while the page is still open: closing the browser below ends the
    // live connection on purpose.
    live?.release();
    return { ...state, frame, viewport, recovery, issues: [...issues] };
  } catch (error) {
    operationError = new Error(`packaged browser smoke at ${phase} after ${Date.now() - startedAt}ms: ${error.message}; `
      + `last readiness=${state?.ready ?? "unknown"}; issues=${JSON.stringify(issues)}`, { cause: error });
    throw operationError;
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
  profileRoot = PROFILE_ROOT,
  mode = "full",
  frameExpectation = "rendered",
  allowedRemoteUrls = [],
  webglRecoveryTimeoutMs = 10000,
  chromeArgs = [],
  requiredEntries = [],
  live = null,
}) {
  smokeProbePlan(mode);
  remoteEndpointMatcher(allowedRemoteUrls);
  validateFrameExpectation(frameExpectation);
  const packagePath = resolve(zipPath);
  const entries = readStoreZip(readFileSync(packagePath));
  let release;
  try { release = JSON.parse(entries.get("release.json")?.toString("utf8") || "null"); }
  catch (error) { throw new Error(`reopened package release.json is invalid: ${error.message}`); }
  if (!release || release.schema !== "ai_studio.game.release.v2" || release.target !== expectedTarget
      || release.entrypoint !== "index.html" || !SHA256.test(release.runtimeBuildFingerprint || "")
      || !entries.has(release.entrypoint) || !requiredEntries.every((path) => entries.has(path))) {
    throw new Error(`reopened package release contract is invalid: ${basename(packagePath)}`);
  }
  const server = await serveEntries(entries, release.entrypoint);
  // the poki adapter is REQUIRED to fetch the official SDK from the poki CDN;
  // that single origin is exempt from the hermetic no-remote rule (offline the
  // adapter degrades to no-ops, which the smoke still proves)
  const allowedRemote = expectedTarget === "poki"
    ? [/^https:\/\/([a-z0-9-]+\.)?poki\.com\//,
       /^https:\/\/([a-z0-9-]+\.)?poki-cdn\.com\//,
       /^https:\/\/([a-z0-9-]+\.)?poki\.io\//] : [];
  // the ad stack the poki SDK pulls, poki's own ad settings endpoint included:
  // BLOCKED (adblock simulation) and its failures tolerated; any other non-poki
  // remote request still fails the smoke. ads.poki.com belongs here and not in
  // allowedRemote because it is unreachable off poki's own hosting anyway (it
  // answers no cross-origin request), which is the same shape an adblocked
  // player sees, and the game is required to become playable through it.
  const AD_STACK_HOSTS = [
    "ads.poki.com",
    "doubleclick.net", "googlesyndication.com", "googletagservices.com",
    "googletagmanager.com", "google-analytics.com", "imasdk.googleapis.com",
    "amazon-adsystem.com", "adsafeprotected.com",
  ];
  const toleratedRemote = expectedTarget === "poki"
    ? [...AD_STACK_HOSTS.map((host) => new RegExp(`(^|[/.])${host.replaceAll(".", "\\.")}(/|$|[:?#'" )])`)),
       /cdn\.jsdelivr\.net\/gh\/prebid/] : [];
  const blockedUrlPatterns = expectedTarget === "poki"
    ? [...AD_STACK_HOSTS.map((host) => `*${host}*`), "*cdn.jsdelivr.net/gh/prebid*"] : [];
  let session = null;
  try {
    // live: { prepare(pageUrl) -> { url, attach(client), checkpoint(name, deadline),
    // release(), finish(mode) -> failures, stop(), diagnose() -> text } } keeps a
    // service the page talks to alive through the probe and judges it at each
    // capture; finish() runs after browser teardown, which it must also survive.
    session = live ? await live.prepare(server.url) : null;
    const observation = await probe({
      url: session?.url ?? server.url,
      expectedRuntimeBuildFingerprint: release.runtimeBuildFingerprint,
      chromePath,
      timeoutMs,
      webglRecoveryTimeoutMs,
      chromeArgs,
      live: session,
      allowedRemote,
      toleratedRemote,
      blockedUrlPatterns,
      allowedRemoteUrls,
      profileRoot,
      mode,
    });
    const failures = assessPackagedWebObservation(observation, release.runtimeBuildFingerprint, { mode, frameExpectation });
    if (session) failures.push(...session.finish(mode));
    if (failures.length > 0) throw new Error(`packaged web smoke failed: ${failures.join("; ")}`);
    return observation;
  } catch (error) {
    if (!session) throw error;
    throw new Error(`${error.message}; ${session.diagnose()}`, { cause: error });
  } finally {
    try { await session?.stop(); }
    finally { await server.close(); }
  }
}
