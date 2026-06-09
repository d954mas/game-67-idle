# DevAPI Pattern

## Command Bus Shape

Use JSON-lines over a persistent local transport. Each request is one JSON object line; each response is one JSON object line. Batch requests are one JSON array line.

Request fields:

```json
{"request_id":1,"method":"input.key","params":{"key":"D","mode":"tap"}}
```

Response fields:

```json
{"request_id":1,"ok":true,"result":{}}
```

Use `request_id` only for request/response correlation. Widget/entity ids inside `params` should be stable strings.
`request_id` may be a number or string; return it unchanged.

## Command Registry

The runtime command registry is the source of truth. Register each command once with its handler and metadata; do not keep a separate JSON command database unless it generates the runtime registry.

Minimum command descriptor:

```c
typedef struct nt_devapi_command_desc {
    const char *method;          /* "ui.click" */
    const char *layer;           /* "engine" or "game" */
    const char *summary;
    const char *params_shape;
    const char *result_shape;
    const char *frame_behavior;  /* read, enqueue input, wait barrier, capture */
    const char *side_effects;    /* none, input, frame_wait, capture */
    nt_devapi_handler_fn handler;
    void *user;
} nt_devapi_command_desc;
```

Discovery commands:

```json
{"request_id":1,"method":"endpoints","params":{}}
{"request_id":2,"method":"command.describe","params":{"method":"ui.click"}}
{"request_id":3,"method":"command.describe_all","params":{}}
{"request_id":4,"method":"features","params":{}}
```

`endpoints` should be cheap and return at least `method`, `layer`, and `summary`.
`command.describe` should return the full descriptor for one command, including `params_shape`, `result_shape`, `frame_behavior`, and `side_effects`.

Agents should use this order:

1. call `endpoints`;
2. call `command.describe` for commands they will use;
3. use `frame_behavior` to decide whether `frame.wait` is needed;
4. use `side_effects` to distinguish read-only commands from input/capture/wait commands;
5. inspect C code only if metadata is missing or runtime behavior disagrees with the descriptor.

Use build/runtime policy as an allow-list. If a command appears in `endpoints`, it should work in that build. Disabled or unavailable modules should normally omit their commands.

Common endpoints:

```json
{"request_id":1,"method":"ping","params":{}}
{"request_id":2,"method":"endpoints","params":{}}
{"request_id":3,"method":"command.describe","params":{"method":"ui.tree"}}
{"request_id":4,"method":"view","params":{}}
{"request_id":5,"method":"frame.current","params":{}}
{"request_id":6,"method":"frame.wait","params":{"frames":1}}
{"request_id":7,"method":"ui.tree","params":{}}
{"request_id":8,"method":"ui.element","params":{"id":"upgrade.button"}}
{"request_id":9,"method":"ui.click","params":{"id":"upgrade.button"}}
{"request_id":10,"method":"ui.drag","params":{"id":"inventory.item.sword","to_id":"equipment.weapon","frames":8}}
{"request_id":11,"method":"ui.scroll","params":{"id":"inventory.list","dx":0,"dy":-480,"frames":6}}
{"request_id":12,"method":"input.gesture","params":{"type":"drag","from_x":100,"from_y":200,"to_x":300,"to_y":200,"frames":8}}
{"request_id":13,"method":"game.state","params":{}}
{"request_id":14,"method":"input.key","params":{"key":"D","mode":"tap"}}
{"request_id":15,"method":"input.move","params":{"x":480,"y":320}}
{"request_id":16,"method":"input.click","params":{"x":480,"y":320,"button":"left"}}
{"request_id":17,"method":"input.pointer","params":{"phase":"down","x":480,"y":320,"button":"left"}}
{"request_id":18,"method":"input.wheel","params":{"x":480,"y":320,"dx":0,"dy":-120}}
{"request_id":19,"method":"input.button","params":{"button":"left","state":"down"}}
```

Keep commands deterministic and frame-aware. Apply synthetic input after the engine polls real input for the frame.
Reject commands before mutating runtime state when their event queues do not have enough capacity. A failed command must not leave a partial key-down, pointer-down, drag, or wheel event behind.

## Ordered Batches and Frame Sync

Batch requests must execute in order. `frame.wait` is a barrier: commands after it run only after the target frame is reached, and the batch response is returned after all commands complete.
Cap wait lengths and gesture frame counts. Long waits should fail fast with a structured error instead of blocking the command queue indefinitely.

```json
[
  {"request_id":"tap","method":"input.key","params":{"key":"D","mode":"tap"}},
  {"request_id":"wait","method":"frame.wait","params":{"frames":1}},
  {"request_id":"state","method":"game.state","params":{}}
]
```

Use this instead of sleeps in bot and smoke scripts. A synchronous client can expose this as `step(action)`:

1. send input/action;
2. wait one or more frames;
3. observe state.

`input.key` `tap` should enqueue a key-down event and a key-up event. `hold_frames` controls the gap:

```json
{"method":"input.key","params":{"key":"SPACE","mode":"tap","hold_frames":2}}
```

Multiple input requests in one frame must be ordered events, not overwritten state flags. If a game needs separate repeated presses of the same key, put frame waits between them or let the input queue serialize them across frames.

## UI and Gestures

Widget-targeted input is reusable when the engine can expose a UI tree with stable ids and bounds:

```text
{"method":"ui.tree","params":{}}
{"method":"ui.element","params":{"id":"settings.audio.volume"}}
{"method":"ui.click","params":{"id":"settings.close","button":"left"}}
{"method":"ui.drag","params":{"id":"inventory.item.sword","to_id":"equipment.weapon","frames":12}}
{"method":"ui.scroll","params":{"id":"inventory.list","dx":0,"dy":-480,"frames":6}}
```

Raw gesture input is also reusable:

```text
{"method":"input.gesture","params":{"type":"drag","from_x":100,"from_y":200,"to_x":300,"to_y":200,"frames":8,"button":"left"}}
{"method":"input.gesture","params":{"type":"scroll","x":500,"y":400,"dx":0,"dy":-480,"frames":6}}
{"method":"input.gesture","params":{"type":"tap","x":100,"y":200,"button":"left"}}
```

Implement `ui.*` by resolving widget ids to framebuffer coordinates, then enqueueing the same low-level pointer/wheel events as raw input. This keeps tests readable without making game-specific actions part of the engine API.

Minimum temporary adapter:

- expose `scene.viewport` as a stable root element when no real UI tree exists;
- keep ids as strings;
- return role, text, parent id, children, state flags, and bounds in framebuffer coordinates;
- implement `ui.click` and `ui.scroll` through `input.click` and `input.wheel` semantics.

Minimum UI node contract:

```json
{
  "id": "shop.buy_1",
  "parent_id": "shop.panel",
  "role": "button",
  "label": "Buy button",
  "text": "Buy",
  "bounds": {"x": 10, "y": 20, "w": 120, "h": 40},
  "visible": true,
  "enabled": true,
  "children": []
}
```

This is a minimum contract, not a frozen final schema. The exact current shape should be discoverable through `command.describe` for `ui.tree` and `ui.element`.

`ui.tree` should return enough data for a bot to choose and click a target without extra round trips. `ui.element` should return the same core fields for one id, plus any future detail fields.

Stable id rule:

- prefer explicit developer-assigned ids;
- allow labels/text only as a fallback for test scripts;
- avoid generated tree indices as long-term selectors.

## Layer Boundary

Low-level input emulation is reusable across games:

```text
{"method":"input.key","params":{"key":"D","mode":"down"}}
{"method":"input.key","params":{"key":"D","mode":"up"}}
{"method":"input.pointer","params":{"id":0,"phase":"down","x":100,"y":200,"button":"left"}}
{"method":"input.pointer","params":{"id":0,"phase":"move","x":140,"y":200}}
{"method":"input.pointer","params":{"id":0,"phase":"up","x":140,"y":200,"button":"left"}}
{"method":"input.wheel","params":{"x":500,"y":400,"dx":0,"dy":-120}}
{"method":"input.gesture","params":{"type":"tap","x":100,"y":200,"frames":1}}
{"method":"input.gesture","params":{"type":"drag","from_x":100,"from_y":200,"to_x":300,"to_y":200,"frames":8}}
{"method":"input.gesture","params":{"type":"scroll","x":500,"y":400,"dx":0,"dy":-120}}
```

Semantic actions are game-specific wrappers:

```text
{"method":"action.tap","params":{"name":"move_right"}}
{"method":"action.hold","params":{"name":"attack","frames":12}}
```

Use semantic actions only inside a game project when they make bots clearer. Keep engine and reusable skills focused on raw input, frame sync, capture, and snapshots.

## Bot Scripts

Store reusable project automation under `tools/devapi/scenarios/`. Keep scripts small and composable. Repeated manual checks should become scenarios.

Bots should:

1. launch a fresh game with automation enabled, unless the script explicitly asks to attach to an existing process;
2. call `ping` and `endpoints`;
3. read state;
4. issue one input/action;
5. wait only as long as needed;
6. re-read state and assert the expected change.

Prefer a shared client/harness over per-script socket code. A good client should expose:

- `request(method, params)` for raw commands;
- `batch([...])` and `batch_results([...])` for ordered frame-aware batches;
- `wait_frames(n)`;
- `observe()`;
- `step(method, params, wait_frames=1)` as `act -> frame.wait -> observe`;
- small helpers such as `key_tap`, `click_ui`, `scroll_ui`, `gesture`.
- capture helpers such as `capture_screenshot` and `record_gameplay` that call project tools after frame sync.
- optional recording helpers such as `start_recording` and `stop` for evidence capture.

For deterministic tests, fail when the requested port is already occupied instead of silently attaching to stale state. Keep attach/reuse behavior as an explicit option for manual debugging.

Keep conditional logic in bot scripts:

```python
with running_game(port=9123) as game:
    for _ in range(8):
        state = game.observe()
        if state["shape"] == "cylinder":
            break
        game.key_tap("D")
```

## Capture

Desktop options:

- Prefer engine screenshots when available.
- Otherwise use OS capture tools for evidence.
- Use `ffmpeg` for short recordings when installed.

If capture is not yet engine-native, keep it as external tooling with the same observe/act/wait rhythm:

1. run the game with DevAPI;
2. drive input with JSON commands;
3. use `frame.wait` until the visual state is stable;
4. call the project screenshot or recording script;
5. report the saved file path.

Bot harnesses can wrap those scripts directly:

```python
with running_game(port=9123) as game:
    game.key_tap("D")
    path = game.capture_screenshot("build/captures/after_key.png", wait_frames=2)
```

For controlled recording, use a context manager so exceptions stop the recorder. Always pass or keep default hard limits (`max_seconds`, `max_megabytes`) so an orphaned recorder cannot fill the disk:

```python
with running_game(port=9123) as game:
    with game.start_recording("build/captures/check.mp4", x=0, y=0, width=1280, height=720, max_seconds=60, max_megabytes=512):
        game.click_ui("test.button", wait_frames=2)
        game.wait_frames(30)
```

Only add `capture.screenshot` / `capture.record_*` protocol endpoints when the engine can capture its own framebuffer. Until then, keep capture as an external evidence tool so the DevAPI remains focused on runtime state and input.

Web options:

- Prefer browser automation screenshots of the canvas/page.
- Use canvas pixel checks to reject blank frames.

## Replacement Rule

When the engine gains native automation, keep scripts and command names stable. Replace only the adapter implementation and update build flags.
