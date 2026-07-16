import test from "node:test";
import assert from "node:assert/strict";
import {
  captureCatalogScope,
  captureEditScope,
  scopeIsCurrent,
  scopedUndo,
} from "../site/request_scope.js";

test("catalog request scope rejects a late response after catalog selection changes", () => {
  const state = { selectedId: "template:a", catalogRequest: 7, detailRequest: 3 };
  const scope = captureCatalogScope(state);
  assert.equal(scopeIsCurrent(state, scope), true);
  state.selectedId = "template:b";
  state.catalogRequest += 1;
  assert.equal(scopeIsCurrent(state, scope), false);
});

test("edit scope and inverse patch retain their owning catalog", () => {
  const state = { selectedId: "template:a", selectedItemId: "a.sword", catalogRequest: 7, detailRequest: 3 };
  const scope = captureEditScope(state, "a.sword");
  assert.equal(scopeIsCurrent(state, scope), true);
  const undo = scopedUndo(scope, { item: "a.sword", expected_source_hash: "sha256:x" });
  assert.equal(undo.catalogId, "template:a");
  assert.equal(undo.itemId, "a.sword");
  state.selectedId = "template:b";
  assert.equal(scopeIsCurrent(state, scope), false);
});
