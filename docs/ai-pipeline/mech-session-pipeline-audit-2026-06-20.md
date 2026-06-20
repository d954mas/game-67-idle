# Mech Session Pipeline Audit

Post-prototype pipeline audit after the `Mech Builder Battler` session.

## Scope

This records reusable workflow failures, not game status. The game prototype is
stopped; use this as evidence for pipeline improvements before the next game.

## Findings

1. **Hard visual/runtime invariants were not loaded strongly enough.**
   Y-up, engine font/text renderer, and shape/debug renderers as debug-only must
   be visible in hot context and checked during playable/visual slices.

2. **Ready free assets were sourced too late.**
   The agent spent time on procedural/kitbashed visuals before searching for
   legal downloaded assets. Important visual targets need asset/source search
   early, with license, integrity, preview, and project import boundaries.

3. **Downloaded assets lacked a reusable catalog.**
   A folder of binaries is not enough. The shared asset library needs OKF-style
   Markdown records with tags, descriptions, resource paths, licenses, formats,
   and status so agents can search before opening files.

4. **Texture generation was procedural, not a workflow.**
   The agent generated one studs texture but did not establish repeatable
   texture briefs, tiling contracts, model UV assumptions, map-set records, 2x2
   previews, or seam audits.

5. **Product gates were too soft after lead rejection.**
   A gate can pass while still missing the exact lead-rejected quality if the
   gate treats it as minor debt. Lead rejection should freeze expansion until
   that rejection reason has direct evidence.

6. **Smoke tests coupled unrelated claims.**
   Movement, combat pacing, reward loops, attack cooldowns, and visual framing
   were checked in one long smoke. Future runtime proof should split these
   checks so one design change does not invalidate unrelated evidence.

7. **Task/status context drifted.**
   `AGENTS.md`, `tasks/STATUS.md`, and taskboard state can lag behind a stopped
   prototype. Post-prototype cleanup should refresh current work before the next
   agent starts.

## Immediate Fixes

- Shared asset library becomes an OKF-style Markdown bundle at
  `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`.
- `game-asset-pipeline` must search catalog Markdown records before binary
  files and preserve license/integrity metadata before project import.
- Texture work uses `game-texture-generation` with explicit usage class,
  tiling contract, model UV assumption, 2x2 preview, and seam audit.
- New helper tools cover download intake and tileable texture audit.
- `tools/product_gate/close_slice.mjs` blocks strict closeout of
  `lead-rejection` tasks unless `--resolved-rejection` names the exact rejected
  issue and proof.
- The project-specific Mech Builder Battler playable smoke now exposes split
  suites: `contract`, `asset-load`, `visual-framing`, `movement`,
  `combat-pacing`, `reward-loop`, and `upgrade-special`.

## Remaining Work

- Clean or archive stopped-prototype taskboard noise before the next game.
