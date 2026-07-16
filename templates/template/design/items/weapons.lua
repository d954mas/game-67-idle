local items = require("studio.items")
local levels = require("studio.levels")

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
  levels = levels.single({}),
})
