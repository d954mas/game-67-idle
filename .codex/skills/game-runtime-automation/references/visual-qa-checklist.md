# Visual QA Checklist

Load this reference when the task is to verify what the player actually sees,
whether a scene is readable/playable, or whether a runtime visual change is
healthy.

## Visual QA

1. Build and run the primary runtime target through the project's normal launch
   path.
2. For native UI, capture at least one real desktop-window proof at the intended
   PC size before relying on framebuffer-only captures. Framebuffer captures are
   useful evidence, but they can miss actual window scale or focus problems.
3. Capture screenshots or observations in the project scratch area.
4. Check the requested behavior plus basic visual health:
   - nonblank output
   - correct viewport/camera framing
   - readable UI text
   - no incoherent overlap
   - controls respond
   - no obvious rendering errors
5. For first-screen or UI-focus checks, answer explicitly:
   - where the player is
   - what is active now
   - what can be clicked now
   - what is locked or future-only
   - whether DevAPI/input enabled state matches the visible affordances
6. Report concise findings with paths to evidence.

Platform order when the project defines none: native desktop first, then web,
then other platforms only when requested or relevant. Prefer real screenshots
and run logs over claims.

## Web Scope

Run WASM/web visual checks only when explicitly requested or when the task
targets web behavior.

Web options:

- Prefer browser automation screenshots of the canvas/page.
- Use canvas pixel checks to reject blank frames.

## Capture Notes

Desktop options:

- Prefer engine screenshots when available.
- Otherwise use OS capture tools for evidence.
- Use `ffmpeg` for short recordings when installed.

If capture is not engine-native, keep it as external tooling with the same
observe/act/wait rhythm:

1. run the game with DevAPI;
2. drive input with JSON commands;
3. use `frame.wait` until the visual state is stable;
4. call the project screenshot or recording script;
5. report the saved file path.
