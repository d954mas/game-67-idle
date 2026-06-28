---
type: Knowledge
title: Game Art Contract
description: Reusable per-game taste anchor for art direction fit review.
tags: [visual, art-direction, quality, validation, reusable]
timestamp: 2026-06-20T00:00:00Z
---

# Game Art Contract

Use this when a game needs screenshots judged for art direction fit, not only for
blank/broken captures.

The contract is a project-specific taste anchor for `QART_001`. It keeps generic
player clarity checks separate from the question "does this look right for this game?"

## Layering

```text
QCLR common/player-clarity checks
  -> game art contract
  -> QART_001 art direction fit review
  -> lead review when uncertain
```

`QCLR` checks catch objective player-facing clarity problems. The art contract carries
game-specific taste: audience, fantasy, references, palette, materials, and
what should count as cheap or expensive for this particular game.

## Art Contract File

Recommended path:

```text
gamedesign/projects/<game-id>/art/art_contract.json
```

Starter shape:

```json
{
  "schema": "game.art_contract",
  "version": 1,
  "game_id": "<game-id>",
  "audience": "who should find this instantly appealing",
  "genre": "one-line genre and platform",
  "taste": {
    "fantasy": "what role/transformation the player should feel",
    "shape_language": "soft, chunky, toy-like, sharp, realistic, miniature, ...",
    "materials": ["plastic", "stone", "cloth"],
    "juiciness": "saturation/contrast target",
    "scale_rule": "how characters, rewards, and buttons read on screen",
    "silhouette_rule": "how each object reads in one color",
    "feedback_rule": "how tap, reward, unlock, error, completion look"
  },
  "palette": {
    "primary": "",
    "secondary": "",
    "accent": "",
    "warning": "",
    "success": "",
    "locked": "",
    "background": ""
  },
  "ui_style": { "controls": "", "text": "", "hierarchy": "" },
  "scene_style": { "camera": "", "world": "", "fg_bg_separation": "" },
  "references": {
    "yes": [{ "ref": "", "why": "" }],
    "no": [{ "ref": "", "why": "" }],
    "approved_dir": "gamedesign/projects/<game-id>/art/approved",
    "rejected_dir": "gamedesign/projects/<game-id>/art/rejected"
  },
  "forbidden": ["debug wireframes / visible grid", "thin grey low-contrast UI"],
  "cheap_signals": ["placeholder/prototype look", "plastic cube silhouettes"],
  "expensive_signals": ["authored silhouettes", "material separation"],
  "state_matrix": "gamedesign/projects/<game-id>/visual/live_state_acceptance_matrix.json"
}
```

The `approved/` and `rejected/` folders are small reference banks. Keep them
curated and explain why each reference belongs there.

## Review Axes

Use these stable axes in `QART_001` notes when a structured review helps:

- `composition`
- `readability`
- `ui_controls`
- `action_direction`
- `art_direction_fit`
- `audience_fit`

Do not treat scores as proof by themselves. The review must cite screenshots,
references, and the specific mismatch or reason to continue.

## New Game Checklist

A new game provides:

- `art_contract.json`;
- a few approved/rejected reference images;
- current runtime screenshots;
- a live-state matrix when more than one screen/state matters.

The AI agent applies `QART_001` against this evidence. If the result is
uncertain or contested, route it to lead review instead of inventing certainty.

## Links

- [Visual Direction](visual_direction.md) - the style brief checklist this
  contract makes concrete.
- [Live-State UI Acceptance Matrix](live_state_acceptance_matrix.md) - key states
  to cover before a broad visual pass.
- [Asset Semantic And Style Gate](asset_semantic_style_gate.md) - reject
  generated assets that cut cleanly but mean the wrong thing or mix icon styles.
- [Accessibility](accessibility.md) - contrast, text size, and target size
  baselines for readability checks.
