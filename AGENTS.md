# AGENTS.md

## Project

- This is an AI-first game development experiment: improve the game and the AI workflow together.
- The engine lives in `external/neotolis-engine` as a submodule; do not edit it unless explicitly asked.
- Game design lives in `gamedesing/`; game code lives in `src/`.
- Reusable project skills live in `.codex/skills/`; keep them generic enough to reuse in other games.

## Direction

- `67` is the core identity: a meme-symbol of legendary status, downfall, and climb back.
- Keep tone meme-heavy, sharp, readable, and playable.
- Prefer small playable iterations over large speculative systems.

## Validation

- Product target for Game 67 is mobile portrait + web. Native desktop/PC is the development and automation harness.
- Default validation should use the native desktop/PC build because it is faster. Run WASM/web builds only when the user explicitly asks for them or the task is specifically about web/WASM behavior.
- When validating playable or visual changes, use screenshots and emulated input. Cover desktop browser and a mobile portrait viewport when the surface is web-based.
- Pack building is explicit; do not wire pack generation into every normal game build.
- If a task reveals repeated friction, propose updating `AGENTS.md` or creating a project skill.
