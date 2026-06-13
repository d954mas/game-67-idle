---
id: T0020
title: Add reference study gate to AI pipeline
status: review
epic: ""
priority: P1
tags: [pipeline, reference, skills]
created: 2026-06-12
updated: 2026-06-13
---

## What

Strengthen the reusable AI pipeline so named reference games/styles are studied
through a concrete ref deconstruction gate before gameplay, UI, economy, or art
implementation.

## Done when

- [x] Project rules require source-backed reference study before
      reference-driven implementation.
- [x] Portable pipeline docs describe the same gate.
- [x] Relevant skills enforce the gate.
- [x] Reusable knowledge template explains how to study refs: sources, first
      10/60 seconds, 1-5 minute loop, screen grammar, mechanics/balance,
      borrow/avoid/copy-risk, and current-build mismatch audit.
- [x] Rule now defines the four-pass study method: source packet, player
      transcript, systems extraction, and translation gate with screenshot or
      scenario proof.
- [x] Rule now requires an auditable source matrix and durable deconstruction
      doc for central gameplay references, with gameplay footage/long
      screenshot evidence for interaction claims.
- [x] Rule now defines the agent action sequence for "make it like X" feedback:
      reference question, source packet, evidence record, visible transcript,
      systems extraction, safe translation, native mismatch, and next proof.
- [x] Rule now defines reference-study modes: quick check, central
      deconstruction, and deep deconstruction, with quick checks explicitly
      barred from unlocking core gameplay/economy/balance/art-direction work.
- [x] Rule now defines when an agent may say a reference was "studied": durable
      source packet with links/paths, checked dates, video/walkthrough or long
      screenshot evidence, official/store/trailer visuals when available,
      supporting guide/review/deconstruction sources for balance claims,
      timestamped/framed observations, and current native mismatch capture.
- [x] Rule now requires a Reference Lock before implementation/final art:
      study mode, reference question, durable doc path, source packet, current
      native capture plan/path, no-coding boundary, and expected native proof;
      implementation unlocks only after observed facts, mismatch, and next
      proof are recorded.
- [x] Rule now requires Reference Intake before defending the current build or
      starting another implementation pass after a named ref or "not like the
      ref" challenge: exact question, study mode, doc path, source packet,
      native capture plan/path, no-coding/no-final-art boundary, first proof,
      and evidence labels for observed/inferred/user-provided/unknown claims.
- [x] Rule now defines a Reference Study Definition of Ready and stop behavior:
      if mode, doc path, Reference Lock, source matrix, current native capture,
      observation ledger, borrow/avoid/copy-risk, current-build mismatch, or
      next native proof are missing, the agent must say the study is not ready
      for implementation and continue source gathering/deconstruction instead
      of coding or final art.
- [x] Rule now requires a user-inspectable Reference Digest before
      implementation resumes: study mode, sources checked, 3-5 observed facts,
      current-build mismatch, borrow/avoid/copy-risk, and next native proof.
- [x] Rule now requires a Source Ladder for central/deep refs: user-provided
      material, official/store/trailer visuals, raw gameplay or long screenshot
      evidence, then guides/reviews/lectures/deconstructions/wikis/community
      notes as supporting interpretation, with source role, checked date, and
      watched/read scope recorded before conclusions.
- [x] Rule now requires a Reference Evidence Board for central/deep refs:
      cited frames/screenshots for first screen, first input, visible response,
      reward feedback, upgrade/progression UI, and friction/blocked state,
      plus gameplay/walkthrough timing evidence and supporting guide/review/
      deconstruction sources for balance or friction claims.
- [x] Rule now states that parallel reference work is research-only until
      unlocked: source gathering/transcription/native mismatch capture may run
      beside unrelated setup, but reference-driven implementation/final art
      waits for the digest, mismatch audit, borrow/avoid/copy-risk, and next
      native proof.
- [x] Taskboard and skill validation pass.

## Open questions

None.

## Log

- 2026-06-12: Started after user asked to add a stronger rule for studying
  references before implementation.
- 2026-06-12: Added the reference study gate to `AGENTS.md`,
  `AI_PIPELINE.md`, primary GDD, gameplay iteration, visual art direction, and
  `gamedesign/knowledge/reference_deconstruction.md`.
- 2026-06-12: Evidence passed: `node tools/skills_sync.mjs`;
  `node tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md gamedesign/knowledge/reference_deconstruction.md tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md`.
- 2026-06-12: Reinforced after the Cow Evolution feedback: reference study is
  now phrased as a hard implementation gate, requires checked sources/current
  build mismatch, and must name the screenshot/scenario that proves the next
  pass.
- 2026-06-12: Added the operational "observe like a player before designing
  like a developer" rule: source quorum, first screen, first input, visible
  response, screen grammar, pacing, translation, and explicit not-enough cases.
- 2026-06-12: Evidence passed after operational rule update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md .claude/skills/game-feature-iteration/SKILL.md .claude/skills/primary-gdd-pipeline/SKILL.md`.
- 2026-06-12: Added the four-pass ref-study rule after user asked to improve
  the pipeline for studying references: source packet, player transcript,
  systems extraction, translation gate. Evidence passed:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/game-feature-iteration/SKILL.md .claude/skills/game-visual-art-direction/SKILL.md`.
- 2026-06-12: Added auditable ref-study rule after user asked how to study refs:
  central gameplay refs now require a durable deconstruction doc plus source
  matrix (source quality, checked date, what it proves, uncertainty), gameplay
  footage or long screenshot sequence for interaction claims, and a rule to
  stop implementation if the doc cannot answer a ref challenge.
- 2026-06-12: Evidence passed after auditable-rule update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/game-feature-iteration/SKILL.md`.
- 2026-06-12: Added the operational observation-ledger rule for studying refs:
  at least 5 timestamped/framed beats with visible screen state, player action,
  visible response, reward/UI feedback, and inferred meaning before any design
  summary or implementation. Evidence passed: `node tools/skills_sync.mjs`;
  `node tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`.
- 2026-06-12: Added a concrete Agent Action Sequence for "make it like X" or
  "this does not match the ref" cases: state the reference question, collect
  source packet, record evidence, transcribe visible actions, extract systems,
  translate safely, compare against the native build, and name the next proof.
- 2026-06-12: Evidence passed after Agent Action Sequence update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .claude/skills/primary-gdd-pipeline/references/reference-research-playbook.md tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md`.
- 2026-06-13: Added reference-study modes after user asked to improve how refs
  are studied: quick checks for narrow details, central deconstruction for
  named gameplay/art drivers, and deep deconstruction for first-session pacing,
  one-hour balance, child-test UX, retention/monetization pressure, or
  release-critical readability.
- 2026-06-13: Evidence passed after study-mode rule update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/primary-gdd-pipeline/references/reference-research-playbook.md tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md`.
- 2026-06-13: Added the "studied means source-backed" rule after user asked to
  improve how refs are studied: a gameplay ref cannot be called studied unless
  the durable artifact contains source links/paths, checked dates,
  gameplay/video or long screenshot evidence, official/store/trailer visuals
  when available, supporting guide/review/deconstruction sources for balance
  claims, timestamped/framed observations, and a current native mismatch
  capture.
- 2026-06-13: Evidence passed after source-backed study rule update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md .claude/skills/primary-gdd-pipeline/references/reference-research-playbook.md .claude/skills/game-feature-iteration/SKILL.md tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md`.
- 2026-06-13: Added the Reference Lock after user asked to improve the
  reference-study rule: before implementation/final art, agents must name the
  study mode, question, doc path, source packet, current native capture plan,
  no-coding boundary, and expected native proof; implementation only unlocks
  after observed source facts, current-build mismatch, borrow/avoid/copy-risk,
  and one scoped next pass are recorded.
- 2026-06-13: Evidence passed after Reference Lock update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .claude/skills/primary-gdd-pipeline/SKILL.md tasks/STATUS.md`;
  `rg -n "[ \t]+$" AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md` returned no matches.
- 2026-06-13: Added the Reference Study Definition of Ready after user asked to
  improve the rule for studying refs: the gate now has explicit ready criteria
  and a required "not ready for implementation" stop state when evidence is
  missing, instead of letting agents continue from memory, vibes, or a partial
  source packet.
- 2026-06-13: Evidence passed after Definition of Ready update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md AI_PIPELINE_ITERATION_LOG.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/game-feature-iteration/SKILL.md .claude/skills/game-visual-art-direction/SKILL.md tools/skills_eval.mjs tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md`;
  `rg -n "[ \t]+$" AGENTS.md AI_PIPELINE.md AI_PIPELINE_ITERATION_LOG.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md tools/skills_eval.mjs tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md` returned no matches.
- 2026-06-13: Added Reference Intake after user asked to improve the ref-study
  pipeline again: after a named ref or "not like the ref" challenge, agents
  must state the exact reference question, study mode, durable doc path, source
  packet, current native capture plan/path, no-coding/no-final-art boundary,
  first proof screenshot/scenario, and evidence labels before defending,
  redesigning, coding, or generating final art.
- 2026-06-13: Evidence passed after Reference Intake update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`.
- 2026-06-13: Added Reference Digest after user asked to improve the ref-study
  pipeline: agents must show the lead the mode, sources checked, observed
  facts, current-build mismatch, borrow/avoid/copy-risk, and next native proof
  before implementation or final art resumes.
- 2026-06-13: Evidence passed after Reference Digest update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`.
- 2026-06-13: Added Source Ladder after user asked to improve the ref-study
  pipeline again: central/deep refs now require user-provided material,
  official/store/trailer visuals, raw gameplay or long screenshot evidence,
  then guides/reviews/lectures/deconstructions/wikis/community notes as
  supporting interpretation, with source role, checked date, watched/read
  scope, and evidence type recorded before conclusions.
- 2026-06-13: Evidence passed after Source Ladder update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md AI_PIPELINE_ITERATION_LOG.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/game-feature-iteration/SKILL.md .claude/skills/game-visual-art-direction/SKILL.md tools/skills_eval.mjs tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md`;
  `rg -n "[ \t]+$" AGENTS.md AI_PIPELINE.md AI_PIPELINE_ITERATION_LOG.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/game-feature-iteration/SKILL.md .claude/skills/game-visual-art-direction/SKILL.md tools/skills_eval.mjs tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md` returned no matches.
- 2026-06-13: Added parallel-reference rule after user asked to improve how
  refs are studied: research lanes may gather sources, frames, transcripts, and
  native mismatch evidence, but gameplay/UI/economy/balance/final-art
  implementation that depends on the named reference remains locked until the
  Reference Digest and proof target exist.
- 2026-06-13: Evidence passed after parallel-reference rule update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/game-feature-iteration/SKILL.md .claude/skills/game-visual-art-direction/SKILL.md tools/skills_eval.mjs tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md`;
  `rg -n "[ \t]+$" AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/game-feature-iteration/SKILL.md .claude/skills/game-visual-art-direction/SKILL.md tools/skills_eval.mjs tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md` returned no matches.
- 2026-06-13: Added Reference Evidence Board after user asked again to improve
  how refs are studied: central/deep refs now require concrete cited frames for
  first screen, first input, visible response, reward feedback,
  upgrade/progression UI, and friction/blocked state before the agent can claim
  the ref was studied enough for implementation.
- 2026-06-13: Evidence passed after Reference Evidence Board update:
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE.md gamedesign/knowledge/reference_deconstruction.md .codex/skills/primary-gdd-pipeline/SKILL.md .codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md .codex/skills/game-feature-iteration/SKILL.md .codex/skills/game-visual-art-direction/SKILL.md .claude/skills/primary-gdd-pipeline/SKILL.md .claude/skills/game-feature-iteration/SKILL.md .claude/skills/game-visual-art-direction/SKILL.md tools/skills_eval.mjs tasks/active/T0020-add-reference-study-gate-to-ai-pipeline.md tasks/STATUS.md`;
  `rg -n "[ \t]+$" ...` returned no matches for the same existing files.
