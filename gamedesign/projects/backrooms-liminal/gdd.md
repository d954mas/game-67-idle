# Backrooms Liminal GDD

## One-Line Concept

Native 3D liminal horror: find a humming exit fuse in shifting yellow corridors while light, sound, and fear pressure force risky route choices.

## Audience

PC players who want a short, readable liminal-horror loop rather than a lore
dump. Controls stay simple enough for a first run: move, look, use, flashlight.

## Core Loop

1. Read the corridor: exit door behind you, fuse hum ahead, unstable fluorescent
   light, fear meter, and flashlight battery.
2. Move toward the fuse while choosing whether to spend battery for safer sight
   or stay in the dark and let fear rise.
3. Press `E` at the humming fuse to power the exit. The hallway reacts with a
   light surge, a louder hum, and a pursuing silhouette.
4. Return to the exit door and press `E` before fear peaks. Success ends the
   slice; failure resets the loop with the lesson "light buys time, distance
   creates pressure."

## First Playable Slice

- One native PC 3D corridor scene with WASD movement, arrow-key turn, `F`
  flashlight toggle, and `E` use.
- One clear goal: find the glowing fuse at the far fluorescent bay, then return
  to the exit door.
- One feedback moment: collecting the fuse changes the objective, raises the
  horror pressure, and spawns the chasing shadow/light failure state.
- One visual proof screenshot for product-read review, plus a screenshot after
  the fuse state changes.
- One filled `reviews/first_slice_visual_gate.md` before broad runtime work.
- One filled `data/core_loop.json` with player verbs, rules, feedback, risk,
  goals, replay reason, and reference grounding. Do not assume hands-off
  progression, away-time rewards, or reset-meta loops unless the lead
  explicitly chooses that direction.
- One project-specific `visual/live_state_acceptance_matrix.json` that names
  required UI/player-read states before broad visual acceptance.
- One visual-first session contract: goal, non-goal, proof, stop condition,
  likely files.
- One screenshot-vs-target mismatch list before runtime visual code and after
  meaningful render changes.
- If the slice depends on beauty, casual readability, generated UI, or a fake
  shot match, one strict visual product gate using `--visual-strict`.
- Optional critic packet from `tools/product_gate/visual_critique_packet.mjs`
  before the strict gate verdict.

## Art Direction Stub

Sickly mono-yellow wallpaper, damp carpet, low ceiling tiles, fluorescent
strips, and hard pools of light. The scene should feel empty but watched: long
perspective corridors, repeating wall seams, subtle grime texture, flicker,
contact shadows, fogged distance, and one impossible dark silhouette after the
fuse is taken. UI is minimal diegetic survival HUD: objective, fear, battery,
fuse/exit state, and short control hints on solid dark plates.

## Visual Target

Textual target for the first runtime pass:

- Camera: first-person eye height, centered corridor vanishing point, slight
  head-bob, 70-degree-feel perspective.
- Palette: fungal yellow wallpaper, brown carpet, gray ceiling, cold-white
  fluorescent pools, black-blue shadow corners.
- Lighting: at least three visible fluorescent fixtures, flicker, flashlight
  cone, distance fog, dark side openings, and soft contact shadows under wall
  bases/door/fuse.
- Horror: the first screen is quiet and wrong; after fuse pickup a black
  silhouette appears down the corridor and fear pressure becomes visible.
- First-player read: "Find fuse" must be readable within 10 seconds, with
  controls and meters visible.

## Reference Digest

Mode: quick visual/lore deconstruction, enough for first-slice implementation
because the user named the Backrooms but did not require release-final canon.

Sources checked:

- Wikipedia, "The Backrooms"
- Wired, "How to 'No-Clip' Reality and Arrive in the Backrooms"

Observed facts used:

- The core setting is an impossibly large liminal interior reached by
  no-clipping out of reality.
- The iconic visual grammar is pale/yellow walls, beige or damp carpet,
  drop-ceiling/fluorescent lighting, emptiness, and no ordinary windows or
  furniture.
- The horror depends more on isolation, wrongness, repetition, hum, and unseen
  threat than on immediate gore or explicit monster combat.

Borrow:

- Mono-yellow maze grammar, fluorescent hum, damp carpet, empty rooms, and
  unexplained spatial wrongness.

Avoid:

- Directly recreating a specific copyrighted video, found-footage sequence, or
  named extended-lore entity. The slice uses an original "fuse/exit" objective
  and a generic silhouette pressure.

Current-build mismatch before runtime coding:

- Current seed is a bright 2D shape demo, not a 3D liminal horror space.
- No shadows, light sources, first-person movement, objective, fear pressure,
  or player-readable horror loop exist yet.

Next native proof:

- Desktop screenshot of the first corridor state and a second state after fuse
  pickup, judged against `reviews/first_slice_visual_gate.md`.
