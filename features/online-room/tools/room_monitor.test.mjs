import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { assessLocalRoomLiveness, createLocalRoomMonitor, localRoom, stopRoom } from "./room_monitor.mjs";

const ROOM_URL = "ws://127.0.0.1:7199";
const FULL = ["initial-before", "initial", "recovery-1-before", "recovery-1", "recovery-2-before", "recovery-2"];

function socketFixture() {
  const listeners = new Map();
  return {
    client: { on(name, callback) { listeners.set(name, [...(listeners.get(name) || []), callback]); } },
    emit(name, payload) { for (const callback of listeners.get(name) || []) callback(payload); },
  };
}

function openSocket(emit, url = ROOM_URL, requestId = "room-1") {
  emit("Network.webSocketCreated", { requestId, url: new URL(url).href });
  emit("Network.webSocketHandshakeResponseReceived", { requestId, response: { status: 101 } });
}

const livingRoom = (lines = []) => ({ lines, exitCode: null, signalCode: null });

function attachedMonitor(child = livingRoom()) {
  const { client, emit } = socketFixture();
  const monitor = createLocalRoomMonitor(child, ROOM_URL, { pollMs: 1 });
  monitor.attach(client);
  return { monitor, emit, child };
}

async function freshCheckpoint(monitor, child, name) {
  setTimeout(() => child.lines.push("plw_room_stats t=2.000 seated=1"), 2);
  return monitor.checkpoint(name, Date.now() + 1000);
}

test("room checkpoints require telemetry newer than each checkpoint request", async () => {
  const { monitor, emit, child } = attachedMonitor(livingRoom(["plw_room_stats t=1 seated=1"]));
  openSocket(emit);
  await assert.rejects(monitor.checkpoint("initial", Date.now() + 15), /fresh local-room telemetry/);
  const points = [];
  for (const name of FULL) points.push(await freshCheckpoint(monitor, child, name));
  assert.deepEqual(points.map((point) => point.lineIndex), [1, 2, 3, 4, 5, 6]);
  assert.deepEqual(monitor.finish(), []);
});

test("latest unseated telemetry cannot be masked by an earlier seated line", async () => {
  const { monitor, emit, child } = attachedMonitor();
  openSocket(emit);
  await freshCheckpoint(monitor, child, "initial");
  setTimeout(() => child.lines.push("plw_room_stats t=2 seated=1", "plw_room_stats t=3 seated=0"), 2);
  await assert.rejects(monitor.checkpoint("recovery-1", Date.now() + 1000), /lost its seated player/);
});

test("dead room and unexpectedly closed, erroring or replaced sockets fail live checkpoints", async () => {
  for (const breakLink of [
    (emit, child) => { child.exitCode = 1; },
    (emit) => emit("Network.webSocketClosed", { requestId: "room-1" }),
    (emit) => emit("Network.webSocketFrameError", { requestId: "room-1", errorMessage: "connection reset" }),
    (emit) => emit("Network.webSocketFrameReceived", { requestId: "room-1", response: { opcode: 8 } }),
    (emit) => openSocket(emit, ROOM_URL, "replacement"),
  ]) {
    const { monitor, emit, child } = attachedMonitor();
    openSocket(emit);
    await freshCheckpoint(monitor, child, "initial");
    breakLink(emit, child);
    await assert.rejects(monitor.checkpoint("recovery-1", Date.now() + 100), /local room/);
    assert.notDeepEqual(monitor.finish(), []);
  }
});

test("a created socket without a 101 handshake is not an active room connection", async () => {
  const { monitor, emit, child } = attachedMonitor();
  emit("Network.webSocketCreated", { requestId: "room-1", url: `${ROOM_URL}/` });
  setTimeout(() => child.lines.push("plw_room_stats t=1 seated=1"), 2);
  await assert.rejects(monitor.checkpoint("initial", Date.now() + 15), /active WebSocket/);
});

test("teardown after release is outside monitoring, and every checkpoint of the mode is required", async () => {
  const { monitor, emit, child } = attachedMonitor();
  openSocket(emit);
  const point = await freshCheckpoint(monitor, child, "ready");
  monitor.release();
  emit("Network.webSocketClosed", { requestId: "room-1" });
  assert.deepEqual(monitor.finish("quick"), []);
  assert.equal(monitor.finish("full").length, 6);
  const evidence = { roomUrl: `${ROOM_URL}/`, processAlive: true, socketOpen: true, requestId: "room-1", faults: [], checkpoints: [point] };
  assert.match(assessLocalRoomLiveness({ ...evidence, checkpoints: [{ ...point, lineIndex: -1 }] }, ROOM_URL, "quick").join(" "), /at ready/);
  child.exitCode = 1;
  assert.match(monitor.finish("quick").join(" "), /process\/WebSocket evidence/);
});

test("stopRoom escalates to SIGKILL and returns when the child never exits", async () => {
  const signals = [];
  const stuck = { pid: 1, exitCode: null, signalCode: null, kill: (signal) => signals.push(signal), once() {} };
  await stopRoom(stuck, 5);
  assert.deepEqual(signals, ["SIGTERM", "SIGKILL"]);
});

test("localRoom starts the room on loopback, points the page at it and stops it", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "room-monitor-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const binary = join(root, "room-server");
  const child = livingRoom(["plw_room_listening port=7199"]);
  let stopped = null;
  const room = localRoom({
    binary, pollMs: 1,
    start: async (path, args, listening) => {
      assert.equal(path, binary);
      assert.deepEqual(args, ["--bind", "127.0.0.1", "--port", "0", "--stats-seconds", "1"]);
      return { child, match: listening.exec(child.lines[0]) };
    },
    stop: async (value) => { stopped = value; },
  });
  await assert.rejects(room.prepare("http://127.0.0.1:8123/index.html"), /requires a built local room server/);
  writeFileSync(binary, "fixture");
  const session = await room.prepare("http://127.0.0.1:8123/index.html");
  assert.equal(new URL(session.url).searchParams.get("room"), ROOM_URL);
  const { client, emit } = socketFixture();
  session.attach(client);
  openSocket(emit);
  await freshCheckpoint(session, child, "ready");
  session.release();
  assert.deepEqual(session.finish("quick"), []);
  assert.match(session.diagnose(), /exit=running .*plw_room_stats/);
  await session.stop();
  assert.equal(stopped, child);
});
