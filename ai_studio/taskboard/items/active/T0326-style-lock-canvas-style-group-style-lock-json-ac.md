---
id: T0326
title: "style lock: canvas style group + style_lock.json + acceptance gate in asset pipeline"
status: doing
project: P001
epic: E010
priority: P1
tags: [style-lock, assets, canvas, art-gate, vibejam-retro]
created: 2026-07-06
updated: 2026-07-17
---

## What

VibeJam retro item 3: art was generated before a locked style existed — the #1
lead-attention sink (nav v3→v11, coin fights, "программер арт"). Design agreed
in discussion 2026-07-06 (industry research: tmp/style_lock_research_2026-07-06.md):

- ONE art direction per game, per-domain sheets (world/sprites + GUI) sharing
  palette and Do/Don't; not two independent styles.
- Canvas group `style` per game: style passport card, palette swatches,
  2-3 OWNED exemplar assets (the canon), refs/moodboard (separate from
  exemplars), Do/Don't card (negative examples are first-class — outsourcing
  practice shows negatives beat extra positives).
- Machine twin `games/<id>/design/style_lock.json`: prompt_preamble,
  negative_prompt (the don'ts), palette[] (feeds PROMPTS, not the gate — lead
  2026-07-06: palette-distance gating is unreliable for multi-color/gradient
  styles), bg_rule (magenta/green cutout), exemplar_refs (canvas:// ids, owned),
  asset_size.
- QA split (lead-corrected): DETERMINISTIC gates only for technical correctness
  — bg purity/chroma spill, halo, alpha quality, crop bbox, aspect (= T0317).
  STYLE conformity — vision-model compare vs exemplars + Do/Don't, advisory
  3-way verdict, lead backstop (same philosophy as the existing visual gate:
  no embeddings, contract=taste). No palette-ΔE gate, no CLIP gate.
- LoRA/per-game style model: NOT for current short projects (lead: долго и
  дорого) — parked as an idea for long projects; optional checkpoint field
  stays in the schema unused.
- Enforcement (refined vs original plan): EXPLORE mode ungated but outputs
  quarantined (cannot slice/pack/promote); PRODUCTION mode requires accepted
  lock, stamps asset origin with lock id; hard gate at pack/promote, soft at
  generation; --no-lock taints origin for review. Matches "gates advisory,
  lead is backstop".
- Jam cadence: lock thin and after first-playable direction feels right
  (divergence→convergence), never at hour 0; 1-2h explore phase then lock.
- Style library: locks + exemplars portable across games (--from <past-lock>),
  stored like the shared asset library (search library first).
- T0317 art-QA gate reads thresholds from the lock (answers its open question:
  per-style thresholds).

## Done when

Increments (lead accepted two-mode design 2026-07-06; every generated asset
starts QUARANTINED, flag visible in canvas; only accepted art reaches the game):

- [x] 1. `style_lock.json` schema + canvas `style` group convention (passport,
      palette, exemplars, refs, Do/Don't) — doc + example.
- [x] 2. Canvas asset status flag `quarantine → checked → accepted`, visible as
      a badge on canvas cards; CLI can set/read it.
- [x] 3. Generation paths default to quarantine; production mode stamps the
      lock id into the sidecar/origin; `--no-lock` taints origin.
- [x] 4. `checked` = technical auto-gates pass (T0317: bg purity/spill, halo,
      alpha, bbox, aspect).
- [x] 5. `accepted` = style verdict: vision compare vs lock exemplars +
      Do/Don't, 3-way advisory verdict, lead backstop.
- [ ] 6. Hard gate at promote: only `accepted` assets can enter
      `games/<id>/assets/` (and therefore the pack); quarantined art stays in
      canvas/staging, visible but not promotable.
- [ ] 7. Style library: `--from <past-game-lock>` seeds a new game's lock.

## Open questions

- (parked) LoRA/per-game style model — idea for long projects only.

## Log

- 2026-07-14: Absorbed T0208. Per-style model routing remains only the parked
  long-project option already described here; no separate task is needed.

- 2026-07-06: created from retro walkthrough item 3 discussion; full cited
  research saved at tmp/style_lock_research_2026-07-06.md.
- 2026-07-06: lead accepted two-mode gate; confirmed mental model: generation
  always allowed, art carries a visible quarantine flag in canvas until it
  passes checks; passing = usable in game. Decomposed into 7 increments.
- 2026-07-17: Started increment 1 only: a schema-backed games/<id>/design/style_lock.json contract, Canvas style-group convention, and example. Existing art_contract remains the broader taste/review brief; style_lock is its operational generation/asset-gate twin and will link to it rather than create a competing taste source. Later quarantine, generation, technical gate, acceptance, promote, and library increments stay out of this slice.
- 2026-07-17: Increment 1 review hardening: the portable JSON Schema is explicitly structural while validate.mjs owns cross-field Canvas semantics; CLI validation now binds game_id to public/private workspace game paths and requires the linked art contract to resolve physically inside the game design directory. Deferred technical threshold field names to T0317 so measurement formulas and calibration evidence define the contract before v1 consumption.
- 2026-07-17: Increment 1 complete after TDD, full 10-domain Studio verify, and clean independent re-review. Added structural v1 schema, fail-closed semantic/file validator, public/private Canvas ownership rules, example, Canvas style-group convention, and art-contract linkage. T0326 remains doing with increments 2-7 open; T0317 is next so deterministic formulas can define threshold fields before consumption.
- 2026-07-17: Increment 1 published in draft PR #6. GitHub Actions run 29581176566 passed blocking Studio verification on Ubuntu (3m43s) and Windows (5m25s).
- 2026-07-17: Increment 2 implementation complete: image-only assetStatus contract (quarantine, checked, accepted), journaled read/set operations, CLI and HTTP parity, history labels, and text badges in Canvas workspace/layers. Public transports may initialize quarantine, no-op, or downgrade; upward checked/accepted transitions require later gate/verdict evidence and cannot be forged. Focused Canvas suite passed 56/56, real Chrome audit covered workspace/layers and 320/768/1024/1440 widths, and independent re-review is clean. Required follow-up for increments 3-4 before promotion enforcement: every pixel-changing operation must reset/downgrade prior review state so accepted art cannot remain accepted after mutation.
- 2026-07-17: Increment 2 publication verification complete: changed verify passed assets and work-management; full Studio verify passed all 10 domains including template-release; Taskboard, doc-reference, architecture-map, and diff checks are clean.
- 2026-07-17: Increment 3 slice A complete in TDD: recipe, pack-sheet, animation, slice, cutout, and dual-plate generation now share an internal Canvas mint path that defaults every new pixel result to `quarantine`; ordinary uploads/imports remain untracked and cannot supply review state. Production-mode lock-id origin stamping and `--no-lock` tainting remain the next slice, so increment 3 stays open.
- 2026-07-17: Increment 3 slice B wires recipe and pack generation to frozen `meta.origin`: unowned Canvas work remains explore, game-owned production refuses before paid work without an accepted game lock, accepted runs stamp the lock id, and CLI/API no-lock bypasses are explicitly tainted. Animation and dual-plate origin wiring remains slice C, so increment 3 stays open.
- 2026-07-17: Slice B review hardening fails closed on malformed persisted Canvas ownership before any path lookup, blocks public/private path aliases, and adds explicit owned pack refusal plus multi-sheet no-lock taint coverage. The soft-generation interpretation is intentional: production refuses by default, but the explicit tainted override always remains available; only later promotion is unoverrideable.
- 2026-07-17: Increment 3 slice C completes the shared generation-origin contract for animation and AI dual-plate generation, including CLI/API `noLock` transport, fail-closed type validation, and refusal before generator/background-check work. Focused TDD covers unowned explore, game-owned missing-lock refusal, tainted override, and transport flags. Increment 3 is complete; increment 4 continues through T0317's checked technical-gate transition.
- 2026-07-17: Increment 4 started through T0317 slice 2: Canvas now has a trusted explicit technical-gate operation that can promote PASS to `checked` or downgrade FAIL to `quarantine` with frozen evidence. Increment 4 stays open until alpha outputs invoke it automatically.
- 2026-07-17: 2026-07-17: Increment 4 now auto-checks manual alphaDualPlate outputs against accepted style-lock thresholds in the same journal commit. Increment remains open until the remaining alphaCutout/corridorkey paths and promote flow are wired.
- 2026-07-17: Increment 4 complete: explicit checks plus automatic alphaCutout and alphaDualPlate outputs use accepted style-lock thresholds for checked/quarantine transitions. Hard promote enforcement remains increment 6; style advisory acceptance remains increment 5.
- 2026-07-17: Increment 5 slice A adds the trusted advisory `asset-style-check` seam. It requires current technical PASS evidence for the same source/lock, vision-compares the target with all 2-3 owned lock exemplars using `prompt_preamble` as Do and `negative_prompt` as Don't, and freezes a strict accept|revise|reject report in one conflict-safe undo. CLI/API bodies cannot inject verdicts, and the model deliberately leaves status checked/accepted unchanged. Increment 5 stays open for the separate explicit lead-decision/backstop transition.
- 2026-07-17: Increment 5 slice A review hardening pins the vision judge to an ephemeral read-only Codex process, resolves the real Windows codex.exe without a cmd wrapper, bounds untrusted report size, and revalidates the external style-lock plus every exemplar source_ref after the slow call. Evidence now binds logical exemplar refs to their exact Canvas source refs; any lock, exemplar, or Canvas-head change conflicts before write.
- 2026-07-17: Increment 5 slice A re-review hardening confines the Codex process to an isolated temporary root containing only copied target/exemplar images, preserves explicit oversized-output failures, and normalizes vanished or malformed final style-lock/exemplar inputs to HEAD_CONFLICT before any write.
- 2026-07-17: Quality: QTECH_001=pass; evidence: focused Canvas style/API/CLI suite passed 68/68, changed verify passed assets and work-management, full Studio verify passed all 10 domains, and independent final re-review found no actionable issues.
- 2026-07-17: Increment 5 slice A published in draft PR #16. GitHub Actions run 29607884365 passed blocking Studio verification on Ubuntu (4m29s) and Windows (4m39s).
- 2026-07-17: Increment 5 complete: asset-style-decide records an explicit bounded lead reason and keeps the model advisory, allowing the lead to accept despite reject or quarantine despite accept. The one undoable decision requires current source-bound technical PASS, the exact full style-lock snapshot, and unchanged owned exemplars; accept alone mints accepted. Increment 6 hard promotion enforcement is next.
- 2026-07-17: Increment 5 review hardening closes a public nodes-paste acceptance bypass: caller-authored image specs always re-enter quarantine and lose technical/style/decision evidence, while unrelated metadata remains. The separate internal live-node duplicate path preserves already-persisted review state. API and CLI regression tests cover forged accepted payloads.
- 2026-07-17: Increment 5 re-review hardening closes the remaining source-mutation bypass: cleanupApply and single/batch bakeFilters now quarantine newly materialized pixels and remove old technical/style/decision evidence while retaining unrelated provenance. Accepted-input regression tests cover all three paths; undo still restores exact prior state.
- 2026-07-17: Quality: QTECH_001=pass; evidence: combined lead-decision, style, API/CLI, cleanup, and filter-bake tests passed 96/96; changed verify passed assets/work-management; final full Studio verify passed all 10 domains; third independent re-review was clean after closing both acceptance bypasses.
