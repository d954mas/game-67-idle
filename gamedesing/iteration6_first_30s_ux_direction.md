# Iteration 6 First 30s UX Direction

Status: designer handoff for runtime implementation.  
Scope: first 30 seconds only. Owns docs and existing generated UI asset decisions. No `src/` edits.

## Goal

Make the first loop readable in one glance on `390x844` portrait:

`TAP 67 -> reach 5 coins -> BUY first upgrade -> start/claim first job reward`.

The runtime currently draws reliable numbers, progress bars, simple shapes, and packed PNG icons/shells. It does not draw player-facing letters on screen yet. Therefore the runtime-ready first pass must use numbers plus icons as the real player-facing language, with word copy reserved for DevAPI labels, accessibility, and a future text renderer.

## Player Behavior

1. First 0-3 seconds: player sees status `1` and `67`, the hero, a coin counter, and the giant `67` tap button with the tap-hand icon.
2. First 3-12 seconds: player taps the giant `67` button five times. Coin pill count increments `0 -> 5`; a `5` goal marker fills toward complete.
3. At 5 coins: first upgrade card becomes the only highlighted card. Player taps the card with tap-hand icon plus `5` coin cost.
4. After buy: status changes to `2` and `67`; job card becomes active with reward `8` coin. Player starts the job, waits for the short timer, then claims the reward.

## Runtime-Ready On-Screen Copy

Use this table as the exact first-pass screen language until a full text renderer exists.

| Moment | On-screen numeric copy | Icon/shell | Meaning |
| --- | --- | --- | --- |
| Status | `1` left, `67` right | `ui_first_status_badge_shell_9s.png` | "1/67" without relying on slash text |
| Coin pill | current coin count, e.g. `0`, `1`, `5` | `ui_runtime_resource_pill_9s.png` + `icon_meme_coin_67.png` | current meme coins |
| Main action | large `67` | `button_67_gesture.png` + `icon_tap_hand_67.png` | tap to do 67 |
| First goal rail | current count at left, `5` at right | `ui_first_action_plate_9s.png` + progress fill + `icon_meme_coin_67.png` | get to 5 coins |
| First upgrade card | `5` + coin icon; after purchase show `2` + `67` | `ui_runtime_goal_card_9s.png` + `icon_tap_hand_67.png` | buy first upgrade for 5 coins, then status becomes 2/67 |
| First job card locked | `8` + coin icon at 40-50% opacity | `ui_runtime_goal_card_9s.png` | reward exists but is locked until upgrade |
| First job running | progress fill only, with `8` + coin icon still visible | `ui_runtime_progress_bar_9s.png` or existing toy bar | job timer is running |
| First job ready | `8` + coin icon pulsing once | `ui_runtime_goal_card_9s.png` | claim reward |

Do not show runtime words like `Jobs`, `Up`, `Goal`, `Buy`, or `Tap` until a real text renderer is available. The current numeric renderer cannot draw them, and baked text in PNGs would lock localization and can become unreadable.

## DevAPI And Future Text Copy

The DevAPI/accessibility labels may use English ASCII now because they are not the visual UI. If a text renderer lands, use these exact short labels:

| UI element | DevAPI label now | Future visible copy |
| --- | --- | --- |
| `main.do67` | `Tap 67` | `TAP 67` |
| `main.coins` | `Meme coins` | `COINS` |
| `main.status` | `Status 1 of 67` | `1/67` |
| `main.upgrade.first` | `Buy first upgrade, cost 5 coins` | `BUY 5` |
| `main.job.first` | `Start job, reward 8 coins` | `JOB 8` |
| `main.claim` | `Claim job reward, 8 coins` | `CLAIM 8` |

If Cyrillic text is restored later, the copy source remains `p0_ui_copy.md`; this note only defines the runtime-safe first-30s version.

## 390x844 Portrait Layout

Use these safe-area targets as implementation constraints, not decorative mockup advice.

| Zone | Target rectangle | Rule |
| --- | --- | --- |
| Top HUD | x `12..378`, y `10..76` | one coin pill only in first 30s; no reset/debug button in this zone |
| Status badge | center x, y `78..142`, width `158..188`, height `58..70` | numbers must be at least 25px high |
| Hero | center x, y `150..345` | cap to shoes visible; feet may overlap button by max 12px |
| Main tap button | center x, y `330..510`, size `176..190` | largest tap target on screen |
| First loop rail | x `16..374`, y `520..580`, height `44..60` | shows progress to `5`; do not place it above the hero |
| Upgrade/job cards | x `16..374`, y `594..744` | two compact cards or one active card; hide secondary detail before shrinking tap button |
| Bottom tabs | x `0..390`, y `782..844` | fully visible; selected/locked shells only, no flat blocks |

Important correction from earlier passes: `ui_first_action_plate_9s.png` should be the first-loop goal rail under the main tap button, not a second primary button above the hero. The only primary action is the giant round `67` button.

## System Behavior

- Initial state: upgrade card disabled but visible; job card visible at reduced opacity or hidden behind the upgrade card. The screen should not ask the player to choose between two equal cards before the first purchase.
- Every tap: increment coin pill by current click power and emit 3-5 small coin particles toward the pill.
- At exactly 5 coins: upgrade card gets one green/gold pulse. The job card must not pulse yet.
- On first upgrade buy: spend 5, set status visual to `2` and `67`, change card focus from upgrade to job.
- On job start: job card keeps reward `8` visible while the progress bar fills.
- On job ready: progress fill is complete and the `8` reward pulses once.
- On claim: add reward coins and fly coin particles to the coin pill.

## Feedback

- Tap feedback: button depresses by 2px for 80-120ms, then rebounds. No layout shift.
- Coin feedback: `icon_meme_coin_67.png` particles are bright gold with black outline; avoid gray sparkles.
- First upgrade affordable feedback: card outline pulses gold once, not continuously.
- Job ready feedback: reward `8` and coin icon bounce once; do not flash the whole screen.

## Accepted Visual Decisions

- Accept the toy-like generated background, hero, gold button, coin icon, tap-hand icon, goal-card shell, and resource-pill shell.
- Accept numeric-first UI for this iteration because it matches the current renderer and keeps the screen readable.
- Accept using the same goal-card shell for upgrade and job only if their icon/number states differ clearly.
- Accept bright gold, green, blue sky/yard, and black toy outlines as the first-screen palette.
- Accept one visible first objective at a time: get 5 coins, then buy, then job.

## Rejected Visual Decisions

- Reject gray, blue-gray, magenta, purple, or flat debug rectangles as visible UI surfaces.
- Reject `10` as the first upgrade/card price in any first-30s fake shot or runtime screen. The runtime cost is `5`.
- Reject visible text labels that the current renderer cannot actually draw, including `Jobs`, `Up`, `Goal`, `Buy`, and `Tap`.
- Reject putting the first-loop rail above the hero; that reads like a banner/debug bar instead of the next step.
- Reject two equally bright cards before the first upgrade. The first loop must not split attention.
- Reject clipped tabs, clipped cards, or a reset/debug button stealing the first 30 seconds.

## Tuning Knobs

- First upgrade cost: `5` coins.
- First job duration: `6` seconds.
- First job reward: `8` coins.
- Primary button size on `390x844`: `176..190` px square.
- First-loop rail progress fill: clamp `coins / 5`, with complete state at `5`.
- Disabled job opacity before upgrade: `0.40..0.50`.

## Validation

- On a fresh `390x844` screenshot, a reviewer should point to the loop in this order without reading docs: giant `67` tap button, coin count, `5` goal/cost, job reward `8`.
- At 4 coins, only the main tap button should feel urgent.
- At 5 coins, the upgrade card should be the brightest non-button UI element.
- After buying, the job card should become brighter than the upgrade card.
- No visible UI surface may resemble a placeholder/debug rectangle.
- Native desktop validation is enough unless the task specifically asks for web/WASM.

## Runtime UI Fixes Still Needed

1. Move the first-loop progress rail below the giant tap button and treat it as goal feedback, not as a second button.
2. Replace or hide first-30s text-dependent tab/card labels until a real text renderer exists; use icons and numbers only on screen.
3. Remove reset/debug visual prominence from the first-30s safe area and make the upgrade/job focus state single-step: `5` upgrade first, `8` job second.
