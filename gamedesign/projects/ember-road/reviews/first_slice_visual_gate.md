# First Slice Visual Gate

Project: `ember-road` / Ember Road

Fill this before broad runtime or content expansion. This is a stop/go artifact,
not a notes dump.

## Session Contract

- Goal: prove a beautiful fantasy RPG first screen and loop: town quest,
  map node, automated battle entry/result, loot, XP, and item equip choice.
- Non-goal: no broad world, party system, PvP, guilds, crafting, market,
  second town, or copied reference UI/assets.
- Proof: accepted fake shot or direction board, then native screenshot/product
  gate with state-matrix coverage for first screen, HUD, primary action,
  feedback, reward, locked state, and transient battle feedback.
- Stop condition: lead rejects the visual direction as ugly/not fantasy RPG/not
  like the target, reference digest remains incomplete for ref-driven final
  art, or strict visual product gate has any blocker/major issue.
- Likely files: `gamedesign/projects/ember-road/gdd.md`,
  `gamedesign/projects/ember-road/data/*.json`,
  `gamedesign/projects/ember-road/references/legend_legacy_dragons_digest.md`,
  `gamedesign/projects/ember-road/art/`, `src/`.

## Target

- Fake shot / visual target path:
  `gamedesign/projects/ember-road/art/ember-road-old-gate-fakeshot-v001.png`;
  direction target only, not a fused runtime UI asset.
- Reference digest path, if any:
  `gamedesign/projects/ember-road/references/legend_legacy_dragons_digest.md`
- Adjacent UX reference set:
  `gamedesign/projects/ember-road/references/fantasy_browser_rpg_ux_reference_set.md`
- Central deconstruction after lead rejection:
  `gamedesign/projects/ember-road/references/fantasy_browser_rpg_central_deconstruction.md`
- Art job:
  `gamedesign/projects/ember-road/art_requests/ember-road-old-gate-fakeshot-v001.json`
- Source-family coverage:
  `gamedesign/projects/ember-road/reviews/ember-road-old-gate-fakeshot-v001-source_family_coverage_audit.md`
  is PASS for fake shot, background, character/enemy, icon, and UI-frame source
  families.
- Runtime crop manifest:
  `gamedesign/projects/ember-road/data/ember-road-old-gate-fakeshot-v001-crop_manifest.json`
  exists and slices 57 project-local PNG assets into
  `assets/runtime/ember-road-old-gate-fakeshot-v001/`.
- Runtime asset manifest:
  `gamedesign/projects/ember-road/data/ember-road-old-gate-fakeshot-v001-asset_manifest.json`.
- Slice9 design policy audit:
  `gamedesign/projects/ember-road/reviews/ember-road-old-gate-fakeshot-v001-slice9_design_policy_audit.md`
  is PASS for route strips, destination plaques, HUD/quest panels, reward
  slots, buttons, and reusable frame pieces.
- Runtime atlas:
  `ember_road_old_gate_atlas` is packed into `ember_road_base.ntpack` and
  rendered by the native first screen.
- Art bible / style target path, if any:

## Lead Rejection Reset

- Date: 2026-06-20.
- Rejection: current visual is not like the desired game; even the UX is wrong.
- Current mismatch artifact: `build/captures/iterate.png`.
- Consequence: the existing screen is not an acceptable base for visual polish.
  Do not continue by only generating blank UI kit pieces or ornate frames.
- Required next target: a composed fantasy browser-RPG fake shot/direction
  board that proves the first-screen UX: Old Gate as one illustrated place,
  hero, Gate Warden/NPC quest focus, integrated map route, locked Old Mine
  reason, reward preview, and auto-battle preview/result surface.
- Current result: direction fake shot, accepted source families, crop manifest,
  runtime PNG outputs, atlas pack integration, and native rendering exist.
  Updated product gate is PASS for the first visual/native slice.

## Current Native Proof

- Native build/run command:
- Current native screenshot path or capture plan:
  `build/captures/iterate.png`; asset-backed first-screen proof after replacing
  detached town/map panels and debug-shape art with one illustrated Old Gate
  hub, quest rail, route strip, locked Old Mine hint, Road Wolf preview, reward
  promise, and fantasy UI frames.
- Previous rejected screenshot:
  `build/captures/iterate.png`; wrong visual and UX target.
- Latest gate:
  `gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_live_states_review.md`
  is PASS for the current atlas-backed native proof plus live-state matrix.
  It unlocks only the next narrow progression slice, not broad content.
  The previous strict FAIL is
  `gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_ux_reset.md`.
- Screenshot-vs-target mismatch list:
  - [x] Separated town panel plus detached map panel was replaced by one
        town-hub composition with integrated quest rail and route strip.
  - [x] Text/button-only quest flow was replaced by NPC/quest focus with
        reward preview and primary action in one connected rail.
  - [x] Abstract detached map panel was replaced by a route/travel strip tied
        to Old Gate, North Road, and locked Old Mine.
  - [x] Current hero is a small block and stats line; target needs hero
        identity, status, equipment/reward promise, and readable RPG feedback.
  - [x] First screen now promises automated battle through a Road Wolf preview
        and XP/gold/ring reward text.
  - [x] Fake shot target now shows a full browser-RPG frame, painted Old Gate,
        hero, Gate Warden, route strip, wolf preview, reward icons, and bottom
        log/action belt.
  - [x] Fake shot has been decomposed into project-local source families
        instead of being used as one fused runtime image.
  - [x] Runtime PNG crops exist for Old Gate, North Road, hero, Gate Warden,
        Road Wolf, route/reward icons, UI panels, buttons, and overlays.
  - [x] Native runtime now renders packed atlas assets for location, hero,
        Gate Warden, wolf, reward/icon previews, route plaques, quest rail,
        HUD, and buttons.
  - [x] Slice9 metadata exists and the slice9 design policy audit passes for
        reusable panels/buttons.
  - [x] Updated product gate exists for the atlas-backed proof and live-state
        matrix.
  - [x] Additional live-state screenshots exist for reward-active,
        locked/disabled, and transient feedback coverage.
  - [x] Route plaques, reward controls, and quest-panel secondary text reached
        first-slice readable quality in the PASS gate.
  - [ ] Final-release art cleanup remains minor debt; some generated cutout
        edges are still prototype-quality.

## Visual Critic Packet

- Packet command:
  ```powershell
  node tools/ai.mjs critic --project ember-road --task <task-id> --surface desktop --screenshot <native-screenshot.png> --target <fake-shot-or-target-path> --brief "<casual audience, core action, target style>" --output gamedesign/projects/ember-road/reviews/first_slice_visual_critic_packet.md --json-output gamedesign/projects/ember-road/reviews/first_slice_visual_critic_packet.json
  ```
- Packet Markdown path: `gamedesign/projects/ember-road/reviews/first_slice_visual_critic_packet.md`
- Packet JSON path: `gamedesign/projects/ember-road/reviews/first_slice_visual_critic_packet.json`
- Use this packet for a self-review or separate critic pass before writing the strict product gate verdict.

## Live-State Matrix

- Matrix doc: `gamedesign/projects/ember-road/visual/live_state_acceptance_matrix.md`
- Matrix JSON: `gamedesign/projects/ember-road/visual/live_state_acceptance_matrix.json`
- Live capture matrix:
  `gamedesign/projects/ember-road/visual/live_state_capture_matrix.json`
- Live capture report:
  `gamedesign/projects/ember-road/reviews/live_state_capture_report.json`
- Required first proof states:
  - [x] `first_screen`:
        `build/captures/ember-road/state_first_screen.png`
  - [x] `hud_visible`:
        `build/captures/ember-road/state_hud_visible.png`
  - [x] `primary_action_ready`:
        `build/captures/ember-road/state_primary_action_ready.png`
  - [x] `primary_action_feedback`:
        `build/captures/ember-road/state_primary_action_feedback.png`
  - [x] `reward_active`:
        `build/captures/ember-road/state_reward_active.png`
  - [x] `locked_or_disabled_state`:
        `build/captures/ember-road/state_locked_or_disabled_state.png`
  - [x] `transient_stress_state`:
        `build/captures/ember-road/state_transient_stress_state.png`
  - [ ] `progression_panel_open`: explicit debt; inline reward/equip only in
        the first slice.
  - [ ] `modal_or_choice_open`: explicit debt; no modal/choice yet.
  - [ ] `resume_or_reentry_state`: explicit debt; out of scope for this slice.
- Any required state not captured by the current screenshot must be passed as
  `--not-covered-state <tag>:"<reason>"`, not silently implied.

## Product-Read Gate

- Gate command:
  ```powershell
  node tools/ai.mjs gate --project ember-road --task <task-id> --surface desktop --screenshot <native-screenshot.png> --verdict fail --strict --visual-strict --state-matrix gamedesign/projects/ember-road/visual/live_state_acceptance_matrix.json --require-state first_screen --covered-state first_screen:<native-screenshot-or-probe> --covered-state hud_visible:<hud-zoom-or-screenshot> --covered-state primary_action_ready:<native-screenshot-or-probe> --not-covered-state modal_or_choice_open:"not in this first slice yet" --not-covered-state resume_or_reentry_state:"not in this first slice yet" --where "<where am I?>" --action "<what can I do?>" --response "<what changed?>" --reward "<why continue?>" --game-look "<why game?>" --problem "<specific visual/player-read problem>" --next "<smallest next visual fix>" --visual-score composition=1 --visual-score readability=1 --visual-score ui_controls=1 --visual-score action_direction=1 --visual-score art_quality=1 --visual-score audience_fit=1 --visual-issue blocker:readability:"<concrete issue>"
  ```
- Gate artifact path:
  `gamedesign/projects/ember-road/reviews/T0014_first_slice_product_gate_live_states_review.md`
- Verdict: PASS for the first visual/native slice; broad expansion is still
  blocked by normal slice discipline. Next allowed work is T0015 progression
  panel/return reward state.
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

- Decision: T0014 first visual/native slice passed. Fake shot,
  source-family coverage, runtime PNG crops, slice9 metadata, atlas pack,
  first-screen native proof, and live-state captures now exist.
- If continuing, exact next expansion allowed: T0015 progression panel/return
  reward state for the existing Old Gate -> North Road loop only.
- Still blocked: broad locations, enemy families, economy, party, crafting, or
  other systems until the T0015 proof gate is recorded.
