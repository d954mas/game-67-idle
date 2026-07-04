# Top HUD V1 Art Brief

Date: 2026-07-04

Status: artlead candidate, pending subagent review.

## Direction

V1 uses compact rugged tokens over the existing top fade instead of a heavy top
panel.

Components:

- right player status cluster with portrait socket, HP bar, XP bar, coin chip,
  and supplies chip;
- center location plaque for runtime location text;
- small settings gear button;
- standalone coin chip;
- standalone supplies chip;
- tiny level badge.

## Why This Candidate Works

- Keeps the left side clear for Poki.
- Matches the dark metal/leather/brass material language of the v11 bottom nav.
- Avoids premium currency, mail, social, clan, and MMO clutter.
- Avoids a full-width panel and keeps the background scene dominant.
- Leaves text empty for runtime rendering.

## Review Risks

- The player cluster may need aggressive scaling in the 960x540 layout.
- The component sheet still needs slicing/alpha cleanup before runtime use.
- Lock/badge/resource state mapping should stay runtime-owned.

## Source

- Sheet:
  `games/rb-dark-rpg/assets/ui/generated/top_hud_tokens_01/top_hud_component_sheet_01.png`
- Provenance:
  `games/rb-dark-rpg/assets/ui/generated/top_hud_tokens_01/provenance.md`
