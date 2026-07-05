# Release 30 Minute Fake Shot Notes

Visual tier: fake shot.

Files:

- `release_30min_fake_shot.svg` - editable source.
- `release_30min_fake_shot.png` - preview render for quick review.

Purpose: prove how the 30-minute release balance reads in one gameplay state.
The shot is not final art, not a sliced UI atlas, and not a runtime capture.

Gameplay visible:

- level, HP, gold, and XP progress toward level 10;
- current critical quest near the 23-27 minute hideout beat;
- enemy preview with threat label;
- gear tier ladder with T4 active and T5 as the next promise;
- reward preview and primary action;
- bottom navigation density for PC and phone.

Intentionally placeholder:

- character silhouettes;
- exact final copy;
- final background art;
- exact widget slicing and engine text rendering.

Next gate after approval:

1. Expand authored content JSON from `release_balance.json`.
2. Replace placeholder silhouettes with sourced or generated assets carrying
   provenance.
3. Build a runtime capture that matches this composition using the engine text
   renderer.
