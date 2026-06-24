---
type: ProductReadGate
project: blockside-heat
task: T0121
surface: desktop
verdict: pass
timestamp: 2026-06-23T14:58:34.889Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/market-watch-latest.png`

## Player Read

- Where am I? The player has reached the market watch point after confirming the east van rumor.
- What should I do now? The next action is to watch for the courier.
- What changed after input? Starting the stakeout advances state to market_watch, sets market_watch_active, raises wanted level to 1, and updates the UI tree to JOB: COURIER WATCH.
- What is the reward / why continue? The repo chain now has a visible surveillance beat after the van rumor, with higher pressure through WANTED 1.
- Why does this look like a game? The low-poly street, orange market building, red compact, engine-font HUD, and confirmation toast remain readable in native 3D.

## State Coverage

Required states:
- first_screen
- in_car_movement
- vehicle_route_complete
- pursuit_pressure
- pickup_stress
- reward_active
- rita_contact
- repo_drive
- repo_scout_complete
- stash_lead
- van_rumor
- market_watch

Covered states:
- first_screen: tmp/blockside-heat/first-native-screenshot-latest.png
- in_car_movement: tmp/blockside-heat/in-car-movement-latest.png
- vehicle_route_complete: tmp/blockside-heat/vehicle-route-complete-latest.png
- pursuit_pressure: tmp/blockside-heat/pursuit-pressure-latest.png
- pickup_stress: tmp/blockside-heat/pickup-stress-latest.png
- reward_active: tmp/blockside-heat/job-complete-latest.png
- rita_contact: tmp/blockside-heat/rita-contact-latest.png
- repo_drive: tmp/blockside-heat/repo-drive-latest.png
- repo_scout_complete: tmp/blockside-heat/repo-scout-complete-latest.png
- stash_lead: tmp/blockside-heat/stash-lead-latest.png
- van_rumor: tmp/blockside-heat/van-rumor-latest.png
- market_watch: tmp/blockside-heat/market-watch-latest.png

Not covered / debt:
- (none)

## Review

Problem: The courier is still only the next lead; the player cannot yet spot or tail the courier.

Next: Implement the courier-spotting follow-up as the next narrow playable beat.
