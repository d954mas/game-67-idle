import assert from "node:assert/strict";
import { createHash } from "node:crypto";
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
