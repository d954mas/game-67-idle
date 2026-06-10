# Iteration 7 Icon/Number First 30s UI Handoff

Status: designer handoff for RC visual implementation.  
Scope: first 30 seconds only. Owns this `iteration7_*` doc; no `src/` or engine edits.

## Goal

Remove rough visible word labels from the first loop and make the UI readable through icons, numbers, state color, and motion.

Target read on a fresh screen:

`tap big 67 -> reach 5 coins -> buy tap upgrade -> run job -> claim 8 coins`

Runtime values for this handoff are the current packed/runtime first-loop values:

- first upgrade cost: `5`
- first upgrade result: status `2/67`, tap power `x2`
- first job duration: `6s`
- first job reward: `8`

## Asset Set To Use

Use what is already packed or present in the generated runtime asset set. Do not request new text-baked PNGs for RC.

| Role | Asset |
| --- | --- |
| Coin icon | `gamedesing/assets/generated/ui/icon_meme_coin_67.png` or packed `icon_meme_coin_67` |
| Tap hand icon | `gamedesing/assets/generated/ui/icon_tap_hand_67.png` or packed `icon_tap_hand_67` |
| Upgrade art | `gamedesing/assets/generated/ui/card_upgrade_tap.png` or packed `card_upgrade_tap` |
| Job art | `gamedesing/assets/generated/ui/card_job_kiosk.png` or packed `card_job_kiosk` |
| Goal card shell | `gamedesing/assets/generated/ui/ui_runtime_goal_card_9s.png` or packed shell |
| Progress frame | `gamedesing/assets/generated/ui/ui_runtime_progress_bar_9s.png` or toy bar fallback |
| Resource pill | `gamedesing/assets/generated/ui/ui_runtime_resource_pill_9s.png` |
| Selected tab shell | `gamedesing/assets/generated/ui/ui_runtime_tab_selected_9s.png` |
| Locked tab shell | `gamedesing/assets/generated/ui/ui_runtime_tab_locked_9s.png` |
| Home tab icon | `gamedesing/assets/generated/ui/tab_home.png` or `gamedesing/assets/icons/tab_home.png` |
| City tab icon | `gamedesing/assets/generated/ui/tab_city.png` or `gamedesing/assets/icons/tab_city.png` |
| Other tab icons when needed | `gamedesing/assets/icons/tab_deals.png`, `gamedesing/assets/icons/tab_upgrades.png` |
| Lock symbol | `gamedesing/assets/icons/locked.png` |
| New/ready symbol | `gamedesing/assets/icons/new_unlocked.png` |

## Remove These Visible Words

Replace runtime text labels with the states below:

| Current rough word | RC replacement |
| --- | --- |
| `TAP TO 5` | tap-hand icon + coin icon + progress/count `0..5 / 5`, no words |
| `BUY TAP` | tap-hand icon + coin icon + big `5`, gold/green affordable state |
| `TAP X2` | tap-hand icon + `2` + small `67`, completed/owned state |
| `OWNED` | check/new-unlocked symbol or quiet completed sparkle, no words |
| `COST` | coin icon + number only |
| `JOB LOCKED` | lock icon + dim `8` coin reward + dependency arrow from upgrade card |
| `START JOB` | job/kiosk art + `8` coin reward, tappable bright state |
| `JOB RUNS` | progress bar + `8` coin reward held visible |
| `CLAIM JOB` | pulsing `8` coin reward + new-unlocked/check symbol |
| `HOME` | home icon centered in selected tab shell |
| `CITY` | city icon centered in locked/inactive tab shell |

Words may remain in DevAPI/accessibility labels. They are rejected only as player-visible first-30s UI.

## Card State Spec

All card states use the same basic geometry: icon/art left, primary number right, state accent on the outer edge. Numbers must be larger than decorative icons and readable at `390x844`.

### Fresh: 0 Coins, No Upgrade

Upgrade card:

- Surface: `ui_runtime_goal_card_9s` or `card_upgrade_tap` at normal brightness.
- Left: tap-hand icon.
- Right: coin icon + big `5`.
- State accent: neutral cream/gold, no pulse.
- Meaning: first target exists, but player needs coins.

Job card:

- Surface: same card shell at `40..50%` opacity.
- Left: lock icon over or beside kiosk/job art.
- Right: dim coin icon + big `8`.
- State accent: gray/blue low-saturation overlay only if the card shell remains toy-like.
- Meaning: reward exists, but upgrade comes first.

Screen focus: giant `67` button is the only urgent object.

### 4 Coins

Upgrade card:

- Left: tap-hand icon.
- Right: coin icon + big `5`.
- Add progress marker: show `4` near the coin pill or goal rail and `5` on the card; do not write `TAP TO 5`.
- State accent: slightly warmer than fresh, but no affordable pulse yet.

Job card:

- Same locked state as fresh.

Screen focus: main tap button remains brighter than both cards.

### 5 Coins Affordable

Upgrade card:

- Left: tap-hand icon.
- Right: coin icon + big `5`.
- State accent: gold/green outline pulse once, then hold bright border.
- Optional symbol: `new_unlocked` icon in a small top-right badge.
- Interaction read: this card is now the next tap.

Job card:

- Still locked/dim; do not pulse.

Screen focus: upgrade card becomes the brightest non-button UI element.

### After First Upgrade

Upgrade card:

- Surface: completed/quiet state at `70..80%` brightness.
- Left: tap-hand icon.
- Right: big `2` + small `67`, showing new status `2/67`.
- Optional small check/new-unlocked symbol may flash once and then settle.
- Do not show `OWNED`, `TAP X2`, or other words.

Job card:

- Surface: active bright state.
- Left: job/kiosk art or city/kiosk symbol.
- Right: coin icon + big `8`.
- State accent: gold edge or green corner, no lock.
- Interaction read: this is the next tap.

Screen focus: job card is now louder than the completed upgrade card.

### Job Running

Upgrade card:

- Keep completed/quiet state.

Job card:

- Left: job/kiosk art remains visible.
- Center/bottom: progress bar filling over `6s`.
- Right: coin icon + big `8` remains visible for the whole timer.
- State accent: active blue/green fill, no claim pulse until complete.
- Do not show `JOB RUNS`.

Screen focus: progress motion carries the read. The player should see that the reward is coming.

### Job Ready / Claim

Upgrade card:

- Keep completed/quiet state.

Job card:

- Progress bar is full.
- Big `8` + coin icon pulses or bounces once every `1.2..1.8s`.
- Add `new_unlocked` or check symbol near the reward.
- State accent: green/gold claim glow on the card edge.
- On claim: coin particles fly to the resource pill, then card returns to active job-start state.
- Do not show `CLAIM JOB`.

Screen focus: reward number is the only pulsing card element.

## Tab/Icon Symbols

Until a real font exists, first-30s bottom tabs are icon-only.

| Tab | Symbol | State in first 30s |
| --- | --- | --- |
| Home | house/home icon | selected shell, bright |
| City | city/building icon | locked or inactive shell, dim |
| Deals | small job/kiosk or coin-run icon | hide if only two tabs are implemented; otherwise inactive shell |
| Upgrades | tap-hand/up-arrow/badge icon | hide if only two tabs are implemented; otherwise inactive shell |

Rules:

- No `HOME`, `CITY`, `Jobs`, `Up`, or other visible tab words in RC.
- Center icons inside the tab shells; do not align to old debug block centers if the shell has padding.
- Icon size target: `28..36px` inside a `56..64px` high mobile tab.
- Selected tab: bright shell plus icon at `100%` opacity.
- Locked/inactive tab: locked shell plus icon at `45..65%` opacity.
- If only Home and City exist in runtime, two icon tabs are accepted for RC. Do not fake four tabs with text or empty blocks.

## Accepted For RC

- Icon/number-only first 30s player UI.
- Existing generated toy UI art, even if two cards share one shell, as long as states differ clearly.
- Engine-drawn numbers for `0..5`, `2/67`, and `8`.
- DevAPI/accessibility strings using English labels while visual UI stays icon/number-first.
- Two visible bottom tabs if those are the only implemented runtime tabs.
- One-shot pulse on affordable upgrade and ready reward.
- Native desktop/PC validation as the default pass, with mobile portrait screenshot if web surface is touched.

## Rejected For RC

- Visible `TAP TO 5`, `JOB LOCKED`, `HOME`, `CITY`, `BUY TAP`, `START JOB`, `JOB RUNS`, `CLAIM JOB`, `OWNED`, or `COST`.
- Baked text inside new PNGs.
- Flat debug rectangles replacing toy shells.
- Two equally bright cards before the first upgrade.
- Job card pulsing before the upgrade is bought.
- Reward `8` disappearing while the job timer runs.
- First upgrade cost other than `5` in first-30s UI.
- Bottom tabs clipped, text-led, or empty placeholder blocks.

## Validation Signals

RC passes visually when a reviewer can infer the loop from screenshots without reading words:

1. Fresh: tap button is obvious; upgrade card says "need 5 coins" through icon + `5`; job card says "locked reward 8" through lock + dim `8`.
2. At 4 coins: card still asks for `5`; the main tap button remains the next action.
3. At 5 coins: upgrade card is the clear next action; job card remains quiet.
4. After first upgrade: `2/67` or `2` + `67` is visible; job reward `8` becomes the next action.
5. During job: progress is moving and reward `8` stays visible.
6. Ready: reward `8` pulses; no rough word label appears.
7. Tabs: icon-only Home/City read as navigation and no tab text is visible.

## Top Runtime Recommendations

1. Replace visible bitmap-text labels in upgrade/job cards and tabs with icon/number state drawing before tuning layout further.
2. Make `5` and `8` the dominant card values; use icons and opacity to explain action/lock/ready.
3. Keep DevAPI labels descriptive, but treat them as non-visual accessibility/debug metadata until a real font/localization path exists.
