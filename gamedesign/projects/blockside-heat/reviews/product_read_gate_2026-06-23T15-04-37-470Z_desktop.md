---
type: ProductReadGate
project: blockside-heat
task: T0122
surface: desktop
verdict: pass
timestamp: 2026-06-23T15:04:37.471Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/courier-spotted-latest.png`

## Player Read

- Where am I? The player has reached the courier cue after starting the market-watch stakeout.
- What should I do now? The next action is to keep distance on the tail route.
- What changed after input? Spotting the courier advances state to courier_spotted, sets courier_spotted, keeps WANTED 1 pressure, and updates the UI tree to JOB: TAIL ROUTE.
- What is the reward / why continue? The repo chain now continues from surveillance into a visible tail-route lead.
- Why does this look like a game? The low-poly road, courier cue, red compact, engine-font HUD, and confirmation toast are readable in native 3D, with minor bottom-HUD crowding to watch.

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
- courier_spotted

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
- courier_spotted: tmp/blockside-heat/courier-spotted-latest.png

Not covered / debt:
- (none)

## Review

Problem: The tail route is still only the next lead; there is no following-distance gameplay yet.

Next: Implement the first tail-route movement beat as the next narrow playable slice.
