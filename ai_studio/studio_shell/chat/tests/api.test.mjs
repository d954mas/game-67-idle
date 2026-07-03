// Chat HTTP/SSE API tests (T0242 increment 3), an in-process invoke harness (no port
// binding) mirroring assets/canvas/tests/api.test.mjs's own approach. EVERY test injects a
// fake transport via createChatApi(root, {transport}) — codex NEVER spawns in this suite.
// Run: node --test ai_studio/studio_shell/chat/tests/api.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { fileURLToPath, URL } from "node:url";
import { createChatApi } from "../api.mjs";
import { addImage, createProject } from "../../../assets/canvas/ops.mjs";
import { solidPng } from "../../../assets/canvas/tests/png_fixture.mjs";

// Metadata-only ops (createProject, no text/font reads), so any placeholder root works —
// same convention as the canvas suite's own metadata-only tests.
const ROOT = "C:/unused-repo-root";

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "chat-api-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// Minimal req/res doubles for a plain JSON (non-SSE) route: collects Buffer chunks and
// resolves with the parsed status/headers/body on res.end() — mirrors api.test.mjs's own
// invokeApi.
function invokeJson(handler, method, path, body) {
  return new Promise((resolveCall) => {
    const req = new EventEmitter();
    req.method = method;
    req.setEncoding = () => {};
    req.destroy = () => {};
    const chunks = [];
    const res = {
      statusCode: 0,
      headers: {},
      writeHead(status, headers) {
        this.statusCode = status;
        this.headers = headers || {};
      },
      write(chunk) {
        chunks.push(Buffer.from(chunk));
        return true;
      },
      end(chunk) {
        if (chunk !== undefined && chunk !== null && chunk !== "") chunks.push(Buffer.from(chunk));
        const buffer = Buffer.concat(chunks);
        let json = null;
        try {
          json = JSON.parse(buffer.toString("utf8"));
        } catch {
          // some routes never reach here in this suite; leave json null
        }
        resolveCall({ status: this.statusCode, headers: this.headers, json });
      },
    };
    const url = new URL(`http://localhost${path}`);
    handler(req, res, url);
    if (body !== undefined) {
      process.nextTick(() => {
        req.emit("data", JSON.stringify(body));
        req.emit("end");
      });
    } else {
      process.nextTick(() => req.emit("end"));
    }
  });
}

// SSE double: collects every res.write() call as one raw string in order (each is one
// `event: X\ndata: {...}\n\n` block), parsed into {event, data} pairs for assertions.
function invokeSSE(handler, method, path, body) {
  return new Promise((resolveCall) => {
    const req = new EventEmitter();
    req.method = method;
    req.setEncoding = () => {};
    req.destroy = () => {};
    const writes = [];
    const res = {
      statusCode: 0,
      headers: {},
      writeHead(status, headers) {
        this.statusCode = status;
        this.headers = headers || {};
      },
      write(chunk) {
        writes.push(String(chunk));
        return true;
      },
      end() {
        const events = writes.map((block) => {
          const eventMatch = /^event: (.+)\n/.exec(block);
          const dataMatch = /\ndata: (.+)\n\n$/.exec(block);
          return { event: eventMatch ? eventMatch[1] : null, data: dataMatch ? JSON.parse(dataMatch[1]) : null };
        });
        resolveCall({ status: this.statusCode, headers: this.headers, events });
      },
    };
    const url = new URL(`http://localhost${path}`);
    handler(req, res, url);
    if (body !== undefined) {
      process.nextTick(() => {
        req.emit("data", JSON.stringify(body));
        req.emit("end");
      });
    } else {
      process.nextTick(() => req.emit("end"));
    }
  });
}

function seedProject() {
  return createProject(ROOT, { title: "Chat Fixture" }).id;
}

// ---- POST .../message: SSE event sequence -----------------------------------------------

test("POST .../message: progress -> final on a clean turn with no head advance", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const fakeTransport = async () => ({ text: "hello back", sessionId: "sess-1" });
  const handler = createChatApi(ROOT, { transport: fakeTransport });
  const result = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "hi" });
  assert.equal(result.status, 200);
  assert.match(result.headers["content-type"], /text\/event-stream/);
  assert.deepEqual(
    result.events.map((e) => e.event),
    ["progress", "final"],
  );
  assert.equal(result.events[1].data.text, "hello back");
  assert.equal(result.events[1].data.sessionId, "sess-1");
  assert.equal(result.events[1].data.seqRange, null);
  assert.deepEqual(result.events[1].data.flags, []);
});

test("POST .../message: emits op-committed with the seq range BEFORE final when the transport's turn advances the head", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  // A fake transport that mutates the project itself (standing in for the codex turn
  // actually running canvas CLI ops via cli.mjs) — one journaled addImage bumps
  // history_seq exactly the way a real op would, for this test's purpose (listHistory's
  // head).
  const fakeTransport = async () => {
    addImage(ROOT, projectId, { name: "new.png", bytes: solidPng(4, 3) });
    return { text: "added an image", sessionId: "sess-2" };
  };
  const handler = createChatApi(ROOT, { transport: fakeTransport });
  const result = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "add an image" });
  assert.deepEqual(
    result.events.map((e) => e.event),
    ["progress", "op-committed", "final"],
  );
  assert.deepEqual(result.events[1].data.seqRange, [0, 1]);
  assert.deepEqual(result.events[2].data.seqRange, [0, 1]);
});

test("POST .../message: transport error surfaces as an SSE error event, never thrown to the caller", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const fakeTransport = async () => {
    throw new Error("codex exec exited 1: boom");
  };
  const handler = createChatApi(ROOT, { transport: fakeTransport });
  const result = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "hi" });
  assert.equal(result.status, 200); // SSE headers already committed before the error is known
  assert.deepEqual(
    result.events.map((e) => e.event),
    ["progress", "error"],
  );
  assert.match(result.events[1].data.message, /boom/);
});

test("POST .../message: empty text is a loud SSE error, no transport call", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  let called = false;
  const fakeTransport = async () => {
    called = true;
    return { text: "x", sessionId: "s" };
  };
  const handler = createChatApi(ROOT, { transport: fakeTransport });
  const result = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "   " });
  assert.equal(called, false);
  assert.deepEqual(
    result.events.map((e) => e.event),
    ["progress", "error"],
  );
  assert.match(result.events[1].data.message, /non-empty text/);
});

test("POST .../message: unknown project surfaces as an SSE error (buildChatContext's own loud failure)", async (t) => {
  tempProjects(t);
  const handler = createChatApi(ROOT, { transport: async () => ({ text: "x", sessionId: "s" }) });
  const result = await invokeSSE(handler, "POST", "/api/chat/projects/nope/message", { text: "hi" });
  assert.deepEqual(
    result.events.map((e) => e.event),
    ["progress", "error"],
  );
  assert.match(result.events[1].data.message, /canvas project not found/);
});

test("POST .../message: a resumed conversation persists the transport's sessionId and passes the PRIOR one in", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const seen = [];
  const handler = createChatApi(ROOT, {
    transport: async ({ sessionId }) => {
      seen.push(sessionId);
      return { text: "turn " + seen.length, sessionId: "sess-fixed" };
    },
  });
  await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "first" });
  await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "second" });
  assert.deepEqual(seen, [null, "sess-fixed"]);
  const transcript = await invokeJson(handler, "GET", `/api/chat/projects/${projectId}/transcript`);
  const roles = transcript.json.transcript.map((row) => row.role);
  assert.deepEqual(roles, ["user", "assistant", "user", "assistant"]);
});

// ---- POST .../cancel ----------------------------------------------------------------------

test("POST .../cancel: kills the tracked child of the CURRENT in-flight turn", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  let killed = false;
  const fakeChild = { kill: (signal) => { killed = signal; } };
  let releaseTurn;
  const turnGate = new Promise((r) => { releaseTurn = r; });
  const handler = createChatApi(ROOT, {
    transport: async ({ onChild }) => {
      onChild(fakeChild);
      await turnGate; // stay "in flight" until the test cancels
      return { text: "too late", sessionId: "s1" };
    },
  });
  const messagePromise = invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "long op" });
  // Let the message handler reach the point where onChild has registered the fake child.
  await new Promise((r) => setTimeout(r, 10));
  const cancelResult = await invokeJson(handler, "POST", `/api/chat/projects/${projectId}/cancel`, {});
  assert.deepEqual(cancelResult.json, { ok: true, cancelled: true });
  assert.equal(killed, "SIGTERM");
  releaseTurn();
  await messagePromise;
});

test("POST .../cancel: no turn running -> ok:true, cancelled:false", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const handler = createChatApi(ROOT, { transport: async () => ({ text: "x", sessionId: "s" }) });
  const result = await invokeJson(handler, "POST", `/api/chat/projects/${projectId}/cancel`, {});
  assert.deepEqual(result.json, { ok: true, cancelled: false });
});

// ---- GET .../transcript --------------------------------------------------------------------

test("GET .../transcript: 200 with an empty array for a project with no chat yet (never 404)", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const handler = createChatApi(ROOT, {});
  const result = await invokeJson(handler, "GET", `/api/chat/projects/${projectId}/transcript`);
  assert.equal(result.status, 200);
  assert.deepEqual(result.json, { transcript: [] });
});

test("GET .../transcript: roundtrips appended turns in order after a message", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const handler = createChatApi(ROOT, { transport: async () => ({ text: "reply text", sessionId: "sess-1" }) });
  await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "hello" });
  const result = await invokeJson(handler, "GET", `/api/chat/projects/${projectId}/transcript`);
  assert.equal(result.json.transcript.length, 2);
  assert.equal(result.json.transcript[0].role, "user");
  assert.equal(result.json.transcript[0].text, "hello");
  assert.equal(result.json.transcript[1].role, "assistant");
  assert.equal(result.json.transcript[1].text, "reply text");
});

// ---- POST .../clear -------------------------------------------------------------------------

test("POST .../clear: archives the transcript and nulls the session; a later message starts a FRESH session", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const seenSessionIds = [];
  const handler = createChatApi(ROOT, {
    transport: async ({ sessionId }) => {
      seenSessionIds.push(sessionId);
      return { text: "ok", sessionId: "sess-A" };
    },
  });
  await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "first" });

  const clearResult = await invokeJson(handler, "POST", `/api/chat/projects/${projectId}/clear`, {});
  assert.equal(clearResult.status, 200);
  assert.equal(clearResult.json.ok, true);
  assert.equal(clearResult.json.sessionId, null);
  assert.ok(clearResult.json.archivedAs);

  const transcriptAfterClear = await invokeJson(handler, "GET", `/api/chat/projects/${projectId}/transcript`);
  assert.deepEqual(transcriptAfterClear.json.transcript, []);

  await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "second, new conversation" });
  assert.deepEqual(seenSessionIds, [null, null]); // both were first-turns — clear really reset it
});

// ---- routing basics -------------------------------------------------------------------------

test("unmatched route -> 404; wrong method on a matched route -> 405", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const handler = createChatApi(ROOT, {});
  const notFound = await invokeJson(handler, "GET", "/api/chat/bogus", undefined);
  assert.equal(notFound.status, 404);
  const wrongMethod = await invokeJson(handler, "GET", `/api/chat/projects/${projectId}/message`, undefined);
  assert.equal(wrongMethod.status, 405);
});
