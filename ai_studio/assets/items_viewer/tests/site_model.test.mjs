import test from "node:test";
import assert from "node:assert/strict";
import { formatCost } from "../site/item_detail.js";
import { buildEditPatch, editableFields, editableOperations } from "../site/semantic_editor.js";

test("Workbench formats normalized free, single, and composite cost lists", () => {
  const gold = { __studio_kind: "cost", count: 100, item: { id: "game.gold" } };
  const wood = { __studio_kind: "cost", count: 2, item: { id: "game.wood" } };
  assert.equal(formatCost({ __studio_kind: "free" }), "Free");
  assert.equal(formatCost(gold), "100 × game.gold");
  assert.equal(formatCost({ __studio_kind: "costs", entries: [gold, wood] }), "100 × game.gold + 2 × game.wood");
  assert.equal(formatCost(null), "—");
});

test("Workbench exposes only source shapes supported by the shared semantic writer", () => {
  const detail = (authoringMode, provenances) => ({
    fields: [{ member: "attack" }],
    item: { item: {
      values: { authoring_mode: authoringMode },
      levels: provenances.map((provenance, index) => ({ level: index + 1, provenance: { attack: provenance } })),
    } },
  });
  assert.deepEqual(editableOperations(detail("table", ["table", "table"])), ["level-set"]);
  assert.deepEqual(editableOperations(detail("columns", ["columns", "override"])), ["curve-set", "override-set"]);
  assert.deepEqual(editableOperations(detail("generate", ["generate"])), []);
  assert.deepEqual(editableOperations({ fields: [], item: { item: { levels: [] } } }), []);
});

test("Workbench offers only fields whose provenance matches the selected operation", () => {
  const detail = {
    fields: [{ member: "attack" }, { member: "defense" }, { member: "speed" }],
    item: { item: { levels: [
      { provenance: { attack: "columns", defense: "override", speed: "table" } },
      { provenance: { attack: "override", defense: "columns", speed: "table" } },
    ] } },
  };
  assert.deepEqual(editableFields(detail, "level-set"), ["speed"]);
  assert.deepEqual(editableFields(detail, "curve-set"), ["attack", "defense"]);
  assert.deepEqual(editableFields(detail, "override-set"), ["attack", "defense"]);
  assert.deepEqual(editableFields(detail, "rewrite-lua"), []);
});

test("Workbench builds the exact T0366 patch contract with expected hash", () => {
  assert.deepEqual(buildEditPatch({
    operation: "level-set",
    item: "game.sword",
    field: "attack",
    level: "2",
    value: "17",
    expectedSourceHash: `sha256:${"a".repeat(64)}`,
  }), {
    schema: "items.cli.patch.v1",
    operation: "level-set",
    item: "game.sword",
    field: "attack",
    level: 2,
    value: 17,
    expected_source_hash: `sha256:${"a".repeat(64)}`,
  });
  assert.deepEqual(buildEditPatch({
    operation: "curve-set",
    item: "game.sword",
    field: "attack",
    parameter: "step",
    value: "7",
    expectedSourceHash: `sha256:${"b".repeat(64)}`,
  }), {
    schema: "items.cli.patch.v1",
    operation: "curve-set",
    item: "game.sword",
    field: "attack",
    parameter: "step",
    value: 7,
    expected_source_hash: `sha256:${"b".repeat(64)}`,
  });
  assert.throws(() => buildEditPatch({ operation: "rewrite-lua", value: 1 }), /Unsupported/);
});
