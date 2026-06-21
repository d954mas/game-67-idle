---
type: ProductReadGate
project: ember-road
task: T0018
surface: desktop
verdict: fail
timestamp: 2026-06-20T18:51:10.567Z
---

# Product Read Gate - ember-road / desktop

Verdict: **FAIL**

Screenshot: `build/captures/ember-road/state_modal_or_choice_open.png`

## Player Read

- Where am I? Lead-rejected gameplay screenshot under review.
- What should I do now? Stop feature expansion and resolve the rejected visual issue.
- What changed after input? A strict fail gate records the rejection before more implementation.
- What is the reward / why continue? Next pass must close this visual mismatch before acceptance.
- Why does this look like a game? The lead rejected the current game look, so it is not accepted yet.

## State Coverage

Required states:
- (none)

Covered states:
- (none)

Not covered / debt:
- (none)

## Review

Problem: Lead rejected the current Ember Road visual and UX as not matching the desired game; the Old Mine screen still reads as a decorative MMO frame over a non-playable choice, with SCOUT marked NEXT SLICE and no clear encounter, reward, route decision, or accepted reference grammar.

Next: Stop gameplay expansion, refresh the Reference Digest with closer RPG refs and current-screenshot mismatch, then build a new accepted direction/fake-shot target that preserves Y-up layout before runtime UI/art changes.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 2
- readability: 3
- ui_controls: 1
- action_direction: 1
- art_quality: 2
- audience_fit: 2

Issues:
- blocker / ui_controls: Lead rejected the current Ember Road visual and UX as not matching the desired game; the Old Mine screen still reads as a decorative MMO frame over a non-playable choice, with SCOUT marked NEXT SLICE and no clear encounter, reward, route decision, or accepted reference grammar.
