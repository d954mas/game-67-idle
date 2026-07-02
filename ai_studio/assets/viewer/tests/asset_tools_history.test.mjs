import { strict as assert } from "node:assert";
import test from "node:test";
import {
  createRegionHistory,
  historyCanRedo,
  historyCanUndo,
  historyPush,
  historyRedo,
  historyUndo,
} from "../asset_tools_history.mjs";

const snapshot = (id, count = 1) => ({
  regions: Array.from({ length: count }, (_value, index) => ({ id: `${id}_${index}`, rect: [index, index, 10, 10] })),
  selectedId: `${id}_0`,
});

test("region history restores undo and redo snapshots", () => {
  let history = createRegionHistory(snapshot("initial"));
  history = historyPush(history, snapshot("after_add", 2));

  const undone = historyUndo(history);
  assert.equal(undone.snapshot.regions.length, 1);
  assert.equal(historyCanRedo(undone.history), true);

  const redone = historyRedo(undone.history);
  assert.equal(redone.snapshot.regions.length, 2);
  assert.equal(historyCanUndo(redone.history), true);
});

test("pushing a new snapshot clears redo stack", () => {
  let history = createRegionHistory(snapshot("initial"));
  history = historyPush(history, snapshot("after_add", 2));
  history = historyUndo(history).history;
  history = historyPush(history, snapshot("new_branch", 3));

  assert.equal(historyCanRedo(history), false);
  assert.equal(historyCanUndo(history), true);
});

test("equal snapshots are not pushed", () => {
  let history = createRegionHistory(snapshot("initial"));
  history = historyPush(history, snapshot("initial"));

  assert.equal(historyCanUndo(history), false);
});
