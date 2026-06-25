---
type: Knowledge
title: Game Art Contract And Visual Gate
description: Reusable per-game taste anchor plus the three-way visual gate flow that separates universal objective checks from game-specific taste.
tags: [visual, art-direction, product-gate, visual-gate, validation, reusable]
timestamp: 2026-06-20T00:00:00Z
---

# Game Art Contract And Visual Gate

Use this when a game needs its screens judged for art quality, not just for
broken/blank captures. It explains the reusable layering that keeps universal
checks game-agnostic while a per-game **art contract** carries the taste.

The lesson this fixes is recorded in pipeline history: automation went green but
the lead still rejected the screen. The cause was not a missing check, it was a
mismatch: the gate validated hand-typed numbers and never looked at the screen,
and there was no machine-readable anchor for *this game's* taste.

## The Layering

Keep taste out of the universal checks. Five stages, in order:

```text
Universal Visual Gate -> Game Art Contract -> Visual Critic(s) -> Art Lead Judge -> Human Review Queue
```

1. **Universal Visual Gate** - game-agnostic objective checks that are the same
   for every game: screenshot is non-empty / non-flat / non-debug, text is
   readable, UI contrast is adequate, UI stays in bounds, key controls do not
   overlap, touch targets are large enough, the primary action is reachable, the
   background does not fight the UI, and **key states are covered, not one pretty
   shot**. These catch objective garbage. They never encode a game's style.
2. **Game Art Contract** - the per-game taste file (`art_contract.json`, below).
   It is the machine form of the [Visual Direction](visual_direction.md) Style
   Brief Checklist plus reference banks and a state matrix. This is what makes a
   critic judge like *your* art lead instead of generic internet aesthetics.
3. **Visual Critic(s)** - a pass that actually LOOKS at the screenshots with the
   contract and reference images in context and scores the six universal axes.
   Start with one combined art-lead critic; split into UI / Scene / Full-Frame
   critics only if the single critic proves too coarse in practice.
4. **Art Lead Judge** - distills the findings into the single biggest visual
   debt and ONE high-impact next fix, not a cosmetics list. In the schema this
   is the required `smallest_next_fix` field.
5. **Human Review Queue** - doubtful cases (`review` verdict) go to the lead.
   The AI does not give itself the final grade; independent critics can disagree,
   and disagreement routes to review and feeds the approved/rejected banks.

Verdict is three-way `pass / review / fail`, never just `pass / fail`. `review`
is the lane for "looked, not confident" - the lead arbitrates.

## What Stays Universal vs What Is Taste

The six visual axes are universal quality dimensions and stay fixed in the gate:
`composition`, `readability`, `ui_controls`, `action_direction`, `art_quality`,
`audience_fit`. The art contract does **not** redefine them. The contract supplies
the taste the critic reasons with (audience, genre, references, palette, forbidden
solutions, what reads as cheap vs expensive) plus an optional `pass_threshold`
override. Subtract, do not add: this reuses the existing product gate instead of a
parallel scoring stack.

## Art Contract File

Lives at `gamedesign/projects/<game-id>/art/art_contract.json`. The gate
auto-resolves it from `--project`, or take it explicitly with `--contract`.

```json
{
  "schema": "game.art_contract",
  "version": 1,
  "game_id": "<game-id>",
  "audience": "who should find this instantly appealing",
  "genre": "one-line genre and platform",
  "pass_threshold": 4,
  "taste": {
    "fantasy": "what role/transformation the player should feel",
    "shape_language": "soft, chunky, toy-like, sharp, realistic, miniature, ...",
    "materials": ["plastic", "stone", "cloth"],
    "juiciness": "saturation/contrast target, e.g. saturated accents on calm base",
    "scale_rule": "how large characters, rewards, and buttons read on screen",
    "silhouette_rule": "how each object reads in one color",
    "feedback_rule": "how tap, reward, unlock, error, completion look"
  },
  "palette": {
    "primary": "", "secondary": "", "accent": "",
    "warning": "", "success": "", "locked": "", "background": ""
  },
  "ui_style": { "controls": "", "text": "", "hierarchy": "" },
  "scene_style": { "camera": "", "world": "", "fg_bg_separation": "" },
  "references": {
    "yes": [{ "ref": "", "why": "" }],
    "no":  [{ "ref": "", "why": "" }],
    "approved_dir": "gamedesign/projects/<game-id>/art/approved",
    "rejected_dir": "gamedesign/projects/<game-id>/art/rejected"
  },
  "forbidden": ["debug wireframes / visible grid", "thin grey low-contrast UI"],
  "cheap_signals": ["placeholder/prototype look", "plastic cube silhouettes"],
  "expensive_signals": ["authored silhouettes", "material separation"],
  "state_matrix": "gamedesign/projects/<game-id>/art/state_matrix.json"
}
```

The `approved/` and `rejected/` directories hold a small bank of real reference
crops. They are fed directly into the critic prompt **in context** - there is no
embedding store, vector DB, or fine-tuning. Rejected frames come straight from
documented lead rejections.

## Critic Output Contract

The visual critic writes a `game.visual_critique` JSON that the product gate
ingests with `--critique`, so the scores come from a critic that looked at the
screen, not from hand-typed CLI flags:

```json
{
  "schema": "game.visual_critique",
  "verdict": "pass | review | fail",
  "scores": {
    "composition": 4, "readability": 4, "ui_controls": 4,
    "action_direction": 4, "art_quality": 4, "audience_fit": 4
  },
  "issues": [
    { "severity": "blocker|major|minor", "axis": "readability", "text": "concrete evidence" }
  ],
  "answers": {
    "where": "", "action": "", "response": "", "reward": "", "game_look": ""
  },
  "smallest_next_fix": "the single highest-impact fix before any new content"
}
```

Explicit CLI values still win, so a lead can override any field. A critique
implies the strict visual rubric (all six axes required).

## Running The Critic

Produce the `game.visual_critique` with the vision art-lead critic (critic pass
plus an independent refute pass; disagreement becomes `review`). Verified on this
box with the codex CLI (gpt-5.5 vision): the instruction is piped on stdin and the
screenshots are attached with `-i`. `{IMAGES}` expands to the screenshot paths.

```powershell
node tools/product_gate/visual_critic_run.mjs --project <game-id> `
  --shot first_screen:build/captures/state_first_screen.png `
  --shot combat:build/captures/state_combat.png `
  --out gamedesign/projects/<game-id>/art/latest_critique.json `
  --model-cmd "codex exec --dangerously-bypass-approvals-and-sandbox --skip-git-repo-check -i {IMAGES} -"
```

Omit `--model-cmd` for emit mode: it writes the critic instruction so a human or
another agent can run any vision model manually and save the JSON.

## Running The Gate

```powershell
node tools/product_gate/review.mjs `
  --project <game-id> `
  --task <task-id> `
  --surface desktop `
  --screenshot build/captures/state_first_screen.png `
  --critique gamedesign/projects/<game-id>/art/latest_critique.json `
  --state-matrix gamedesign/projects/<game-id>/art/state_matrix.json `
  --covered-state first_screen:build/captures/state_first_screen.png
```

The contract is auto-resolved from `--project`. A `pass` still requires the
player-read answers (the critique can supply them); a `review` routes to the
lead; `close_slice.mjs` blocks a `review` gate in strict mode and is advisory
otherwise.

## What A New Game Provides

Almost everything is universal and already built. A new game plugs in exactly
**four things**, then runs two commands. The schema above and the
`capture_states.py` template below are the references to copy; the universal
half is exercised by `tools/devapi/state_capture_test.py`.

| You provide | Where | What it is |
|---|---|---|
| `art_contract.json` | `gamedesign/projects/<id>/art/` | the taste (data); fill from the Visual Direction Style Brief Checklist using the schema above |
| `approved/` + `rejected/` | `gamedesign/projects/<id>/art/` | a few real target-reference PNGs and a few lead-rejected frames |
| `capture_states.py` | `tools/<id>/` | the route (code): drive the game and `capture(tag)` at each key state (see the template below) |
| required states | `art_contract.json` `state_matrix` + `.require(...)` in the script | which states matter for this game's loop (adapt the categories in [Live-State UI Acceptance Matrix](live_state_acceptance_matrix.md)) |

The art lane, palette, references, and forbidden solutions are **each game's own
choice** — the pipeline stays neutral and never imposes a house style. It only
checks a screen against the taste that game declared for itself.

Universal — do **not** rewrite per game: the six axes, the product gate, the
visual critic + refute pass, `StateCapture` (`tools/devapi/state_capture.py`), and
the `game.live_state_acceptance_matrix` + `game.visual_critique` schemas.

### Template: `tools/<id>/capture_states.py`

The custom half is small: drive the game with its own DevAPI commands, call
`capture(tag)` at each state, write the matrix. The universal `StateCapture`
helper owns capture, coverage, pixel-health, and the matrix schema.

```python
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "devapi"))
from devapi_client import running_game
from state_capture import StateCapture

with running_game(fresh_state=True) as game:
    sc = StateCapture(game, "<game-id>").require("first_screen", "primary_action_ready", "reward_active")
    sc.capture("first_screen")
    game.result("game.playtest.<your-command>")      # drive to the next state (game-specific)
    sc.capture("primary_action_ready")
    # ... capture each required state; sc.mark_debt(tag, reason) for ones this slice skips ...
    sc.write_matrix("gamedesign/projects/<game-id>/art/state_matrix.json")
```

### Then two universal commands

```powershell
py -3.12 tools/<id>/capture_states.py             # per-state PNGs + state_matrix.json
node tools/product_gate/visual_critic_run.mjs --project <id> `
  --state-matrix gamedesign/projects/<id>/art/state_matrix.json `
  --model-cmd "codex exec --dangerously-bypass-approvals-and-sandbox --skip-git-repo-check -i {IMAGES} -"
# then: node tools/product_gate/review.mjs --project <id> --screenshot <one-state>.png --critique <critique.json>
```

## Links

- [Visual Direction](visual_direction.md) - the Style Brief Checklist this
  contract makes machine-readable.
- [Live-State UI Acceptance Matrix](live_state_acceptance_matrix.md) - key states
  to cover before a broad visual pass.
- [Asset Semantic And Style Gate](asset_semantic_style_gate.md) - the
  style-group contract for "assets do not conflict by style".
- [Accessibility](accessibility.md) - contrast, text size, target size baselines
  for the universal readability checks.
