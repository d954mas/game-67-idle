# remote-image Install

The template already compiles the pack (`REMOTE_IMAGE_SOURCES` in
`templates/template/CMakeLists.txt`), calls `remote_image_ensure_stb` from `cmake/RemoteImage.cmake` and links `stb_image` and registers its
test. A game wires it in three steps.

## Install

1. Pick the limits and call `remote_image_init` once, after `nt_http_init`
   and the graphics context. Every field is required; the numbers below are
   a starting point for a list of small pictures, not a default the pack
   knows about:

   ```c
   #include "features/remote-image/remote_image.h"

   const remote_image_config_t config = {
       .capacity = 24,          /* one screen of rows plus headroom */
       .max_bytes = 256 * 1024, /* a larger body is not a small picture */
       .max_dim = 128,          /* fitted on the CPU before upload */
       .retry_delay_s = 15.0,   /* dropped connection, 5xx, 429 */
       .missing_delay_s = 600.0,/* 404, not an image */
       .max_in_flight = 2,      /* NT_HTTP_MAX_REQUESTS is 8 and shared */
       .backend = NULL,         /* the engine */
   };
   remote_image_init(&config);
   ```

2. Call `remote_image_update` once per frame, after `nt_http_update`. It
   collects finished fetches, decodes and uploads them, starts queued ones up
   to the cap, and closes the frame for the eviction rule.

3. Where a picture is drawn, ask every frame:

   ```c
   switch (remote_image_state(url)) {
   case REMOTE_IMAGE_READY:
       draw_texture(remote_image_get(url), rect);
       break;
   case REMOTE_IMAGE_PENDING:
       draw_spinner(rect);
       break;
   case REMOTE_IMAGE_FAILED:
       draw_fallback(rect);
       break;
   }
   ```

   Asking is what keeps an entry alive: a URL not asked about for a frame
   becomes a candidate for eviction. Do not cache the texture handle across
   frames; `remote_image_get` is the handle's only valid source and it is
   invalid the moment the entry is evicted.

Call `remote_image_shutdown` before `nt_gfx` and `nt_http` go down; it
releases every fetch in flight and destroys every texture.

## Capacity

Eviction only drops entries nobody asked about this frame. If a screen asks
about more URLs than the capacity in one frame, the surplus stays `PENDING`
forever and no fetch is wasted on it; give the table one screen plus
headroom so that scrolling evicts what left the screen, not what is on it.

## Verify

- `cmake --build templates/template/build/native-debug --target test_remote_image`
- `ctest --test-dir templates/template/build/native-debug -R remote_image --output-on-failure`

The visual proof that a real image reaches the screen is the first
consumer's, with a real URL.

## Uninstall

Remove the init, update and shutdown calls and the draw-site switch. The
pack has no save keys and no manifest.
