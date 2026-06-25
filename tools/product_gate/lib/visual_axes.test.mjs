import test from "node:test";
import assert from "node:assert/strict";
import { VISUAL_AXES, isAxisScore } from "./visual_axes.mjs";

test("VISUAL_AXES is the six canonical axes in order", () => {
  assert.deepEqual(VISUAL_AXES, [
    "composition",
    "readability",
    "ui_controls",
    "action_direction",
    "art_quality",
    "audience_fit",
  ]);
});

test("isAxisScore accepts integers 1-5 and rejects everything else", () => {
  for (const ok of [1, 2, 3, 4, 5]) assert.equal(isAxisScore(ok), true, `${ok} should be valid`);
  for (const bad of [0, 6, -1, 2.5, "3", NaN, null, undefined]) {
    assert.equal(isAxisScore(bad), false, `${String(bad)} should be invalid`);
  }
});
