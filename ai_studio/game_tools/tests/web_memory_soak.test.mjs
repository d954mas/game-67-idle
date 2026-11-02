import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { judgeSoak, runMemorySoak, seededRandom, uiMonkeyDriver } from "../web_memory_soak.mjs";

const MB = 1048576;

function flat(length = 30) {
  return Array.from({ length }, (_, i) => ({ t: i * 10, wasm_bytes: (i < 6 ? 64 + i : 70) * MB, wasm_grows: i < 6 ? i : 6,
    gpu_bytes: 60 * MB, gpu_objects: 400, programs: 31, js_heap_used_bytes: (3 + (i % 3)) * MB }));
}

test("a soak judges growth on post-warm-up peaks, not on the warm-up climb", () => {
  assert.equal(judgeSoak(flat(), 60).pass, true);
  const leak = flat().map((s, i) => ({ ...s, gpu_bytes: (60 + i * 0.5) * MB }));
  const verdict = judgeSoak(leak, 60);
  assert.equal(verdict.pass, false);
  assert.ok(verdict.series.gpu_bytes.slope_per_hour > 0);
  const lateGrow = flat().map((s, i) => ({ ...s, wasm_grows: i > 25 ? 7 : s.wasm_grows }));
  assert.equal(judgeSoak(lateGrow, 60).series.wasm_grows.ok, false, "a heap grow after warm-up is growth");
  assert.equal(judgeSoak(flat(7), 60).pass, false, "too few settled samples cannot pass");
  assert.equal(judgeSoak(flat().map(({ gpu_bytes, ...s }) => s), 60).pass, false, "a judged series that stops reporting cannot pass");
  const game = judgeSoak(flat().map((s, i) => ({ ...s, pack_bytes: i * MB })), 60, { tolerance: { pack_bytes: MB } });
  assert.equal(game.series.pack_bytes.ok, false, "a driver's own series is judged with its tolerance");
});

test("the seeded random repeats for one seed", () => {
  const a = seededRandom(7), b = seededRandom(7);
  assert.deepEqual([a(), a(), a()], [b(), b(), b()]);
});

test("a driver plugs its page, actions and samples into the soak, and the report is written", async (t) => {
  const out = mkdtempSync(join(tmpdir(), "memory-soak-"));
  t.after(() => rmSync(out, { recursive: true, force: true }));
  const done = [];
  const page = {
    errors: [], closed: false, viewport: { width: 390, height: 844 },
    census: async () => ({ wasm_bytes: 64 * MB, wasm_grows: 0, gpu_bytes: 30 * MB, kinds: { texture: { count: 3, bytes: 1 } }, programs: 4, js_heap_used_bytes: MB }),
    close: async () => { page.closed = true; },
  };
  const driver = {
    openPage: async ({ url }) => { assert.equal(url, "http://game.test/"); return page; },
    ready: async () => {},
    tick: ["tick", async () => { done.push("tick"); }],
    actions: [["open", async ({ pick }) => { done.push(pick(["a", "b"])); }]],
    sample: async () => ({ pack_bytes: 5 }),
    tolerance: { pack_bytes: 0 },
    pauseMs: [5, 10],
  };
  const report = await runMemorySoak({ url: "http://game.test/", driver, driverName: "fake", minutes: 0.01, warmup: 0, sample: 0.05, out, log: () => {} });
  assert.ok(page.closed);
  assert.ok(done.includes("tick") && done.some((name) => name === "a" || name === "b"));
  assert.equal(report.samples[0].pack_bytes, 5);
  assert.equal(report.samples[0].gpu_objects, 3);
  assert.ok(report.verdict.series.pack_bytes, "the driver's series is judged");
  assert.match(readFileSync(join(out, "soak.md"), "utf8"), /Soak \(fake\)/);
});

test("the UI monkey only taps, drags and presses keys", () => {
  assert.deepEqual(uiMonkeyDriver().actions.map(([name]) => name), ["tap", "drag", "key"]);
});
