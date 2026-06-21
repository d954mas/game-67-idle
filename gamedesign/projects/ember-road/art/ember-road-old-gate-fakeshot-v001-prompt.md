---
type: FakeShotPromptPacket
job_id: ember-road-old-gate-fakeshot-v001
source_family: composed old gate fake shot
target_output: gamedesign/projects/ember-road/art/ember-road-old-gate-fakeshot-v001.png
---

# Fake Shot Prompt Packet: Old Gate First Screen

## Prompt

Use case: ui-mockup
Asset type: game fake shot / visual target, not final runtime UI
Primary request: Create a polished 16:9 fantasy browser RPG first-screen fake
shot for Ember Road's Old Gate. The image should communicate a playable PC RPG
screen with a painted town-gate location as the main scene, a hero near the
gate, a Gate Warden quest focus, a route strip from Old Gate to North Road and
a locked Old Mine, a Road Wolf auto-battle preview, ring/XP/gold reward icons,
and a persistent RPG frame.
Scene/backdrop: stone town gate at sunset, warm torchlight, road leaving the
gate toward dark woods, small market/guard details, heroic but readable.
Subject: broad fantasy RPG UI composition with location first, then quest,
route, enemy, reward, and hero progression.
Style/medium: painterly 2D fantasy browser RPG, ornate but restrained frame,
readable icon silhouettes, no copied game art or exact reference layout.
Composition/framing: 1280x720 landscape. Main painted location takes the left
two-thirds. Right quest rail shows NPC portrait area, objective area, reward
slots, and one large primary action base. Top hero/status bar and bottom
log/action belt frame the screen. Route strip sits inside the scene near the
lower third. Use clear empty content areas where runtime text will be overlaid.
Lighting/mood: warm heroic fantasy, serious adventure, not comedy, not horror.
Color palette: ember gold, deep forest green, weathered stone, leather,
parchment, iron, muted red accents.
Materials/textures: painted stone, worn parchment, leather, brass/iron trim,
wooden plaques, subtle magic ember glow.
Text: no readable text, no fake letters, no numbers; reserve blank areas for
runtime text.
Constraints: all UI text, counters, labels, and state values must remain blank
for runtime composition. Do not bake readable text into panels. Do not copy
Legend, Dragon Eternity, AQW, Shakes and Fidget, or Wartune assets, characters,
icons, names, or exact UI ornament shapes. The fake shot must imply a Y-up
layout translation for game/UI logic; do not include coordinate diagrams.
Avoid: debug rectangles, flat programmer art, modern mobile VIP/event clutter,
city-builder UI, idle/away-time cues, unreadable pseudo-text, watermarks,
logos, stock-photo look, one-color purple/blue palette.

## Acceptance Checklist

- Looks like a fantasy browser RPG screen, not a dashboard.
- Old Gate location is the primary read.
- Hero, Gate Warden, Road Wolf, route, locked Old Mine, and ring/XP/gold reward
  promise are visible without explanation.
- UI frame supports later runtime composition: blank text areas, separate icon
  ideas, safe panel interiors.
- No baked labels, fake text, watermark, or copied reference art.
- Composition can translate to native runtime while keeping all logical layout
  coordinates Y-up.
- If accepted, generate separate runtime source families; do not use this fused
  fake shot as final runtime UI.
