local items = require("studio.items")

items.define({
  id = "tmpl.potion",
  created = "2026-07-07",
  name = "Healing Potion",
  icon = "icons/potion",
  kind = "consumable",
  tags = { "heal" },
  base_value = 10,
  stack = 99,
  use = { effect_id = "heal", params = { amount = 25 } },
})
