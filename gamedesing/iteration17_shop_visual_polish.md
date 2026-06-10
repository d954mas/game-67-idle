# Iteration 17 SHOP Visual Polish

Native desktop target: 390x844 portrait. No WASM/web validation for this pass.

## Goal

After `STND` and the first `SHOP` claim, the player should instantly read: the stand is active, the next job is bigger, and the reward came from the stand.

## SHOP Card

- Label: `SHOP`
- Reward: `+90`
- Available/running strip: blue.
- Ready/claim strip: green.
- Completed post-claim feedback: `+90` in green near the big 67 button for consistency, plus stand-side coins.

Implementation notes for `src/main.c` simple drawing:

- Keep `SHOP` at `job_slots.label_x`, but use scale `2.65`.
- Move `+90` left from the current far-right edge: use about `job_slots.value_x - 18` instead of `-4`; scale `2.55`.
- Do not let `+90` touch the right card border or the small blue card decoration. Leave at least 18 px right padding.
- Keep the progress bar at the bottom of the card; it must not overlap `SHOP` or `+90`.

## Stand Payoff

At `status >= 11`, make the stand read as active business:

- Add a small customer/helper directly beside the stand, not near the big tap button:
  - head: pink circle, radius 9-10 px;
  - body: blue rounded/simple rect, 16x24 px;
  - place at `stand_x + 48`, `stand_y + 4`.
- Add 3 coin bubbles above the stand roof:
  - radius 5-6 px;
  - y range `stand_y - 78` to `stand_y - 62`;
  - keep all coins between x `stand_x - 28` and `stand_x + 34`.
- Add one small green awning/sign with white `67`; do not add extra text.

## Must Not Overlap

- Stand/customer/coins must not touch the big `67` tap button.
- Coin bubbles must not touch top resource/status pills.
- `+90` must not overlap the card's right-side blue decoration.
- `SHOP` and `+90` must remain fully readable in a 390x844 capture.

## Screenshot Acceptance

In `build/captures/iteration16/framebuffer_after_shop.png`, the next polish pass should make these readable without docs:

1. Top status is `11/67`.
2. Upgrade is done: `X10`.
3. Job card is the next/action loop: `SHOP +90`.
4. The stand has an attached customer/helper and coin bubbles, clearly tied to the reward.
