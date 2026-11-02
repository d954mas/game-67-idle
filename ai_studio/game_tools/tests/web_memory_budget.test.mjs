import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { assessMemoryBudget, measureMemoryPoints, memoryBudgetFrom, memorySummary, readMemoryBudget, settleMemoryCensus } from "../web_memory_budget.mjs";

const MB = 1048576; // MiB

test("the memory section is optional and its limits must name census fields", () => {
  assert.equal(memoryBudgetFrom({ limits: { wasm_raw: 1 } }), null);
  assert.deepEqual(memoryBudgetFrom({ memory: { limits: { wasm_bytes: 96 * MB, wasm_grows: 0 } } }),
    { viewport: "390x844@2", afterSeconds: 0, limits: { wasm_bytes: 96 * MB, wasm_grows: 0 } });
  assert.throws(() => memoryBudgetFrom({ memory: { limits: { heap: 1 } } }), /"heap" is not one of/);
  assert.throws(() => memoryBudgetFrom({ memory: { limits: { gpu_bytes: -1 } } }), /non-negative integer/);
  assert.throws(() => memoryBudgetFrom({ memory: { limits: {} } }), /non-empty/);
  assert.throws(() => memoryBudgetFrom({ memory: { viewport: "phone", limits: { gpu_bytes: 1 } } }), /bad viewport/);
});

test("a game directory without web_budget.json has no memory budget", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "memory-budget-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  assert.equal(readMemoryBudget(dir), null);
  mkdirSync(join(dir, "content"));
  writeFileSync(join(dir, "content", "web_budget.json"), JSON.stringify({ limits: {}, memory: { viewport: "360x640", limits: { gpu_bytes: 5 } } }));
  assert.deepEqual(readMemoryBudget(dir), { viewport: "360x640", afterSeconds: 0, limits: { gpu_bytes: 5 } });
});

test("every limit passed or left unmeasured is a failure; equal is within the limit", () => {
  const budget = { viewport: "390x844@2", limits: { wasm_bytes: 96 * MB, gpu_bytes: 64 * MB, wasm_grows: 0, js_heap_used_bytes: 8 * MB } };
  const census = { wasm_bytes: 96 * MB, gpu_bytes: 65 * MB, wasm_grows: 1, js_heap_used_bytes: null };
  assert.deepEqual(assessMemoryBudget(census, budget), [
    "memory gpu_bytes 65.0 MiB is over its limit 64.0 MiB",
    "memory wasm_grows 1 is over its limit 0",
    "memory js_heap_used_bytes was not measured",
  ]);
  assert.deepEqual(assessMemoryBudget(null, budget), ["memory census missing: the page was never measured"]);
  assert.deepEqual(assessMemoryBudget(census, null), []);
});

test("startup settles once heap, GPU and programs hold still, and gives up at the cap", async () => {
  let clock = 0;
  const series = [10, 20, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30];
  let reads = 0;
  const evaluate = async () => ({ wasm_bytes: 64, gpu_bytes: series[Math.min(reads++, series.length - 1)], programs: 3 });
  const settled = await settleMemoryCensus(evaluate, { settleMs: 2000, capMs: 20000, pollMs: 500, now: () => clock, sleep: async (ms) => { clock += ms; } });
  assert.equal(settled.settled, true);
  assert.equal(settled.gpu_bytes, 30);
  clock = 0;
  const moving = await settleMemoryCensus(async () => ({ wasm_bytes: 64, gpu_bytes: clock, programs: 3 }),
    { settleMs: 2000, capMs: 5000, pollMs: 500, now: () => clock, sleep: async (ms) => { clock += ms; } });
  assert.equal(moving.settled, false);
});

test("a second point after streamed content is measured and judged, and an unsettled point fails", async () => {
  assert.equal(memoryBudgetFrom({ memory: { after_seconds: 60, limits: { gpu_bytes: 1 } } }).afterSeconds, 60);
  assert.throws(() => memoryBudgetFrom({ memory: { after_seconds: 601, limits: { gpu_bytes: 1 } } }), /after_seconds/);
  let clock = 0;
  let gpu = 40 * MB;
  const calls = [];
  const points = await measureMemoryPoints({
    call: async (method, params) => calls.push([method, params.width, params.mobile]),
    evaluate: async (expression) => (/__memoryCensusRead/.test(expression) ? { wasm_bytes: 64 * MB, wasm_grows: 0, gpu_bytes: gpu, programs: 3 } : null),
    viewport: "390x844@2", afterSeconds: 60,
    settle: { settleMs: 1000, capMs: 5000, pollMs: 500, now: () => clock, sleep: async (ms) => { clock += ms; } },
    sleep: async (ms) => { clock += ms; gpu = 62 * MB; },
  });
  assert.deepEqual(calls, [["Emulation.setDeviceMetricsOverride", 390, true]]);
  assert.deepEqual(points.map((p) => [p.point, p.gpu_bytes, p.settled]), [["startup", 40 * MB, true], ["after 60 s", 62 * MB, true]]);
  const budget = { limits: { gpu_bytes: 60 * MB } };
  assert.deepEqual(assessMemoryBudget(points, budget), ["memory gpu_bytes at after 60 s 62.0 MiB is over its limit 60.0 MiB"]);
  assert.deepEqual(assessMemoryBudget([{ point: "startup", settled: false, waited_ms: 20000, gpu_bytes: 1 }], budget),
    ["memory census at startup did not settle in 20 s: heap, GPU or programs still changing"]);
  assert.match(memorySummary(points), /^startup: wasm 64\.0 MiB \(0 grows\), gpu 40\.0 MiB, programs 3; after 60 s: wasm 64\.0 MiB/);
});
