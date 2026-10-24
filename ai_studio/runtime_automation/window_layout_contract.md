# Game Window Layout Contract

Each game owns `design/ui_window_contracts.json`. The same Studio checker runs
every game; it contains no rules for named actions or individual games.

```json
{
  "schema": "ai_studio.ui_window_contracts.v1",
  "windows": [
    {
      "id": "settings/window",
      "scene": "settings",
      "source": {"file": "src/ui/settings.c", "function": "settings_draw_panel"}
    },
    {
      "id": "inventory/window",
      "scene": "inventory",
      "layout": "adaptive",
      "scroll": ["inventory/bag/scroll"],
      "reason": "The item grid and equipped hero need different placement in portrait",
      "source": {"file": "src/ui/inventory.c", "function": "inventory_draw_panel"}
    }
  ],
  "non_window_scenes": [
    {"id": "cutscene", "reason": "Full-screen modal scene without a window"}
  ]
}
```

`layout` defaults to `single`; `scroll` defaults to an empty list. `adaptive`
and every allowed scroll region require a reason. Settings always use `single`
with no scroll, regardless of the exception list. A window inside another
scene omits `scene` and supplies `open`, a nonempty sequence of
`{"method":"...","params":{...}}` DevAPI calls. A modal scene that needs
route arguments also supplies `open`. An open command may set `waitFor` to a
window root ID before the next command (for example, before `ui.click`).
`context` optionally names its UI
context for `ui.tree` and `ui.scroll`.

The window root has a stable semantic ID; new windows use a `/window` suffix.
The checker compares those literals found under `src/` with the contract, and compares
the contract's scene IDs with `game.scene.list` during capture. A modal scene
that does not produce a window must be listed in `non_window_scenes` with a
reason. A source ID ending in `/window` that names a HUD element rather than a
window goes in `non_window_roots` with a reason. Dynamic windows inside gameplay scenes still need explicit entries;
the source scan cannot discover IDs assembled at runtime.

An empty `windows` array requires `no_windows_reason`; only use it after
inspecting a game that truly has no windows. Check every active game with
`src/` in one pass; games frozen in `game.json` are skipped:

```powershell
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/window_layout_contract.py --studio-root .
```

This check fails on every missing active-game contract. Active games need their
own inventories before it can become a blocking Studio gate. The static pass
cannot prove a game's window appearance or discover dynamically assembled
root IDs; the runtime scene catalog and capture complete that check.

Run the native DevAPI capture after building the game:

```powershell
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/window_layout_capture.py --game <game-id>
```

The runner opens each window at 390x844 and 844x390, saves screenshots and
complete `ui.tree` snapshots, sends wheel input to the window or each declared
scroll region, then checks `build/quality/ui_windows/proof.json`. The checker
requires the same named controls and normalized geometry for `single` windows;
`adaptive` may change geometry but must retain the controls. It checks window
fit, unexpected clipping, scroll movement outside allowed regions, and direct
scroll primitives in no-scroll draw functions. Run the proof with the same
locale, content and platform state in both orientations; use game-owned `open`
commands to set that state. Inspect the saved screenshots for legibility,
touch targets, and visual overlap, which `ui.tree` cannot judge.

Clay can place window controls in floating containers outside the window's
probe-tree ancestry. The checker includes same-namespace named controls whose
centers lie inside the window, and allows small edge overlap for floating
buttons. Full-screen backdrops are not treated as window children.

For a static contract check or a saved capture, run
`window_layout_contract.py --contract <path> --game-root <path>` and add
`--proof <proof.json> --scene-list <scene-list.json>` for the full check.
