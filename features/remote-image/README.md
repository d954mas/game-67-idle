# remote-image

A small cache that turns a URL into a texture and forgets it again. The
consumer asks about a URL every frame it draws it and gets one of three
answers: `PENDING` (draw a spinner), `READY` (draw the texture), `FAILED`
(draw a fallback). The contract is section 10 of `features/leaderboard/SPEC.md`.

## Layer

L1, in-place. Depends on the engine's `nt_http`, `nt_gfx`, `nt_time` and the
vendored `stb_image`.

## Contents

- `include/features/remote-image/remote_image.h` — the API, the config, the
  backend seam and the RGBA fit helper.
- `src/remote_image.c` — the table and every policy decision: touch order,
  eviction, the negative cache, the concurrency cap, the fit.
- `src/remote_image_engine.c` — the engine behind the seam. Excluded from the
  test link by `REMOTE_IMAGE_TESTING`.
- `tests/test_remote_image.c` — core tier, canned everything.

## What It Owns

- One fixed table keyed by URL. Asking is a touch. When the table is full,
  the least recently touched entry that nobody asked about this frame is
  dropped; an entry touched this frame is never dropped. When every entry is
  frame-hot the new URL is not admitted and stays `PENDING` until a frame
  passes without a touch on one of them — the capacity is one screen of
  images plus headroom, not a memory budget.
- A negative cache on two schedules. A transport error, a 5xx, a 429 or a GPU
  refusal are transient and retry after `retry_delay_s`; a 4xx, an empty or
  oversized body, or bytes that do not decode are not going to change soon
  and retry after `missing_delay_s`. Neither is re-fetched every frame.
- A concurrency cap. `NT_HTTP_MAX_REQUESTS` is shared with everything else in
  the game; the pack never holds more than `max_in_flight` of the slots.
  Queued fetches start in touch order, so the top of a list loads first.
- Decoding to RGBA8 and fitting to `max_dim` by area average. A source more
  than 32 times `max_dim` on a side is a decode bomb and counts as not an
  image.

## What It Does Not Own

- Any limit. Capacity, byte clamp, max dimension, both delays and the
  concurrency cap arrive in `remote_image_config_t`; the header carries no
  default and the pack ships none.
- What the images are. Nothing here knows an avatar from a banner.
- Drawing. The consumer binds the texture through its own sprite or UI path.
- A cancel mid-download. The byte clamp is applied when the body has
  arrived; the transport has no partial abort.

## Commands

- `ctest --test-dir templates/template/build/native-debug -R remote_image --output-on-failure`

## References

- `features/leaderboard/SPEC.md` section 10 — the approved design.
- `INSTALL.md` — wiring a game.
