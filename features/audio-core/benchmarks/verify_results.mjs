import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";

const root = new URL("./results/windows-2026-07-12/", import.meta.url);
const result = JSON.parse(await readFile(new URL("result.json", root), "utf8"));
const web = JSON.parse(await readFile(new URL("web-result.json", root), "utf8"));
const report = await readFile(new URL("REPORT.md", root), "utf8");

assert.equal(web.schema, "audio_core.web_build_benchmark.v1");
assert.equal(result.measurement_scope, "Windows native plus paired Emscripten Release build");
assert.equal(result.compile.runs_per_tu, 3);
assert.equal(result.source.baseline_commit, web.baseline_commit);
for (const field of ["baseline_build_ms", "current_build_ms", "baseline_js_bytes", "current_js_bytes", "baseline_wasm_bytes", "current_wasm_bytes"]) {
  assert.equal(result.web_build[field], web[field], `web field drift: ${field}`);
}
assert.equal(result.web_build.delta_ms, Number((web.current_build_ms - web.baseline_build_ms).toFixed(4)));
assert.equal(result.web_build.js_delta_bytes, web.current_js_bytes - web.baseline_js_bytes);
assert.equal(result.web_build.wasm_delta_bytes, web.current_wasm_bytes - web.baseline_wasm_bytes);
assert.ok(!result.unmeasured.includes("Web compile/build/runtime"));
for (const value of [web.baseline_build_ms, web.current_build_ms, web.baseline_js_bytes, web.current_js_bytes, web.baseline_wasm_bytes, web.current_wasm_bytes]) {
  assert.ok(report.includes(String(value)), `report omits web evidence value: ${value}`);
}
console.log("audio benchmark result consistency: PASS");
