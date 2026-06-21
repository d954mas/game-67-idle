---
type: ProductReadGate
project: ember-road
task: T0018
surface: direction-fakeshot
verdict: review
timestamp: 2026-06-20T18:58:07.534Z
---

# Product Read Gate - ember-road / direction-fakeshot

Verdict: **REVIEW**

Screenshot: `gamedesign/projects/ember-road/art/ember-road-old-mine-scout-result-direction-v001.png`

## Player Read

- Where am I? Old Mine scout/result direction target: dominant cave entrance scene with integrated RPG chrome.
- What should I do now? Use this as the next visual/UX target only if the lead accepts the scene-first route/scout grammar.
- What changed after input? The target replaces NEXT SLICE scaffolding with visible route, threat, resource, reward, and log surfaces.
- What is the reward / why continue? The player can understand why scouting matters: cave threat, ember shard resource, depth marker, XP/gold/ring reward preview.
- Why does this look like a game? This reads closer to a fantasy browser RPG target than the rejected runtime screenshot, but it is a fake shot and not runtime-ready.

## State Coverage

Required states:
- (none)

Covered states:
- (none)

Not covered / debt:
- (none)

## Review

Problem: Needs lead acceptance before runtime UI/art/code changes; generated UI pieces are not sliced or reusable yet.

Next: If accepted, derive runtime layout/assets from this target while preserving Y-up layout semantics; if rejected, update the digest before coding.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 4
- readability: 4
- ui_controls: 4
- action_direction: 4
- art_quality: 4
- audience_fit: 4

Issues:
- minor / art_quality: Direction fake shot is not sliced runtime UI and cannot be used as final packed UI art.
