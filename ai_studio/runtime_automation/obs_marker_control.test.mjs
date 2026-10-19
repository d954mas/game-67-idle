import assert from "node:assert/strict";
import test from "node:test";

import { obsAuthentication, parseArgs, setMarkerEnabled } from "./obs_marker_control.mjs";

class FakeSocket {
  static current = null;

  constructor(url) {
    this.url = url;
    this.listeners = new Map();
    this.sent = [];
    this.closed = false;
    this.throwOnSend = false;
    FakeSocket.current = this;
  }

  addEventListener(name, callback) {
    const callbacks = this.listeners.get(name) || [];
    callbacks.push(callback);
    this.listeners.set(name, callbacks);
  }

  send(message) {
    if (this.throwOnSend) throw new Error("closed");
    this.sent.push(JSON.parse(message));
  }

  close() {
    this.closed = true;
  }

  emit(name, payload) {
    for (const callback of this.listeners.get(name) || []) callback(payload);
  }
}

const tick = () => new Promise((resolve) => setImmediate(resolve));

test("auth proof follows the OBS WebSocket v5 salt and challenge contract", () => {
  assert.equal(
    obsAuthentication("password", "salt", "challenge"),
    "zTM5ki6L2vVvBQiTG9ckH1Lh64AbnCf6XZ226UmnkIA=",
  );
});

test("marker control authenticates and waits for frames rendered after the visibility acknowledgement", async () => {
  const pending = setMarkerEnabled({
    port: 4455,
    password: "a".repeat(32),
    scene: "Scene",
    itemId: 2,
    enabled: false,
    WebSocketClass: FakeSocket,
  });
  const socket = FakeSocket.current;
  socket.emit("message", { data: JSON.stringify({ op: 0, d: { authentication: { salt: "salt", challenge: "challenge" } } }) });
  await tick();
  assert.equal(socket.sent[0].op, 1);
  assert.equal(socket.sent[0].d.rpcVersion, 1);
  assert.ok(socket.sent[0].d.authentication);
  socket.emit("message", { data: JSON.stringify({ op: 2, d: {} }) });
  await tick();
  assert.deepEqual(socket.sent[1], {
    op: 6,
    d: {
      requestType: "SetSceneItemEnabled",
      requestId: "capture-marker",
      requestData: { sceneName: "Scene", sceneItemId: 2, sceneItemEnabled: false },
    },
  });
  socket.emit("message", { data: JSON.stringify({
    op: 7,
    d: { requestId: "capture-marker", requestStatus: { result: true } },
  }) });
  await tick();
  assert.deepEqual(socket.sent[2], {
    op: 6,
    d: { requestType: "GetStats", requestId: "capture-marker-barrier-baseline" },
  });
  socket.emit("message", { data: JSON.stringify({
    op: 7,
    d: { requestId: "capture-marker-barrier-baseline", requestStatus: { result: true }, responseData: { renderTotalFrames: 10 } },
  }) });
  await tick();
  assert.deepEqual(socket.sent[3], {
    op: 6,
    d: { requestType: "GetStats", requestId: "capture-marker-after" },
  });
  socket.emit("message", { data: JSON.stringify({
    op: 7,
    d: { requestId: "capture-marker-after", requestStatus: { result: true }, responseData: { renderTotalFrames: 13 } },
  }) });
  const receipt = await pending;
  assert.ok(receipt.roundTripMilliseconds >= 0);
  assert.equal(receipt.beforeRenderFrames, 10);
  assert.equal(receipt.afterRenderFrames, 13);
  assert.equal(socket.closed, true);
});

test("marker control rejects a visibility request that OBS did not accept", async () => {
  const pending = setMarkerEnabled({
    port: 4455,
    password: "a".repeat(32),
    scene: "Scene",
    itemId: 2,
    enabled: true,
    WebSocketClass: FakeSocket,
  });
  const socket = FakeSocket.current;
  socket.emit("message", { data: JSON.stringify({ op: 0, d: { authentication: { salt: "salt", challenge: "challenge" } } }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 2, d: {} }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({
    op: 7,
    d: { requestId: "capture-marker", requestStatus: { result: false } },
  }) });
  await assert.rejects(pending, /rejected/);
});

test("argument parsing rejects an unactionable marker target", () => {
  assert.throws(() => parseArgs(["--port", "0", "--scene", "Scene", "--item-id", "2", "--enabled", "true"]), /usage/);
});

test("marker control rejects an OBS hello that omits or empties authenticated access", async () => {
  const pending = setMarkerEnabled({
    port: 4455,
    password: "a".repeat(32),
    scene: "Scene",
    itemId: 2,
    enabled: true,
    WebSocketClass: FakeSocket,
  });
  FakeSocket.current.emit("message", { data: JSON.stringify({ op: 0, d: { authentication: { salt: "", challenge: "" } } }) });
  await assert.rejects(pending, /authenticated/);
});

test("marker control rejects a disconnect after acknowledgement", async () => {
  const pending = setMarkerEnabled({
    port: 4455,
    password: "a".repeat(32),
    scene: "Scene",
    itemId: 2,
    enabled: true,
    WebSocketClass: FakeSocket,
  });
  const socket = FakeSocket.current;
  socket.emit("message", { data: JSON.stringify({ op: 0, d: { authentication: { salt: "salt", challenge: "challenge" } } }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 2, d: {} }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 7, d: { requestId: "capture-marker", requestStatus: { result: true } } }) });
  socket.emit("close", {});
  await assert.rejects(pending, /closed/);
});

test("frames rendered before acknowledgement cannot satisfy the post-ack barrier", async () => {
  const pending = setMarkerEnabled({
    port: 4455,
    password: "a".repeat(32),
    scene: "Scene",
    itemId: 2,
    enabled: true,
    WebSocketClass: FakeSocket,
  });
  const socket = FakeSocket.current;
  socket.emit("message", { data: JSON.stringify({ op: 0, d: { authentication: { salt: "salt", challenge: "challenge" } } }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 2, d: {} }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 7, d: { requestId: "capture-marker", requestStatus: { result: true } } }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 7, d: { requestId: "capture-marker-barrier-baseline", requestStatus: { result: true }, responseData: { renderTotalFrames: 40 } } }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 7, d: { requestId: "capture-marker-after", requestStatus: { result: true }, responseData: { renderTotalFrames: 42 } } }) });
  await new Promise((resolve) => setTimeout(resolve, 15));
  assert.equal(socket.closed, false);
  socket.emit("message", { data: JSON.stringify({ op: 7, d: { requestId: "capture-marker-after", requestStatus: { result: true }, responseData: { renderTotalFrames: 43 } } }) });
  const receipt = await pending;
  assert.equal(receipt.beforeRenderFrames, 40);
  assert.equal(receipt.afterRenderFrames, 43);
});

test("retry send failure rejects instead of leaving a pending control request", async () => {
  const pending = setMarkerEnabled({
    port: 4455,
    password: "a".repeat(32),
    scene: "Scene",
    itemId: 2,
    enabled: true,
    WebSocketClass: FakeSocket,
  });
  const socket = FakeSocket.current;
  socket.emit("message", { data: JSON.stringify({ op: 0, d: { authentication: { salt: "salt", challenge: "challenge" } } }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 2, d: {} }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 7, d: { requestId: "capture-marker", requestStatus: { result: true } } }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 7, d: { requestId: "capture-marker-barrier-baseline", requestStatus: { result: true }, responseData: { renderTotalFrames: 40 } } }) });
  await tick();
  socket.emit("message", { data: JSON.stringify({ op: 7, d: { requestId: "capture-marker-after", requestStatus: { result: true }, responseData: { renderTotalFrames: 40 } } }) });
  socket.throwOnSend = true;
  await assert.rejects(pending, /closed/);
});
