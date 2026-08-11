local field = require("studio.field")
local items = require("studio.items")
local levels = require("studio.levels")
local tracks = require("studio.tracks")
local kinds = require("game.kinds")

items.extend_schema({ level_row = {
  haul_mul = field.f64({
    id = "game.hauler.level.haul_mul", required_for = { kinds.hauler },
    min = 0.0, max = 10.0, unit = "x", label_key = "track.haul_mul",
  }),
}})

local gold = items.ref("game.gold")

tracks.define({
  id = "hauler",
  kind = kinds.hauler,
  mode = "manual",
  levels = levels.columns({
    max_level = 3,
    haul_mul = levels.linear({ start = 1.0, step = 0.5 }),
    cost_to_reach = levels.values({ [2] = items.cost(gold, 25), [3] = items.free() }),
  }),
})
