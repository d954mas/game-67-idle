local items = require("studio.items")
local levels = require("studio.levels")
local gold = items.ref("tmpl.gold")
local wood = items.ref("tmpl.wood")

items.define({
  id = "tmpl.sword",
  created = "2026-07-07",
  name = "Iron Sword",
  icon = "icons/sword",
  kind = "weapon",
  tags = { "melee" },
  base_value = 50,
  stack = 1,
  equip = { slot = "weapon" },
  acquire = { cost = items.costs({
    items.cost(gold, 10),
    items.cost(wood, 2),
  }) },
  levels = levels.table({
    [1] = {},
    [2] = { cost_to_reach = items.costs({
      items.cost(gold, 5),
      items.cost(wood, 1),
    }) },
    [3] = { cost_to_reach = items.free() },
  }),
})
