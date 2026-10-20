// An order-keeping TCP proxy that delays traffic for the native Windows dev
// loop, where the docker netem lab (tools/roomload/impairment_lab.py) does not
// reach a native room and client. The netem lab stays the acceptance matrix;
// this is for playing and tuning at a given latency.
//
//   node tools/netproxy/netproxy.mjs --target 127.0.0.1:7001 --rtt 150 --jitter 30 [--listen 7101] [--seed 1] [--spin <ms>]
//
// Each direction holds every chunk for rtt / 2 plus a uniform offset in
// [-jitter / 2, +jitter / 2], so one round trip takes rtt +- jitter. A chunk is
// never delivered before the one sent ahead of it: TCP order is kept, and
// jitter that would reorder chunks stretches the later one instead. So in a
// dense stream each chunk takes the largest recent draw, and its delay drifts
// toward rtt / 2 + jitter / 2 per direction; isolated messages see the full
// spread.
//
// Precision: a hop lands up to (timer lead - spin) ms late, where the timer
// lead is how late this machine's timers fire (printed at start: about 1 ms on
// Linux, up to a 15.6 ms scheduler tick on Windows). --spin is the window
// before each due time the proxy busy-yields instead of sleeping. By default it
// is the measured lead, so delays are exact and proofs at a stated rtt +-
// jitter hold, at the cost of a busy core while traffic is due. The dev loop
// passes --spin 2 for low CPU. A performance capture never runs through a
// precise proxy: its busy core skews the numbers.
import net from "node:net";
import { fileURLToPath } from "node:url";
import { resolve } from "node:path";

const USAGE = "usage: node tools/netproxy/netproxy.mjs --target <host:port> --rtt <ms> [--jitter <ms>] [--listen <port>] [--host <address>] [--seed <n>] [--spin <ms>]";

// Mulberry32: a seeded jitter makes a latency run repeatable.
export function seededRandom(seed) {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export function parseArgs(argv) {
  const args = { listenPort: 0, listenHost: "127.0.0.1", targetHost: "", targetPort: 0, rttMs: -1, jitterMs: 0, seed: undefined, spinMs: undefined };
  const value = (index) => {
    const next = argv[index + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${argv[index]}\n${USAGE}`);
    return next;
  };
  const number = (text, option, min, max) => {
    const parsed = Number(text);
    if (!Number.isFinite(parsed) || parsed < min || parsed > max) throw new Error(`${option} takes ${min}..${max}\n${USAGE}`);
    return parsed;
  };
  for (let index = 0; index < argv.length; index += 2) {
    const option = argv[index];
    if (option === "--target") {
      const match = /^(.+):(\d+)$/.exec(value(index));
      if (!match) throw new Error(`--target takes host:port\n${USAGE}`);
      args.targetHost = match[1];
      args.targetPort = number(match[2], "--target port", 1, 65535);
    } else if (option === "--rtt") args.rttMs = number(value(index), option, 0, 60000);
    else if (option === "--jitter") args.jitterMs = number(value(index), option, 0, 60000);
    else if (option === "--listen") args.listenPort = number(value(index), option, 0, 65535);
    else if (option === "--host") args.listenHost = value(index);
    else if (option === "--seed") args.seed = number(value(index), option, 0, 4294967295);
    else if (option === "--spin") args.spinMs = number(value(index), option, 0, 20);
    else throw new Error(`unknown option: ${option}\n${USAGE}`);
  }
  if (!args.targetHost) throw new Error(`--target is required\n${USAGE}`);
  if (args.rttMs < 0) throw new Error(`--rtt is required\n${USAGE}`);
  if (args.jitterMs > args.rttMs) throw new Error(`--jitter must not exceed --rtt: a one-way delay cannot go below 0\n${USAGE}`);
  return args;
}

// How late a timer fires past its time on this machine, in ms: about 1 on
// Linux, up to a 15.6 ms scheduler tick on Windows.
export async function measureTimerLead(samples = 8) {
  let worst = 0;
  for (let index = 0; index < samples; index += 1) {
    const start = performance.now();
    await new Promise((resolvePromise) => setTimeout(resolvePromise, 1));
    worst = Math.max(worst, performance.now() - start - 1);
  }
  return Math.min(20, Math.ceil(worst) + 1);
}

// One direction of one connection: chunks wait in arrival order and leave at
// their due time, never before the chunk ahead of them. A timer sleeps until
// `spinMs` before the due time; from there the pipe yields to the event loop
// turn by turn (socket I/O keeps running) until the exact moment, burning CPU
// only in that window. A full receiver pauses the sender, as TCP would.
function delayedPipe(from, to, oneWayMs, jitterMs, random, spinMs) {
  const queue = [];
  let lastDue = 0;
  let scheduled = false;
  let stopped = false;
  let ended = false;
  let timer = null;
  const flush = () => {
    timer = null;
    if (stopped) return;
    const now = performance.now();
    while (queue.length > 0 && queue[0].due <= now) {
      const { chunk } = queue.shift();
      if (!to.destroyed && !to.write(chunk) && !from.isPaused()) {
        from.pause();
        to.once("drain", () => from.resume());
      }
    }
    if (queue.length > 0) wake(queue[0].due - now);
    else {
      scheduled = false;
      if (ended && !to.destroyed) to.end();
    }
  };
  const wake = (waitMs) => {
    scheduled = true;
    if (waitMs > spinMs) timer = setTimeout(flush, waitMs - spinMs);
    else setImmediate(flush);
  };
  from.on("data", (chunk) => {
    const due = Math.max(performance.now() + oneWayMs + (random() - 0.5) * jitterMs, lastDue);
    lastDue = due;
    queue.push({ due, chunk });
    if (!scheduled) wake(due - performance.now());
  });
  from.on("end", () => {
    ended = true;
    if (!scheduled) flush();
  });
  return () => {
    stopped = true;
    if (timer !== null) clearTimeout(timer);
    queue.length = 0;
  };
}

export function createProxy({ listenPort = 0, listenHost = "127.0.0.1", targetHost, targetPort, rttMs, jitterMs = 0,
  random = Math.random, spinMs = 2, onError = (error) => console.error(`netproxy: ${error.message}`) }) {
  const sockets = new Set();
  // Half-open sockets: a side that has finished sending still receives the
  // replies the proxy is holding for it. An error on either side closes both
  // at once, dropping whatever was held.
  const server = net.createServer({ allowHalfOpen: true }, (client) => {
    const upstream = net.connect({ host: targetHost, port: targetPort, allowHalfOpen: true });
    client.setNoDelay(true);
    upstream.setNoDelay(true);
    sockets.add(client);
    sockets.add(upstream);
    const stops = [
      delayedPipe(client, upstream, rttMs / 2, jitterMs, random, spinMs),
      delayedPipe(upstream, client, rttMs / 2, jitterMs, random, spinMs),
    ];
    const fail = () => {
      for (const stop of stops) stop();
      client.destroy();
      upstream.destroy();
    };
    client.on("error", fail);
    upstream.on("error", fail);
    client.on("close", () => sockets.delete(client));
    upstream.on("close", () => sockets.delete(upstream));
  });
  return new Promise((resolvePromise, reject) => {
    server.once("error", reject);
    server.listen(listenPort, listenHost, () => {
      server.off("error", reject);
      server.on("error", onError);
      resolvePromise({
        port: server.address().port,
        close: () => new Promise((done) => {
          for (const socket of sockets) socket.destroy();
          server.close(() => done());
        }),
      });
    });
  });
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const random = args.seed === undefined ? Math.random : seededRandom(args.seed);
  const timerLeadMs = await measureTimerLead();
  args.spinMs ??= timerLeadMs;
  const proxy = await createProxy({ ...args, random });
  console.log(`netproxy listening on ${args.listenHost}:${proxy.port} -> ${args.targetHost}:${args.targetPort} `
    + `(rtt ${args.rttMs} ms +- ${args.jitterMs} ms; timer lead ${timerLeadMs} ms, spin ${args.spinMs} ms: `
    + `a hop lands up to ${Math.max(0, timerLeadMs - args.spinMs)} ms late)`);
  const stop = () => proxy.close().then(() => process.exit(0));
  process.on("SIGINT", stop);
  process.on("SIGTERM", stop);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main().catch((error) => {
    console.error(error?.message || String(error));
    process.exitCode = String(error?.message || "").includes("usage:") ? 2 : 1;
  });
}
