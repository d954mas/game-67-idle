# Install body sync

## Link

The feature compiles in place. Add the modules it stands on first, then this
one, after defining the consuming target:

```cmake
set(NET_LINK_DIR "${GAME_REPO_ROOT}/features/net-link")
set(NET_BITS_DIR "${GAME_REPO_ROOT}/features/net-bits")
set(COOP_NET_DIR "${GAME_REPO_ROOT}/features/coop-net")
set(BODY_SYNC_DIR "${GAME_REPO_ROOT}/features/body-sync")
add_subdirectory("${NET_LINK_DIR}" "${CMAKE_BINARY_DIR}/net-link")
add_subdirectory("${NET_BITS_DIR}" "${CMAKE_BINARY_DIR}/net-bits")
add_subdirectory("${COOP_NET_DIR}" "${CMAKE_BINARY_DIR}/coop-net")
add_subdirectory("${BODY_SYNC_DIR}" "${CMAKE_BINARY_DIR}/body-sync")
target_link_libraries(game PRIVATE body_sync)
```

`body_sync` brings Box3D through `features/box3d/cmake/Box3D.cmake` and links
`nt::box3d` publicly, since its header names `b3BodyId`. `body_sync` is the
real seam; `body_sync_stub` is the naive baseline with the same header, and a
program links one of them, never both. A game that ships the seam links only
`body_sync`; the stand (`body_sync_stand` on the real seam,
`body_sync_stand_stub` on the stub, `body_sync_stand_logic`, and the command
lines `body_sync_stand_cli` and `body_sync_stand_cli_stub`) is for tests and
tools. Pass `body_sync_write_state` the session's `coop_session_state_max` as
the capacity: the link's limit less the session's own header. Record
the source and version of each feature from its `feature.json` in the game's
`dependencies.json`.

## Use the seam

1. `body_sync_create` per endpoint world: its seat, the session's seat count
   and tick rate, and the box and speeds every bound body stays within (a
   peer's pose outside them is refused, so give a margin past the level's
   bounds). Bind every moving body that both sides build with the same id
   (`body_sync_id_make(slot, generation)`), the same starting authority and
   the same number (0 for a body of the level). Set `character_category` to
   the Box3D category bits of this endpoint's own characters, so a LIGHT copy
   does not block them (the host's simulation moves it); the seam then owns
   those mask bits of a LIGHT copy's shapes and gives the masks back exactly
   when the body is local. `delay_min` and `delay_max` (ticks, 0 for 4 and 15)
   bound the playback delay. `light_mass_max` (kilograms, 0 off) turns on the second
   layer on a guest: free LIGHT copies up to that mass keep the character bits,
   are simulated here while something of this side's moves at them, and rest on
   their owner's pose (README "The second layer"); draw them as any copy, leave
   the ones `body_sync_is_predicted` names out of playback measurements, and
   move this side's bodies after `body_sync_apply` as below.
2. Each tick, in this order: `body_sync_set_peer_tick` for each seat (from
   `coop_session_peer_tick`), `body_sync_receive_state` and
   `body_sync_receive_record` for what the session delivered, `body_sync_apply`,
   the game's actions, `b3World_Step`, the game's judging of the step's
   contacts, `body_sync_collect`, then for each seat
   `body_sync_write_state` until false (send each with
   `coop_session_send_state` and report the number with `body_sync_state_sent`),
   the fates `body_sync_unresolved` asks for, and `body_sync_peek_record` /
   `body_sync_pop_record` until none is left; then `coop_session_end_tick`.
3. Seam records travel on the session's reliable lane. When a game sends records
   of its own on the same lane, it frames them (the stand prefixes one byte:
   1 seam, 2 stand) and strips the byte before `body_sync_receive_record`.
4. Grabs: `body_sync_claim` on the grabbing endpoint at once; the host reads
   `body_sync_poll_claim` and answers every polled claim with
   `body_sync_answer(sync, &claim, granted)`. A move that answers no claim (a
   rule takes a body back) is `body_sync_assign(sync, id, seat, hold)`. Throws:
   `body_sync_set_holder(id, 0)` then
   `body_sync_claim(id, BODY_SYNC_HOLD_UNTIL_SETTLED)`. Shapes of bound bodies
   need contact events on for the settled hold to see contacts.
5. Draw a body at its physics pose plus `body_sync_render_offset`: a copy is
   placed rather than driven past `max_speed`, and a take-over continues from
   the newest state; the offset keeps the drawn pose still through both and
   gives the move back over a few ticks. Physics never sees it.
6. Animation state and other small per-body data go in `body_sync_set_data`
   (8 bytes, local bodies) and read with `body_sync_data`, on a copy as of its
   playback time; flag bits stay for game state.
7. A body that becomes local continues its last owner's state, motion type
   included. When a seat leaves, its bodies come back to the host still naming
   their carrier: drop what that player carried by the game's own rule.
8. A FORMAT result means the peer's bytes broke the format: close that peer
   (`coop_session_kick` on the host).

## Add a scene and a stand target

A game's scenes are C data built by functions; it runs the stand's command line
with them from a target of its own:

```c
#include "features/body_sync/body_sync_stand.h"

static void build_lobby(body_sync_scene_t *s) {
    body_sync_scene_reset(s, "lobby");
    /* add the floor, props and player spawns with body_sync_scene_add */
}

static const body_sync_scene_entry_t scenes[] = {{"lobby", "the run's first room", build_lobby}};

int main(int argc, char **argv) { return body_sync_stand_main(argc, argv, scenes, 1); }
```

```cmake
add_executable(game_stand tools/game_stand.c)
target_link_libraries(game_stand PRIVATE body_sync_stand)
```

`body_sync_scene_validate` runs on every scene before a world is built, and a
refusal names the body. The drawing side reads `body_sync_stand_body` for one
endpoint each frame; `model_tag` is the game's to map.

For a window that draws the stand with the game's models, link `body_sync_view`
(added by `example/CMakeLists.txt` where the engine's window and renderer exist)
and call `body_sync_view_main(argc, argv, scenes, count, &drawer)` with a
`body_sync_view_drawer_t` whose `draw_body` draws one `body_sync_body_view_t`.
The library links no engine codec: the game's executable links its own
transcoder and mesh codec, or the engine's stubs as the example does:

```cmake
add_executable(game_stand_view EXCLUDE_FROM_ALL stand_view_main.c)
target_link_libraries(game_stand_view PRIVATE body_sync_view game_render nt_basisu_transcoder nt_meshwire)
```

## Verify

```powershell
node ai_studio/feature_testing/run.mjs
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "body_sync" --output-on-failure
node features/validate_contracts.mjs
```

## Uninstall

Remove the `add_subdirectory` of `features/body-sync` and the link to
`body_sync`; nothing else is installed.
