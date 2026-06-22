---
id: T0110
title: "M3: careers, relationships, skills, families"
status: review
epic: E007
priority: P3
tags: [little-lives, milestone-3, systems]
created: 2026-06-22
updated: 2026-06-22
---

## What

Add Sims-like progression systems on top of the sandbox: skills, a career with
promotions, relationships between Sims, and family households.

## Done when

- [x] Skills: per-Sim cooking/logic/charisma (0..10) that grow from using the
      matching object (fridge/computer/sofa) and from socializing.
- [x] Career: going to work pays § scaled by career level + logic skill;
      performance accrues to promotions (up to CAREER_MAX_LEVEL).
- [x] Relationships: pairwise matrix; socializing raises friendship (faster with
      charisma); housemates start as family.
- [x] Families: each lot is a household; members share a warm starting bond.
- [x] Skills + career shown in HUD; all exposed via DevAPI; verified + screenshot.

## Open questions

- Romance/marriage, having children, and skill-gated career tracks are future work.

## Log

- Sim gains `skills[3]`, `career_level`, `career_perf`, `shifts`; global
  `s_rel[N][N]` relationship matrix (housemates init 45).
- Sim behaviour: USING trains object_skill; SOCIAL raises charisma + s_rel
  (charisma-scaled); WORK pays `WORK_SALARY*(1+0.4*level)*(0.7+0.06*logic)`,
  accrues perf (mood+logic), promotes at 100.
- HUD: 3 skill mini-bars + career-level pips for the selected Sim.
- DevAPI sims[] adds skills/career/relationships.
- Verified: `python tmp/ll_m3.py` — cooking 0.00->0.43 (fridge), rel Alex->Bella
  45->48.8 (social), +224§ one shift, perf 34.
