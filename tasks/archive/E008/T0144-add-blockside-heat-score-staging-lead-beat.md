---
id: T0144
title: Add Blockside Heat score-staging lead beat
status: dropped
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story]
created: 2026-06-24
updated: 2026-06-24
---

## What

After `repo_tool_cache`, add the next smallest score-staging beat: the player
reaches one staging marker, story records a named `repo_score_staging` state,
and HUD/state expose the next playable hook. Keep this as a lead beat, not a
full heist system.

Scope exclusions: no new district, mission menu, weapon inventory, economy
system, traffic simulation, or crew AI in this slice.

## Done when

- [ ] After `repo_tool_cache`, reaching one marker advances named
      `repo_score_staging` state.
- [ ] HUD/state tells the player the score is staged and what is next.
- [ ] Native capture/probe evidence covers `repo_tool_cache` and
      `repo_score_staging`.
- [ ] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-24: Created after T0143 resolved the lead visual rejection. Continue
  story expansion only from the improved city-block baseline.
- 2026-06-24: Dropped before closeout. Lead clarified that the main failure is
  automatic acceptance of flat one-color GLB rendering as "Roblox-like" visuals.
  Feature/story expansion is frozen until the material/texture floor passes.
