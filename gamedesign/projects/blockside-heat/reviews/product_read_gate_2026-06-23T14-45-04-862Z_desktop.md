---
type: ProductReadGate
project: blockside-heat
task: T0119
surface: desktop
verdict: pass
timestamp: 2026-06-23T14:45:04.863Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/stash-lead-latest.png`

## Player Read

- Where am I? The player is at the purple-block stash lead after finishing the repo scout route.
- What should I do now? The next action is to follow the van rumor east.
- What changed after input? Opening the stash lead changes state to stash_lead, sets stash_lead_active, and updates the UI tree to JOB: VAN RUMOR.
- What is the reward / why continue? The repo chain now continues from curb scout into a new visible van-rumor lead.
- Why does this look like a game? The low-poly street, purple lead area, van-rumor marker, and engine-font HUD remain readable in native 3D.

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

Not covered / debt:
- (none)

## Review

Problem: The van rumor is still a marker and story hook; following it is the next slice.

Next: Implement the east van-rumor follow-up as the next narrow playable beat.
