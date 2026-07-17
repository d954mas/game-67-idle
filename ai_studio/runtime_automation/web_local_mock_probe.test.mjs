import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import {
  assessLocalMockObservation,
  createLocalMockEvidence,
  probeLocalMockPage,
  writeLocalMockEvidence,
} from "./web_local_mock_probe.mjs";

function passingObservation() {
  return {
    url: "http://127.0.0.1:8092/",
    finalUrl: "http://127.0.0.1:8092/",
    readyState: "complete",
    config: { target: "local", platformSdk: "mock", release: true },
    overlay: {
      present: true,
      className: "is-hidden",
      display: "none",
      opacity: "0",
      progressPercent: 100,
    },
    canvas: { width: 1280, height: 720 },
    lifecycleTranscript: [
      { kind: "progress", value: 0.01, source: "shell" },
      { kind: "progress", value: 0.45, source: "shell" },
      { kind: "progress", value: 1, source: "c-bridge" },
      { kind: "finished", value: null, source: "c-bridge" },
    ],
    issues: [],
  };
}

test("local mock observation requires the hidden 100% overlay and a real canvas", () => {
  assert.deepEqual(assessLocalMockObservation(passingObservation()), []);

  const visible = passingObservation();
  visible.overlay.display = "grid";
  visible.overlay.progressPercent = 45;
  visible.issues.push({ kind: "console.error", text: "runtime failed" });
  assert.deepEqual(assessLocalMockObservation(visible), [
    "loading overlay is still visible",
    "loading progress did not reach 100%",
    "browser reported 1 error",
  ]);
});

test("passing final DOM without captured C bridge calls is not lifecycle proof", () => {
  const observation = passingObservation();
  observation.lifecycleTranscript = [
    { kind: "progress", value: 1, source: "shell" },
    { kind: "finished", value: null, source: "shell" },
  ];

  assert.deepEqual(assessLocalMockObservation(observation), [
    "C loading progress never reached the JS bridge",
    "C loading finished never reached the JS bridge",
  ]);
});

test("local mock observation fails closed on release mode, final URL, and transcripts", () => {
  const observation = passingObservation();
  observation.finalUrl = "https://example.com/fake";
  observation.config.release = false;
  observation.lifecycleTranscript = null;
  observation.issues = null;

  assert.deepEqual(assessLocalMockObservation(observation), [
    "final page URL left localhost",
    "page is not the local mock target",
    "C lifecycle transcript is missing",
    "browser error transcript is missing",
  ]);
});

test("local mock observation is deterministic and never claims a release binding", (t) => {
  const root = mkdtempSync(join(tmpdir(), "local-mock-probe-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const evidence = createLocalMockEvidence({ observation: passingObservation() });

  assert.equal(evidence.schema, "ai_studio.runtime.local_mock_web_observation.v1");
  assert.equal(evidence.result, "pass");
  assert.equal(Object.hasOwn(evidence, "release"), false);
  assert.equal(Object.hasOwn(evidence, "timestamp"), false);
  assert.equal(evidence.observation.lifecycleTranscript.at(-1).kind, "finished");

  const outputPath = join(root, "local-mock-observation.json");
  writeLocalMockEvidence(outputPath, evidence);
  writeLocalMockEvidence(outputPath, evidence);
  assert.deepEqual(JSON.parse(readFileSync(outputPath, "utf8")), evidence);
  assert.throws(
    () => writeLocalMockEvidence(outputPath, { ...evidence, result: "fail" }),
    /different local mock evidence/i,
  );
});

test("raw CDP probe instruments before navigation and uses a fresh loopback target", async () => {
  const commands = [];
  const fetchCalls = [];
  class FakeWebSocket {
    constructor(url) {
      this.url = url;
      this.listeners = new Map();
      queueMicrotask(() => this.emit("open", {}));
    }

    addEventListener(type, callback, options = {}) {
      const listeners = this.listeners.get(type) || [];
      listeners.push({ callback, once: options.once === true });
      this.listeners.set(type, listeners);
    }

    emit(type, event) {
      const listeners = this.listeners.get(type) || [];
      this.listeners.set(type, listeners.filter((listener) => !listener.once));
      for (const listener of listeners) listener.callback(event);
    }

    send(raw) {
      const command = JSON.parse(raw);
      commands.push(command);
      const result = command.method === "Runtime.evaluate"
        ? { result: { value: passingObservation() } }
        : {};
      queueMicrotask(() => this.emit("message", { data: JSON.stringify({ id: command.id, result }) }));
      if (command.method === "Network.enable" && FakeWebSocket.remoteUrl) {
        queueMicrotask(() => this.emit("message", { data: JSON.stringify({
          method: "Network.webSocketCreated",
          params: { url: FakeWebSocket.remoteUrl },
        }) }));
      }
    }

    close() {
      this.emit("close", {});
    }
  }
  const fetchImpl = async (url, options) => {
    fetchCalls.push({ url: String(url), options });
    return {
      ok: true,
      async json() {
        return { id: "fresh", webSocketDebuggerUrl: "ws://127.0.0.1:9222/devtools/page/fresh" };
      },
    };
  };

  const observation = await probeLocalMockPage({
    cdpEndpoint: "http://127.0.0.1:9222",
    url: "http://127.0.0.1:8092/",
    timeoutMs: 1000,
    fetchImpl,
    WebSocketClass: FakeWebSocket,
  });

  assert.deepEqual(assessLocalMockObservation(observation), []);
  assert.equal(fetchCalls[0].options.method, "PUT");
  assert.equal(fetchCalls[0].options.redirect, "error");
  assert.ok(commands.findIndex((command) => command.method === "Page.addScriptToEvaluateOnNewDocument")
    < commands.findIndex((command) => command.method === "Page.navigate"));
  assert.equal(commands.some((command) => command.method === "Network.enable"), true);

  FakeWebSocket.remoteUrl = "wss://example.com/remote";
  await assert.rejects(
    probeLocalMockPage({
      cdpEndpoint: "http://127.0.0.1:9222",
      url: "http://127.0.0.1:8092/",
      timeoutMs: 100,
      fetchImpl,
      WebSocketClass: FakeWebSocket,
    }),
    /example\.com/i,
  );
  FakeWebSocket.remoteUrl = null;
});

test("raw CDP probe rejects a remote target socket and bounds an unresponsive socket", async () => {
  const remoteFetch = async () => ({
    ok: true,
    async json() { return { id: "remote", webSocketDebuggerUrl: "wss://example.com/devtools/page/fake" }; },
  });
  await assert.rejects(
    probeLocalMockPage({
      cdpEndpoint: "http://127.0.0.1:9222",
      url: "http://127.0.0.1:8092/",
      timeoutMs: 100,
      fetchImpl: remoteFetch,
      WebSocketClass: class {},
    }),
    /loopback ws:\/\//i,
  );

  class NeverOpensWebSocket {
    static closed = false;
    addEventListener() {}
    close() { NeverOpensWebSocket.closed = true; }
  }
  let closeCalls = 0;
  const localFetch = async (url) => {
    if (String(url).includes("/json/close/")) {
      closeCalls += 1;
      return { ok: true };
    }
    return {
      ok: true,
      async json() { return { id: "stuck", webSocketDebuggerUrl: "ws://127.0.0.1:9222/devtools/page/stuck" }; },
    };
  };
  const started = Date.now();
  await assert.rejects(
    probeLocalMockPage({
      cdpEndpoint: "http://127.0.0.1:9222",
      url: "http://127.0.0.1:8092/",
      timeoutMs: 200,
      fetchImpl: localFetch,
      WebSocketClass: NeverOpensWebSocket,
    }),
    /timed out/i,
  );
  assert.ok(Date.now() - started < 500);
  assert.equal(NeverOpensWebSocket.closed, true);
  assert.equal(closeCalls, 1);

  const connectedCommands = [];
  class CloseStallsWebSocket {
    constructor() {
      this.listeners = new Map();
      queueMicrotask(() => this.emit("open", {}));
    }
    addEventListener(type, callback, options = {}) {
      const listeners = this.listeners.get(type) || [];
      listeners.push({ callback, once: options.once === true });
      this.listeners.set(type, listeners);
    }
    emit(type, event) {
      const listeners = this.listeners.get(type) || [];
      this.listeners.set(type, listeners.filter((listener) => !listener.once));
      for (const listener of listeners) listener.callback(event);
    }
    send(raw) {
      const command = JSON.parse(raw);
      connectedCommands.push(command.method);
      if (command.method === "Page.close") return;
      const result = command.method === "Runtime.evaluate"
        ? { result: { value: passingObservation() } }
        : {};
      queueMicrotask(() => this.emit("message", { data: JSON.stringify({ id: command.id, result }) }));
    }
    close() { this.emit("close", {}); }
  }
  let connectedHttpCloseCalls = 0;
  const connectedFetch = async (url) => {
    if (String(url).includes("/json/close/")) {
      connectedHttpCloseCalls += 1;
      return { ok: false };
    }
    return {
      ok: true,
      async json() { return { id: "close-stalls", webSocketDebuggerUrl: "ws://127.0.0.1:9222/devtools/page/close-stalls" }; },
    };
  };
  const connectedStarted = Date.now();
  const connectedObservation = await probeLocalMockPage({
    cdpEndpoint: "http://127.0.0.1:9222",
    url: "http://127.0.0.1:8092/",
    timeoutMs: 200,
    fetchImpl: connectedFetch,
    WebSocketClass: CloseStallsWebSocket,
  });
  assert.deepEqual(assessLocalMockObservation(connectedObservation), []);
  assert.equal(connectedHttpCloseCalls, 1);
  assert.equal(connectedCommands.includes("Page.close"), true);
  assert.ok(Date.now() - connectedStarted < 500);
});
