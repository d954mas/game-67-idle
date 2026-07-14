local items = require("studio.items")
local levels = require("studio.levels")

items.define({
  id = "game.iron_sword",
  kind = "weapon",
  stack = 1,
  levels = levels.single({ attack = 15 }),
  acquire = { cost = items.cost(items.ref("game.gold"), 100) },
})

items.define({
  id = "game.levelled_sword",
  kind = "weapon",
  stack = 1,
  levels = levels.table({
    [1] = { attack = 10 },
    [2] = {
      attack = 15,
      cost_to_reach = items.cost(items.ref("game.gold"), 100),
    },
    [3] = { attack = 20, cost_to_reach = items.free() },
  }),
})
