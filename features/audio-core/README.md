# Audio Core

## Purpose

`audio-core` is the reusable in-place L1 playback module consumed by templates
and games. It owns handles, pools, buses, mix state, playback policy, and the
private native/web backend seam. The game owns cue/music catalogs,
codec-neutral BLOB IDs, source assets and provenance, pack registration,
persisted settings, platform lifecycle composition, and user-gesture wiring.

## Public surface

The only game-facing header is
`include/features/audio/audio.h`. It exposes lifecycle and status, loading by a
ready `NT_ASSET_BLOB` hash, generation-checked clip/voice handles, playback and
stop queries, MUSIC/SFX buses, mix controls, pause/enable state, and browser
user-gesture unlock. Its fixed limits are 64 decoded clips and 32 voices.
Backend types, file paths, codecs, and JavaScript handles stay private.

## Validation

Run the focused native, resource, web-library, and template-catalog checks in
`INSTALL.md`, then build and test the consumer's real native and Emscripten
targets. Source presence or the version string alone is not compatibility
evidence.

## Compatibility

Version `1.0.0` identifies the existing public contract above; it is not a
claim that T0393 or every platform integration is complete. A consumer records
the exact version it validated. `feature.json.version` is mandatory SemVer:

- PATCH: compatible fixes, tests, or documentation;
- MINOR: backward-compatible additions to the public contract;
- MAJOR: breaking public API, metadata, or observable behavior changes.

The native adapter is pinned to miniaudio 0.11.25. The web adapter uses
WebAudio. Vendored version, license, source, and integrity hashes are recorded
in `vendor/miniaudio/README.md`.

## Extension points

Add game-specific catalogs, assets, settings, and lifecycle wiring in the game,
not in this module. A backend may be replaced behind `src/audio_backend.h`
without changing the public header. If a game needs incompatible playback
semantics, copy the module into that game and own the fork; do not add a
speculative shared switch.

See `INSTALL.md` for wiring, validation, and removal.
