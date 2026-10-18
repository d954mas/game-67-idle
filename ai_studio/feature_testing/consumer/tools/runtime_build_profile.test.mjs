import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import test from "node:test";

import { runtimeBuildWitness, validateRuntimeBuildRecord } from "./lib/runtime_build.mjs";

const inputs = [
  { id: "game", source: ".", files: 1, sha256: "1".repeat(64) },
  { id: "engine", source: "external/neotolis-engine", files: 1, sha256: "2".repeat(64) },
];
const profile = {
  target: "poki", adapter: "poki", preset: "wasm-release",
  debugUi: false, devapi: false, analytics: false, eventsLogMirror: false,
};
const fingerprint = createHash("sha256").update(JSON.stringify({ inputs, profile })).digest("hex");

test("runtime build witness binds target adapter and web preset", () => {
  const record = { schema: "ai_studio.runtime_build.v2", fingerprint, inputs, profile };
  assert.deepEqual(validateRuntimeBuildRecord(record), record);
  assert.equal(
    runtimeBuildWitness(record),
    `ai_studio.runtime_build:${fingerprint};t:poki;a:poki;p:wasm-release;du:0;d:0;a:0;l:0`,
  );
  assert.throws(() => validateRuntimeBuildRecord({ ...record, profile: { ...profile, target: "yandex" } }), /fingerprint/i);
  assert.throws(() => validateRuntimeBuildRecord({ ...record, profile: { ...profile, analytics: true } }), /fingerprint/i);
});

// Two sides own the runtime identity a browser reports: the witness the build
// bakes into the wasm, and the JS that republishes it as a bare fingerprint for
// the page config and the packaged smoke. Nothing links them at compile time,
// so the witness gained profile fields once while the parse kept the whole
// tail, and every packaged web build stopped reaching readiness.
test("the wasm marker publishes the witness fingerprint the page compares", () => {
  const source = readFileSync(new URL("../src/runtime_build_marker.c", import.meta.url), "utf8");
  const body = source.match(
    /EM_JS\(void, runtime_build_marker_publish_js,[^{]*\{([\s\S]*?)\n\}\)/,
  )?.[1];
  assert.ok(body, "runtime build marker publish body was not found");
  const publish = new Function("globalThis", "UTF8ToString", "marker_ptr", body);

  const witness = runtimeBuildWitness({ schema: "ai_studio.runtime_build.v2", fingerprint, inputs, profile });
  const published = {};
  publish(published, () => witness, 0);
  assert.equal(published.__AI_STUDIO_RUNTIME_BUILD_FINGERPRINT__, fingerprint);

  const foreign = {};
  publish(foreign, () => "some.other.marker:value", 0);
  assert.equal(foreign.__AI_STUDIO_RUNTIME_BUILD_FINGERPRINT__, "");
});
