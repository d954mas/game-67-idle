# Runway Awakening MVP telemetry contract

The 500-player test uses the existing `game-events` spine and the existing
`platform-sdk` facade. Game code never calls Poki JavaScript directly.

## Game-owned events

All payloads are fixed-size, bounded integers/booleans. They contain no player
identifier, free-form text, outfit names, device data, or account data.

| Event | Payload | Meaning |
|---|---|---|
| `runway.awakening_start` | recipe `0..5`, support signature `0..2`, MVP round `1..8`, recipe/look known flags | A valid two-Essence awakening began. |
| `runway.recipe_reveal` | recipe/signature/round, outcome `0 discovery / 1 remix / 2 replay`, recipes `0..6`, looks `0..18` | The recipe card was reached or an exact replay was skipped to it. Exactly once per accepted awakening. |
| `runway.lookbook_open` | recipes `0..6`, looks `0..18` | The player opened the Lookbook. |
| `runway.collection_mastery` | milestone `1 / 3 / 6`, recipes `1..6` | A new recipe crossed a collection threshold. Replays and remixes never emit it. |

Exact duplicate looks emit `replay` and do not increment `rounds_completed`.
A known recipe with a new support signature emits `remix` and increments the
unique-look round count. A first recipe emits `discovery`.

## Portal lifecycle

`game.loading_finished`, `gameplay.start`, and `gameplay.stop` remain owned by
the shared platform facade. Loading finishes only after the playable shell has
been presented; gameplay starts only after player input and stops while gameplay
is not allowed. The selected Poki adapter is a build-time backend behind that
facade.

Native tests and local mock runs validate this contract and event ordering.
They do **not** prove Poki portal acceptance, production SDK initialization,
portal inspector state, iframe policy, or account metadata. Those require a
Poki-target release artifact and real portal-side QA.

## Poki measure translation

One recorder subscribes to the typed events above and forwards only these
stable triples through the platform SDK sink:

- `round`, `1..8`, `start|complete` for discoveries and remixes;
- `recipe`, one of the six literal recipe IDs, `discovered` once;
- `lookbook`, `main`, `open`;
- `collection`, `1|3|6`, `complete` once.

Replay emits no Poki measure. Recipe discovery and collection mastery have
independent once-guards. The Poki adapter calls
`PokiSDK.measure(category, what, action)`; other adapters are safe no-ops.
