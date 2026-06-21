---
type: FakeShotPromptPacket
job_id: ember-road-town-forge-v2
source_family: composed town forge fake shot
target_output: gamedesign/projects/ember-road/art/ember-road-town-forge-v2.png
---

# Fake Shot Prompt Packet: Town Forge V2

## Prompt

Use case: ui-mockup
Asset type: game fake shot / visual target, not final runtime UI.
Primary request: Create a polished 16:9 fantasy browser RPG screen for Ember
Road's town forge upgrade moment. The image must communicate that the player
returned from the Old Mine cache, spends ember shards, forges/equips the Mine
Lantern, and opens the next mine depth.

Scene/backdrop: Old Gate town square with a visible forge/workbench near the
gate, warm firelight, anvil or smithing table, Mine Lantern object glowing on
the bench, ember shard resource glow, road and mine route markers visible.
Subject: serious fantasy RPG equipment progression screen with the scene as the
first read, not a form. Hero stands near the forge. A blacksmith or Gate Warden
is present as the upgrade focus. The lantern result and Depth 2 route promise
are visible before reading UI text.
Style/medium: painterly 2D fantasy browser RPG, ornate but restrained frame,
readable silhouettes, weathered stone, leather, brass, iron, parchment, ember
gold accents. No copied game art or exact reference layout.
Composition/framing: 1280x720 landscape. Main painted location takes the left
two-thirds. The forge/workbench and lantern are inside the main scene with a
clear visual glow. Right rail is an equipment/result summary, not the main
fantasy. Route strip shows Old Gate, North Road, Old Mine, and a Depth 2 open
or lit marker. Bottom log belt is present but secondary confirmation.
Runtime composition needs: leave blank content areas where runtime text, costs,
state values, and labels will be overlaid. Show icon slots and object surfaces,
not readable text.
Layout translation: this prompt is only a rendered image target, but all future
runtime placement derived from it must remain Y-up. Larger logical Y means
higher on the game screen. Do not imply top-left/Y-down coordinates as layout
truth; any Y-down conversion belongs only to renderer/input/screenshot/DevAPI
boundaries.
Lighting/mood: warm forge glow against cool evening stone, heroic progression,
clear and readable, not horror, not comedy.
Color palette: ember gold, deep forest green, weathered stone, iron, leather,
parchment, muted red accents.
Text: no readable text, no fake letters, no numbers; reserve blank areas for
runtime text.
Constraints: all UI text, counters, labels, costs, and state values must remain
blank for runtime composition. Do not bake readable text into panels. Do not
copy War of Dragons, Dragon Eternity, DragonFable, AdventureQuest, AQW, Shakes
and Fidget, Wartune, Hero Wars, Dragon Awaken, or League of Angels assets,
characters, icons, names, or exact UI ornament shapes. The fake shot must imply
a Y-up layout translation for game/UI logic; do not include coordinate diagrams.
Avoid: debug rectangles, flat programmer art, modern mobile VIP/event clutter,
gacha formation UI, city-builder UI, idle/wait timers, unreadable pseudo-text,
watermarks, logos, stock-photo look, panel-only shop UX, one-color purple/blue
palette.

## Acceptance Checklist

- Looks like a fantasy browser RPG forge/equipment event, not a dashboard.
- The forge/workbench and Mine Lantern are visible in the main scene.
- Hero and upgrade NPC/focus are readable at gameplay size.
- Ember shard cost, lantern result, and Depth 2 unlock have visual anchors.
- Right rail summarizes the event; it does not carry the whole UX.
- Bottom log confirms the event; it is not the only reward feedback.
- UI frame supports later runtime composition: blank text areas, separate icon
  ideas, safe panel interiors.
- No baked labels, fake text, watermark, or copied reference art.
- Composition can translate to native runtime while keeping all logical layout
  coordinates Y-up: larger logical Y is higher on the game screen.
- If accepted, generate separate runtime source families; do not use this fused
  fake shot as final runtime UI.
