# Evidence — A3 (HUD cohesion)

## Sprint contract
Unify the HUD into one cohesive flat system matched to the diorama's warm grade:
shared warm-charcoal panels, a consistent button style (top highlight + base
shade + accent outline when active), and a single accent family (gold = active,
blue = info/action, green = build/confirm). Engine font only (already true).

## Named acceptance checks
- [x] One panel token (warm charcoal + light top edge + dark base) used by all
      panels (top-right status, bottom need bar, build palette).
- [x] One `draw_button` helper styles mode/map/work/lot buttons consistently.
- [x] Active state reads via gold accent outline (map open, active lot).
- [x] Build mode HUD intact (palette swatches, green BUILD button, ghost).
- [x] No handmade/debug text in product view (engine font only) — guard green.
- [x] Build green; gameplay unchanged.

## Evidence
- Live HUD: `tasks/evidence/night-A3/05-A3-hud-live.png`.
- Build HUD: `tasks/evidence/night-A3/06-A3-hud-build.png`.

## Verdict
PASS — HUD reads as one cohesive system rather than ad-hoc coloured buttons.
