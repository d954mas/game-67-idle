#!/usr/bin/env node

import { createHash, randomUUID } from "node:crypto";
import {
  existsSync,
  linkSync,
  mkdirSync,
  lstatSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const SCHEMA = "ai_studio.runtime.local_mock_web_observation.v1";
const USAGE = [
  "usage: node ai_studio/runtime_automation/web_local_mock_probe.mjs",
  "  --url http://127.0.0.1:<port>/ --out <observation>.json",
  "  [--cdp http://127.0.0.1:9222] [--timeout-ms 30000]",
].join(" ");
const jsonBytes = (value) => Buffer.from(`${JSON.stringify(value, null, 2)}\n`, "utf8");
const SHA256 = /^[0-9a-f]{64}$/;

function exactKeys(value, keys) {
  return value && typeof value === "object" && !Array.isArray(value)
    && JSON.stringify(Object.keys(value).sort()) === JSON.stringify([...keys].sort());
}

function validRuntimeBuildRecord(record) {
  if (!exactKeys(record, ["schema", "fingerprint", "inputs"])
      || record.schema !== "ai_studio.runtime_build.v1" || !Array.isArray(record.inputs)
      || record.inputs.length < 2) return false;
  const ids = new Set();
  for (const input of record.inputs) {
    if (!exactKeys(input, ["id", "source", "files", "sha256"])
        || !/^(?:game|engine|feature:[a-z][a-z0-9-]*)$/.test(input.id || "") || ids.has(input.id)
        || typeof input.source !== "string" || !input.source || input.source.includes("\\")
        || (!Number.isSafeInteger(input.files) || input.files < 1) || !SHA256.test(input.sha256 || "")) return false;
    ids.add(input.id);
    const expectedSource = input.id === "game" ? "."
      : input.id === "engine" ? "external/neotolis-engine" : `features/${input.id.slice("feature:".length)}`;
    if (input.source !== expectedSource) return false;
  }
  if (record.inputs[0].id !== "game" || record.inputs[0].source !== "." || record.inputs[1].id !== "engine"
      || record.inputs.slice(2).some((input, index, rows) => index > 0 && rows[index - 1].id >= input.id)) return false;
  const fingerprint = createHash("sha256").update(JSON.stringify(record.inputs)).digest("hex");
  return record.fingerprint === fingerprint;
}

function localHttpUrl(raw, label) {
  const url = new URL(raw);
  const host = url.hostname.toLowerCase();
  if (url.protocol !== "http:" || !["127.0.0.1", "localhost", "::1", "[::1]"].includes(host)) {
    throw new Error(`${label} must be a local HTTP URL`);
  }
  if (url.username || url.password) throw new Error(`${label} must not contain credentials`);
  return url;
}

function localWebSocketUrl(raw) {
  const url = new URL(raw);
  const host = url.hostname.toLowerCase();
  if (url.protocol !== "ws:" || !["127.0.0.1", "localhost", "::1", "[::1]"].includes(host)
      || url.username || url.password) {
    throw new Error("CDP target WebSocket must stay on loopback ws://");
  }
  return url;
}

function isLocalPageUrl(raw) {
  try {
    localHttpUrl(raw, "final page URL");
    return true;
  } catch {
    return false;
  }
}

export function assessLocalMockObservation(observation) {
  const failures = [];
  if (observation?.readyState !== "complete") failures.push("document did not finish loading");
  if (!isLocalPageUrl(observation?.finalUrl)) failures.push("final page URL left localhost");
  if (observation?.config?.target !== "local" || observation?.config?.platformSdk !== "mock"
      || observation?.config?.release !== true) {
    failures.push("page is not the local mock target");
  }
  if (!validRuntimeBuildRecord(observation?.runtimeBuild)) failures.push("runtime build record is invalid");
  else if (observation.config?.runtimeBuildFingerprint !== observation.runtimeBuild.fingerprint) {
    failures.push("page runtime build fingerprint does not match runtime-build.json");
  } else if (observation.compiledRuntimeBuildFingerprint !== observation.runtimeBuild.fingerprint) {
    failures.push("executed WASM runtime build marker does not match runtime-build.json");
  }
  if (!observation?.overlay?.present) failures.push("loading overlay is missing");
  else if (observation.overlay.display !== "none") failures.push("loading overlay is still visible");
  if (observation?.overlay?.progressPercent !== 100) failures.push("loading progress did not reach 100%");
  if (!(observation?.canvas?.width > 0) || !(observation?.canvas?.height > 0)) {
    failures.push("render canvas has no visible area");
  }
  const transcript = observation?.lifecycleTranscript;
  if (!Array.isArray(transcript)) failures.push("C lifecycle transcript is missing");
  else {
    const finalProgress = transcript.findIndex((event) => event?.kind === "progress"
      && event?.source === "c-bridge" && event?.value === 1);
    const finished = transcript.findIndex((event) => event?.kind === "finished" && event?.source === "c-bridge");
    if (finalProgress < 0) failures.push("C loading progress never reached the JS bridge");
    if (finished < 0) failures.push("C loading finished never reached the JS bridge");
    else if (finalProgress >= 0 && finished < finalProgress) failures.push("C loading finished preceded final progress");
  }
  if (!Array.isArray(observation?.issues)) failures.push("browser error transcript is missing");
  else if (observation.issues.length > 0) {
    const errorCount = observation.issues.length;
    failures.push(`browser reported ${errorCount} error${errorCount === 1 ? "" : "s"}`);
  }
  return failures;
}

export function createLocalMockEvidence({ observation }) {
  const failures = assessLocalMockObservation(observation);
  if (failures.length > 0) throw new Error(`local mock browser proof failed: ${failures.join("; ")}`);
  return {
    schema: SCHEMA,
    result: "pass",
    observation: {
      url: observation.url,
      finalUrl: observation.finalUrl,
      readyState: observation.readyState,
      config: observation.config,
      runtimeBuild: observation.runtimeBuild,
      compiledRuntimeBuildFingerprint: observation.compiledRuntimeBuildFingerprint,
      overlay: observation.overlay,
      canvas: observation.canvas,
      lifecycleTranscript: observation.lifecycleTranscript,
      issues: [],
    },
  };
}

export function writeLocalMockEvidence(outputPath, evidence) {
  const path = resolve(outputPath);
  const bytes = jsonBytes(evidence);
  mkdirSync(dirname(path), { recursive: true });
  if (existsSync(path) && lstatSync(path).isSymbolicLink()) {
    throw new Error(`refusing symbolic-link local mock evidence output: ${path}`);
  }
  const tempPath = join(dirname(path), `.local-mock-${process.pid}-${randomUUID()}.tmp`);
  try {
    writeFileSync(tempPath, bytes, { flag: "wx" });
    try {
      linkSync(tempPath, path);
    } catch (error) {
      if (error?.code === "EEXIST" && existsSync(path) && readFileSync(path).equals(bytes)) return;
      if (error?.code === "EEXIST") throw new Error(`refusing to replace different local mock evidence: ${path}`);
      throw error;
    }
  } finally {
    rmSync(tempPath, { force: true });
  }
}

class CdpClient {
  constructor(socket) {
    this.socket = socket;
    this.nextId = 0;
    this.pending = new Map();
    this.listeners = new Map();
    socket.addEventListener("message", (event) => this.#message(event));
    socket.addEventListener("close", () => {
      for (const { reject } of this.pending.values()) reject(new Error("CDP connection closed"));
      this.pending.clear();
    });
  }

  static async connect(url, deadline, WebSocketClass = globalThis.WebSocket) {
    if (typeof WebSocketClass !== "function") throw new Error("Node.js with built-in WebSocket support is required");
    const socket = new WebSocketClass(url);
    await new Promise((resolveOpen, rejectOpen) => {
      const fail = (error) => {
        try { socket.close(); } catch {}
        rejectOpen(error);
      };
      const timer = setTimeout(() => fail(new Error("CDP WebSocket connection timed out")), remaining(deadline));
      socket.addEventListener("open", () => { clearTimeout(timer); resolveOpen(); }, { once: true });
      socket.addEventListener("error", () => { clearTimeout(timer); fail(new Error("could not connect to Chrome CDP")); }, { once: true });
    });
    return new CdpClient(socket);
  }

  async #message(event) {
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
    const listeners = this.listeners.get(method) || [];
    listeners.push(listener);
    this.listeners.set(method, listeners);
  }

  call(method, params = {}, deadline) {
    return new Promise((resolveCall, rejectCall) => {
      const id = ++this.nextId;
      const timer = setTimeout(() => {
        this.pending.delete(id);
        rejectCall(new Error(`${method} timed out`));
      }, remaining(deadline));
      this.pending.set(id, {
        resolve: (value) => { clearTimeout(timer); resolveCall(value); },
        reject: (error) => { clearTimeout(timer); rejectCall(error); },
      });
      this.socket.send(JSON.stringify({ id, method, params }));
    });
  }

  close() {
    this.socket.close();
  }
}

function remaining(deadline) {
  const value = deadline - Date.now();
  if (value <= 0) throw new Error("local mock browser proof timed out");
  return value;
}

const LIFECYCLE_INSTRUMENTATION = `(() => {
  const transcript = [];
  Object.defineProperty(globalThis, "__AI_STUDIO_LOCAL_MOCK_LIFECYCLE__", {
    configurable: false,
    enumerable: false,
    writable: false,
    value: transcript
  });
  const intercept = (name, kind, cMarker) => {
    Object.defineProperty(globalThis, name, {
      configurable: true,
      set(callback) {
        const wrapped = function (...args) {
          const stack = String(new Error().stack || "");
          transcript.push({
            kind,
            value: kind === "progress" ? Number(args[0]) : null,
            source: stack.includes(cMarker) ? "c-bridge" : "shell"
          });
          return callback.apply(this, args);
        };
        Object.defineProperty(globalThis, name, {
          configurable: true,
          enumerable: true,
          writable: true,
          value: wrapped
        });
      }
    });
  };
  intercept("__platformSdkSetLoadingProgress", "progress", "platform_sdk_web_backend_game_loading_progress");
  intercept("__platformSdkHideLoadingOverlay", "finished", "platform_sdk_web_backend_game_loading_finished");
})()`;

const PAGE_OBSERVATION = `(() => {
  const overlay = document.getElementById("loading-overlay");
  const percent = document.getElementById("loading-percent");
  const canvas = document.querySelector("canvas");
  const progressPercent = Number.parseInt(percent?.textContent || "", 10);
  const rect = canvas?.getBoundingClientRect();
  return {
    finalUrl: location.href,
    readyState: document.readyState,
    config: globalThis.__PLATFORM_SDK_CONFIG__ || null,
    compiledRuntimeBuildFingerprint: globalThis.__AI_STUDIO_RUNTIME_BUILD_FINGERPRINT__ || null,
    overlay: {
      present: Boolean(overlay),
      className: overlay?.className || "",
      display: overlay ? getComputedStyle(overlay).display : null,
      opacity: overlay ? getComputedStyle(overlay).opacity : null,
      progressPercent: Number.isFinite(progressPercent) ? progressPercent : null
    },
    canvas: { width: rect?.width || 0, height: rect?.height || 0 },
    lifecycleTranscript: Array.isArray(globalThis.__AI_STUDIO_LOCAL_MOCK_LIFECYCLE__)
      ? globalThis.__AI_STUDIO_LOCAL_MOCK_LIFECYCLE__.map((event) => ({
          kind: event.kind,
          value: event.value,
          source: event.source
        }))
      : null
  };
})()`;

function issueText(value) {
  if (typeof value === "string") return value;
  if (value?.description) return value.description;
  if (value?.value != null) return String(value.value);
  return "browser runtime error";
}

function isRemoteNetworkUrl(raw) {
  try {
    const url = new URL(raw);
    if (!["http:", "https:", "ws:", "wss:"].includes(url.protocol)) return false;
    return !["127.0.0.1", "localhost", "::1", "[::1]"].includes(url.hostname.toLowerCase());
  } catch {
    return true;
  }
}

async function closeFreshTarget({ endpoint, fetchImpl, targetId, deadline }) {
  if (!targetId || Date.now() >= deadline) return false;
  const response = await fetchImpl(new URL(`/json/close/${encodeURIComponent(targetId)}`, endpoint), {
    method: "GET",
    redirect: "error",
    signal: AbortSignal.timeout(remaining(deadline)),
  });
  return response.ok === true;
}

async function fetchRuntimeBuild({ pageUrl, fetchImpl, deadline }) {
  const url = new URL("runtime-build.json", pageUrl);
  const response = await fetchImpl(url, {
    method: "GET",
    redirect: "error",
    signal: AbortSignal.timeout(remaining(deadline)),
  });
  if (!response?.ok) throw new Error(`runtime-build.json request failed with HTTP ${response?.status || "error"}`);
  const bytes = Buffer.from(await response.arrayBuffer());
  if (bytes.length === 0 || bytes.length > 65536) throw new Error("runtime-build.json size is invalid");
  let record;
  try { record = JSON.parse(bytes.toString("utf8").replace(/^\uFEFF/, "")); }
  catch (error) { throw new Error(`runtime-build.json is invalid JSON: ${error.message}`); }
  if (!validRuntimeBuildRecord(record)) throw new Error("runtime-build.json record is invalid");
  return record;
}

export async function probeLocalMockPage({
  cdpEndpoint,
  url,
  timeoutMs = 30000,
  fetchImpl = fetch,
  WebSocketClass = globalThis.WebSocket,
}) {
  const deadline = Date.now() + timeoutMs;
  const cleanupBudget = Math.min(500, Math.max(50, Math.floor(timeoutMs / 2)));
  const operationDeadline = deadline - cleanupBudget;
  const endpoint = localHttpUrl(cdpEndpoint, "CDP endpoint");
  const pageUrl = localHttpUrl(url, "page URL");
  const runtimeBuildBefore = await fetchRuntimeBuild({ pageUrl, fetchImpl, deadline: operationDeadline });
  let target = null;
  let client = null;
  const issues = [];
  const addIssue = (kind, text) => {
    const issue = { kind, text: String(text || "browser runtime error").slice(0, 1000) };
    if (!issues.some((entry) => entry.kind === issue.kind && entry.text === issue.text)) issues.push(issue);
  };
  try {
    const newTargetResponse = await fetchImpl(new URL("/json/new?about%3Ablank", endpoint), {
      method: "PUT",
      redirect: "error",
      signal: AbortSignal.timeout(remaining(operationDeadline)),
    });
    target = newTargetResponse.ok ? await newTargetResponse.json() : null;
    if (!target?.id || !target?.webSocketDebuggerUrl) throw new Error("CDP endpoint has no closable page target");
    const targetSocket = localWebSocketUrl(target.webSocketDebuggerUrl);
    client = await CdpClient.connect(targetSocket.href, operationDeadline, WebSocketClass);
    client.on("Runtime.exceptionThrown", ({ exceptionDetails }) => {
      addIssue("page.exception", exceptionDetails?.exception?.description || exceptionDetails?.text);
    });
    client.on("Runtime.consoleAPICalled", ({ type, args }) => {
      if (type === "error") addIssue("console.error", (args || []).map(issueText).join(" "));
    });
    client.on("Log.entryAdded", ({ entry }) => {
      if (entry?.level === "error") addIssue("log.error", entry.text);
    });
    const rejectRemoteUrl = ({ url: requestUrl }) => {
      if (isRemoteNetworkUrl(requestUrl)) addIssue("network.remote", requestUrl);
    };
    client.on("Network.requestWillBeSent", ({ request }) => rejectRemoteUrl({ url: request?.url }));
    client.on("Network.webSocketCreated", rejectRemoteUrl);
    client.on("Network.webTransportCreated", rejectRemoteUrl);
    client.on("Network.directTCPSocketCreated", ({ remoteAddr }) => {
      addIssue("network.direct", `direct TCP socket: ${remoteAddr || "unknown"}`);
    });
    client.on("Network.directUDPSocketCreated", ({ options }) => {
      addIssue("network.direct", `direct UDP socket: ${options?.remoteAddr || "unconnected"}`);
    });
    await Promise.all([
      client.call("Page.enable", {}, operationDeadline),
      client.call("Runtime.enable", {}, operationDeadline),
      client.call("Log.enable", {}, operationDeadline),
      client.call("Network.enable", {}, operationDeadline),
    ]);
    await client.call("Page.addScriptToEvaluateOnNewDocument", { source: LIFECYCLE_INSTRUMENTATION }, operationDeadline);
    const navigation = await client.call("Page.navigate", { url: pageUrl.href }, operationDeadline);
    if (navigation.errorText) throw new Error(`page navigation failed: ${navigation.errorText}`);
    let state = null;
    while (Date.now() <= operationDeadline) {
      const result = await client.call("Runtime.evaluate", { expression: PAGE_OBSERVATION, returnByValue: true }, operationDeadline);
      if (result.exceptionDetails) addIssue("probe.exception", result.exceptionDetails.text);
      state = result.result?.value || null;
      const observation = { url: pageUrl.href, ...state, runtimeBuild: runtimeBuildBefore, issues };
      if (issues.length > 0) {
        throw new Error(`local mock browser proof failed: ${issues.map((issue) => issue.text).join(" | ")}`);
      }
      if (state?.readyState === "complete" && assessLocalMockObservation(observation).length === 0) {
        const runtimeBuildAfter = await fetchRuntimeBuild({ pageUrl, fetchImpl, deadline: operationDeadline });
        if (JSON.stringify(runtimeBuildAfter) !== JSON.stringify(runtimeBuildBefore)) {
          throw new Error("runtime-build.json changed during local mock browser proof");
        }
        return observation;
      }
      await new Promise((resolveWait) => setTimeout(resolveWait, Math.min(100, remaining(operationDeadline))));
    }
    const observation = { url: pageUrl.href, ...state, runtimeBuild: runtimeBuildBefore, issues };
    const details = issues.length > 0 ? ` (${issues.map((issue) => issue.text).join(" | ")})` : "";
    throw new Error(`local mock browser proof timed out: ${assessLocalMockObservation(observation).join("; ")}${details}`);
  } finally {
    let targetClosed = false;
    if (target?.id && Date.now() < deadline) {
      try {
        const httpCloseDeadline = Date.now() + Math.max(1, Math.floor((deadline - Date.now()) / 2));
        targetClosed = await closeFreshTarget({
          endpoint,
          fetchImpl,
          targetId: target.id,
          deadline: Math.min(deadline, httpCloseDeadline),
        });
      } catch {}
    }
    if (client && !targetClosed && Date.now() < deadline) {
      try {
        await client.call("Page.close", {}, deadline);
      } catch {}
    }
    if (client) client.close();
  }
}

function parseArgs(argv) {
  const options = { cdpEndpoint: "http://127.0.0.1:9222", timeoutMs: 30000 };
  for (let index = 0; index < argv.length; index += 2) {
    const flag = argv[index];
    const value = argv[index + 1];
    if (!value) throw new Error(USAGE);
    if (flag === "--url") options.url = value;
    else if (flag === "--cdp") options.cdpEndpoint = value;
    else if (flag === "--out") options.outputPath = resolve(value);
    else if (flag === "--timeout-ms") options.timeoutMs = Number(value);
    else throw new Error(USAGE);
  }
  if (!options.url || !options.outputPath
      || !Number.isSafeInteger(options.timeoutMs) || options.timeoutMs < 1000 || options.timeoutMs > 120000) {
    throw new Error(USAGE);
  }
  return options;
}

export async function main(argv = process.argv.slice(2)) {
  try {
    const options = parseArgs(argv);
    const observation = await probeLocalMockPage(options);
    const evidence = createLocalMockEvidence({ observation });
    writeLocalMockEvidence(options.outputPath, evidence);
    console.log(`local mock observation recorded: ${options.outputPath}`);
    return 0;
  } catch (error) {
    console.error(error?.message || String(error));
    return String(error?.message || "").startsWith("usage:") ? 2 : 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  process.exitCode = await main();
}
