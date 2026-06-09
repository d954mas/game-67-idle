---
name: game-visual-qa
description: Use when visually testing, screenshotting, auditing, or debugging a running game build, UI, camera, rendering, animation, controls, canvas, viewport, or platform-specific visual output. Triggers include requests to check how the game looks, verify interactions, compare desktop and web builds, inspect screenshots, or confirm that a scene is nonblank, readable, correctly framed, and playable.
---

# Game Visual QA

Use this skill to verify what the player actually sees and can do.

## Workflow

1. Read project rules to determine the primary runtime target.
2. Build the target if needed.
3. Run the game through the project's normal launch path.
4. Capture screenshots or observations in the project scratch area.
5. Check the requested behavior plus basic visual health:
   - nonblank output
   - correct viewport/camera framing
   - readable UI text
   - no incoherent overlap
   - controls respond
   - no obvious rendering errors
6. Report concise findings with paths to evidence when available.

## Platform Order

Use the project-defined priority. If none exists:

1. Native desktop/dev build.
2. Web build.
3. Other platforms only when requested or relevant.

## Evidence

Prefer real screenshots and run logs over claims. Keep temporary captures in local scratch/temp folders unless the user asks for permanent documentation assets.

