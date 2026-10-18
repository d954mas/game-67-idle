local items = require("studio.items")
local tracks = require("studio.tracks")

-- Every kind the catalog knows is declared once, here. Items and tracks are
-- separate spaces: one name in both would be two unrelated kinds. Declarations
-- elsewhere take the handle, so a misspelling cannot invent a second kind.
return {
  consumable = items.kind({ id = "consumable" }),
  currency = items.kind({ id = "currency" }),
  material = items.kind({ id = "material" }),
  weapon = items.kind({ id = "weapon" }),

  hero = tracks.kind({ id = "hero" }),
}
