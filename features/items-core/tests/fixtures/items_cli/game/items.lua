local field = require("studio.field")
local items = require("studio.items")
local levels = require("studio.levels")

items.extend_schema({ level_row = {
  attack = field.i64({
    id = "game.weapon.level.attack", required_for = { "weapon" },
    min = 0, max = 1000000, unit = "damage", rounding = "exact",
    label_key = "item.attack",
    ui = { format = "integer", description_key = "item.attack.description" },
    evolution = { since = 1, deprecated = false },
  }),
}})

local gold = items.ref("game.gold")

items.define({
  id = "game.gold", kind = "currency", stack = 0,
})

items.define({
  id = "game.iron_sword", kind = "weapon", stack = 1,
  levels = levels.single({ attack = 15 }),
  acquire = { cost = items.cost(gold, 100) },
})

items.define({
  id = "game.levelled_sword", kind = "weapon", stack = 1,
  levels = levels.table({
    [1] = { attack = 10 },
    [2] = { attack = 15, cost_to_reach = items.cost(gold, 100) },
    [3] = { attack = 20, cost_to_reach = items.free() },
  }),
})

items.define({
  id = "game.curve_sword", kind = "weapon", stack = 1,
  levels = levels.columns({
    max_level = 3,
    attack = levels.linear({ start = 10, step = 5 }),
    cost_to_reach = levels.values({
      [2] = items.free(), [3] = items.free(),
    }),
    overrides = { [3] = { attack = 21 } },
  }),
})
