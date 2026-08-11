local items = require("studio.items")
local tracks = require("studio.tracks")

-- A kind is declared once here and travels by handle: nothing downstream looks
-- one up by name, so a misspelling cannot become a second kind.
return {
  currency = items.kind({ id = "currency" }),
  weapon = items.kind({ id = "weapon" }),
  hauler = tracks.kind({ id = "hauler" }),
  rank = tracks.kind({ id = "rank" }),
}
