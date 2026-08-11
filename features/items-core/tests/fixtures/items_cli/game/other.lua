local items = require("studio.items")
local levels = require("studio.levels")
local kinds = require("game.kinds")

items.define({
  id = "game.other_sword", kind = kinds.weapon, stack = 1,
  levels = levels.table({
    [1] = { attack = 30 },
  }),
})
