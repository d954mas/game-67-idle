local items = require("studio.items")
local kinds = require("template.items.kinds")

items.define({
  id = "tmpl.wood",
  created = "2026-07-07",
  name = "Wood",
  icon = "icons/wood",
  kind = kinds.material,
  base_value = 2,
  stack = 999,
})
