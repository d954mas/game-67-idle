import test from "node:test";
import assert from "node:assert/strict";
import { formatCost } from "../site/item_detail.js";

test("Workbench formats normalized free, single, and composite cost lists", () => {
  const gold = { __studio_kind: "cost", count: 100, item: { id: "game.gold" } };
  const wood = { __studio_kind: "cost", count: 2, item: { id: "game.wood" } };
  assert.equal(formatCost({ __studio_kind: "free" }), "Free");
  assert.equal(formatCost(gold), "100 × game.gold");
  assert.equal(formatCost({ __studio_kind: "costs", entries: [gold, wood] }), "100 × game.gold + 2 × game.wood");
  assert.equal(formatCost(null), "—");
});
