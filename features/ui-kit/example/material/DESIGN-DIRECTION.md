# Original studio game UI

## User decision

The studio UI kit is fully original. LAYERLAB store previews, the local Yandex Disk packs, two studio games are references for analysis only. They are not a source of components or art to import into the new kit.

Do not extract, trace, recolor, resell, or lightly reskin vendor sprites, icons, PSDs, prefabs or screen layouts. Do not pass vendor images into image generation. Their presence on disk does not change this decision. No vendor asset purchase or import is needed for this work.

## What references inform

- Keep the game world dominant; place concise HUD information and actions around it.
- Make important icons, numbers and actions readable at game size.
- Use clear states for available, selected, locked, cooling down and completed actions.
- Give upgrades and rewards appropriate emphasis without cluttering the shared default.

These principles do not prescribe a vendor's shapes, icon silhouettes, palette, font pairing, decorative treatment or exact layout.

## What we author

An original component vocabulary, icon family, typography system, shapes, spacing, surface treatment, motion and default theme. Typography system means our selection and rules; any existing font files still follow their own license rather than becoming a studio-authored typeface by assertion.

Prioritize resource counters, status strips, ability controls, upgrade/perk cards, reward/result compositions and adaptive game overlays. Settings and generic inputs support the game-facing set.

Shared component behavior belongs to the studio kit. Each game owns its theme, art, content and screen composition, and can create its own components from shared primitives. No economy, inventory, progression or store mechanics are mandatory merely because their UI examples exist.

## Selected direction: B

The user selected B, the contrast-heavy original game direction. Use strong ink contours, compact filled pictograms, bold readable text, a short solid lower edge on buttons and visible game states. Preserve a world-first overlay composition. Palette, shape and typography remain game-owned choices.

The current interactive preview is `../bold/`; its README records scope and validation. The selected fake shot is `tmp/ui-directions-2026-09-11/original-game-kit/b-bold-play.png`. No production engine or C kit integration follows from visual selection alone.

## Earlier prototype status

The Material browser example tests theme separation and interaction behavior. It is not the accepted final visual style. Its source contains original HTML/CSS and simple prototype icons, with no vendor pack assets. Reference captures under tmp remain evidence only and must not enter shipping asset manifests.

The B browser iteration shows original game-facing components over an original generated scene. Implementation in the engine and production asset acceptance still require their own subsequent proof.
