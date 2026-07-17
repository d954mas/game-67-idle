# Generation and export

Generation uses injected/default tool seams from the operation domain and
commits only successful immutable outputs. Engine-specific parameters and
references are frozen into provenance; partial multi-engine outcomes are
reported explicitly. Every newly minted generated or pixel-derived image enters
the asset review workflow with top-level `assetStatus: "quarantine"`; ordinary
user imports remain untracked until a review operation enrolls them.

Recipe, pack, animation, and AI dual-plate generation freeze `meta.origin` before
slow/paid work. Unowned Canvas projects record untainted `explore` origin.
Game-owned projects default to `production`, require an accepted
`design/style_lock.json`, and stamp its stable lock id. CLI `--no-lock` / API
`noLock: true` is the explicit escape hatch: it generates quarantined explore
output with `tainted: true` and `taint_reason: "no-lock"`.
Missing/draft/invalid production locks refuse before a generator call; the
dual-plate path also refuses before its Python background check.

Long generation runs happen outside the project lock. Their short final commit
must still validate the current project/history head, import immutable output,
record provenance, and journal one accepted result. A generated result lands
beside its card in the card's parent scope, never inside the card where it could
silently become a future input.

Element export rows support `png`, `jpg`, and `webp`, Figma-style multiplier or
fixed-dimension scales, source/canvas bases, and bounded output dimensions.
Element/source export emits raw source pixels; screen/project export applies
the visible Canvas transforms. Project export renders only visible groups
marked as screens. Multi-file page delivery uses the confined generated export
folder and store-mode zip route. Once a supported folder picker has opened, a
picker failure or cancellation never silently falls back to browser downloads.
No recipe, style, animation, or pack type is special-cased during export; those
groups simply are not marked as screens.

The browser owns only save-dialog/download delivery. Naming, render order,
filters, fonts, scale resolution, manifests, and output bytes are operation
contracts shared with CLI/API.

Export is review-neutral and may write previews anywhere the caller chooses;
game asset promotion is a separate hard-gated operation. `asset-promote` accepts
only game-owned, currently accepted Canvas images and writes the owning game's
`assets/packs/canvas-promotions` Pack Manifest. Its metadata JSON must explicitly
provide asset id/title/description/kind/tags, `origin` (`mine|ai|sourced`),
license name/URL/kind, source page, author/vendor, provenance, credit, and all
six rights/publish flags as `"true"|"false"`. The license decision must pass the
release/public-binary gate; pending values and `private|unknown` license kinds
are refused. Promotion verifies the immutable content-addressed source bytes,
physically confines every game-pack directory, and serializes writers per game.
There is no `--no-lock`, force, overwrite, or raw destination flag.
