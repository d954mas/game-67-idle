---
type: ProductReadGate
project: blockside-heat
task: T0120
surface: desktop
verdict: pass
timestamp: 2026-06-23T14:51:31.409Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/van-rumor-latest.png`

## Player Read

- Where am I? The player has followed the east van-rumor marker after the purple stash lead.
- What should I do now? The next action is to stake out the market lead.
- What changed after input? Following the marker advances state to van_rumor, sets van_rumor_active, awards $15, and updates the UI tree to JOB: MARKET WATCH.
- What is the reward / why continue? The repo chain now continues from stash lead into a confirmed market-watch lead with visible cash progress.
- Why does this look like a game? The low-poly street, red compact, east marker, engine-font HUD, and confirmation toast remain readable in native 3D.

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

Not covered / debt:
- (none)

## Review

Problem: The market watch is still only a next lead; the player cannot yet stake it out.

Next: Implement the market-watch stakeout as the next narrow playable beat.
