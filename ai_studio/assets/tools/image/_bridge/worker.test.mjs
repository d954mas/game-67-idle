// Warm Python worker manager tests (T0202). Exercises the REAL worker (no mocks) with
// tiny temp Python fixtures, so it needs the studio venv; it skips cleanly only when the
// interpreter itself is missing. Covers: the warm second call skipping the import floor,
// a failing script surfacing LOUDLY (and NOT killing the worker), a crash mid-request
// rejecting loudly + respawning, FIFO serialization, and the idle-timeout kill.
//   node --test ai_studio/assets/tools/image/_bridge/worker.test.mjs
import test, { after } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";

import { resolvePythonPath } from "./bridge.mjs";
import { runPythonScript, shutdownImageWorkers, __workerForTest } from "./worker.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../../../..", import.meta.url)));

let PYTHON = "";
try {
  PYTHON = resolvePythonPath(REPO_ROOT);
} catch {
  PYTHON = "";
}

const FIX = mkdtempSync(join(tmpdir(), "worker-fix-"));
const script = (name, body) => {
  const path = join(FIX, name);
  writeFileSync(path, body);
  return path;
};
const OK = script("ok.py", "print('pong')\n");
const HEAVY = script("heavy.py", "import numpy as np\nprint(np.__version__)\n");
const BOOM = script("boom.py", "raise SystemExit('boom detail 42')\n");
const SLEEP = script("sleep.py", "import time\ntime.sleep(3)\nprint('woke')\n");

after(() => {
  shutdownImageWorkers();
  rmSync(FIX, { recursive: true, force: true });
});

const wait = (ms) => new Promise((r) => setTimeout(r, ms));

test("warm second call skips the interpreter+import floor", { skip: !PYTHON }, async () => {
  shutdownImageWorkers();
  const t0 = performance.now();
  await runPythonScript(REPO_ROOT, PYTHON, HEAVY, []);
  const cold = performance.now() - t0; // worker boot + numpy import

  const t1 = performance.now();
  await runPythonScript(REPO_ROOT, PYTHON, HEAVY, []);
  const warm = performance.now() - t1; // process alive + numpy cached in sys.modules

  assert.ok(warm < cold, `warm (${warm.toFixed(1)}ms) should be < cold (${cold.toFixed(1)}ms)`);
  assert.ok(warm < 100, `warm call should be well under 100ms, was ${warm.toFixed(1)}ms`);
});

test("a failing script is a LOUD error and does not kill the worker", { skip: !PYTHON }, async () => {
  shutdownImageWorkers();
  await runPythonScript(REPO_ROOT, PYTHON, OK, []); // boot
  const worker = __workerForTest(PYTHON);
  const child = worker.child;

  await assert.rejects(() => runPythonScript(REPO_ROOT, PYTHON, BOOM, []), /boom detail 42/);

  // A script error is NOT a worker crash: the same process keeps serving.
  const result = await runPythonScript(REPO_ROOT, PYTHON, OK, []);
  assert.match(result.stdout, /pong/);
  assert.equal(__workerForTest(PYTHON).child, child, "worker process survived a script failure");
});

test("a crash mid-request rejects loudly and the next request respawns cleanly", { skip: !PYTHON }, async () => {
  shutdownImageWorkers();
  await runPythonScript(REPO_ROOT, PYTHON, OK, []); // boot a worker
  const worker = __workerForTest(PYTHON);
  const doomedChild = worker.child;

  const pending = runPythonScript(REPO_ROOT, PYTHON, SLEEP, []); // long op, in flight
  await wait(300); // let the worker actually start the sleep
  assert.equal(worker.inflight != null, true, "sleep op should be in flight");
  doomedChild.kill(); // simulate a crash

  await assert.rejects(() => pending, /worker (exited|unavailable|failed)/i);

  // The NEXT request spawns a fresh worker and succeeds — no silent retry of the failed op.
  const result = await runPythonScript(REPO_ROOT, PYTHON, OK, []);
  assert.match(result.stdout, /pong/);
  assert.notEqual(__workerForTest(PYTHON).child, doomedChild, "a fresh worker replaced the crashed one");
});

test("requests serialize FIFO on one worker (second waits, no deadlock)", { skip: !PYTHON }, async () => {
  shutdownImageWorkers();
  const first = runPythonScript(REPO_ROOT, PYTHON, HEAVY, []);
  const second = runPythonScript(REPO_ROOT, PYTHON, OK, []);
  const worker = __workerForTest(PYTHON);
  assert.equal(worker.queue.length, 1, "the second op waits in the FIFO queue while the first runs");
  const results = await Promise.all([first, second]);
  assert.match(results[1].stdout, /pong/);
});

test("an idle worker is killed after the idle timeout", { skip: !PYTHON }, async () => {
  shutdownImageWorkers();
  const previous = process.env.AI_STUDIO_IMAGE_WORKER_IDLE_MS;
  process.env.AI_STUDIO_IMAGE_WORKER_IDLE_MS = "150";
  try {
    await runPythonScript(REPO_ROOT, PYTHON, OK, []);
    const worker = __workerForTest(PYTHON);
    assert.ok(worker.child, "worker is alive right after a request");
    await wait(500); // > idle timeout
    assert.equal(worker.child, null, "the idle worker process was killed after the timeout");
  } finally {
    if (previous === undefined) delete process.env.AI_STUDIO_IMAGE_WORKER_IDLE_MS;
    else process.env.AI_STUDIO_IMAGE_WORKER_IDLE_MS = previous;
  }
});
