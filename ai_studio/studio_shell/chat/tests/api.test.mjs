// Chat HTTP/SSE API tests (T0242 increment 3), an in-process invoke harness (no port
// binding) mirroring assets/canvas/tests/api.test.mjs's own approach. EVERY test injects a
// fake transport via createChatApi(root, {transport}) — codex NEVER spawns in this suite.
// Run: node --test ai_studio/studio_shell/chat/tests/api.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { fileURLToPath, URL } from "node:url";
import { createChatApi as createChatApiImpl, ensurePermissionAllowed } from "../api.mjs";
import { addImage, createProject } from "../../../assets/canvas/ops.mjs";
import { solidPng } from "../../../assets/canvas/tests/png_fixture.mjs";
import { selectCanvasStore, withCanvasStore } from "../../../assets/canvas/stores.mjs";

// Metadata-only ops (createProject, no text/font reads), so any placeholder root works —
// same convention as the canvas suite's own metadata-only tests.
const ROOT = "C:/unused-repo-root";
const TEST_TOKEN = "launch-secret";
const TEST_SECURITY_HEADERS = {
  host: "localhost",
  origin: "http://localhost",
  "content-type": "application/json",
  "x-ai-studio-chat-token": TEST_TOKEN,
};

function createChatApi(root, options = {}) {
  const configured = { launchToken: TEST_TOKEN, allowedHosts: ["localhost"], ...options };
  if (configured.transport && configured.transport.approvalAware === undefined) {
    configured.transport.approvalAware = true;
  }
  return createChatApiImpl(root, configured);
}

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

function tempPrivateWorkspace(t) {
  const root = mkdtempSync(join(tmpdir(), "chat-api-private-root-"));
  const publicRoot = join(root, "public-canvas");
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = publicRoot;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(root, { recursive: true, force: true });
  });
  return { root, publicRoot };
}

function ensurePrivateGameMount(root, gameId = "secret-game") {
  const gameRoot = join(root, "games", gameId);
  mkdirSync(gameRoot, { recursive: true });
  execFileSync("git", ["init"], { cwd: root, encoding: "utf8" });
  execFileSync("git", ["init"], { cwd: gameRoot, encoding: "utf8" });
  mkdirSync(join(root, ".git", "info"), { recursive: true });
  writeFileSync(
    join(root, ".git", "info", "exclude"),
    `ai_studio/workspace/games.local.json\ngames/${gameId}/\n`,
    "utf8",
  );
  mkdirSync(join(root, "ai_studio", "workspace"), { recursive: true });
  writeFileSync(join(root, "ai_studio", "workspace", "games.local.json"), JSON.stringify({
    schema: "ai_studio.workspace.games.local.v1",
    games: [{
      schemaVersion: 1,
      storeId: `game:${gameId}`,
      kind: "game",
      gameId,
      root: `games/${gameId}`,
      visibility: "private",
      gitRoot: `games/${gameId}`,
      commitPolicy: "nested-private",
      enabledStores: ["canvas"],
    }],
  }, null, 2) + "\n", "utf8");
  return {
    gameId,
    gameRoot,
    storeId: `game:${gameId}`,
    canvasRoot: join(gameRoot, ".ai_studio", "canvas", "projects"),
  };
}

function createPrivateProject(root, storeId, title = "Private Chat Fixture") {
  return withCanvasStore(selectCanvasStore(root, { store: storeId }), () => createProject(root, { title }));
}

// Minimal req/res doubles for a plain JSON (non-SSE) route: collects Buffer chunks and
// resolves with the parsed status/headers/body on res.end() — mirrors api.test.mjs's own
// invokeApi.
function invokeJson(handler, method, path, body, headers = {}) {
  return new Promise((resolveCall) => {
    const req = new EventEmitter();
    req.method = method;
    const requestHeaders = method === "POST" ? { ...TEST_SECURITY_HEADERS, ...headers } : headers;
    req.headers = Object.fromEntries(Object.entries(requestHeaders).map(([key, value]) => [key.toLowerCase(), value]));
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
function invokeSSE(handler, method, path, body, headers = {}, onEvent) {
  return new Promise((resolveCall) => {
    const req = new EventEmitter();
    req.method = method;
    const requestHeaders = method === "POST" ? { ...TEST_SECURITY_HEADERS, ...headers } : headers;
    req.headers = Object.fromEntries(Object.entries(requestHeaders).map(([key, value]) => [key.toLowerCase(), value]));
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
        const block = String(chunk);
        writes.push(block);
        const eventMatch = /^event: (.+)\n/.exec(block);
        const dataMatch = /\ndata: (.+)\n\n$/.exec(block);
        if (onEvent && eventMatch && dataMatch) onEvent(eventMatch[1], JSON.parse(dataMatch[1]));
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

test("chat mutations require exact launch token, Origin, Host, and JSON before SSE", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const handler = createChatApi(ROOT, {
    launchToken: "launch-secret",
    allowedHosts: ["localhost"],
    transport: Object.assign(async () => ({ text: "no", sessionId: "s" }), { approvalAware: true }),
  });
  const missing = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "hi" }, {
    host: "localhost", origin: "http://localhost", "content-type": "application/json", "x-ai-studio-chat-token": "",
  });
  assert.equal(missing.status, 403);
  assert.deepEqual(missing.events, []);
  const wrongOrigin = await invokeJson(handler, "POST", `/api/chat/projects/${projectId}/clear`, {}, {
    host: "localhost", origin: "http://evil.test", "content-type": "application/json", "x-ai-studio-chat-token": "launch-secret",
  });
  assert.equal(wrongOrigin.status, 403);
  const wrongType = await invokeJson(handler, "POST", `/api/chat/projects/${projectId}/clear`, {}, {
    host: "localhost", origin: "http://localhost", "content-type": "text/plain", "x-ai-studio-chat-token": "launch-secret",
  });
  assert.equal(wrongType.status, 415);
});

test("bootstrap returns the launch token only for an allowed same-origin host", async () => {
  const handler = createChatApi(ROOT, { launchToken: "launch-secret", allowedHosts: ["localhost"] });
  const allowed = await invokeJson(handler, "GET", "/api/chat/bootstrap", undefined, { host: "localhost", origin: "http://localhost" });
  assert.equal(allowed.status, 200);
  assert.equal(allowed.json.token, "launch-secret");
  assert.equal(allowed.headers["cache-control"], "no-store");
  const browserGet = await invokeJson(handler, "GET", "/api/chat/bootstrap", undefined, {
    host: "localhost", "sec-fetch-site": "same-origin",
  });
  assert.equal(browserGet.status, 200, "same-origin browser GETs do not reliably include Origin");
  const crossSite = await invokeJson(handler, "GET", "/api/chat/bootstrap", undefined, {
    host: "localhost", "sec-fetch-site": "cross-site",
  });
  assert.equal(crossSite.status, 403);
  const denied = await invokeJson(handler, "GET", "/api/chat/bootstrap", undefined, { host: "evil.test", origin: "http://evil.test" });
  assert.equal(denied.status, 403);
});

test("permission SSE preserves exact request and allow decision gates mutation", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const exactRequest = { method: "tools/call", params: { name: "shell", arguments: { command: "sentinel" } } };
  let executedCommand = null;
  const transport = Object.assign(async ({ requestPermission }) => {
    const permission = requestPermission(exactRequest);
    exactRequest.params.arguments.command = "tampered after request";
    const approved = await permission;
    executedCommand = approved.exactRequest.params.arguments.command;
    return { text: "allowed", sessionId: "permission-session" };
  }, { approvalAware: true });
  const security = { host: "localhost", origin: "http://localhost", "content-type": "application/json", "x-ai-studio-chat-token": "launch-secret" };
  const handler = createChatApi(ROOT, { launchToken: "launch-secret", allowedHosts: ["localhost"], transport });
  let decisionPromise;
  const result = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "mutate" }, security, (event, data) => {
    if (event === "permission-request") {
      assert.deepEqual(data.exactRequest, exactRequest);
      assert.equal(executedCommand, null);
      decisionPromise = invokeJson(handler, "POST", `/api/chat/projects/${projectId}/permissions/${data.id}/decision`, { decision: "allow" }, security);
    }
  });
  await decisionPromise;
  assert.equal(executedCommand, "sentinel");
  assert.deepEqual(result.events.map((event) => event.event), ["progress", "permission-request", "permission-decision", "final"]);
});

test("simultaneous cancel and allow never lets the transport mutate", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  let mutated = false;
  let actions;
  const transport = Object.assign(async ({ requestPermission }) => {
    await requestPermission({ command: "sentinel" });
    mutated = true;
    return { text: "must not run", sessionId: "race" };
  }, { approvalAware: true });
  const handler = createChatApi(ROOT, { transport });
  const result = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "race" }, {}, (event, data) => {
    if (event !== "permission-request") return;
    actions = Promise.all([
      invokeJson(
        handler,
        "POST",
        `/api/chat/projects/${projectId}/permissions/${data.id}/decision`,
        { decision: "allow" },
      ),
      invokeJson(handler, "POST", `/api/chat/projects/${projectId}/cancel`, {}),
    ]);
  });
  await actions;
  assert.equal(mutated, false);
  assert.equal(result.events.at(-1).event, "error");
  assert.match(result.events.at(-1).data.message, /cancelled/);
});

test("an allowed settlement is refused when cancel wins before transport continuation", () => {
  assert.throws(
    () => ensurePermissionAllowed({ cancelled: true }, { state: "allowed", exactRequest: { command: "sentinel" } }),
    /permission cancelled/,
  );
});

test("deny, cancel, and expiry settle pending permission before mutation or child spawn", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const exactRequest = { capability: "opaque", scope: { command: "sentinel" } };

  for (const terminalState of ["denied", "cancelled", "expired"]) {
    let mutated = false;
    let childSpawned = false;
    let actionPromise;
    const transport = Object.assign(async ({ requestPermission, onChild }) => {
      await requestPermission(exactRequest);
      childSpawned = true;
      onChild({ kill() {} });
      mutated = true;
      return { text: "must not run", sessionId: "blocked" };
    }, { approvalAware: true });
    const handler = createChatApi(ROOT, {
      transport,
      permissionTtlMs: terminalState === "expired" ? 5 : 1000,
    });
    const result = await invokeSSE(
      handler,
      "POST",
      `/api/chat/projects/${projectId}/message`,
      { text: terminalState },
      {},
      (event, data) => {
        if (event !== "permission-request") return;
        if (terminalState === "denied") {
          actionPromise = invokeJson(
            handler,
            "POST",
            `/api/chat/projects/${projectId}/permissions/${data.id}/decision`,
            { decision: "deny" },
          );
        } else if (terminalState === "cancelled") {
          actionPromise = invokeJson(handler, "POST", `/api/chat/projects/${projectId}/cancel`, {});
        }
      },
    );
    if (actionPromise) await actionPromise;
    assert.equal(mutated, false, `${terminalState} must gate the sentinel mutation`);
    assert.equal(childSpawned, false, `${terminalState} must settle before child spawn`);
    const decision = result.events.find((event) => event.event === "permission-decision");
    assert.equal(decision.data.state, terminalState);
    assert.equal(result.events.at(-1).event, "error");
  }
});

test("permission decisions are same-store/project bound and stale decisions are refused", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const otherProjectId = seedProject();
  const transport = Object.assign(async ({ requestPermission }) => {
    await requestPermission({ arbitrary: ["exact", "request"] });
    return { text: "denied", sessionId: "never" };
  }, { approvalAware: true });
  const handler = createChatApi(ROOT, { transport });
  let wrongProject;
  let denied;
  const result = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "bind" }, {}, (event, data) => {
    if (event !== "permission-request") return;
    wrongProject = invokeJson(
      handler,
      "POST",
      `/api/chat/projects/${otherProjectId}/permissions/${data.id}/decision`,
      { decision: "allow" },
    ).then((response) => {
      denied = invokeJson(
        handler,
        "POST",
        `/api/chat/projects/${projectId}/permissions/${data.id}/decision`,
        { decision: "deny" },
      );
      return response;
    });
  });
  assert.equal((await wrongProject).status, 409);
  assert.equal((await denied).status, 200);
  const permissionId = result.events.find((event) => event.event === "permission-request").data.id;
  const stale = await invokeJson(
    handler,
    "POST",
    `/api/chat/projects/${projectId}/permissions/${permissionId}/decision`,
    { decision: "allow" },
  );
  assert.equal(stale.status, 409);
});

test("default legacy codex-exec transport fails closed before spawning", async (t) => {
  tempProjects(t);
  const projectId = seedProject();
  const handler = createChatApiImpl(ROOT, { launchToken: TEST_TOKEN, allowedHosts: ["localhost"] });
  const result = await invokeSSE(handler, "POST", `/api/chat/projects/${projectId}/message`, { text: "mutate" });
  assert.deepEqual(result.events.map((event) => event.event), ["progress", "error"]);
  assert.match(result.events[1].data.message, /not approval-aware/);
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

test("chat API routes project chat through the selected private Canvas store header", async (t) => {
  const { root, publicRoot } = tempPrivateWorkspace(t);
  const privateStore = ensurePrivateGameMount(root);
  const privateProject = createPrivateProject(root, privateStore.storeId);
  const privateHeaders = { "x-ai-studio-store": privateStore.storeId };
  const seenContexts = [];
  const handler = createChatApi(root, {
    transport: async ({ context }) => {
      seenContexts.push(context);
      return { text: "private reply", sessionId: "sess-private" };
    },
  });

  const publicMiss = await invokeSSE(handler, "POST", `/api/chat/projects/${privateProject.id}/message`, { text: "hi" });
  assert.deepEqual(
    publicMiss.events.map((event) => event.event),
    ["progress", "error"],
  );
  assert.match(publicMiss.events[1].data.message, /canvas project not found/);

  const privateResult = await invokeSSE(
    handler,
    "POST",
    `/api/chat/projects/${privateProject.id}/message`,
    { text: "hi private" },
    privateHeaders,
  );
  assert.deepEqual(
    privateResult.events.map((event) => event.event),
    ["progress", "final"],
  );
  assert.equal(privateResult.events[1].data.text, "private reply");
  assert.equal(seenContexts.length, 1);
  assert.equal(seenContexts[0].storeId, privateStore.storeId);
  assert.equal(seenContexts[0].visibility, "private");
  assert.equal(seenContexts[0].qualifiedId, `${privateStore.storeId}:${privateProject.id}`);

  const privateTranscriptPath = join(privateStore.canvasRoot, privateProject.id, "chat", "transcript.jsonl");
  assert.equal(existsSync(privateTranscriptPath), true, "private transcript is stored under the game-owned Canvas store");
  assert.equal(
    existsSync(join(publicRoot, privateProject.id, "chat", "transcript.jsonl")),
    false,
    "private transcript is not mirrored into the public Studio Canvas store",
  );

  const privateTranscript = await invokeJson(
    handler,
    "GET",
    `/api/chat/projects/${privateProject.id}/transcript`,
    undefined,
    privateHeaders,
  );
  assert.deepEqual(privateTranscript.json.transcript.map((row) => row.role), ["user", "assistant"]);

  const publicTranscript = await invokeJson(handler, "GET", `/api/chat/projects/${privateProject.id}/transcript`);
  assert.deepEqual(publicTranscript.json.transcript, []);

  const mismatch = await invokeJson(
    handler,
    "GET",
    `/api/chat/projects/${privateProject.id}/transcript?store=studio`,
    undefined,
    privateHeaders,
  );
  assert.equal(mismatch.status, 400);
  assert.match(mismatch.json.error, /mismatch/);
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
