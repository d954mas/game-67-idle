local items = require("studio.items")
local levels = require("studio.levels")
local tracks = require("studio.tracks")
local kinds = require("game.kinds")
local gold = items.ref("game.gold")

-- Row 1 is the un-upgraded state and carries the track's zero contribution.
tracks.define({
  id = "hauler",
  kind = kinds.hauler,
  mode = "auto",
  levels = levels.table({
    [1] = { capacity = 0, speed_mul = 0.0 },
    [2] = { capacity = 2, speed_mul = 0.25, cost_to_reach = items.cost(gold, 10) },
    [3] = { capacity = 5, speed_mul = 0.75, cost_to_reach = items.free() },
  }),
})

tracks.define({
  id = "rank",
  kind = kinds.rank,
  mode = "threshold",
  levels = levels.generate({
    max_level = 3,
    xp_to_reach = function(level, math)
      if level == 1 then return nil end
      return math.mul(50, math.sub(level, 1))
    end,
    payout = function(level, math)
      return math.pow(1.5, math.tofloat(math.sub(level, 1)))
    end,
  }),
})
