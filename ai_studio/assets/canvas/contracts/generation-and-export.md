# Generation and export

Generation uses injected/default tool seams from the operation domain and
commits only successful immutable outputs. Engine-specific parameters and
references are frozen into provenance; partial multi-engine outcomes are
reported explicitly. Every newly minted generated or pixel-derived image enters
the asset review workflow with top-level `assetStatus: "quarantine"`; ordinary
user imports remain untracked until a review operation enrolls them.

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
