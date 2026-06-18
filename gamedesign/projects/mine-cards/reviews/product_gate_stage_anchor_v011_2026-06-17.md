---
type: ProductReadGate
project: mine-cards
task: T0001
surface: responsive
verdict: fail
timestamp: 2026-06-17T18:29:34.088Z
---

# Product Read Gate - mine-cards / responsive

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_stage_anchor_v011_landscape_surface.png`

## Player Read

- Where am I? Mine Cards mining screen with native 3D miner, node target, reward callout, and lower idle board.
- What should I do now? Watch the miner mine Surface Stone, then use the lower board to understand nodes and the Copper Pickaxe goal.
- What changed after input? v011 improves the actor-target-reward chain: the rock is in the tool contact area, the reward callout is in a lane beside the mined target, and landscape subtitle no longer crosses the actor.
- What is the reward / why continue? Stone, coins, XP, progress, node lock, and pickaxe cost are visible, but the reward loop still relies on text and placeholder panels.
- Why does this look like a game? It is closer to a native game screen than the previous technical proof, but still lacks authored stage art, mine floor, lighting, and hierarchy.

## State Coverage

Required states:
- first_screen_idle
- geode_event
- portrait_idle
- portrait_geode

Covered states:
- first_screen_idle: build/captures/mine_cards_stage_anchor_v011_landscape_surface.png
- geode_event: build/captures/mine_cards_stage_anchor_v011_landscape_geode.png
- portrait_idle: build/captures/mine_cards_stage_anchor_v011_portrait_surface.png
- portrait_geode: build/captures/mine_cards_stage_anchor_v011_portrait_geode.png

Not covered / debt:
- (none)

## Review

Problem: v011 fixes the most obvious actor/reward placement bugs, but the screen still fails the product bar because the stage has no authored mine art and the lower mechanics board has weak hierarchy.

Next: Generate or accept the stage background/floor/reward FX source sheet, integrate the smallest stage-art proof, then rerun landscape and portrait product gate before adding mechanics.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 3
- readability: 4
- ui_controls: 3
- action_direction: 3
- art_quality: 2
- audience_fit: 3

Issues:
- major / composition: The top stage still reads as an empty framed panel; it needs mine floor/background/light and stronger stage ownership.
- major / action_direction: Actor, rock, and reward are now connected, but the hit moment still lacks impact FX and a proper staged pose.
- major / ui_controls: The lower board remains too same-weight: active Mining, future tabs, nodes, upgrade, and nav all use similar rectangular treatment.
- major / audience_fit: Portrait is captured and functional, but still feels like stacked desktop panels rather than a phone-first idle RPG composition.
