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
```

## What It Owns

- Fixed per-frame event log and payload arena.
- Descriptor contract for generated or hand-written typed event payloads.
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

## Extension points

Add descriptors, optional sinks, and guarded adapters through documented
registration seams; game-specific event policy stays game-owned.
