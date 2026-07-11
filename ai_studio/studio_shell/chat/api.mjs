// Chat HTTP/SSE adapter (T0242 increment 3). Studio Shell mounts this handler on the
// /api/chat/ prefix, beside /api/canvas/ (server.mjs) — bind stays 127.0.0.1, the SAME
// local-only trust boundary as the rest of studio_shell. T0350 additionally authenticates
// every POST and refuses the legacy codex transport because it cannot await approvals.
//
// Routes:
//   POST /api/chat/projects/<id>/message    {text, selection?}   -> SSE stream
//   POST /api/chat/projects/<id>/cancel                          -> kill the CURRENT turn's child
//   GET  /api/chat/projects/<id>/transcript                      -> the jsonl parsed to JSON
//   POST /api/chat/projects/<id>/clear                           -> archive + reset session
//   POST /api/chat/projects/<id>/permissions/<id>/decision       -> allow/deny pending request
//
// SSE event contract (POST .../message), one per line as `event: <name>\ndata: <json>\n\n`:
//   progress      {message}                              spawn started (v1: coarse, one event)
//   op-committed  {seqRange:[before,after]}               ONLY when the head advanced; head is
//                 read once before the turn and once after — no per-op live streaming (design
//                 doc section 3's "keep simple" call)
//   final         {text, sessionId, seqRange, flags}      seqRange is null when nothing committed
//   error         {message}                               loud, never swallowed; the stream still
//                 gets a normal 200 (SSE headers are already committed by the time an error can
//                 be known) — the panel renders this as a red message either way
// Exactly one of `final`/`error` ends every stream; the HTTP response itself always ends 200.
import { readTranscript, appendTurn, buildChatContext, clearConversation, readChatState, writeChatState } from "./context.mjs";
import { runChatTurn } from "./agent.mjs";
import { createPermissionBroker } from "./permission_broker.mjs";
import { listHistory } from "../../assets/canvas/ops.mjs";
import { selectCanvasStore, withCanvasStore } from "../../assets/canvas/stores.mjs";
import { randomBytes, randomUUID, timingSafeEqual } from "node:crypto";

// Chat message bodies are small (text + a handful of selection refs) — nowhere near
// image-upload size, so a much tighter cap than the canvas API's 20MB image cap.
const maxBodyBytes = 256 * 1024;

function sendJson(res, status, data, headers = {}) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8", ...headers });
  res.end(JSON.stringify(data));
}

function readJsonBody(req) {
  return new Promise((resolveBody, rejectBody) => {
    let size = 0;
    let data = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => {
      size += Buffer.byteLength(chunk);
      if (size > maxBodyBytes) {
        rejectBody(new Error("request body too large"));
        req.destroy();
        return;
      }
      data += chunk;
    });
    req.on("end", () => {
      try {
        resolveBody(data ? JSON.parse(data) : {});
      } catch {
        rejectBody(new Error("invalid JSON body"));
      }
    });
    req.on("error", rejectBody);
  });
}

function sseHead(res) {
  res.writeHead(200, {
    "content-type": "text/event-stream; charset=utf-8",
    "cache-control": "no-cache",
    connection: "keep-alive",
  });
}

function sseEvent(res, event, data) {
  res.write(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`);
}

function errorMessage(error) {
  return error && error.message ? error.message : String(error);
}

function headerValue(req, name) {
  const value = req.headers && req.headers[name.toLowerCase()];
  return Array.isArray(value) ? value[0] : (value || "");
}

function tokenMatches(expected, actual) {
  const expectedBytes = Buffer.from(expected);
  const actualBytes = Buffer.from(actual);
  return expectedBytes.length === actualBytes.length && timingSafeEqual(expectedBytes, actualBytes);
}

function sameOriginRequest(req, allowedHosts) {
  const host = headerValue(req, "host");
  const origin = headerValue(req, "origin");
  if (!allowedHosts.has(host) || !origin) return false;
  try {
    const parsed = new URL(origin);
    return parsed.protocol === "http:" && parsed.host === host && parsed.origin === origin;
  } catch {
    return false;
  }
}

function bootstrapRequestAllowed(req, allowedHosts) {
  const host = headerValue(req, "host");
  if (!allowedHosts.has(host)) return false;
  const origin = headerValue(req, "origin");
  if (origin) return sameOriginRequest(req, allowedHosts);
  const fetchSite = headerValue(req, "sec-fetch-site");
  return !fetchSite || fetchSite === "same-origin";
}

function authorizeMutation(req, res, security) {
  if (!sameOriginRequest(req, security.allowedHosts)
      || !tokenMatches(security.launchToken, headerValue(req, "x-ai-studio-chat-token"))) {
    sendJson(res, 403, { error: "chat request rejected" });
    return false;
  }
  if (headerValue(req, "content-type").split(";", 1)[0].trim().toLowerCase() !== "application/json") {
    sendJson(res, 415, { error: "content-type must be application/json" });
    return false;
  }
  return true;
}

function chatStoreRequestArgs(req, url) {
  const headerStore = headerValue(req, "x-ai-studio-store");
  const queryStore = url.searchParams.get("store") || "";
  if (headerStore && queryStore && headerStore !== queryStore) {
    throw new Error(`Chat store mismatch between header and query: ${headerStore} != ${queryStore}`);
  }
  return {
    store: headerStore || queryStore,
    game: url.searchParams.get("game") || "",
  };
}

function chatRunKey(store, projectId) {
  return `${store.storeId}:${projectId}`;
}

export function ensurePermissionAllowed(turn, settled) {
  if (turn.cancelled) throw new Error("permission cancelled");
  if (settled.state !== "allowed") throw new Error(`permission ${settled.state}`);
  return settled;
}

// Runs one full chat turn over an already-open SSE stream. `runningChildren` is the
// store-qualified Map<storeId:projectId, ChildProcess> the cancel route reads — populated
// via runChatTurn's `onChild` seam, which the default transport (agent.mjs) calls
// synchronously right after spawn.
async function handleMessage(root, activeStore, projectId, body, res, transport, runningChildren, runningTurns, permissions) {
  sseHead(res);
  sseEvent(res, "progress", { message: "spawn started" });
  const text = body && typeof body.text === "string" ? body.text.trim() : "";
  const runKey = chatRunKey(activeStore, projectId);
  const turn = { turnId: randomUUID(), cancelled: false };
  const binding = { storeId: activeStore.storeId, projectId, turnId: turn.turnId };
  try {
    if (!text) throw new Error("message requires non-empty text");
    // v1 keeps this simple: reject a second concurrent Send rather than queue it (design
    // doc section 4's Cancel/kill note) — one running child per project at a time.
    if (runningTurns.has(runKey)) {
      throw new Error(`a turn is already running for project ${projectId} — cancel it or wait for it to finish`);
    }
    if (!transport || transport.approvalAware !== true) {
      throw new Error("chat transport is not approval-aware; legacy codex-exec is disabled until T0351");
    }
    runningTurns.set(runKey, turn);
    const context = buildChatContext(root, { projectId, selection: body.selection || [], store: activeStore });
    const before = listHistory(root, { projectId }).head;
    appendTurn(root, { projectId, role: "user", text });
    const state = readChatState(root, { projectId });
    const onChild = (child) => {
      if (turn.cancelled) child.kill("SIGTERM");
      else runningChildren.set(runKey, child);
    };
    const requestPermission = async (exactRequest) => {
      if (turn.cancelled) throw new Error("permission cancelled");
      const pending = permissions.request({ ...binding, exactRequest });
      sseEvent(res, "permission-request", {
        id: pending.permission.id,
        state: pending.permission.state,
        exactRequest: pending.permission.exactRequest,
      });
      const settled = await pending.settled;
      sseEvent(res, "permission-decision", { id: settled.id, state: settled.state });
      return ensurePermissionAllowed(turn, settled);
    };
    let result;
    try {
      result = await runChatTurn({
        context, message: text, sessionId: state.sessionId, transport, onChild, requestPermission,
      });
    } catch (turnError) {
      // A cancelled-but-already-started turn may still have captured a session id
      // (agent.mjs attaches it to the thrown Error) — persist it so R3's "the session
      // survives, only that message was cancelled" holds even on this path.
      if (turnError && turnError.sessionId) {
        try {
          writeChatState(root, { projectId, sessionId: turnError.sessionId });
        } catch {
          // best-effort only — the original turnError is what must surface below.
        }
      }
      throw turnError;
    } finally {
      runningChildren.delete(runKey);
      permissions.cancelTurn(binding);
      runningTurns.delete(runKey);
    }
    const after = listHistory(root, { projectId }).head;
    const seqRange = after > before ? [before, after] : null;
    if (seqRange) sseEvent(res, "op-committed", { seqRange });
    appendTurn(root, { projectId, role: "assistant", text: result.text, seqRange: seqRange || undefined });
    writeChatState(root, { projectId, sessionId: result.sessionId });
    sseEvent(res, "final", { text: result.text, sessionId: result.sessionId, seqRange, flags: result.flags });
  } catch (error) {
    runningChildren.delete(runKey);
    permissions.cancelTurn(binding);
    runningTurns.delete(runKey);
    sseEvent(res, "error", { message: errorMessage(error) });
  } finally {
    res.end();
  }
}

// `transport`, when given, is forwarded into every runChatTurn call — the ONE seam tests
// use to keep codex out of the suite (mirrors createCanvasApi(root)'s own factory shape;
// Production deliberately leaves the legacy transport unset, so it fails closed until T0351.
export function createChatApi(root, {
  transport,
  launchToken = randomBytes(32).toString("base64url"),
  allowedHosts = [],
  permissionTtlMs,
} = {}) {
  // One entry per store-qualified project with a turn currently in flight — Map, not a
  // single slot, so a stuck/slow project never blocks chat on a different project.
  const runningChildren = new Map();
  const runningTurns = new Map();
  const permissions = createPermissionBroker(permissionTtlMs === undefined ? {} : { ttlMs: permissionTtlMs });
  const security = { launchToken, allowedHosts: new Set(allowedHosts) };

  return async function handleChatApi(req, res, url) {
    const parts = url.pathname.split("/").filter(Boolean); // ["api","chat","projects", id, sub]
    if (parts[0] !== "api" || parts[1] !== "chat") {
      sendJson(res, 404, { error: "not found" });
      return true;
    }

    if (parts.length === 3 && parts[2] === "bootstrap") {
      if (req.method !== "GET") sendJson(res, 405, { error: "method not allowed" });
      else if (!bootstrapRequestAllowed(req, security.allowedHosts)) sendJson(res, 403, { error: "chat request rejected" });
      else sendJson(res, 200, { token: security.launchToken }, { "cache-control": "no-store" });
      return true;
    }

    const isProjectRoute = parts[2] === "projects" && parts.length === 5;
    const isDecisionRoute = parts[2] === "projects" && parts.length === 7
      && parts[4] === "permissions" && parts[6] === "decision";
    if (!isProjectRoute && !isDecisionRoute) {
      sendJson(res, 404, { error: "not found" });
      return true;
    }
    const projectId = decodeURIComponent(parts[3]);
    const sub = parts[4];
    let activeStore;
    try {
      activeStore = selectCanvasStore(root, chatStoreRequestArgs(req, url));
    } catch (error) {
      sendJson(res, 400, { error: errorMessage(error) });
      return true;
    }

    if (isDecisionRoute) {
      if (req.method !== "POST") {
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }
      if (!authorizeMutation(req, res, security)) return true;
      try {
        const body = await readJsonBody(req);
        const settled = permissions.decide({
          storeId: activeStore.storeId,
          projectId,
          permissionId: decodeURIComponent(parts[5]),
          decision: body.decision,
        });
        sendJson(res, 200, { ok: true, id: settled.id, state: settled.state });
      } catch (error) {
        sendJson(res, 409, { error: errorMessage(error) });
      }
      return true;
    }

    if (sub === "message") {
      if (req.method !== "POST") {
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }
      if (!authorizeMutation(req, res, security)) return true;
      let body;
      try {
        body = await readJsonBody(req);
      } catch (error) {
        sendJson(res, 400, { error: errorMessage(error) });
        return true;
      }
      await withCanvasStore(activeStore, () => handleMessage(
        root, activeStore, projectId, body, res, transport, runningChildren, runningTurns, permissions,
      ));
      return true;
    }

    if (sub === "cancel") {
      if (req.method !== "POST") {
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }
      if (!authorizeMutation(req, res, security)) return true;
      const runKey = chatRunKey(activeStore, projectId);
      const turn = runningTurns.get(runKey);
      if (turn) {
        turn.cancelled = true;
        permissions.cancelTurn({ storeId: activeStore.storeId, projectId, turnId: turn.turnId });
      }
      const child = runningChildren.get(runKey);
      if (child) {
        child.kill("SIGTERM");
        runningChildren.delete(runKey);
      }
      sendJson(res, 200, { ok: true, cancelled: Boolean(turn || child) });
      return true;
    }

    if (sub === "transcript") {
      if (req.method !== "GET") {
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }
      // Documented choice (never 404 here): a project with no chat yet has no
      // chat/transcript.jsonl on disk, which readTranscript reads as "nothing yet" — same
      // tolerant-read stance as store.mjs's own readErrors/readJournal for their sidecar
      // files. Always 200; an empty array IS the "sane" answer for a fresh project.
      sendJson(res, 200, { transcript: withCanvasStore(activeStore, () => readTranscript(root, { projectId })) });
      return true;
    }

    if (sub === "clear") {
      if (req.method !== "POST") {
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }
      if (!authorizeMutation(req, res, security)) return true;
      try {
        const result = withCanvasStore(activeStore, () => clearConversation(root, { projectId }));
        sendJson(res, 200, { ok: true, ...result });
      } catch (error) {
        sendJson(res, 400, { error: errorMessage(error) });
      }
      return true;
    }

    sendJson(res, 404, { error: "not found" });
    return true;
  };
}
