# Iteration 5 Runtime Visual Direction

Status: designer handoff after Iteration 4 screenshots.  
Scope: runtime screen after asset integration. No runtime ids, code, CMake, state, or DevAPI changes in this pass.

## Screens Reviewed

- `build/captures/iteration4/wasm_qa_mobile_file_chrome.png`
- `build/captures/iteration4/wasm_qa_desktop_file_chrome.png`

## Verdict

Iteration 4 has the correct core fantasy: warm room/yard background, small comeback hero, and a big golden tap button read as Game 67. The screen stops feeling finished where UI falls back to flat debug rectangles: top bars are unclear, the blue/gray card stack has no component identity, bottom tabs look like layout blocks, and the 390x844 portrait screenshot clips the bottom content.

The next runtime pass should not repaint the background/hero/button first. Replace debug bars/cards/tabs with UI shells, then tune layout safe areas.

## Added Candidate UI Assets

All files are transparent PNG candidates in `gamedesing/assets/generated/ui/`. They contain no player-facing text or numbers; runtime must draw labels, prices, counts, and localization.

| File | Size | Runtime use |
| --- | ---: | --- |
| `ui_runtime_resource_pill_9s.png` | 320x96 | top-left/top-right coin or tap-power resource pill |
| `ui_runtime_progress_bar_9s.png` | 384x76 | compact top status/progress bar replacing the unclear cream strip |
| `ui_runtime_goal_card_9s.png` | 384x156 | next goal / first upgrade card replacing blue-gray debug cards |
| `ui_runtime_tab_selected_9s.png` | 160x88 | active bottom tab shell |
| `ui_runtime_tab_locked_9s.png` | 160x88 | inactive/locked bottom tab shell |

These are integration candidates, not packed runtime ids yet. If the implementation agent adds them to the pack, keep stable ids close to the filenames.

## Accept Visual Decisions

- Accept the current background direction: warm starter room opening into bright city/yard is strong and readable.
- Accept the current hero scale on desktop; on mobile it can be slightly smaller only to fix clipping.
- Accept the giant gold coin/tap button direction as the primary first action.
- Accept high contrast, toy-like black outlines on UI shells.
- Accept minimal copy in the first screen; icons plus runtime numbers should carry the first read.

## Reject Visual Decisions

- Reject flat blue, gray, purple, and cream rectangles as final UI, even as temporary web visuals.
- Reject any baked tiny numbers or labels inside generated PNGs.
- Reject top bars that do not explain what they track. Every top element must be either resource, status, or progress.
- Reject portrait layouts where bottom tabs or cards are cut off by the viewport.
- Reject stacking the hero, status badge, tap button, and cards so tightly that the hero feet or tap button silhouette overlap.

## Runtime Screen Direction

Visual hierarchy from strongest to weakest:

1. `PowerBadge` / status: `1/67` must be above or near the hero and readable before the player taps.
2. Hero: centered and unobscured; feet can overlap the button only if the button still reads as the largest tap target.
3. Main tap button: largest UI object, gold, bottom-middle, at least 72px high.
4. Next goal card: one cream/gold card shell, not two debug bars.
5. Resource pill(s): compact, top edge, clear icon + runtime number.
6. Bottom tabs: stable height, icon/label centered, never clipped.

Top HUD:

- Replace the left yellow block and right purple block with `ui_runtime_resource_pill_9s.png`.
- Use `icon_meme_coin_67.png` in the coin tray or as an overlaid icon.
- Top status/progress should use `ui_runtime_progress_bar_9s.png`; do not draw a full-width cream strip without a label/icon.
- Keep top HUD inside a 12px mobile safe margin.

Cards:

- Replace the blue and gray debug cards with a single `ui_runtime_goal_card_9s.png` stack area.
- First card content should be one action/goal: icon left, short runtime title, value/price right.
- If there are two rows, the selected/affordable row uses gold accent; locked row uses 60% opacity.

Bottom tabs:

- Replace magenta/blue blocks with `ui_runtime_tab_selected_9s.png` and `ui_runtime_tab_locked_9s.png`.
- Four tabs max in P0.
- Tab labels/icons must be centered inside the shell, not aligned to debug grid edges.

## Acceptance Checklist: 390x844

- Entire bottom tab row visible; no clipped pixels at the bottom edge.
- Last visible gameplay element above tabs has at least 8px vertical breathing room.
- Main tap button rendered height: 92-118px.
- Hero body visible from cap to shoes; button overlap may cover no more than lower shoes.
- Status `1/67` readable at first glance, minimum 30px high number glyphs.
- Top resource pill(s) fit inside x=12..378 and y=10..86.
- No flat debug rectangles remain visible: blue `#2b80ef`, gray `#808890`, magenta `#df2d92`, purple `#7138dc` blocks are not acceptable as UI surfaces.
- Background remains visible behind the hero; UI cannot cover more than 45% of the screen height before tabs.
- All runtime text is 13px or larger, with no negative letter spacing.
- Screenshot read in 2 seconds: "hero is 1/67, tap gold button, earn coin/next goal."

## Acceptance Checklist: 360x640

- Entire bottom tab row visible; minimum tab height 56px, no clipping.
- Top HUD consumes no more than 74px total height.
- Hero max height: 210px from cap to shoes.
- Main tap button rendered width: 276-328px; height: 84-104px.
- Next goal/card area is one compact card, max 116px high.
- Vertical order is stable: top HUD, status/hero, tap button, card, tabs.
- At least 6px gap between top HUD and status/hero; at least 6px gap between button and card.
- No element overlaps the browser/viewport bottom edge.
- Resource number and `1/67` remain readable after mobile scaling.
- If content cannot fit, hide secondary card details before shrinking the tap button below 84px.

## Next Visual Bugs

1. Portrait bottom clipping: fix layout safe area and tab/card heights before adding more content.
2. Top bars unclear: assign each bar a semantic asset role, icon, and runtime label/value.
3. Debug cards: replace blue/gray surfaces with a single cream/gold goal card component.
4. Debug tabs: replace magenta/blue blocks with selected/locked tab shells.
5. Status badge placement: current desktop badge is too close to the hero head; keep 8-14px gap.
