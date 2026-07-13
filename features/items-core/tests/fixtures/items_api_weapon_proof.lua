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

items.view({
  id = "game.weapon.balance", layout = "table",
  order = { "game.weapon.level.attack" },
  chart = { field_ids = { "game.weapon.level.attack" } },
})

local gold = items.ref("game.gold")
local metal = items.ref("game.metal")
local rare_resource = items.ref("game.extraordinarily_long_balance_resource_identifier")

items.define({ id = "game.gold", kind = "currency", tags = { "economy" }, stack = 0 })
items.define({ id = "game.metal", kind = "material", tags = { "crafting" }, stack = 999 })
items.define({
  id = "game.extraordinarily_long_balance_resource_identifier",
  kind = "material", tags = { "crafting", "rare" }, stack = 999,
})

items.define({
  id = "game.fixed_sword", kind = "weapon", tags = { "melee" }, stack = 1,
  levels = levels.single({ attack = 15 }),
  acquire = items.cost(gold, 100),
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
    attack = function(level) return 8 + (level - 1) * 3 end,
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
      [2] = items.costs({
        { item = gold, count = 100 }, { item = metal, count = 5 },
      }),
      [3] = items.free(),
    }),
    overrides = { [3] = { attack = 21 } },
  }),
})
