# First Slice Visual Gate

Project: `cozy-automation` / Cozy Automation

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
  node tools/ai.mjs critic --project cozy-automation --task <task-id> --surface desktop --screenshot <native-screenshot.png> --target <fake-shot-or-target-path> --brief "<casual audience, core action, target style>" --output gamedesign/projects/cozy-automation/reviews/first_slice_visual_critic_packet.md --json-output gamedesign/projects/cozy-automation/reviews/first_slice_visual_critic_packet.json
  ```
- Packet Markdown path: `gamedesign/projects/cozy-automation/reviews/first_slice_visual_critic_packet.md`
- Packet JSON path: `gamedesign/projects/cozy-automation/reviews/first_slice_visual_critic_packet.json`
- Use this packet for a self-review or separate critic pass before writing the strict product gate verdict.

## Live-State Matrix

- Matrix doc: `gamedesign/projects/cozy-automation/visual/live_state_acceptance_matrix.md`
- Matrix JSON: `gamedesign/projects/cozy-automation/visual/live_state_acceptance_matrix.json`
- Required first proof states:
  - [ ] `first_screen`
  - [ ] `hud_visible`
  - [ ] `primary_action_ready`
  - [ ] `primary_action_feedback`
  - [ ] `reward_active`
  - [ ] `locked_or_disabled_state`
  - [ ] `transient_stress_state`
- Any required state not captured by the current screenshot must be passed as
  `--not-covered-state <tag>:"<reason>"`, not silently implied.

## Product-Read Gate

- Gate command:
  ```powershell
  node tools/ai.mjs gate --project cozy-automation --task <task-id> --surface desktop --screenshot <native-screenshot.png> --verdict fail --strict --visual-strict --state-matrix gamedesign/projects/cozy-automation/visual/live_state_acceptance_matrix.json --require-state first_screen --covered-state first_screen:<native-screenshot-or-probe> --covered-state hud_visible:<hud-zoom-or-screenshot> --covered-state primary_action_ready:<native-screenshot-or-probe> --not-covered-state modal_or_choice_open:"not in this first slice yet" --not-covered-state resume_or_reentry_state:"not in this first slice yet" --where "<where am I?>" --action "<what can I do?>" --response "<what changed?>" --reward "<why continue?>" --game-look "<why game?>" --problem "<specific visual/player-read problem>" --next "<smallest next visual fix>" --visual-score composition=1 --visual-score readability=1 --visual-score ui_controls=1 --visual-score action_direction=1 --visual-score art_quality=1 --visual-score audience_fit=1 --visual-issue blocker:readability:"<concrete issue>"
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
