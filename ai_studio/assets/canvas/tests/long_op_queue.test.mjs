// The page's long-op FIFO limiter (site/long_op_queue.mjs) is pure/DOM-free, so its
// scheduling is unit-testable in node: slot accounting, FIFO order, a failed op freeing
// its slot, and cancel of a still-queued op. Run:
//   node --test ai_studio/assets/canvas/tests/long_op_queue.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { LongOpQueue } from "../site/long_op_queue.mjs";

// A promise the test resolves/rejects by hand, so it controls exactly when an op finishes.
function deferred() {
  let resolve;
  let reject;
  const promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;
  });
  return { promise, resolve, reject };
}

// Flush the microtask queue so run()/settle() side effects land before asserting.
const flush = () => new Promise((r) => setTimeout(r, 0));

test("rejects a bad max", () => {
  assert.throws(() => new LongOpQueue({ max: 0 }));
});

test("slot accounting: 3 submits at max 2 -> 2 running, 1 queued (synchronously)", () => {
  const q = new LongOpQueue({ max: 2 });
  const held = [deferred(), deferred(), deferred()];
  const started = [];
  held.forEach((d, i) =>
    q.submit({ label: `t${i}`, run: () => d.promise, onStart: () => started.push(i) }),
  );
  assert.equal(q.runningCount, 2);
  assert.equal(q.queuedCount, 1);
  assert.deepEqual(started, [0, 1], "onStart fires synchronously only for the two that got a slot");
});

test("FIFO: freeing a slot starts the oldest queued op next", async () => {
  const q = new LongOpQueue({ max: 2 });
  const held = [deferred(), deferred(), deferred(), deferred()];
  const started = [];
  held.forEach((d, i) =>
    q.submit({ label: `t${i}`, run: () => d.promise, onStart: () => started.push(i) }),
  );
  assert.deepEqual(started, [0, 1]);
  held[0].resolve("ok"); // free a slot
  await flush();
  assert.deepEqual(started, [0, 1, 2], "the oldest queued (t2) runs before t3");
  assert.equal(q.queuedCount, 1);
  held[1].resolve("ok");
  await flush();
  assert.deepEqual(started, [0, 1, 2, 3]);
});

test("a failed op frees its slot and passes the error to onSettled", async () => {
  const q = new LongOpQueue({ max: 1 });
  const a = deferred();
  const b = deferred();
  const settled = [];
  q.submit({ label: "a", run: () => a.promise, onSettled: (err) => settled.push(["a", err ? err.message : null]) });
  const startedB = [];
  q.submit({ label: "b", run: () => b.promise, onStart: () => startedB.push("b"), onSettled: (err) => settled.push(["b", err ? err.message : null]) });
  assert.equal(q.runningCount, 1);
  assert.deepEqual(startedB, [], "b waits while a holds the only slot");
  a.reject(new Error("python boom"));
  await flush();
  assert.deepEqual(settled, [["a", "python boom"]]);
  assert.deepEqual(startedB, ["b"], "the failed op's slot freed, so b starts");
  b.resolve("done");
  await flush();
  assert.deepEqual(settled, [["a", "python boom"], ["b", null]]);
});

test("cancel removes a still-queued op (it never runs); a running op cannot be cancelled", async () => {
  const q = new LongOpQueue({ max: 1 });
  const a = deferred();
  const b = deferred();
  const started = [];
  const runningId = q.submit({ label: "a", run: () => a.promise, onStart: () => started.push("a") });
  const queuedId = q.submit({ label: "b", run: () => b.promise, onStart: () => started.push("b") });

  assert.equal(q.cancel(runningId), false, "the running op has no server cancel -> false");
  assert.equal(q.cancel(queuedId), true, "the queued op is removed -> true");
  assert.equal(q.queuedCount, 0);

  a.resolve("ok");
  await flush();
  assert.deepEqual(started, ["a"], "the cancelled op b never started");
  assert.equal(q.runningCount, 0);
  assert.equal(q.cancel("op-does-not-exist"), false);
});

test("queue positions are reported and shift up as slots free", async () => {
  const q = new LongOpQueue({ max: 1 });
  const held = [deferred(), deferred(), deferred()];
  const positions = { 1: [], 2: [] }; // last reported position per task index (1 and 2 queue)
  q.submit({ label: "t0", run: () => held[0].promise });
  q.submit({ label: "t1", run: () => held[1].promise, onQueue: (p) => positions[1].push(p) });
  q.submit({ label: "t2", run: () => held[2].promise, onQueue: (p) => positions[2].push(p) });

  assert.equal(positions[1].at(-1), 1, "t1 is first in line");
  assert.equal(positions[2].at(-1), 2, "t2 is second in line");

  held[0].resolve("ok"); // t1 starts, t2 moves to position 1
  await flush();
  assert.equal(positions[2].at(-1), 1, "t2 shifted up to the front of the queue");
});
