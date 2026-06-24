---
type: ProductReadGate
project: blockside-heat
task: T0117
surface: desktop
verdict: pass
timestamp: 2026-06-23T14:33:37.590Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/repo-drive-latest.png`

## Player Read

- Where am I? The player is in the red compact at the street intersection after talking to Rita.
- What should I do now? The next action is to drive to the orange curb marker.
- What changed after input? Entering the car changes state to repo_drive, sets repo_drive_active, and updates the UI tree to JOB: CURB SCOUT.
- What is the reward / why continue? The Rita intro now becomes a playable driving objective while CASH $75 remains visible.
- Why does this look like a game? The low-poly Roblox-like city, red car, orange route marker, and engine-font HUD are readable in native 3D.

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

Covered states:
- first_screen: tmp/blockside-heat/first-native-screenshot-latest.png
- in_car_movement: tmp/blockside-heat/in-car-movement-latest.png
- vehicle_route_complete: tmp/blockside-heat/vehicle-route-complete-latest.png
- pursuit_pressure: tmp/blockside-heat/pursuit-pressure-latest.png
- pickup_stress: tmp/blockside-heat/pickup-stress-latest.png
- reward_active: tmp/blockside-heat/job-complete-latest.png
- rita_contact: tmp/blockside-heat/rita-contact-latest.png
- repo_drive: tmp/blockside-heat/repo-drive-latest.png

Not covered / debt:
- (none)

## Review

Problem: The repo objective starts and points to a curb target, but reaching/completing that target is still the next slice.

Next: Implement arrival at the curb scout marker and a small repo-route completion response.
