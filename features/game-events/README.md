# game-events

Reusable in-place event and local analytics spine.

## Layer

L0 infrastructure. Higher features emit through this pack, and analytics or
DevAPI tools subscribe to it. The pack does not know platform SDKs, items,
progression, save fragments, or game-specific content.

## Contents

```text
features/game-events/
  README.md
  INSTALL.md
  feature.json
  include/
    game_events.h
    game_event_desc.h
    game_event_render.h
    game_events_log_mirror.h
    game_events_devapi.h
    game_analytics.h
  src/
    game_events.c
    game_event_render.c
    game_events_log_mirror.c
    game_events_devapi.c
    game_analytics.c
  tests/
    test_game_events_ctx.c
```

## What It Owns

- Fixed per-frame event log and payload arena.
- Descriptor contract for generated or hand-written typed event payloads,
  including repeated sections (N fixed-size records packed inline after the
  payload struct, walked through `game_event_record_t`).
- Generic descriptor-driven JSON rendering.
- Optional `nt_log` mirror for ordinary local/debug builds (`[ev] ...`).
- DevAPI tail recorder and `game.events.tail` command in DevAPI builds.
- Local NDJSON analytics writer in analytics-enabled builds.

## What It Does Not Own

- Game-specific events or closed payload vocabularies.
- Portal SDK logic or publish-target policy.
- Analytics decisions such as scorecard thresholds or portal forwarding.
- Save/load persistence semantics.

## Event Model

Events are transient frame data, not state. Producers emit payload copies into a
fixed arena; consumers read them in the same frame during react/record phases.
State remains the source of truth.

## Contexts

The global API is the default context: one log, cleared by
`game_event_frame_reset`, driven through the emit/react/record phases. A caller
that needs a log of its own -- a simulated room, one per process slot -- gives a
`game_events_ctx_t` its own memory (`game_events_ctx_memory_bytes`, aligned to
`max_align_t`) and caps. A context never allocates and shares nothing with
another context or with the default one: its seq, tick, drop counter and
warnings are its own.

A context is double-buffered and has no phases. `game_events_ctx_emit` appends
to `cur`; `game_events_ctx_swap` turns `cur` into `prev` and hands back the old
`prev`, poisoned (`0xDD` in debug) and empty, as the new `cur`. So a tick's
events are readable through `game_events_ctx_prev` for exactly one tick after
the swap that ended it, which is what a system late in the order needs to react
to an event emitted by an earlier tick. Overflow follows the default context's
contract against the context's own caps.

Typed event producers register descriptors from their owning feature or game
layer. `game-events` renders any descriptor-compatible event generically, so
DevAPI and analytics do not need per-feature code.

`game_analytics` writes the same descriptor-rendered event shape and appends
`time_ms` at record time for local NDJSON/scorecard use. The in-frame event log,
DevAPI tail, and ordinary `[ev]` log mirror remain frame-scoped and do not carry
wall-clock timestamps.

## Backdoor

A game with a fundamentally different event spine can copy this pack into its own
tree and own the fork. Do not add speculative switches for one-off consumers.

## Purpose

Provide the reusable L0 event, rendering, DevAPI-tail, and local analytics
spine described above.

## Public surface

The headers and capabilities listed by `feature.json.provides` are public;
template composition and private implementation files are not.
There is no capacity probe: in a healthy frame the arena and the log always
have room, so domain code emits without asking. A frame that runs out is a
developer error -- a runaway cascade or a game that genuinely needs bigger
caps -- and it asserts in development, telling you which one to raise. Release
drops the event and counts it, because the log is telemetry and the state is
the truth; refusing a mutation to protect a telemetry line would trade the
player's action for a diagnostic.

## Validation

Run the `ctest` command from `feature.json`, then
`node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. Patch preserves the public contract,
minor adds backward-compatible surface, and major permits breaking changes.
Consumers pin both this version and an exact repository revision.

2.0.0 removes the `game_event_can_emit` capacity probe. Callers that used it to
refuse a mutation now perform the mutation and emit.

2.1.0 adds repeated sections: `game_event_record_t` plus `records`/`record_count`
on `game_event_desc_t`, rendered as a JSON array of objects. A hand-written
descriptor that leaves the two new members out keeps its old meaning (no records)
but must spell them, since `-Wextra -Werror` rejects a partial initializer.
`game_event_field_t` is unchanged, so field tables were not touched.

Version `3.0.0` removes the render fallback. `game_event_render` no longer emits a
`{ seq, tick, type, truncated:true }` marker when the line will not fit: a repeated
section makes the rendered length content-driven, and a marker carrying none of the
payment is worse than a stop. Every caller must pass a buffer of
`GAME_EVENT_RENDER_LINE_MAX`; a render that still does not fit asserts. A consumer
that sized its own 512-byte line must adopt the shared budget.

3.1.0 adds caller-owned contexts (`game_events_ctx_t`, `game_events_ctx_init`,
`_emit`, `_log`, `_prev`, `_swap`, `_tick`, `_dropped`). The global API is the
default context and behaves as before, except that a debug `frame_reset` now
poisons the used log entries as well as the arena.

## Extension points

Add descriptors, optional sinks, and guarded adapters through documented
registration seams; game-specific event policy stays game-owned.
