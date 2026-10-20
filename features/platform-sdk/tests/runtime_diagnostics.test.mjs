import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { runInNewContext } from "node:vm";

const HERE = dirname(fileURLToPath(import.meta.url));
const SOURCE = join(HERE, "../web/runtime-diagnostics.js");

class Events {
  constructor() {
    this.listeners = new Map();
  }

  addEventListener(name, callback) {
    const callbacks = this.listeners.get(name) || [];
    callbacks.push(callback);
    this.listeners.set(name, callbacks);
  }

  dispatch(name, event = {}) {
    for (const callback of this.listeners.get(name) || []) callback({ type: name, ...event });
  }
}

function makeRoot({ engineRecord = null, pendingRecord = null } = {}) {
  const events = new Events();
  const document = new Events();
  const canvas = new Events();
  const storage = new Map();
  const timers = [];
  const consoleErrors = [];
  let now = 1000;
  if (engineRecord != null) storage.set("demo.crash.v1", engineRecord);
  if (pendingRecord != null) storage.set("demo.crash.pending.v1", pendingRecord);
  const root = {
    addEventListener: events.addEventListener.bind(events),
    dispatch: events.dispatch.bind(events),
    TextEncoder,
    URL,
    __PLATFORM_SDK_CONFIG__: { runtimeBuildFingerprint: "build-current" },
    console: { error(value) { consoleErrors.push(value); } },
    document: Object.assign(document, {
      getElementById(id) { return id === "canvas" ? canvas : null; },
    }),
    localStorage: {
      getItem(key) { return storage.has(key) ? storage.get(key) : null; },
      setItem(key, value) { storage.set(key, String(value)); },
      removeItem(key) { storage.delete(key); },
    },
    location: { href: "https://example.test/game.html" },
    navigator: { userAgent: "test", onLine: true },
    performance: { now() { return now; } },
    setTimeout(callback, delay) { timers.push({ callback, delay }); return timers.length; },
    clearTimeout() {},
  };
  root.window = root;
  return {
    canvas,
    consoleErrors,
    root,
    storage,
    tick(ms = 1) { now += ms; },
    runTimers(limit = 100) {
      for (let index = 0; timers.length && index < limit; index += 1) {
        const timer = timers.shift();
        now += timer.delay;
        timer.callback();
      }
      assert.ok(timers.length === 0, "diagnostics left a bounded timer chain");
    },
  };
}

function install(fixture, options = {}) {
  const source = readFileSync(SOURCE, "utf8");
  const sandbox = { ...fixture.root, globalThis: fixture.root };
  runInNewContext(source, sandbox, { filename: SOURCE });
  return sandbox.installPlatformDiagnostics(fixture.root, {
    storagePrefix: options.storagePrefix || "demo.crash",
    runtimeScript: options.runtimeScript || "game.js",
  });
}

function engineRecord(build = "build-engine", suffix = "tail") {
  return `game: ${build}\nengine: nt web\ngpu_renderer: test\ncontext: menu\ntime: 1\nreason: trap\nstack:\nwasm-function[3]\nlog:\n${suffix}`;
}

test("replays bounded previous slots and delivers current fatal slots in engine-first order", () => {
  const previousWeb = JSON.stringify({ version: 1, at: Date.now(), kind: "loader.failed", stack: "previous web" });
  const fixture = makeRoot({ engineRecord: engineRecord("build-old", "old tail"), pendingRecord: previousWeb });
  const received = [];
  const api = install(fixture);

  api.report("loader.failed", { message: "runtime fetch failed", stack: "Error: fetch" });
  fixture.root.dispatch("nt-crash", { detail: { text: engineRecord("build-live", "live tail") } });
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.runTimers();

  assert.equal(received.length, 4);
  assert.equal(received[0].message, "engine.crash");
  assert.equal(received[1].message, "loader.failed");
  assert.equal(received[2].message, "engine.crash");
  assert.equal(received[3].message, "loader.failed");
  assert.match(received[0].stack, /build-live/);
  assert.match(received[1].stack, /loader\.failed/);
  assert.match(received[2].stack, /build-old/);
  assert.match(received[3].stack, /previous web/);
  assert.equal(fixture.storage.get("demo.crash.v1"), engineRecord("build-old", "old tail"));
  assert.equal(JSON.parse(fixture.storage.get("demo.crash.pending.v1")).kind, "loader.failed");
});

test("suppresses only a deferred engine-eligible fallback when the engine record arrives", () => {
  const fixture = makeRoot();
  const received = [];
  install(fixture);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };

  fixture.root.dispatch("error", {
    filename: "https://example.test/game.js",
    error: { name: "RuntimeError", message: "abort", stack: "wasm-function[12]" },
  });
  fixture.root.dispatch("nt-crash", { detail: { text: engineRecord("engine-wins") } });
  fixture.root.dispatch("error", {
    filename: "https://example.test/game.js",
    error: { name: "TypeError", message: "pure js", stack: "TypeError: pure js" },
  });
  fixture.runTimers();

  assert.equal(received.length, 2);
  assert.match(received[0].stack, /engine-wins/);
  assert.match(received[1].stack, /TypeError: pure js/);
  assert.doesNotMatch(received[1].stack, /RuntimeError/);
});

test("bounds and redacts web reports and records only unexpected graphics signals", () => {
  const fixture = makeRoot();
  const received = [];
  const api = install(fixture);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  api.report("loader.failed", {
    message: "Authorization: Bearer secret-token https://user:password@example.test/path?token=raw",
    stack: `Error: ${"界".repeat(5000)}\nhttps://example.test/path?token=raw\nBearer secret-token`,
  });
  api.intentionalLoss();
  fixture.canvas.dispatch("webglcontextlost", { statusMessage: "test loss" });
  fixture.tick(3000);
  fixture.canvas.dispatch("webglcontextlost", { statusMessage: "real loss" });
  fixture.canvas.dispatch("webglcontextlost", { statusMessage: "duplicate loss" });
  fixture.canvas.dispatch("webglcontextcreationerror", { statusMessage: "creation failed" });
  fixture.runTimers();

  assert.equal(received.length, 3);
  assert.deepEqual(received.map((error) => error.message), ["loader.failed", "gfx.context_creation", "gfx.context_lost"]);
  for (const error of received) assert.ok(new TextEncoder().encode(error.stack).length <= 12000);
  assert.doesNotMatch(received[0].stack, /secret-token|password|token=raw/);
  assert.match(received[0].stack, /\[redacted/);
  const stored = fixture.storage.get("demo.crash.pending.v1");
  assert.ok(stored);
  assert.ok(new TextEncoder().encode(stored).length <= 20000);
  assert.equal(JSON.parse(stored).kind, "loader.failed");
});

test("retains malformed and oversized stored records without queuing them", () => {
  const oversized = "x".repeat(65537);
  const malformedWeb = "{not json";
  const fixture = makeRoot({ engineRecord: oversized, pendingRecord: malformedWeb });
  const received = [];
  install(fixture);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.runTimers();

  assert.deepEqual(received, []);
  assert.equal(fixture.storage.get("demo.crash.v1"), oversized);
  assert.equal(fixture.storage.get("demo.crash.pending.v1"), malformedWeb);
});

test("does not promote a replayed web record with an invalid kind", () => {
  const invalid = JSON.stringify({ version: 1, at: Date.now(), kind: "loader failed", stack: "stored failure" });
  const fixture = makeRoot({ pendingRecord: invalid });
  const received = [];
  install(fixture);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.runTimers();

  assert.deepEqual(received, []);
  assert.equal(fixture.storage.get("demo.crash.pending.v1"), invalid);
});

test("stops readiness polling after ten attempts and preserves an unconfirmed web record", () => {
  const fixture = makeRoot();
  const api = install(fixture);
  api.report("loader.failed", { message: "runtime unavailable" });
  fixture.runTimers();

  assert.equal(fixture.consoleErrors.length, 1);
  assert.equal(JSON.parse(fixture.storage.get("demo.crash.pending.v1")).kind, "loader.failed");
});

test("caps engine delivery while retaining its identity header and newest log tail", () => {
  const raw = engineRecord("build-long", `${"x".repeat(24000)}\nLATEST_LOG_LINE`);
  const fixture = makeRoot({ engineRecord: raw });
  const received = [];
  install(fixture);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.runTimers();

  assert.equal(received.length, 1);
  assert.ok(new TextEncoder().encode(received[0].stack).length <= 12000);
  assert.match(received[0].stack, /game: build-long/);
  assert.match(received[0].stack, /LATEST_LOG_LINE/);
});

test("recognizes Chrome and Firefox runtime frames but rejects a foreign same-path script", () => {
  for (const stack of [
    "TypeError: chrome\n    at boot (https://example.test/game.js:11:4)\n    at run (https://example.test/boot.js:1:1)",
    "boot@https://example.test/game.js:11:4\n@https://example.test/boot.js:1:1",
  ]) {
    const fixture = makeRoot();
    const received = [];
    install(fixture);
    fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
    fixture.root.dispatch("unhandledrejection", { reason: { name: "TypeError", stack } });
    fixture.runTimers();
    assert.equal(received.length, 1);
  }

  const foreign = makeRoot();
  const received = [];
  install(foreign);
  foreign.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  foreign.root.dispatch("error", {
    filename: "https://evil.example/game.js",
    error: { name: "TypeError", stack: "TypeError: foreign" },
  });
  foreign.runTimers();
  assert.deepEqual(received, []);
});

test("captures preview SDK faults only from exact first-party module URLs", () => {
  for (const [url, expected] of [
    ["https://example.test/platform-sdk.js:11:4", 1],
    ["https://example.test/platform-sdk-adapter.js:11:4", 1],
    ["https://example.test/other.js:11:4", 0],
    ["https://example.test/other/platform-sdk.js:11:4", 0],
    ["https://evil.example/platform-sdk.js:11:4", 0],
  ]) {
    const fixture = makeRoot();
    const received = [];
    install(fixture);
    fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
    fixture.root.dispatch("unhandledrejection", {
      reason: { name: "TypeError", stack: `TypeError: callback failed\n    at callback (${url})` },
    });
    fixture.runTimers();
    assert.equal(received.length, expected, url);
  }
});

test("defers an unattributed pre-init wasm trap but excludes foreign portal and extension faults", () => {
  const fixture = makeRoot();
  const received = [];
  install(fixture);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.root.dispatch("error", {
    error: { name: "RuntimeError", message: "trap", stack: "RuntimeError: trap\nwasm-function[77]:0x10" },
  });
  fixture.runTimers();
  assert.equal(received.length, 1);

  const excluded = makeRoot();
  const excludedReceived = [];
  install(excluded);
  excluded.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { excludedReceived.push(error); } }; } };
  excluded.root.dispatch("error", {
    error: { name: "RuntimeError", stack: "RuntimeError\n at sdk (https://sdk.poki.com/core.js:1:2)\nwasm-function[1]" },
  });
  excluded.root.dispatch("error", {
    error: { name: "RuntimeError", stack: "RuntimeError\nchrome-extension://abc/script.js\nwasm-function[2]" },
  });
  excluded.runTimers();
  assert.deepEqual(excludedReceived, []);
});

test("retries retained records after readiness polling exhausts when a later engine fatal triggers a flush", () => {
  const fixture = makeRoot();
  const api = install(fixture);
  api.report("loader.failed", { message: "early failure" });
  fixture.runTimers();
  assert.equal(fixture.consoleErrors.length, 1);

  const received = [];
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.root.dispatch("nt-crash", { detail: { text: engineRecord("ready-later") } });
  fixture.runTimers();
  assert.equal(received.length, 2);
  assert.match(received[0].stack, /ready-later/);
  assert.match(received[1].stack, /early failure/);
});

test("keeps stored records when storage or the sink throws", () => {
  const denied = makeRoot();
  denied.root.localStorage = {
    getItem() { throw new Error("blocked"); },
    setItem() { throw new Error("full"); },
  };
  const api = install(denied);
  assert.doesNotThrow(() => api.report("loader.failed", { message: "storage denied" }));

  const fixture = makeRoot();
  install(fixture).report("loader.failed", { message: "sink retained" });
  let calls = 0;
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError() { calls += 1; throw new Error("sink failed"); } }; } };
  fixture.runTimers();
  assert.equal(calls, 1);
  assert.equal(fixture.consoleErrors.length, 1);
  assert.equal(JSON.parse(fixture.storage.get("demo.crash.pending.v1")).kind, "loader.failed");
});

test("does not replay expired or future web records and keeps legacy identity opaque", () => {
  for (const at of [Date.now() - 86400001, Date.now() + 86400000]) {
    const fixture = makeRoot({ pendingRecord: JSON.stringify({ version: 1, at, kind: "loader.failed", stack: "legacy build-old time-old" }) });
    const received = [];
    install(fixture);
    fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
    fixture.runTimers();
    assert.deepEqual(received, []);
  }

  const fixture = makeRoot({ pendingRecord: JSON.stringify({ version: 1, at: Date.now(), kind: "loader.failed", stack: "legacy build-old time-old" }) });
  const received = [];
  install(fixture);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.runTimers();
  assert.equal(received.length, 1);
  assert.match(received[0].stack, /legacy build-old time-old/);
});

test("late backend readiness retries retained records without restarting polls or sending twice", () => {
  const raw = engineRecord("previous-build");
  const fixture = makeRoot({ engineRecord: raw });
  const received = [];
  const api = install(fixture);
  api.report("loader.failed", { message: "early loader failure" });
  fixture.runTimers();
  assert.equal(fixture.consoleErrors.length, 2);
  fixture.root.dispatch("platform-diagnostics-ready");
  fixture.runTimers();
  assert.equal(fixture.consoleErrors.length, 2);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.root.dispatch("platform-diagnostics-ready");
  fixture.runTimers();
  assert.equal(received.length, 2);
  assert.match(received[0].stack, /early loader failure/);
  assert.match(received[1].stack, /previous-build/);
  fixture.root.dispatch("platform-diagnostics-ready");
  fixture.runTimers();
  assert.equal(received.length, 2);
  assert.equal(fixture.storage.get("demo.crash.v1"), raw);
});

test("deduplicates equal engine records and keeps a pure JS report after a later engine crash", () => {
  const raw = engineRecord("same-build");
  const duplicate = makeRoot({ engineRecord: raw });
  const duplicateReceived = [];
  install(duplicate);
  duplicate.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { duplicateReceived.push(error); } }; } };
  duplicate.root.dispatch("nt-crash", { detail: { text: raw } });
  duplicate.runTimers();
  assert.equal(duplicateReceived.length, 1);

  const fixture = makeRoot();
  const received = [];
  install(fixture);
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.root.dispatch("error", {
    filename: "https://example.test/game.js",
    error: { name: "TypeError", stack: "TypeError: independent JS failure" },
  });
  fixture.root.dispatch("nt-crash", { detail: { text: engineRecord("later-engine") } });
  fixture.runTimers();
  assert.equal(received.length, 2);
  assert.match(received[0].stack, /later-engine/);
  assert.match(received[1].stack, /independent JS failure/);
});

test("keeps crash namespaces isolated on one origin", () => {
  const fixture = makeRoot();
  fixture.storage.set("alpha.crash.v1", engineRecord("alpha-build"));
  fixture.storage.set("beta.crash.v1", engineRecord("beta-build"));
  const received = [];
  install(fixture, { storagePrefix: "alpha.crash" });
  install(fixture, { storagePrefix: "beta.crash" });
  fixture.root.__platformSdkInternalBackend = { errorSink() { return { captureError(error) { received.push(error); } }; } };
  fixture.runTimers();

  assert.equal(received.length, 2);
  assert.match(received[0].stack, /alpha-build/);
  assert.match(received[1].stack, /beta-build/);
  assert.equal(fixture.storage.get("alpha.crash.v1"), engineRecord("alpha-build"));
  assert.equal(fixture.storage.get("beta.crash.v1"), engineRecord("beta-build"));
});
