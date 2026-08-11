local items = require("studio.items")
local tracks = require("studio.tracks")

return {
  currency = items.kind({ id = "currency" }),
  weapon = items.kind({ id = "weapon", label_key = "kind.weapon" }),
  hauler = tracks.kind({ id = "hauler" }),
}
