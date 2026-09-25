local export = require("studio.export")
local stats = require("game.mob_stats")

export("game.mobs", {
  { id = "slime", hp = stats.slime.hp, speed = stats.slime.speed, tags = { "soft" } },
  { id = "golem", hp = stats.golem.hp, speed = stats.golem.speed, tags = { "heavy", "slow" } },
})
export("game.movement", stats)
