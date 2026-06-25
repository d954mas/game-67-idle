import assert from "node:assert/strict";
import test from "node:test";
import { VALIDATE_BOOLEAN_FLAGS, VALIDATE_VALUE_FLAGS } from "./validate_flags.mjs";

test("flag vocabulary is well-formed (all --prefixed, no dupes, no overlap)", () => {
  const all = [...VALIDATE_BOOLEAN_FLAGS, ...VALIDATE_VALUE_FLAGS];
  for (const flag of all) assert.match(flag, /^--[a-z][a-z-]*$/, `bad flag: ${flag}`);
  assert.equal(new Set(all).size, all.length, "duplicate flag across the two sets");
});

test("the vocabulary matches the documented validate contract (regression guard)", () => {
  // If you intentionally add/remove a validate flag, update BOTH this assertion
  // and ai_studio/core_harness/validation/pipeline_validate.mjs usage text.
  assert.deepEqual(VALIDATE_BOOLEAN_FLAGS, [
    "--quick",
    "--full",
    "--review",
    "--dry-run",
    "--reexport-tests",
    "--no-prune",
    "--with-assets",
  ]);
  assert.deepEqual(VALIDATE_VALUE_FLAGS, ["--keep-exports"]);
});
