// The `live` hook of ai_studio/game_tools/package_web_smoke.mjs for room games:
// at every probe checkpoint the loopback room lives, the page's WebSocket to it
// is open, and the room's newest stats line still counts a seated player.
import { spawn } from "node:child_process";
import { existsSync } from "node:fs";

const ROOM_ARGS = ["--bind", "127.0.0.1", "--port", "0", "--stats-seconds", "1"];
const LISTENING = /^plw_room_listening port=(\d+)/;
const SEATED = /^plw_room_stats\b.*\bseated=(\d+)\b/;
const CHECKPOINT_WAIT_MS = 15000;
const STOP_WAIT_MS = 3000;
// The packaged smoke's checkpoints, in the order its probe takes them.
const CHECKPOINTS = {
  quick: ["ready"],
  full: ["initial-before", "initial", "recovery-1-before", "recovery-1", "recovery-2-before", "recovery-2"],
};

function startRoom(binary, args, listening, timeoutMs = 20000) {
  return new Promise((resolveStart, rejectStart) => {
    const child = spawn(binary, args, { stdio: ["ignore", "pipe", "pipe"] });
    child.lines = [];
    let buffered = "";
    let settled = false;
    const finish = (error, match) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      if (error) {
        stopRoom(child).then(() => rejectStart(error));
      } else {
        resolveStart({ child, match });
      }
    };
    const timer = setTimeout(() => finish(new Error(`${binary} printed no ${listening} within ${timeoutMs} ms`)), timeoutMs);
    child.stdout.on("data", (chunk) => {
      buffered += chunk.toString();
      const lines = buffered.split(/\r?\n/);
      buffered = lines.pop();
      for (const line of lines) {
        child.lines.push(line);
        const match = listening.exec(line);
        if (match) finish(null, match);
      }
    });
    child.stderr.on("data", (chunk) => child.lines.push(chunk.toString().trim()));
    child.once("error", (error) => finish(error));
    child.once("exit", (code) => finish(new Error(`${binary} exited (${code}) before ${listening}: ${child.lines.slice(-5).join(" | ")}`)));
  });
}

// SIGTERM, then SIGKILL after a bounded wait; resolves even if the child never
// reports its exit, so cleanup cannot hang the smoke.
export async function stopRoom(child, waitMs = STOP_WAIT_MS) {
  const exited = () => new Promise((resolveExit) => {
    if (!roomAlive(child)) { resolveExit(); return; }
    const timer = setTimeout(resolveExit, waitMs);
    child.once("exit", () => { clearTimeout(timer); resolveExit(); });
  });
  if (!child.pid) return;
  for (const signal of ["SIGTERM", "SIGKILL"]) {
    if (!roomAlive(child)) return;
    try { child.kill(signal); } catch { /* already gone */ }
    await exited();
  }
}

const roomAlive = (child) => child.exitCode === null && child.signalCode === null;

// Every checkpoint of the probe's mode must have seen a stats line printed after
// it was requested, in order.
export function assessLocalRoomLiveness(liveness, rawRoomUrl, mode = "full") {
  const failures = [];
  if (!liveness || liveness.roomUrl !== new URL(rawRoomUrl).href || liveness.processAlive !== true
      || liveness.socketOpen !== true || !liveness.requestId || !Array.isArray(liveness.faults) || liveness.faults.length) {
    failures.push("active local-room process/WebSocket evidence is missing or failed");
  }
  let previous = -1;
  for (const name of CHECKPOINTS[mode]) {
    const point = liveness?.checkpoints?.find((item) => item.name === name);
    if (!point || point.alive !== true || point.socketOpen !== true || !(point.seated > 0)
        || point.requestId !== liveness?.requestId || !Number.isInteger(point.afterLineCount)
        || !Number.isInteger(point.lineIndex) || point.lineIndex < point.afterLineCount || point.lineIndex <= previous
        || !Number.isFinite(point.requestedAt) || !Number.isFinite(point.observedAt) || point.observedAt < point.requestedAt) {
      failures.push(`fresh local-room liveness at ${name} is missing or failed`);
    }
    if (point) previous = point.lineIndex;
  }
  return failures;
}

export function createLocalRoomMonitor(child, rawRoomUrl, { seated = SEATED, pollMs = 50 } = {}) {
  const roomUrl = new URL(rawRoomUrl).href;
  let monitoring = true;
  let requestId = null;
  let socketOpen = false;
  const faults = [];
  const checkpoints = [];
  const fault = (text) => { if (monitoring && !faults.includes(text)) faults.push(text); };
  const matches = (id) => monitoring && requestId !== null && id === requestId;
  const exited = () => fault("local room process exited during the live probe");
  child.on?.("exit", exited);
  let released = null;
  function requireHealthy() {
    if (!roomAlive(child)) {
      const message = `local room process is not alive (exit=${child.exitCode}, signal=${child.signalCode})`;
      fault(message);
      throw new Error(message);
    }
    if (faults.length) throw new Error(faults.join("; "));
  }
  function evidence() {
    return { roomUrl, processAlive: roomAlive(child), socketOpen, requestId, faults: [...faults], checkpoints: checkpoints.map((point) => ({ ...point })) };
  }
  return {
    attach(client) {
      client.on("Network.webSocketCreated", (event) => {
        if (!monitoring) return;
        let url;
        try { url = new URL(event.url).href; } catch { return; }
        if (url !== roomUrl) return;
        if (requestId !== null && requestId !== event.requestId) fault("local room WebSocket was unexpectedly replaced");
        requestId = event.requestId;
        socketOpen = false;
      });
      client.on("Network.webSocketHandshakeResponseReceived", (event) => {
        if (!matches(event.requestId)) return;
        socketOpen = event.response?.status === 101;
        if (!socketOpen) fault(`local room WebSocket handshake failed: ${event.response?.status ?? "missing status"}`);
      });
      client.on("Network.webSocketClosed", (event) => {
        if (!matches(event.requestId)) return;
        socketOpen = false;
        fault("local room WebSocket unexpectedly closed during the live probe");
      });
      client.on("Network.webSocketFrameError", (event) => {
        if (matches(event.requestId)) fault(`local room WebSocket error: ${event.errorMessage || "unknown"}`);
      });
      for (const eventName of ["Network.webSocketFrameReceived", "Network.webSocketFrameSent"]) {
        client.on(eventName, (event) => {
          if (matches(event.requestId) && event.response?.opcode === 8) fault("local room WebSocket sent or received an unexpected CLOSE frame");
        });
      }
    },
    async checkpoint(name, deadline) {
      // Only lines printed after this request count, and the newest one wins:
      // an older seated line must not mask a player who has since left.
      const afterLineCount = child.lines?.length || 0;
      const requestedAt = Date.now();
      const until = Math.min(deadline, requestedAt + CHECKPOINT_WAIT_MS);
      let cursor = afterLineCount;
      let latest = null;
      while (Date.now() < until) {
        requireHealthy();
        const lines = child.lines || [];
        for (; cursor < lines.length; cursor += 1) {
          const match = seated.exec(lines[cursor]);
          if (!match) continue;
          latest = { line: lines[cursor], lineIndex: cursor, seated: Number(match[1]), observedAt: Date.now() };
          if (checkpoints.length && latest.seated === 0) fault("local room lost its seated player during the live probe");
        }
        requireHealthy();
        if (latest?.seated > 0 && socketOpen) {
          const point = { name, requestedAt, afterLineCount, ...latest, alive: true, socketOpen: true, requestId };
          checkpoints.push(point);
          return { ...point };
        }
        await new Promise((done) => setTimeout(done, Math.min(pollMs, Math.max(1, until - Date.now()))));
      }
      requireHealthy();
      throw new Error(`no seated player in fresh local-room telemetry with an active WebSocket (${name})`);
    },
    // Ends monitoring while the page is open: what the browser does to the
    // socket after this is teardown. The process must still outlive it.
    release() {
      released ??= evidence();
      monitoring = false;
      child.removeListener?.("exit", exited);
    },
    finish(mode) {
      this.release();
      return assessLocalRoomLiveness({ ...released, processAlive: released.processAlive && roomAlive(child) }, roomUrl, mode);
    },
  };
}

// The `live` hook for smokePackagedWebArtifact. `binary` is the game's built
// room server; the patterns match its listening and stats lines.
export function localRoom({ binary, args = ROOM_ARGS, listening = LISTENING, seated = SEATED, pollMs = 50,
  start = startRoom, stop = stopRoom }) {
  return {
    async prepare(pageUrl) {
      if (!existsSync(binary)) throw new Error(`packaged smoke requires a built local room server: ${binary}`);
      const room = await start(binary, args, listening);
      const roomUrl = `ws://127.0.0.1:${room.match[1]}`;
      const url = new URL(pageUrl);
      url.searchParams.set("room", roomUrl);
      const monitor = createLocalRoomMonitor(room.child, roomUrl, { seated, pollMs });
      return {
        url: url.href,
        roomUrl,
        attach: monitor.attach,
        checkpoint: monitor.checkpoint,
        release: () => monitor.release(),
        finish: (mode) => monitor.finish(mode),
        stop: () => stop(room.child),
        diagnose: () => `local room exit=${room.child.exitCode ?? "running"} signal=${room.child.signalCode ?? "none"}; `
          + `last room output=${JSON.stringify(room.child.lines?.slice(-8) || [])}`,
      };
    },
  };
}
