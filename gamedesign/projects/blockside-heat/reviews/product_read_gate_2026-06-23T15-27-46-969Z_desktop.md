---
type: ProductReadGate
project: blockside-heat
task: T0125
surface: desktop
verdict: pass
timestamp: 2026-06-23T15:27:46.971Z
---

# Product Read Gate - blockside-heat / desktop

Verdict: **PASS**

Screenshot: `tmp/blockside-heat/tail-pressure-latest.png`

## Player Read

- Where am I? a low-poly city block tailing route under rising wanted pressure
- What should I do now? follow the courier from the turn marker to the pressure marker
- What changed after input? reaching the pressure marker advances state to tail_pressure, sets tail_pressure_active, raises WANTED to 2, and updates the UI tree to JOB: TAIL PRESSURE
- What is the reward / why continue? the repo chain now has a readable pressure escalation after tailing instead of only route markers
- Why does this look like a game? low-poly road, red compact, pressure marker context, engine-font HUD, wanted escalation, and toast are readable in native 3D

## State Coverage

Required states:
- first_screen
- hud_visible
- primary_action_ready
- primary_action_feedback
- reward_active
- progression_panel_open
- modal_or_choice_open
- locked_or_disabled_state
- resume_or_reentry_state
- transient_stress_state
- in_car_movement
- vehicle_route_complete
- pursuit_pressure
- pickup_stress
- rita_contact
- repo_drive
- repo_scout_complete
- stash_lead
- van_rumor
- market_watch
- courier_spotted
- tail_route
- tail_turn
- tail_pressure

Covered states:
- first_screen: tmp/blockside-heat/first-native-screenshot-latest.png
- hud_visible: tmp/blockside-heat/tail-pressure-latest.png
- primary_action_ready: tmp/blockside-heat/tail-turn-latest.png
- primary_action_feedback: tmp/blockside-heat/tail-pressure-latest.png
- reward_active: tmp/blockside-heat/job-complete-latest.png
- transient_stress_state: tmp/blockside-heat/tail-pressure-latest.png
- in_car_movement: tmp/blockside-heat/in-car-movement-latest.png
- vehicle_route_complete: tmp/blockside-heat/vehicle-route-complete-latest.png
- pursuit_pressure: tmp/blockside-heat/pursuit-pressure-latest.png
- pickup_stress: tmp/blockside-heat/pickup-stress-latest.png
- rita_contact: tmp/blockside-heat/rita-contact-latest.png
- repo_drive: tmp/blockside-heat/repo-drive-latest.png
- repo_scout_complete: tmp/blockside-heat/repo-scout-complete-latest.png
- stash_lead: tmp/blockside-heat/stash-lead-latest.png
- van_rumor: tmp/blockside-heat/van-rumor-latest.png
- market_watch: tmp/blockside-heat/market-watch-latest.png
- courier_spotted: tmp/blockside-heat/courier-spotted-latest.png
- tail_route: tmp/blockside-heat/tail-route-latest.png
- tail_turn: tmp/blockside-heat/tail-turn-latest.png
- tail_pressure: tmp/blockside-heat/tail-pressure-latest.png

Not covered / debt:
- progression_panel_open: not in this narrow tail-pressure slice; progression is HUD/state only
- modal_or_choice_open: not in this narrow tail-pressure slice; no dialog choice is introduced
- locked_or_disabled_state: not in this narrow tail-pressure slice; no locked action is introduced
- resume_or_reentry_state: not in this narrow tail-pressure slice; restart coverage remains reset/capture path

## Review

Problem: The next stop/intercept segment is still only a lead; there is no blocking maneuver or courier stop resolution yet.

Next: Implement a small stop/intercept resolution beat after tail_pressure as the next narrow playable slice.
