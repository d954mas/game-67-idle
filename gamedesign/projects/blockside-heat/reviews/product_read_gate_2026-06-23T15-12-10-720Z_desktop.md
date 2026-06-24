---
type: ProductReadGate
project: blockside-heat
task: T0123
surface: desktop
verdict: pass
timestamp: 2026-06-23T15:12:10.721Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/tail-route-latest.png`

## Player Read

- Where am I? The player has driven from courier spotting to the first tail-route marker.
- What should I do now? The next action is to hold distance and watch the courier turns.
- What changed after input? Reaching the marker advances state to tail_route, sets tail_route_active, keeps WANTED 1 pressure, and updates the UI tree to JOB: SAFE DISTANCE.
- What is the reward / why continue? The repo chain now has a first vehicle tailing beat after spotting the courier.
- Why does this look like a game? The low-poly road, red compact, courier position, engine-font HUD, and confirmation toast are readable in native 3D.

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
- tail_route

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
- tail_route: tmp/blockside-heat/tail-route-latest.png

Not covered / debt:
- (none)

## Review

Problem: The next tail segment is still only a lead; there is no turn-watch or spacing failure yet.

Next: Implement a second tail-route turn-watch beat as the next narrow playable slice.
