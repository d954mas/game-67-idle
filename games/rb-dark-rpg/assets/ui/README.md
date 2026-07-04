# Template UI art (base GUI kit)

The slice9 panel/button + slider art the settings panel renders with. Every game
copied from the template starts with this kit; replace/extend it with your own.

| File              | What                  | slice9 corners |
|-------------------|-----------------------|----------------|
| `panel.png`       | window/panel frame    | 10 px          |
| `button.png`      | button frame          | 16 px          |
| `slider_track.png`| slider/bar track      | 8 px           |
| `slider_fill.png` | slider/bar fill       | 8 px           |
| `slider_thumb.png`| slider/toggle thumb   | (circle, none) |

## Provenance

- **Origin:** sourced
- **License:** CC0 1.0 (public domain) вЂ” free to use, no attribution required.
- **Author/source:** Kenney вЂ” UI Pack (https://kenney.nl/assets/ui-pack).
- **How it got here:** source-first search did not find a matching UI kit in the
  shared asset library (`node ai_studio/assets/backlog/storage/search.mjs --query "ui kit" --kind ui --json`),
  so this falls back to the canonical
  free CC0 source (Kenney). These exact PNGs are already vendored CC0 in the
  engine's `examples/ui_showcase/raw/`; reused here so the template is
  self-contained and not coupled to engine-example internals.
