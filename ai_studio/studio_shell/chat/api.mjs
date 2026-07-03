// Chat HTTP/SSE adapter (T0242 increment 3). Studio Shell mounts this handler on the
// /api/chat/ prefix, beside /api/canvas/ (server.mjs) — bind stays 127.0.0.1, the SAME
// local-only trust boundary as the rest of studio_shell (server.mjs:162): this API can
// spawn an UNSANDBOXED codex process per message (agent.mjs's
// --dangerously-bypass-approvals-and-sandbox, R2), so it must never be reachable from
// anything but the lead's own machine.
//
// Routes:
//   POST /api/chat/projects/<id>/message    {text, selection?}   -> SSE stream
//   POST /api/chat/projects/<id>/cancel                          -> kill the CURRENT turn's child
//   GET  /api/chat/projects/<id>/transcript                      -> the jsonl parsed to JSON
//   POST /api/chat/projects/<id>/clear                           -> archive + reset session
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
import { listHistory } from "../../assets/canvas/ops.mjs";

// Chat message bodies are small (text + a handful of selection refs) — nowhere near
// image-upload size, so a much tighter cap than the canvas API's 20MB image cap.
const maxBodyBytes = 256 * 1024;

function sendJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
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

// Runs one full chat turn over an already-open SSE stream. `runningChildren` is the
// per-project Map<projectId, ChildProcess> the cancel route reads — populated via
// runChatTurn's `onChild` seam, which the default transport (agent.mjs) calls synchronously
// right after spawn.
async function handleMessage(root, projectId, body, res, transport, runningChildren) {
  sseHead(res);
  sseEvent(res, "progress", { message: "spawn started" });
  const text = body && typeof body.text === "string" ? body.text.trim() : "";
  try {
    if (!text) throw new Error("message requires non-empty text");
    // v1 keeps this simple: reject a second concurrent Send rather than queue it (design
    // doc section 4's Cancel/kill note) — one running child per project at a time.
    if (runningChildren.has(projectId)) {
      throw new Error(`a turn is already running for project ${projectId} — cancel it or wait for it to finish`);
    }
    const context = buildChatContext(root, { projectId, selection: body.selection || [] });
    const before = listHistory(root, { projectId }).head;
    appendTurn(root, { projectId, role: "user", text });
    const state = readChatState(root, { projectId });
    const onChild = (child) => runningChildren.set(projectId, child);
    let result;
    try {
      result = await runChatTurn({ context, message: text, sessionId: state.sessionId, transport, onChild });
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
      runningChildren.delete(projectId);
    }
    const after = listHistory(root, { projectId }).head;
    const seqRange = after > before ? [before, after] : null;
    if (seqRange) sseEvent(res, "op-committed", { seqRange });
    appendTurn(root, { projectId, role: "assistant", text: result.text, seqRange: seqRange || undefined });
    writeChatState(root, { projectId, sessionId: result.sessionId });
    sseEvent(res, "final", { text: result.text, sessionId: result.sessionId, seqRange, flags: result.flags });
  } catch (error) {
    runningChildren.delete(projectId);
    sseEvent(res, "error", { message: errorMessage(error) });
  } finally {
    res.end();
  }
}

// `transport`, when given, is forwarded into every runChatTurn call — the ONE seam tests
// use to keep codex out of the suite (mirrors createCanvasApi(root)'s own factory shape;
// server.mjs calls createChatApi(root) with no override, so production always gets
// agent.mjs's default codex-exec transport).
export function createChatApi(root, { transport } = {}) {
  // One entry per project with a turn currently in flight — Map, not a single slot, so a
  // stuck/slow project never blocks chat on a different project.
  const runningChildren = new Map();

  return async function handleChatApi(req, res, url) {
    const parts = url.pathname.split("/").filter(Boolean); // ["api","chat","projects", id, sub]
    if (parts[0] !== "api" || parts[1] !== "chat" || parts[2] !== "projects" || parts.length !== 5) {
      sendJson(res, 404, { error: "not found" });
      return true;
    }
    const projectId = decodeURIComponent(parts[3]);
    const sub = parts[4];

    if (sub === "message") {
      if (req.method !== "POST") {
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }
      let body;
      try {
        body = await readJsonBody(req);
      } catch (error) {
        sendJson(res, 400, { error: errorMessage(error) });
        return true;
      }
      await handleMessage(root, projectId, body, res, transport, runningChildren);
      return true;
    }

    if (sub === "cancel") {
      if (req.method !== "POST") {
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }
      const child = runningChildren.get(projectId);
      if (child) {
        child.kill("SIGTERM");
        runningChildren.delete(projectId);
      }
      sendJson(res, 200, { ok: true, cancelled: Boolean(child) });
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
      sendJson(res, 200, { transcript: readTranscript(root, { projectId }) });
      return true;
    }

    if (sub === "clear") {
      if (req.method !== "POST") {
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }
      try {
        const result = clearConversation(root, { projectId });
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
