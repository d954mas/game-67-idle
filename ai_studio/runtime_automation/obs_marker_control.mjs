import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

const USAGE = "usage: obs_marker_control.mjs --port <1-65535> --scene <name> --item-id <positive-int> --enabled <true|false> [--min-render-frames <1-60>] [--timeout-ms <100-30000>]";

function fail(message) {
  throw new Error(message);
}

function sha256Base64(value) {
  return createHash("sha256").update(value).digest("base64");
}

export function obsAuthentication(password, salt, challenge) {
  return sha256Base64(`${sha256Base64(`${password}${salt}`)}${challenge}`);
}

export function parseArgs(argv) {
  const options = { timeoutMs: 5000, minRenderFrames: 3 };
  for (let index = 0; index < argv.length; index += 2) {
    const flag = argv[index];
    const value = argv[index + 1];
    if (value === undefined) fail(USAGE);
    if (flag === "--port") options.port = Number(value);
    else if (flag === "--scene") options.scene = value;
    else if (flag === "--item-id") options.itemId = Number(value);
    else if (flag === "--enabled") options.enabled = value === "true" ? true : value === "false" ? false : null;
    else if (flag === "--min-render-frames") options.minRenderFrames = Number(value);
    else if (flag === "--timeout-ms") options.timeoutMs = Number(value);
    else fail(USAGE);
  }
  if (!Number.isInteger(options.port) || options.port < 1 || options.port > 65535
      || typeof options.scene !== "string" || options.scene.length === 0
      || !Number.isSafeInteger(options.itemId) || options.itemId < 1
      || typeof options.enabled !== "boolean"
      || !Number.isSafeInteger(options.minRenderFrames) || options.minRenderFrames < 1 || options.minRenderFrames > 60
      || !Number.isSafeInteger(options.timeoutMs) || options.timeoutMs < 100 || options.timeoutMs > 30000) {
    fail(USAGE);
  }
  return options;
}

function readPassword() {
  let parsed;
  try {
    parsed = JSON.parse(readFileSync(0, "utf8"));
  } catch {
    fail("OBS WebSocket credentials must be JSON on stdin");
  }
  if (typeof parsed?.password !== "string" || parsed.password.length < 16) {
    fail("OBS WebSocket password is invalid");
  }
  return parsed.password;
}

async function textMessage(event) {
  if (typeof event.data === "string") return event.data;
  return Buffer.from(await event.data.arrayBuffer()).toString("utf8");
}

export async function setMarkerEnabled({
  port,
  password,
  scene,
  itemId,
  enabled,
  minRenderFrames = 3,
  timeoutMs = 5000,
  WebSocketClass = globalThis.WebSocket,
}) {
  if (typeof WebSocketClass !== "function") fail("Node.js with built-in WebSocket support is required");
  const deadline = performance.now() + timeoutMs;
  const socket = new WebSocketClass(`ws://127.0.0.1:${port}`);
  const requestId = "capture-marker";
  const barrierBaselineId = "capture-marker-barrier-baseline";
  const afterStatsId = "capture-marker-after";
  return await new Promise((resolve, reject) => {
    let settled = false;
    let requestStartedAt = 0;
    let barrierStartedAt = 0;
    let barrierBaselineFrames = 0;
    let identifySent = false;
    const settle = (callback, value) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      try { socket.close(); } catch {}
      callback(value);
    };
    const safeSend = (payload) => {
      if (settled) return false;
      try {
        socket.send(JSON.stringify(payload));
        return true;
      } catch {
        settle(reject, new Error("OBS WebSocket closed during marker control"));
        return false;
      }
    };
    const timer = setTimeout(() => settle(reject, new Error("OBS WebSocket request timed out")), timeoutMs);
    socket.addEventListener("error", () => settle(reject, new Error("could not connect to OBS WebSocket")), { once: true });
    socket.addEventListener("close", () => settle(reject, new Error("OBS WebSocket closed before marker control completed")), { once: true });
    const requestStats = (requestId) => safeSend({
      op: 6,
      d: { requestType: "GetStats", requestId },
    });
    socket.addEventListener("message", async (event) => {
      if (settled) return;
      try {
        const message = JSON.parse(await textMessage(event));
        if (settled) return;
        if (message?.op === 0) {
          const authentication = message.d?.authentication;
          if (typeof authentication?.salt !== "string" || authentication.salt.length === 0
              || typeof authentication?.challenge !== "string" || authentication.challenge.length === 0) {
            settle(reject, new Error("OBS WebSocket did not require authenticated access"));
            return;
          }
          safeSend({
            op: 1,
            d: {
              rpcVersion: 1,
              authentication: obsAuthentication(password, authentication.salt, authentication.challenge),
            },
          });
          identifySent = true;
          return;
        }
        if (message?.op === 2) {
          if (!identifySent) {
            settle(reject, new Error("OBS WebSocket identified before authentication"));
            return;
          }
          requestStartedAt = performance.now();
          safeSend({
            op: 6,
            d: {
              requestType: "SetSceneItemEnabled",
              requestId,
              requestData: {
                sceneName: scene,
                sceneItemId: itemId,
                sceneItemEnabled: enabled,
              },
            },
          });
          return;
        }
        if (message?.op !== 7) return;
        if (message.d?.requestStatus?.result !== true) {
          settle(reject, new Error("OBS rejected capture marker control request"));
          return;
        }
        if (message.d?.requestId === requestId) {
          barrierStartedAt = performance.now();
          requestStats(barrierBaselineId);
          return;
        }
        if (message.d?.requestId === barrierBaselineId) {
          barrierBaselineFrames = message.d?.responseData?.renderTotalFrames;
          if (!Number.isSafeInteger(barrierBaselineFrames) || barrierBaselineFrames < 0) {
            settle(reject, new Error("OBS did not report renderTotalFrames"));
            return;
          }
          requestStats(afterStatsId);
          return;
        }
        if (message.d?.requestId !== afterStatsId) return;
        const afterRenderFrames = message.d?.responseData?.renderTotalFrames;
        if (!Number.isSafeInteger(afterRenderFrames) || afterRenderFrames < barrierBaselineFrames) {
          settle(reject, new Error("OBS returned an invalid renderTotalFrames barrier"));
          return;
        }
        if (afterRenderFrames < barrierBaselineFrames + minRenderFrames) {
          const delay = Math.min(10, deadline - performance.now());
          if (delay <= 0) {
            settle(reject, new Error("OBS WebSocket request timed out"));
            return;
          }
          setTimeout(() => { if (!settled) requestStats(afterStatsId); }, delay);
          return;
        }
        settle(resolve, {
          roundTripMilliseconds: Math.max(0, barrierStartedAt - requestStartedAt),
          renderBarrierMilliseconds: Math.max(0, performance.now() - barrierStartedAt),
          beforeRenderFrames: barrierBaselineFrames,
          afterRenderFrames,
        });
      } catch {
        settle(reject, new Error("OBS WebSocket returned invalid JSON"));
      }
    });
  });
}

export async function main(argv = process.argv.slice(2), readCredential = readPassword) {
  try {
    const options = parseArgs(argv);
    const receipt = await setMarkerEnabled({ ...options, password: readCredential() });
    process.stdout.write(`${JSON.stringify(receipt)}\n`);
    return 0;
  } catch (error) {
    process.stderr.write(`${error?.message || String(error)}\n`);
    return String(error?.message || "").startsWith("usage:") ? 2 : 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  process.exitCode = await main();
}
