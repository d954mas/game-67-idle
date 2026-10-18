local items = require("studio.items")
local kinds = require("template.items.kinds")
local levels = require("studio.levels")
local tracks = require("studio.tracks")

local xp = items.ref("tmpl.xp")

-- Row 1 is the un-upgraded state and carries the track's zero contribution, so the
-- twenty levels the demo hero can reach are the rows above it.
tracks.define({
  id = "hero",
  kind = kinds.hero,
  mode = "auto",
  levels = levels.generate({
    max_level = 21,
    cost_to_reach = function(level, math)
      if level == 1 then return nil end
      -- floor(50 * (3/2) ^ (level - 2)), as exact integers: the ratio is applied
      -- as a numerator and a denominator so no rounding happens per step.
      local numerator, denominator = 50, 1
      for _ = 1, math.sub(level, 2) do
        numerator = math.mul(numerator, 3)
        denominator = math.mul(denominator, 2)
      end
      return items.cost(xp, math.idiv(numerator, denominator))
    end,
  }),
})
