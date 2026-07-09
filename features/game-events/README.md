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
    game_events_devapi.h
    game_analytics.h
  src/
    game_events.c
    game_event_render.c
    game_events_devapi.c
    game_analytics.c
```

## What It Owns

- Fixed per-frame event log and payload arena.
- Descriptor contract for generated or hand-written typed event payloads.
- Generic descriptor-driven JSON rendering.
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

## Backdoor

A game with a fundamentally different event spine can copy this pack into its own
tree and own the fork. Do not add speculative switches for one-off consumers.
