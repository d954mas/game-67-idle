---
type: ProductReadGate
project: blockside-heat
task: T0116
surface: desktop
verdict: pass
timestamp: 2026-06-23T14:28:59.931Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/rita-contact-latest.png`

## Player Read

- Where am I? The player is at Rita after the first street job, with the car marker visible on the road.
- What should I do now? The next action is clear: get in the red compact and scout the curb.
- What changed after input? Talking to Rita changes state to repo_intro, sets repo_intro_active, and updates the UI tree to JOB: RED COMPACT.
- What is the reward / why continue? The first job now chains into a named repo beat while keeping CASH $75 visible.
- Why does this look like a game? The low-poly Roblox-like street, blue contact, red car marker, and engine-font HUD remain readable.

## State Coverage

Required states:
- first_screen
- in_car_movement
- vehicle_route_complete
- pursuit_pressure
- pickup_stress
- reward_active
- rita_contact

Covered states:
- first_screen: tmp/blockside-heat/first-native-screenshot-latest.png
- in_car_movement: tmp/blockside-heat/in-car-movement-latest.png
- vehicle_route_complete: tmp/blockside-heat/vehicle-route-complete-latest.png
- pursuit_pressure: tmp/blockside-heat/pursuit-pressure-latest.png
- pickup_stress: tmp/blockside-heat/pickup-stress-latest.png
- reward_active: tmp/blockside-heat/job-complete-latest.png
- rita_contact: tmp/blockside-heat/rita-contact-latest.png

Not covered / debt:
- (none)

## Review

Problem: The repo beat is still an intro; the actual repo driving objective is not playable yet.

Next: Implement the red compact repo driving objective as the next narrow playable slice.
