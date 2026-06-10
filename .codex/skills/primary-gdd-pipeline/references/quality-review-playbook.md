# Quality Review Playbook

Load this file before claiming a primary GDD pipeline task is done, especially
after visual, website, data, art, reference, or handoff changes.

## Review Stance

Review as a product/design director, not as the author. Prioritize blockers,
missing proof, stale source-of-truth, and user-facing confusion. Do not hide
gaps behind a positive summary.

## Gate Review

Check current stage gate:

- Concept gate: hook, fantasy, audience, platform, pillars, no-go list.
- Reference gate: 3-7 refs, source quality, borrow/avoid/copy-risk, synthesis.
- Visual gate: gameplay fake shot or runtime proof exists and is visible.
- Slice gate: first 30 seconds, first 5 minutes, loop, economy, UI flow.
- Handoff gate: source order, phases, commands, acceptance gates, next prompt.

If the claimed gate is not satisfied, report `partial` and name the missing
piece.

## Product Clarity Review

Ask:

- Can a new reader tell what the player does in 5 seconds?
- Are currencies, stats, jobs, activities, upgrades, and unlocks connected?
- Does the visual proof show UI and player action, not only mood?
- Is the meme/tone visible in gameplay, not only lore?
- Does the first playable slice avoid broad speculative systems?
- Are user taste decisions captured rather than guessed?

## Source-Of-Truth Review

Check:

- docs, site, JSON, and handoff do not contradict each other;
- stale files are marked or ignored;
- final images are in durable project paths;
- temp/raw/rejected/source generation is not staged;
- links point to final files, not `tmp/`;
- source order is clear for the next agent.

## Visual Review

For visual deliverables:

- fake shot includes avatar/object, currencies/stats, primary action, upgrade,
  next goal, feedback, and environment/status state;
- runtime asset pack has separate assets, manifest, dimensions, transparency
  expectations, and composed proof;
- web GDD shows the current fake shots and gameplay/economy data;
- desktop and mobile portrait are readable when web is in scope.

## Handoff Review

For implementation handoff:

- first playable slice is implementable without rereading chat history;
- commands are discovered locally or explicitly marked unknown;
- acceptance gates include screenshots and emulated input when relevant;
- forbidden paths/submodules/raw folders are named;
- next prompt is scoped to one playable slice.

## Final Report Rule

End with:

- `done` only if the current gate and DoD are satisfied;
- `partial` if useful work exists but proof or alignment is missing;
- `blocked` only when a concrete user decision or external state is required.

Always include validation results and remaining assumptions.

## Common Review Findings

- GDD says life sim, but screen does not show city/home/work/status.
- Art exists, but no fake gameplay UI exists.
- Currencies exist, but sources/sinks are unclear.
- Site exists, but is not aligned with JSON/source docs.
- Handoff has phases, but no first clickable action.
- Commit includes ignored/raw/temp generation artifacts.
