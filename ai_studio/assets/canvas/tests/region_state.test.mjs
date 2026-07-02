// Region-edit isolation reconcile (site page state). reconcileRegionEdit is the pure
// helper reloadProject runs after every mutating op / undo / redo so the page's
// region-edit mode never renders a dead mode or stale rects. Run:
//   node --test ai_studio/assets/canvas/tests/region_state.test.mjs
//
// Figma isolation-mode rule (already decided): undo/redo operate on the document
// journal, NOT the mode. So this helper only fixes up the page-only mode + region
// selection against the reloaded project — it never re-journals anything.
import test from "node:test";
import assert from "node:assert/strict";
import { reconcileRegionEdit } from "../site/app.js";

// A project with one editable image `el1` carrying regions r1/r2, optionally grouped.
function project(overrides = {}) {
  return {
    id: "p1",
    elements: [
      { id: "el1", type: "image", src: "a.png", visible: true, regions: [{ id: "r1", rect: [0, 0, 4, 4] }, { id: "r2", rect: [5, 5, 4, 4] }], ...overrides.el1 },
      { id: "el2", type: "image", src: "b.png", visible: true },
    ],
    groups: overrides.groups || [],
  };
}

test("no active mode reconciles to a clean exit", () => {
  const out = reconcileRegionEdit(project(), null, new Set(["r1"]));
  assert.equal(out.regionEditId, null);
  assert.equal(out.selectedRegionIds.size, 0);
});

test("edited element still present -> STAY in mode, keep live region selection", () => {
  const out = reconcileRegionEdit(project(), "el1", new Set(["r1", "r2"]));
  assert.equal(out.regionEditId, "el1");
  assert.deepEqual([...out.selectedRegionIds].sort(), ["r1", "r2"]);
});

test("undo removed a selected region -> stay, prune the dead id (no stale rect)", () => {
  // r2 no longer exists after the undo; the mode must stay, selection drops to {r1}.
  const p = project({ el1: { regions: [{ id: "r1", rect: [0, 0, 4, 4] }] } });
  const out = reconcileRegionEdit(p, "el1", new Set(["r1", "r2"]));
  assert.equal(out.regionEditId, "el1");
  assert.deepEqual([...out.selectedRegionIds], ["r1"]);
});

test("undo emptied the regions -> STAY in mode (empty state), selection pruned to none", () => {
  const p = project({ el1: { regions: [] } });
  const out = reconcileRegionEdit(p, "el1", new Set(["r1"]));
  assert.equal(out.regionEditId, "el1", "empty regions still keep the isolation mode");
  assert.equal(out.selectedRegionIds.size, 0);
});

test("undo removed the edited element -> exit to object mode gracefully", () => {
  const p = project();
  p.elements = p.elements.filter((e) => e.id !== "el1");
  const out = reconcileRegionEdit(p, "el1", new Set(["r1"]));
  assert.equal(out.regionEditId, null);
  assert.equal(out.selectedRegionIds.size, 0);
});

test("edited element hidden (element.visible=false) -> exit the mode", () => {
  const p = project({ el1: { visible: false } });
  const out = reconcileRegionEdit(p, "el1", new Set(["r1"]));
  assert.equal(out.regionEditId, null);
});

test("edited element in a hidden group -> exit the mode", () => {
  const p = project({ groups: [{ id: "g1", visible: false }] });
  p.elements[0].groupId = "g1";
  const out = reconcileRegionEdit(p, "el1", new Set(["r1"]));
  assert.equal(out.regionEditId, null);
});

test("edited element in a VISIBLE group -> stay in the mode", () => {
  const p = project({ groups: [{ id: "g1", visible: true }] });
  p.elements[0].groupId = "g1";
  const out = reconcileRegionEdit(p, "el1", new Set(["r1"]));
  assert.equal(out.regionEditId, "el1");
  assert.deepEqual([...out.selectedRegionIds], ["r1"]);
});

test("tolerates a missing/undefined project and missing region set", () => {
  assert.equal(reconcileRegionEdit(undefined, "el1", undefined).regionEditId, null);
  assert.equal(reconcileRegionEdit({}, "el1", new Set(["r1"])).regionEditId, null);
});
