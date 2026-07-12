import test from "node:test";
import assert from "node:assert/strict";
import { EventEmitter } from "node:events";
import { PassThrough } from "node:stream";
import {
  createAppServerTransport,
  killAppServerProcessTree,
  resolveCodexAppServerCommand,
} from "../app_server.mjs";

function fakeAppServer(onRequest) {
  const child = new EventEmitter();
  child.stdin = new PassThrough();
  child.stdout = new PassThrough();
  child.stderr = new PassThrough();
  child.exitCode = null;
  child.killed = false;
  child.kill = (signal = "SIGTERM") => {
    if (child.killed) return false;
    child.killed = true;
    child.exitCode = 0;
    queueMicrotask(() => child.emit("close", 0, signal));
    return true;
  };
  let input = "";
  child.stdin.on("data", (chunk) => {
    input += chunk;
    for (;;) {
      const newline = input.indexOf("\n");
      if (newline < 0) break;
      const line = input.slice(0, newline);
      input = input.slice(newline + 1);
      onRequest(JSON.parse(line), (message) => child.stdout.write(`${JSON.stringify(message)}\n`), child);
    }
  });
  return child;
}

function happyServer({ threadId = "thread-new", approvals } = {}) {
  return fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: { userAgent: "fake" } });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: threadId } } });
    else if (request.method === "thread/resume") send({ id: request.id, result: { thread: { id: request.params.threadId } } });
    else if (request.method === "turn/start") {
      send({ id: request.id, result: { turn: { id: "turn-1" } } });
      if (approvals) {
        send({ id: 900, method: approvals.method, params: approvals.params });
      } else {
        send({ method: "item/agentMessage/delta", params: { threadId, turnId: "turn-1", delta: "done " } });
        send({ method: "item/agentMessage/delta", params: { threadId, turnId: "turn-1", delta: "now" } });
        send({ method: "turn/completed", params: { threadId, turn: { id: "turn-1", status: "completed" } } });
      }
    } else if (request.id === 900) {
      approvals.responses.push(request);
      send({ method: "item/agentMessage/delta", params: { threadId, turnId: "turn-1", delta: "approved" } });
      send({ method: "turn/completed", params: { threadId, turn: { id: "turn-1", status: "completed" } } });
    }
  });
}

test("app-server first turn initializes subscription account, starts a thread, streams text, and persists thread.id", async () => {
  const requests = [];
  const progress = [];
  const child = happyServer({ threadId: "thread-new" });
  child.stdin.on("data", (chunk) => requests.push(JSON.parse(String(chunk).trim())));
  const transport = createAppServerTransport({ spawnProcess: () => child });
  const result = await transport({ prompt: "full prompt", sessionId: null, onProgress: (delta) => progress.push(delta) });
  assert.deepEqual(result, { text: "done now", sessionId: "thread-new" });
  assert.deepEqual(requests.map((item) => item.method), [
    "initialize", "initialized", "account/read", "thread/start", "turn/start",
  ]);
  assert.deepEqual(requests.at(-1).params.input, [{ type: "text", text: "full prompt" }]);
  assert.deepEqual(requests.find((item) => item.method === "account/read").params, { refreshToken: false });
  assert.deepEqual(requests.find((item) => item.method === "thread/start").params.sandbox, "read-only");
  assert.deepEqual(progress, ["done now"]);
  assert.equal(child.killed, false);
  await transport.shutdown();
  assert.equal(child.killed, true);
});

test("progress coalescing emits bounded delta batches instead of repeatedly copying the full answer", async () => {
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
    else if (request.method === "turn/start") {
      send({ id: request.id, result: { turn: { id: "u" } } });
      setImmediate(() => {
        for (let index = 0; index < 1000; index += 1) {
          send({ method: "item/agentMessage/delta", params: { threadId: "t", turnId: "u", delta: "abcdefghij" } });
        }
        send({ method: "turn/completed", params: { threadId: "t", turn: { id: "u", status: "completed" } } });
      });
    }
  });
  const batches = [];
  const result = await createAppServerTransport({ spawnProcess: () => child })({
    prompt: "x", onProgress: (batch) => batches.push(batch),
  });
  assert.equal(result.text.length, 10_000);
  assert.equal(batches.join(""), result.text);
  assert.ok(batches.length < 10);
  assert.ok(batches.every((batch) => batch.length <= 4100));
});

test("explicit Codex command overrides are deterministic and a missing JS path never falls back", () => {
  assert.deepEqual(resolveCodexAppServerCommand({
    env: { CODEX_BIN: "C:/tools/codex.exe" }, platform: "win32", pathExists: () => false,
  }), {
    command: "C:/tools/codex.exe",
    args: ["app-server", "--listen", "stdio://"],
  });
  assert.throws(
    () => resolveCodexAppServerCommand({
      env: { CODEX_APP_SERVER_JS: "C:/missing/codex.js" }, platform: "win32", pathExists: () => false,
    }),
    /configured CODEX_APP_SERVER_JS does not exist/,
  );
});

test("app-server resume uses persisted thread.id, not thread.sessionId", async () => {
  const requests = [];
  const child = happyServer({ threadId: "thread-existing" });
  child.stdin.on("data", (chunk) => requests.push(JSON.parse(String(chunk).trim())));
  const result = await createAppServerTransport({ spawnProcess: () => child })({
    message: "compact message", sessionId: "thread-existing",
  });
  assert.equal(result.sessionId, "thread-existing");
  assert.deepEqual(requests.find((item) => item.method === "thread/resume").params, { threadId: "thread-existing" });
});

for (const method of ["item/commandExecution/requestApproval", "item/fileChange/requestApproval"]) {
  test(`app-server preserves the exact opaque ${method} request until the broker allows it`, async () => {
    const approvals = { method, params: { threadId: "thread-new", turnId: "turn-1", itemId: "item-7", opaque: { keep: true } }, responses: [] };
    const child = happyServer({ approvals });
    let exact;
    const result = await createAppServerTransport({ spawnProcess: () => child })({
      prompt: "do it",
      requestPermission: async (request) => { exact = request; return { state: "allowed" }; },
    });
    assert.deepEqual(exact, { method, params: approvals.params });
    assert.deepEqual(approvals.responses, [{ id: 900, result: { decision: "accept" } }]);
    assert.equal(result.text, "approved");
  });
}

test("app-server denial is returned on the same request id and gates the turn", async () => {
  const approvals = {
    method: "item/commandExecution/requestApproval",
    params: { threadId: "thread-new", turnId: "turn-1", itemId: "item-9" },
    responses: [],
  };
  const child = happyServer({ approvals });
  const transport = createAppServerTransport({ spawnProcess: () => child });
  await assert.rejects(
    () => transport({ prompt: "do it", requestPermission: async () => { throw new Error("permission denied"); } }),
    /permission denied/,
  );
  assert.deepEqual(approvals.responses, [{ id: 900, result: { decision: "decline" } }]);
});

test("app-server cancel sends turn/interrupt before terminating the process", async () => {
  const requests = [];
  let child;
  child = fakeAppServer((request, send) => {
    requests.push(request);
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "thread-cancel" } } });
    else if (request.method === "turn/start") send({ id: request.id, result: { turn: { id: "turn-cancel" } } });
  });
  let handle;
  const pending = createAppServerTransport({ spawnProcess: () => child, cancelGraceMs: 0 })({
    prompt: "wait", onChild: (value) => { handle = value; },
  });
  await new Promise((resolve) => setImmediate(resolve));
  handle.kill("SIGTERM");
  await assert.rejects(pending, /interrupt timed out|exited|cancel/i);
  assert.equal(requests.some((item) => item.method === "turn/interrupt" && item.params.turnId === "turn-cancel"), true);
});

test("app-server lazily restarts after a process crash and never reuses the dead client", async () => {
  const first = happyServer({ threadId: "thread-first" });
  const second = happyServer({ threadId: "thread-second" });
  const children = [first, second];
  let spawns = 0;
  const transport = createAppServerTransport({ spawnProcess: () => children[spawns++] });
  assert.equal((await transport({ prompt: "one" })).sessionId, "thread-first");
  first.exitCode = 1;
  first.emit("close", 1, null);
  assert.equal((await transport({ prompt: "two" })).sessionId, "thread-second");
  assert.equal(spawns, 2);
  transport.shutdown();
});

test("app-server rejects a mismatched resumed thread id instead of silently switching continuity", async () => {
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/resume") send({ id: request.id, result: { thread: { id: "different" } } });
  });
  await assert.rejects(
    createAppServerTransport({ spawnProcess: () => child })({ message: "x", sessionId: "expected" }),
    /mismatched thread\.id/,
  );
});

test("app-server ignores another thread's events and accepts only the active turn", async () => {
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "ours" } } });
    else if (request.method === "turn/start") {
      send({ id: request.id, result: { turn: { id: "our-turn" } } });
      send({ method: "item/agentMessage/delta", params: { threadId: "other", turnId: "other-turn", delta: "wrong" } });
      send({ method: "turn/completed", params: { threadId: "other", turn: { id: "other-turn", status: "completed" } } });
      send({ method: "item/agentMessage/delta", params: { threadId: "ours", turnId: "our-turn", delta: "right" } });
      send({ method: "turn/completed", params: { threadId: "ours", turn: { id: "our-turn", status: "completed" } } });
    }
  });
  assert.equal((await createAppServerTransport({ spawnProcess: () => child })({ prompt: "x" })).text, "right");
});

test("app-server fails closed on an unknown server request", async () => {
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
    else if (request.method === "turn/start") {
      send({ id: request.id, result: { turn: { id: "u" } } });
      send({ id: 44, method: "unknown/request", params: { opaque: true } });
    }
  });
  await assert.rejects(createAppServerTransport({ spawnProcess: () => child })({ prompt: "x" }), /unsupported server request/);
});

test("app-server shutdown interrupts an active turn before killing its process", async () => {
  const requests = [];
  const child = fakeAppServer((request, send) => {
    requests.push(request);
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
    else if (request.method === "turn/start") send({ id: request.id, result: { turn: { id: "u" } } });
  });
  const transport = createAppServerTransport({ spawnProcess: () => child });
  const pending = transport({ prompt: "x" });
  await new Promise((resolve) => setImmediate(resolve));
  transport.shutdown();
  await assert.rejects(pending, /shut down/);
  assert.equal(requests.some((item) => item.method === "turn/interrupt"), true);
  assert.equal(child.killed, true);
});

test("app-server keeps simultaneous project turns isolated on one persistent process", async () => {
  let nextThread = 0;
  const pendingTurns = [];
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") {
      nextThread += 1;
      send({ id: request.id, result: { thread: { id: `thread-${nextThread}` } } });
    } else if (request.method === "turn/start") {
      const threadId = request.params.threadId;
      const turnId = `turn-${threadId}`;
      send({ id: request.id, result: { turn: { id: turnId } } });
      pendingTurns.push({ threadId, turnId, send });
      if (pendingTurns.length === 2) {
        for (const turn of pendingTurns.reverse()) {
          turn.send({ method: "item/agentMessage/delta", params: { threadId: turn.threadId, turnId: turn.turnId, delta: turn.threadId } });
          turn.send({ method: "turn/completed", params: { threadId: turn.threadId, turn: { id: turn.turnId, status: "completed" } } });
        }
      }
    }
  });
  const transport = createAppServerTransport({ spawnProcess: () => child });
  const [first, second] = await Promise.all([
    transport({ prompt: "first" }),
    transport({ prompt: "second" }),
  ]);
  assert.deepEqual(new Set([first.text, second.text]), new Set(["thread-1", "thread-2"]));
  assert.equal(child.killed, false);
  transport.shutdown();
});

test("app-server foreign approval is cancelled on its own id and fails all owned work loudly", async () => {
  const responses = [];
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "ours" } } });
    else if (request.method === "turn/start") {
      send({ id: request.id, result: { turn: { id: "our-turn" } } });
      send({ id: 77, method: "item/fileChange/requestApproval", params: { threadId: "foreign", turnId: "foreign-turn", itemId: "x" } });
    } else if (request.id === 77) responses.push(request);
  });
  await assert.rejects(createAppServerTransport({ spawnProcess: () => child })({ prompt: "x" }), /foreign or stale approval/);
  assert.deepEqual(responses, [{ id: 77, result: { decision: "cancel" } }]);
});

test("early exact approval replays, while another early turn id is cancelled and cannot leak", async () => {
  const responses = [];
  const first = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "ours" } } });
    else if (request.method === "turn/start") {
      send({ id: request.id, result: { turn: { id: "actual" } } });
      send({ id: 81, method: "item/commandExecution/requestApproval", params: { threadId: "ours", turnId: "wrong", itemId: "stale" } });
    } else if (request.id === 81) responses.push(request);
  });
  const second = happyServer({ threadId: "fresh" });
  const children = [first, second];
  let index = 0;
  const transport = createAppServerTransport({ spawnProcess: () => children[index++] });
  await assert.rejects(transport({ prompt: "x" }), /foreign or stale early approval/);
  assert.deepEqual(responses, [{ id: 81, result: { decision: "cancel" } }]);
  assert.equal((await transport({ prompt: "fresh" })).sessionId, "fresh");
  assert.equal(index, 2);
  transport.shutdown();
});

test("scoped cancel uses turn/interrupt RPC and a timely response does not kill the process", async () => {
  const requests = [];
  let handle;
  const child = fakeAppServer((request, send) => {
    requests.push(request);
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
    else if (request.method === "turn/start") send({ id: request.id, result: { turn: { id: "u" } } });
    else if (request.method === "turn/interrupt") {
      send({ id: request.id, result: {} });
      send({ method: "turn/completed", params: { threadId: "t", turn: { id: "u", status: "interrupted" } } });
    }
  });
  const pending = createAppServerTransport({ spawnProcess: () => child, cancelGraceMs: 50 })({
    prompt: "x", onChild: (value) => { handle = value; },
  });
  await new Promise((resolve) => setImmediate(resolve));
  handle.kill();
  await assert.rejects(pending, /turn interrupted/);
  const interrupt = requests.find((item) => item.method === "turn/interrupt");
  assert.deepEqual(interrupt.params, { threadId: "t", turnId: "u" });
  assert.equal(child.killed, false);
});

test("interrupt acknowledgement without exact turn completion poisons and tree-kills the client", async () => {
  let handle;
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
    else if (request.method === "turn/start") send({ id: request.id, result: { turn: { id: "u" } } });
    else if (request.method === "turn/interrupt") send({ id: request.id, result: {} });
  });
  let killed = 0;
  const pending = createAppServerTransport({
    spawnProcess: () => child,
    killProcessTree: (process) => { killed += 1; process.kill("SIGKILL"); },
    cancelGraceMs: 5,
  })({ prompt: "x", onChild: (value) => { handle = value; } });
  await new Promise((resolve) => setImmediate(resolve));
  handle.kill();
  await assert.rejects(pending, /remained active after interrupt acknowledgement/);
  assert.equal(killed, 1);
});

test("interrupt RPC error poisons and tree-kills the client", async () => {
  let handle;
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
    else if (request.method === "turn/start") send({ id: request.id, result: { turn: { id: "u" } } });
    else if (request.method === "turn/interrupt") send({ id: request.id, error: { code: -32000, message: "interrupt failed" } });
  });
  let killed = 0;
  const pending = createAppServerTransport({
    spawnProcess: () => child,
    killProcessTree: (process) => { killed += 1; process.kill("SIGKILL"); },
  })({ prompt: "x", onChild: (value) => { handle = value; } });
  await new Promise((resolve) => setImmediate(resolve));
  handle.kill();
  await assert.rejects(pending, /interrupt failed/);
  assert.equal(killed, 1);
});

test("throwing progress callback poisons the client instead of escaping its event handler", async () => {
  const child = happyServer();
  let killed = 0;
  const transport = createAppServerTransport({
    spawnProcess: () => child,
    killProcessTree: (process) => { killed += 1; process.kill("SIGKILL"); },
  });
  await assert.rejects(
    transport({ prompt: "x", onProgress: () => { throw new Error("render failed"); } }),
    /progress callback failed: render failed/,
  );
  assert.equal(killed, 1);
});

test("RPC and turn timeouts poison the client, kill its process tree, and permit only a fresh lazy restart", async (t) => {
  await t.test("initialize timeout", async () => {
    const first = fakeAppServer(() => {});
    const second = happyServer({ threadId: "fresh" });
    const children = [first, second];
    const killed = [];
    let index = 0;
    const transport = createAppServerTransport({
      spawnProcess: () => children[index++],
      killProcessTree: (child) => { killed.push(child); child.kill("SIGKILL"); },
      timeoutMs: 5,
    });
    await assert.rejects(transport({ prompt: "timeout" }), /initialize timed out/);
    assert.deepEqual(killed, [first]);
    assert.equal((await transport({ prompt: "fresh" })).sessionId, "fresh");
    assert.equal(index, 2);
    transport.shutdown();
  });
  await t.test("turn timeout", async () => {
    const child = fakeAppServer((request, send) => {
      if (request.method === "initialize") send({ id: request.id, result: {} });
      else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
      else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
      else if (request.method === "turn/start") send({ id: request.id, result: { turn: { id: "u" } } });
    });
    let killed = 0;
    const transport = createAppServerTransport({
      spawnProcess: () => child,
      killProcessTree: (process) => { killed += 1; process.kill("SIGKILL"); },
      timeoutMs: 5,
    });
    await assert.rejects(transport({ prompt: "timeout" }), /turn timed out/);
    assert.equal(killed, 1);
  });
});

test("lazy restart waits for asynchronous process-tree cleanup and cleanup failure blocks reuse", async (t) => {
  await t.test("waits for cleanup completion", async () => {
    const first = happyServer({ threadId: "old" });
    const second = happyServer({ threadId: "fresh" });
    const children = [first, second];
    let spawns = 0;
    let releaseCleanup;
    const cleanupGate = new Promise((resolve) => { releaseCleanup = resolve; });
    const transport = createAppServerTransport({
      spawnProcess: () => children[spawns++],
      killProcessTree: (child) => child === first ? cleanupGate : Promise.resolve(),
    });
    await transport({ prompt: "old" });
    first.emit("close", 1, null);
    const pending = transport({ prompt: "fresh" });
    await new Promise((resolve) => setImmediate(resolve));
    assert.equal(spawns, 1);
    releaseCleanup();
    assert.equal((await pending).sessionId, "fresh");
    assert.equal(spawns, 2);
    await transport.shutdown();
  });
  await t.test("observes cleanup failure", async () => {
    const child = happyServer({ threadId: "old" });
    let spawns = 0;
    const transport = createAppServerTransport({
      spawnProcess: () => { spawns += 1; return child; },
      killProcessTree: () => Promise.reject(new Error("tree cleanup failed")),
    });
    await transport({ prompt: "old" });
    child.emit("close", 1, null);
    await assert.rejects(transport({ prompt: "new" }), /process-tree cleanup failed: tree cleanup failed/);
    assert.equal(spawns, 1);
  });
});

test("transport shutdown propagates process-tree cleanup rejection", async () => {
  const child = happyServer({ threadId: "shutdown-cleanup" });
  const transport = createAppServerTransport({
    spawnProcess: () => child,
    killProcessTree: () => Promise.reject(new Error("taskkill rejected")),
  });
  await transport({ prompt: "ready" });
  await assert.rejects(transport.shutdown(), /process-tree cleanup failed: taskkill rejected/);
});

test("shutdown begins observed tree cleanup while the child PID and stdin are still live", async () => {
  const child = happyServer({ threadId: "ordered-shutdown" });
  let cleanupSawLiveChild = false;
  const transport = createAppServerTransport({
    spawnProcess: () => child,
    killProcessTree: (process) => {
      cleanupSawLiveChild = process.exitCode === null && !process.stdin.writableEnded;
      process.kill("SIGKILL");
      return Promise.resolve();
    },
  });
  await transport({ prompt: "ready" });
  await transport.shutdown();
  assert.equal(cleanupSawLiveChild, true);
});

test("default tree cleanup treats an already-exited child as clean without another kill", async () => {
  let killCalls = 0;
  await killAppServerProcessTree({
    exitCode: 0,
    signalCode: null,
    killed: false,
    kill: () => { killCalls += 1; throw new Error("must not run"); },
  });
  assert.equal(killCalls, 0);
});

test("spawn errors fail loudly and pass through observed cleanup", async () => {
  const child = fakeAppServer(() => {});
  let cleanup = 0;
  const pending = createAppServerTransport({
    spawnProcess: () => {
      setImmediate(() => child.emit("error", new Error("configured binary unavailable")));
      return child;
    },
    killProcessTree: () => { cleanup += 1; return Promise.resolve(); },
  })({ prompt: "x" });
  await assert.rejects(pending, /failed to start: configured binary unavailable/);
  assert.equal(cleanup, 1);
});

test("shutdown during initialize kills the starting client and rejects its handshake", async () => {
  const child = fakeAppServer(() => {});
  let killed = 0;
  const transport = createAppServerTransport({
    spawnProcess: () => child,
    killProcessTree: (process) => { killed += 1; process.kill("SIGKILL"); },
  });
  const pending = transport({ prompt: "x" });
  await new Promise((resolve) => setImmediate(resolve));
  transport.shutdown();
  await assert.rejects(pending, /shut down/);
  assert.equal(killed, 1);
});

test("JSONL stdout EOF fails pending work immediately and kills the process tree", async () => {
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
    else if (request.method === "turn/start") send({ id: request.id, result: { turn: { id: "u" } } });
  });
  let killed = 0;
  const pending = createAppServerTransport({
    spawnProcess: () => child,
    killProcessTree: (process) => { killed += 1; process.kill("SIGKILL"); },
  })({ prompt: "x" });
  await new Promise((resolve) => setImmediate(resolve));
  child.stdout.end();
  await assert.rejects(pending, /stdout reached EOF/);
  assert.equal(killed, 1);
});

test("stdin EPIPE fails pending work without an uncaught stream error and observes cleanup", async () => {
  const child = fakeAppServer((request, send) => {
    if (request.method === "initialize") send({ id: request.id, result: {} });
    else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
    else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
    else if (request.method === "turn/start") send({ id: request.id, result: { turn: { id: "u" } } });
  });
  let cleanup = 0;
  const pending = createAppServerTransport({
    spawnProcess: () => child,
    killProcessTree: (process) => { cleanup += 1; process.kill("SIGKILL"); return Promise.resolve(); },
  })({ prompt: "x" });
  await new Promise((resolve) => setImmediate(resolve));
  const epipe = Object.assign(new Error("broken pipe"), { code: "EPIPE" });
  child.stdin.emit("error", epipe);
  await assert.rejects(pending, /stdin stream failed: broken pipe/);
  assert.equal(cleanup, 1);
});

test("app-server rejects non-subscription auth, stale resume, malformed JSONL, and empty replies loudly", async (t) => {
  await t.test("non-subscription auth", async () => {
    const child = fakeAppServer((request, send) => {
      if (request.method === "initialize") send({ id: request.id, result: {} });
      if (request.method === "account/read") send({ id: request.id, result: { account: { type: "apiKey" } } });
    });
    await assert.rejects(createAppServerTransport({ spawnProcess: () => child })({ prompt: "x" }), /ChatGPT subscription/);
    assert.equal(child.killed, true);
  });
  await t.test("stale resume", async () => {
    const child = fakeAppServer((request, send) => {
      if (request.method === "initialize") send({ id: request.id, result: {} });
      else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
      else if (request.method === "thread/resume") send({ id: request.id, error: { code: -32000, message: "thread not found" } });
    });
    await assert.rejects(createAppServerTransport({ spawnProcess: () => child })({ message: "x", sessionId: "stale" }), /thread not found/);
  });
  await t.test("malformed JSONL", async () => {
    const child = fakeAppServer((request, send, process) => {
      if (request.method === "initialize") process.stdout.write("not-json\n");
    });
    await assert.rejects(createAppServerTransport({ spawnProcess: () => child })({ prompt: "x" }), /malformed JSONL/);
  });
  await t.test("empty reply", async () => {
    const child = happyServer();
    child.stdout = new PassThrough();
    // Use a dedicated server without deltas.
    const empty = fakeAppServer((request, send) => {
      if (request.method === "initialize") send({ id: request.id, result: {} });
      else if (request.method === "account/read") send({ id: request.id, result: { account: { type: "chatgpt" } } });
      else if (request.method === "thread/start") send({ id: request.id, result: { thread: { id: "t" } } });
      else if (request.method === "turn/start") {
        send({ id: request.id, result: { turn: { id: "u" } } });
        send({ method: "turn/completed", params: { threadId: "t", turn: { id: "u", status: "completed" } } });
      }
    });
    await assert.rejects(createAppServerTransport({ spawnProcess: () => empty })({ prompt: "x" }), /empty reply/);
  });
});
