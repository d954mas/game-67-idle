local items = require("studio.items")

items.define({ id = "game.gold", kind = "currency", tags = { "economy" }, stack = 0 })
items.define({ id = "game.metal", kind = "material", tags = { "crafting" }, stack = 999 })
items.define({
  id = "game.extraordinarily_long_balance_resource_identifier",
  kind = "material", tags = { "crafting", "rare" }, stack = 999,
})
