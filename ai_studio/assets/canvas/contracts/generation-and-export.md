# Generation and export

Generation uses injected/default tool seams from the operation domain and
commits only successful immutable outputs. Engine-specific parameters and
references are frozen into provenance; partial multi-engine outcomes are
reported explicitly.

Element export rows support `png`, `jpg`, and `webp`, Figma-style multiplier or
fixed-dimension scales, source/canvas bases, and bounded output dimensions.
Project export renders only visible groups marked as screens. Multi-file page
delivery uses the confined generated export folder and store-mode zip route.

The browser owns only save-dialog/download delivery. Naming, render order,
filters, fonts, scale resolution, manifests, and output bytes are operation
contracts shared with CLI/API.
