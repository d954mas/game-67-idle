local items = require("studio.items")

items.define({
  id = "tmpl.gold",
  created = "2026-07-07",
  name = "Gold",
  icon = "icons/gold",
  kind = "currency",
  base_value = 1,
  stack = 0,
  currency = { hud = "counter", cap = 0 },
})

items.define({
  id = "tmpl.xp",
  created = "2026-07-07",
  name = "Experience",
  icon = "icons/xp",
  kind = "currency",
  base_value = 0,
  stack = 0,
  currency = { hud = "bar", cap = 0 },
})

items.define({
  id = "tmpl.energy",
  created = "2026-07-07",
  name = "Energy",
  icon = "icons/energy",
  kind = "currency",
  base_value = 0,
  stack = 0,
  currency = { hud = "counter", cap = 100 },
})
