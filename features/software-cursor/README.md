# Software Cursor

## Purpose

Reusable in-place cursor presentation for games that need a themed pointer,
finger, capture cursor, or their own visual set. The module owns pointer follow,
semantic intent, press pulse, hotspot, enable/disable, and native-cursor mode
coordination. It does not own input polling, asset packing, or rendering.

## Public surface

The consumer supplies:

- one `software_cursor_input_t` per frame;
- a renderer mapping `presentation.visual` to its own sprite/atlas region;
- an optional native-mode callback;
- either a sample theme or a fully game-owned `software_cursor_theme_t`.

### Samples

- `software_cursor_sample_pointer_theme()` uses the Kenney Cursor Pack pointer.
- `software_cursor_sample_finger_theme()` uses Kenney Input Prompts open/closed
  touch hands.

Both sources are CC0. Their untouched PNGs and provenance live in
`example/assets/`. A game may use them, remap their visual ids to equivalent
packed regions, or replace every style with its own art.

### Runtime contract

`software_cursor_update` consumes framebuffer-pixel coordinates and returns a
presentation in the same coordinate space. The consumer converts to its UI
space at the renderer boundary. Motion follows with a bounded first-order
response, so an injected pointer cannot visually teleport between targets.

Enabling a cursor invokes the optional native-mode callback with `hidden=true`;
disabling or shutting down restores it. The engine-side portable implementation
is tracked by neotolis-engine issue 335. Until that API lands, captures may pass
no callback when their recorder already excludes the OS cursor.

No heap allocation occurs in init, update, intent changes, or presentation.

## Validation

`tests/test_software_cursor.c` covers native-mode coordination, initial pointer
snap, bounded follow, press feedback, both sample themes, and complete game
visual replacement. `features/validate_contracts.mjs` validates this router and
manifest. Sample file hashes are locked in `example/assets/provenance.json`.

## Compatibility

PATCH changes preserve structs, enum values, visual ids, and behavior. MINOR
changes may append intents, fields with safe defaults, or sample themes. MAJOR
changes may reorder public enums, alter coordinate space, or remove callbacks.

## Extension points

Games extend visuals through `software_cursor_theme_t`, map opaque visual ids in
their renderer, set semantic intent from their UI, and may provide their own
native-mode callback. New shared sample themes belong in
`software_cursor_samples.*`; game-specific art stays in the game.
