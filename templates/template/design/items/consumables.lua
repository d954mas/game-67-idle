local items = require("studio.items")
local kinds = require("template.items.kinds")

items.define({
  id = "tmpl.potion",
  created = "2026-07-07",
  name = "Healing Potion",
  icon = "icons/potion",
  kind = kinds.consumable,
  tags = { "heal" },
  base_value = 10,
  stack = 99,
  acquire = { cost = items.free() },
  use = { effect_id = "heal", params = { amount = 25 } },
})
