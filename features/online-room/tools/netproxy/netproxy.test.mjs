import assert from "node:assert/strict";
import net from "node:net";
import test from "node:test";

import { createProxy, measureTimerLead, parseArgs, seededRandom } from "./netproxy.mjs";

// performance.now() and the loop's clock differ by under a millisecond, so a
// hop may land that much early. Lateness depends on how busy the box is: the
// median must sit near the configured rtt, and a single slow sample only
// within a loaded margin.
const EARLY_MS = 1;
const MEDIAN_MS = 4;
const LOADED_MS = 25;
const timerLeadMs = await measureTimerLead();

function echoServer() {
  const server = net.createServer({ allowHalfOpen: true }, (socket) => {
    socket.on("data", (chunk) => socket.write(chunk));
    socket.on("end", () => socket.end());
  });
  return new Promise((resolve) => server.listen(0, "127.0.0.1", () => resolve(server)));
}

function connect(port) {
  return new Promise((resolve, reject) => {
    const socket = net.connect({ host: "127.0.0.1", port }, () => resolve(socket));
    socket.setNoDelay(true);
    socket.once("error", reject);
  });
}

// Sends one byte and waits for it to come back; returns milliseconds.
function ping(socket) {
  return new Promise((resolve) => {
    const start = performance.now();
    socket.once("data", () => resolve(performance.now() - start));
    socket.write("p");
  });
}

async function withProxy(options, body) {
  const echo = await echoServer();
  const proxy = await createProxy({ targetHost: "127.0.0.1", targetPort: echo.address().port, spinMs: timerLeadMs, ...options });
  try {
    await body(proxy);
  } finally {
    await proxy.close();
    await new Promise((resolve) => echo.close(resolve));
  }
}

test("a round trip takes the configured rtt, within the jitter", async () => {
  const rttMs = 60;
  const jitterMs = 20;
  await withProxy({ rttMs, jitterMs, random: seededRandom(7) }, async (proxy) => {
    const socket = await connect(proxy.port);
    const samples = [];
    for (let index = 0; index < 12; index += 1) samples.push(await ping(socket));
    socket.destroy();
    samples.sort((a, b) => a - b);
    const low = samples[0];
    const high = samples.at(-1);
    const median = samples[samples.length / 2];
    console.log(`rtt ${rttMs} +- ${jitterMs} ms, timer lead ${timerLeadMs} ms: min ${low.toFixed(1)}, median ${median.toFixed(1)}, max ${high.toFixed(1)}`);
    assert.ok(low >= rttMs - jitterMs - 2 * EARLY_MS, `a round trip of ${low} ms beat the configured minimum`);
    assert.ok(Math.abs(median - rttMs) <= jitterMs / 2 + MEDIAN_MS, `the median round trip ${median} ms is off the configured rtt`);
    assert.ok(high <= rttMs + jitterMs + LOADED_MS, `a round trip of ${high} ms overran the configured maximum`);
    // Jitter spreads the samples; it does not only add delay.
    assert.ok(high - low > 1, "jitter left every round trip the same");
  });
});

test("jitter never reorders the byte stream", async () => {
  await withProxy({ rttMs: 30, jitterMs: 30, random: seededRandom(3) }, async (proxy) => {
    const socket = await connect(proxy.port);
    const sent = [];
    for (let index = 0; index < 300; index += 1) sent.push(`${index},`);
    const expected = sent.join("");
    const received = new Promise((resolve) => {
      let text = "";
      socket.on("data", (chunk) => {
        text += chunk.toString();
        if (text.length >= expected.length) resolve(text);
      });
    });
    for (const piece of sent) {
      socket.write(piece);
      await new Promise((resolve) => setImmediate(resolve));
    }
    assert.equal(await received, expected);
    socket.destroy();
  });
});

test("a closed sender still gets the replies held for it", async () => {
  await withProxy({ rttMs: 40, jitterMs: 0 }, async (proxy) => {
    const socket = net.connect({ host: "127.0.0.1", port: proxy.port, allowHalfOpen: true });
    const reply = new Promise((resolve) => {
      let text = "";
      socket.on("data", (chunk) => { text += chunk.toString(); });
      socket.on("end", () => resolve(text));
    });
    socket.end("last words");
    assert.equal(await reply, "last words");
  });
});

test("arguments are checked", () => {
  assert.deepEqual(parseArgs(["--target", "127.0.0.1:7001", "--rtt", "150", "--jitter", "30", "--listen", "7101", "--seed", "4"]), {
    listenPort: 7101, listenHost: "127.0.0.1", targetHost: "127.0.0.1", targetPort: 7001, rttMs: 150, jitterMs: 30, seed: 4, spinMs: undefined,
  });
  assert.equal(parseArgs(["--target", "127.0.0.1:7001", "--rtt", "150", "--spin", "2"]).spinMs, 2);
  assert.throws(() => parseArgs(["--rtt", "150"]), /--target is required/);
  assert.throws(() => parseArgs(["--target", "127.0.0.1:7001"]), /--rtt is required/);
  assert.throws(() => parseArgs(["--target", "localhost", "--rtt", "10"]), /host:port/);
  assert.throws(() => parseArgs(["--target", "127.0.0.1:7001", "--rtt", "10", "--jitter", "20"]), /must not exceed/);
  assert.throws(() => parseArgs(["--target", "127.0.0.1:7001", "--rtt", "-1"]), /--rtt takes/);
  assert.throws(() => parseArgs(["--target", "127.0.0.1:7001", "--rtt", "10", "--bogus", "1"]), /unknown option/);
});
