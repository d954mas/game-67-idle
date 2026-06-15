# First Slice Visual Gate

Project: `critter-corral` / Critter Corral

Fill this before broad runtime or content expansion. This is a stop/go artifact,
not a notes dump.

## Session Contract

- Goal:
- Non-goal:
- Proof:
- Stop condition:
- Likely files:

## Target

- Fake shot / visual target path:
- Reference digest path, if any:
- Art bible / style target path, if any:

## Current Native Proof

- Native build/run command:
- Current native screenshot path or capture plan:
- Screenshot-vs-target mismatch list:
  - [ ] First-screen composition:
  - [ ] Main action readability:
  - [ ] UI text/readability:
  - [ ] Visual style/appeal:
  - [ ] Performance or capture blocker:

## Visual Critic Packet

- Packet command:
  ```powershell
  node tools/ai.mjs critic --project critter-corral --task <task-id> --surface desktop --screenshot <native-screenshot.png> --target <fake-shot-or-target-path> --brief "<casual audience, core action, target style>" --output gamedesign/projects/critter-corral/reviews/first_slice_visual_critic_packet.md --json-output gamedesign/projects/critter-corral/reviews/first_slice_visual_critic_packet.json
  ```
- Packet Markdown path: `gamedesign/projects/critter-corral/reviews/first_slice_visual_critic_packet.md`
- Packet JSON path: `gamedesign/projects/critter-corral/reviews/first_slice_visual_critic_packet.json`
- Use this packet for a self-review or separate critic pass before writing the strict product gate verdict.

## Product-Read Gate

- Gate command:
  ```powershell
  node tools/ai.mjs gate --project critter-corral --task <task-id> --surface desktop --screenshot <native-screenshot.png> --verdict fail --strict --visual-strict --where "<where am I?>" --action "<what can I do?>" --response "<what changed?>" --reward "<why continue?>" --game-look "<why game?>" --problem "<specific visual/player-read problem>" --next "<smallest next visual fix>" --visual-score composition=1 --visual-score readability=1 --visual-score ui_controls=1 --visual-score action_direction=1 --visual-score art_quality=1 --visual-score audience_fit=1 --visual-issue blocker:readability:"<concrete issue>"
  ```
- Gate artifact path:
- Verdict: pending
- Blocking player-read questions:
  - [ ] What can the player do in the first 5 seconds?
  - [ ] What is the reward/progress feedback?
  - [ ] What looks unclear, ugly, unreadable, or unlike the target?
- Strict visual rubric:
  - [ ] composition score 1-5
  - [ ] readability score 1-5
  - [ ] ui_controls score 1-5
  - [ ] action_direction score 1-5
  - [ ] art_quality score 1-5
  - [ ] audience_fit score 1-5
  - [ ] visual issues use severity `blocker`, `major`, or `minor`
  - [ ] pass requires all six scores >= 4 and no blocker/major issue

## Expansion Decision

- Decision: blocked until filled
- If blocked, smallest next fix:
- If passed, exact content/system expansion allowed next:
