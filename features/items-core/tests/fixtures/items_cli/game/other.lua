local items = require("studio.items")
local levels = require("studio.levels")

items.define({
  id = "game.other_sword", kind = "weapon", stack = 1,
  levels = levels.table({
    [1] = { attack = 30 },
  }),
})
