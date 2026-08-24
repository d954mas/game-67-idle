import assert from "node:assert/strict";
import test from "node:test";

import {
  currencyBalances,
  normalizeSeries,
  progressionRows,
  progressionSeries,
  seriesAtLevel,
} from "../site/economy_model.mjs";

const coins = {
  id: "game.coins",
  kind: "currency",
  name: "Coins",
};

function coinCost(count) {
  return {
    __studio_kind: "cost",
    count,
    item: { id: coins.id },
  };
}

test("currencyBalances reads exact counts from the active wallet", () => {
  const save = {
    features: {
      game: { wallet_container_id: 2 },
      items: {
        containers: [
          { container_id: 1, entries: [] },
          {
            container_id: 2,
            entries: [
              { def_id: "game.coins", count: "4100", quarantined: false },
              { def_id: "game.bad", count: "9", quarantined: true },
            ],
          },
        ],
      },
    },
  };

  assert.deepEqual([...currencyBalances(save)], [["game.coins", "4100"]]);
});

test("progressionSeries exposes costs and numeric effects across every level", () => {
  const track = {
    mode: "upgrade",
    levels: {
      rows: [
        { contribution: 0 },
        { contribution: 1, cost_to_reach: coinCost(10) },
        { contribution: 3, cost_to_reach: coinCost(40) },
      ],
    },
  };

  assert.deepEqual(progressionSeries(track, [coins]), [
    {
      id: "cost:game.coins",
      kind: "cost",
      label: "Coins",
      values: [null, 10, 40],
    },
    {
      id: "effect:contribution",
      kind: "effect",
      label: "Contribution",
      values: [0, 1, 3],
    },
  ]);
});

test("progressionSeries exposes threshold XP", () => {
  const track = {
    mode: "threshold",
    levels: {
      rows: [
        {},
        { xp_to_reach: 100 },
        { xp_to_reach: 250 },
      ],
    },
  };

  assert.deepEqual(progressionSeries(track, []), [
    {
      id: "xp",
      kind: "threshold",
      label: "XP",
      values: [null, 100, 250],
    },
  ]);
});

test("normalizeSeries preserves level positions and normalizes its own scale", () => {
  assert.deepEqual(normalizeSeries([null, 10, 40]), [
    null,
    { level: 2, value: 10, x: 0.5, y: 0 },
    { level: 3, value: 40, x: 1, y: 1 },
  ]);
});

test("seriesAtLevel exposes exact selected value and the real scale", () => {
  assert.deepEqual(seriesAtLevel({
    values: [null, 1_500, 2_700, 4_800],
  }, 3), {
    first: 1_500,
    last: 4_800,
    min: 1_500,
    max: 4_800,
    value: 2_700,
  });
});

test("progressionRows aligns every series into one row per level", () => {
  assert.deepEqual(progressionRows([
    { values: [null, 1_500, 4_800] },
    { values: [0, 1, 3] },
  ]), [
    { level: 1, values: [null, 0] },
    { level: 2, values: [1_500, 1] },
    { level: 3, values: [4_800, 3] },
  ]);
});
