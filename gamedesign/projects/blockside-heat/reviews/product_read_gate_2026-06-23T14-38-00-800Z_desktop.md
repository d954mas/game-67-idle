---
type: ProductReadGate
project: blockside-heat
task: T0118
surface: desktop
verdict: pass
timestamp: 2026-06-23T14:38:00.801Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/repo-scout-complete-latest.png`

## Player Read

- Where am I? The player reached the curb scout marker in the red compact.
- What should I do now? The immediate driving beat is complete; the next lead is near the purple block.
- What changed after input? Arrival changes state to repo_scout_complete, adds scout cash, and updates the UI tree to JOB: STASH SPOTTED.
- What is the reward / why continue? The player gets CASH $100 and a new visible repo lead marker.
- Why does this look like a game? The low-poly street, red compact, purple lead marker, and engine-font HUD stay readable in native 3D.

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

Not covered / debt:
- (none)

## Review

Problem: The next stash-van lead is only a marker and story hook; interacting with it is still the next slice.

Next: Implement the purple-block stash lead interaction as the next narrow playable beat.
