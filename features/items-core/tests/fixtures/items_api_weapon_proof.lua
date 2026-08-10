local field = require("studio.field")
local items = require("studio.items")
local levels = require("studio.levels")
local smath = require("studio.math")

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
local metal = items.ref("game.metal")
local rare_resource = items.ref("game.extraordinarily_long_balance_resource_identifier")

items.define({
  id = "game.gold", kind = "currency", tags = { "economy" }, stack = 0,
  currency = { hud = "counter", cap = 5000 },
})
items.define({ id = "game.metal", kind = "material", tags = { "crafting" }, stack = 999 })
items.define({
  id = "game.extraordinarily_long_balance_resource_identifier",
  kind = "material", tags = { "crafting", "rare" }, stack = 999,
})

items.define({
  id = "game.fixed_sword", kind = "weapon", tags = { "melee" }, stack = 1,
  levels = levels.single({ attack = 15 }),
  acquire = { cost = items.cost(gold, 100) },
})

items.define({
  id = "game.table_sword", kind = "weapon", tags = { "melee", "starter" }, stack = 1,
  levels = levels.table({
    [1] = { attack = 12 },
    [2] = { attack = 18, cost_to_reach = items.free() },
  }),
})

items.define({
  id = "game.generated_sword", kind = "weapon", tags = { "melee" }, stack = 1,
  levels = levels.generate({
    max_level = 2,
    attack = function(level) return smath.add(8, smath.mul(smath.sub(level, 1), 3)) end,
    cost_to_reach = function(level)
      if level == 2 then return items.cost(rare_resource, 2) end
    end,
  }),
})

items.define({
  id = "game.iron_sword", kind = "weapon", tags = { "melee" }, stack = 1,
  levels = levels.columns({
    max_level = 3,
    attack = levels.linear({ start = 10, step = 5 }),
    cost_to_reach = levels.values({
      [2] = items.costs({ items.cost(gold, 100), items.cost(metal, 5) }),
      [3] = items.free(),
    }),
    overrides = { [3] = { attack = 21 } },
  }),
})
