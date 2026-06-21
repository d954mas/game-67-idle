# DevAPI Pattern

The engine now ships a **native** DevAPI. This file is no longer a "how to build
one" design spec — it points at the engine source of truth and records the
game-side deltas a bot must follow. When the live runtime disagrees with this
file, the runtime wins: discover via `endpoints` + `command.describe`.

## Source of Truth

- Engine bus + transport (read these, don't re-describe):
  `external/neotolis-engine/engine/devapi/nt_devapi.h` (init/register/submit/poll),
  `…/nt_devapi_types.h` (descriptor + handler ABI),
  `…/nt_devapi_net.h` (loopback-TCP transport).
- Game-owned commands the engine deliberately omits: `src/game_devapi_ui.c`
  (`ui.tree`/`ui.element`/`ui.click`/`entity.list`) and the generated
  `game.state.*` (from `tools/state_codegen/generate_state.py`).
- Live discovery: `endpoints` then `command.describe` — always authoritative.

## Handler ABI (engine)

Register one descriptor + handler per command; the registry is the source of
truth (no separate command JSON db).

```c
typedef struct nt_devapi_command_desc {
    const char *method;          /* "ui.click"                              */
    const char *group;           /* "core"/"time"/"input"/"discovery"/"game" */
    const char *summary;
    const char *params_shape;
    const char *result_shape;
    const char *frame_behavior;  /* "immediate" / "next-frame" / …          */
    const char *side_effects;    /* "none" / "injects input" / …            */
} nt_devapi_command_desc;

/* Fill result_obj (pre-created) and return true; or fill err and return false. */
typedef bool (*nt_devapi_handler_fn)(const cJSON *params, cJSON *result_obj,
                                     nt_devapi_error *err, void *user_data);
```

Note: the field is `group` (not the old `layer`). Game commands register with
`group="game"`.

## Wire Shape

JSON-lines over the loopback TCP transport. One request object per line; one
response line back. Request: `{"request_id":1,"method":"ui.click","params":{…}}`.
Response: `{"ok":true,"result":{…}}` or `{"ok":false,"error":{"code","message"}}`.
`request_id` is for correlation only; echoed unchanged (number or string).

Current result shapes that differ from the old sidecar (bots must follow):

- `endpoints` -> `{"commands":[{"method","group","summary",…}]}` (not a bare list).
- `game.state.get` -> `{"path","value"}` (a path value can be any JSON, so wrapped).
- `ui.tree` -> `{"nodes":[…]}`; each node has `id,parent_id,role,label,text,
  x,y,w,h,center_x,center_y,visible,enabled`.
- `entity.list` -> `{"entities":[…]}`.
- The seed `game.state` view exposes `shape` (name) + `test_ui_clicks`; the
  generated `game.state.*` view serializes the full schema (incl `shape_index`).

## Frame Sync — NO deferred command inside a batch

The engine **rejects** deferred commands (`frame.wait`) inside a batch array
(it returns a `bad_params` error entry: a batch is one ordered immediate
response). So `step(action)` must run **sequentially**, not as one batch:

1. send the input/action (immediate);
2. `frame.wait` as its own request (the transport delivers the deferred reply
   when the frame deadline passes);
3. read state.

Batches are still fine for *immediate* commands only. Cap wait/gesture frame
counts; long waits fail fast rather than block the queue.

`ui.click` injects a synthetic pointer DOWN+UP at the node center via the engine
input layer; it drains on the **next** sim-advance — always step a frame (or a
few) before observing, never sleep.

## UI / Input

`ui.*` resolve a game-registered node id to logical input coordinates and inject
the same low-level pointer events as raw input (engine `input.*` group). The seed
rebuilds its UI tree each frame (`game_devapi_ui_register_node`) so a bot can
introspect and click without game-specific engine commands. Prefer explicit
developer-assigned string ids; labels/text are fallback selectors only; never
use tree indices as long-term selectors.

## Bot Scripts & Client

Shared harness: `tools/devapi/devapi_client.py` (`request`, `result`,
`wait_frames`, `observe`, sequential `step`, `endpoint_methods`, `click_ui`,
`capture_screenshot`, recording helpers). One-shot CLI:
`tools/devapi/devapi_cli.py`. Smoke scripts are game-specific and should live
under `tools/<game-id>/` for the active prototype.

Bots: launch fresh with DevAPI (fail if the port is busy unless `reuse_existing`),
`ping` + `endpoints`, read state, issue one action, wait only as long as needed,
re-read and assert.

For game-specific playable smoke scripts, prefer named suites over one monolith
when a prototype needs multiple proof types. A project-specific playable smoke
can expose `--suite movement` or `--suite asset-load,visual-framing`; keep
contract, asset load, movement, combat pacing, reward loop, and special/upgrade
proof separable.

```python
with running_game(port=9123) as game:
    for _ in range(8):
        if game.observe().get("shape") == "cylinder":
            break
        game.click_ui("seed.cycle", wait_frames=2)
```

## Capture

The engine has no native framebuffer capture wired for the seed, so capture stays
external (`capture_window.py` / `capture_screen.ps1` / `record_screen_ffmpeg.ps1`)
with the same observe/act/wait rhythm: drive input, `frame.wait` until stable,
call the project screenshot/record script, report the saved path. Recording uses
a context manager with hard `max_seconds`/`max_megabytes` limits.

## Gate

The whole layer is dev-only: `GAME_DEVAPI_ENABLED` (ON for native Debug, OFF for
Release/WASM) forwards to the engine's `NT_DEVAPI_ENABLED`. With the gate off the
`#if NT_DEVAPI_ENABLED` guards compile every command out — shipping builds link
zero `nt_devapi_*` symbols.
