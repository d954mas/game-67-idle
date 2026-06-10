# Studio GDD Patterns

Load this reference only when a task asks for deeper GDD methodology, studio
practice, market research, AI-assisted preproduction, or process improvement.

## Distilled Rules

- Keep the first GDD lightweight: concept, player fantasy, core loop, verbs,
  pillars, first playable slice, risks, and visual proof before broad content.
- Treat the GDD as a living production artifact. Record current decisions,
  source-of-truth order, open questions, and stale sections.
- Use design pillars as filters. Every major mechanic, UI decision, visual
  direction, and economy loop should support at least one pillar.
- Document player verbs before feature lists. If the player does not click,
  choose, compare, wait, upgrade, move, trade, risk, or recover, the feature may
  be decorative.
- Separate concept, pitch, functional spec, balance data, art direction, and
  implementation handoff. Do not hide numbers and flows in prose only.
- Use reference research as a comparison table, not a mood dump: borrow,
  avoid, copy-risk, core loop, UI density, economy, progression, session length.
- Force early visual proof. A fake gameplay screenshot is more useful than ten
  pages of abstract design when the user needs to see the game.
- Add risk gates before implementation: fun, UX, production, technical,
  content pipeline, and licensing/source risk.
- Use AI generation as a fast preproduction tool, but label outputs as
  reference, placeholder, fake shot, or runtime-ready. Runtime-ready requires
  separate assets, dimensions, transparency checks, manifest, and composed proof.
- Validate with playtests or runnable slices. Documents and validators only
  prove alignment; they do not prove that the game works.

## Research Sources

- Tim Ryan, "The Anatomy of a Design Document" on Game Developer.
  Use for document structure, concept/proposal framing, and keeping docs
  production-facing:
  https://www.gamedeveloper.com/design/the-anatomy-of-a-design-document-part-1-documentation-guidelines-for-the-game-concept-and-proposal
- Hunicke, LeBlanc, Zubek, "MDA: A Formal Approach to Game Design and Game
  Research." Use for mapping mechanics -> dynamics -> aesthetics instead of
  listing features without player experience:
  https://users.cs.northwestern.edu/~hunicke/MDA.pdf
- Valve publications, including playtesting, design process, and art/gameplay
  connection talks. Use for playtest culture, iteration, and observing player
  behavior instead of trusting docs:
  https://www.valvesoftware.com/en/publications
- Automated Unity Game Template Generation from GDDs via NLP and Multi-Modal
  LLMs. Use for AI/GDD-to-prototype opportunities and limitations:
  https://arxiv.org/abs/2509.08847
- Towards Game Design via Creative Machine Learning. Use for AI ideation limits,
  novelty, and human creative direction:
  https://arxiv.org/abs/2008.13548
- Automatic Playtesting for Game Parameter Tuning via Active Learning. Use for
  balance-simulation and parameter-tuning risk gates:
  https://arxiv.org/abs/1908.01417

## Public Skill Search Note

Quick web searches for public, reusable "GDD AI agent skill" examples were
sparse. Do not copy random prompt templates as methodology. Prefer this skill's
stage gates plus the sources above.

## Checklist For A Serious First GDD

1. Concept: hook, audience, platform, session, fantasy.
2. Pillars: 3 pillars, proof examples, anti-examples.
3. Player verbs: exact actions and decisions.
4. First loop: input, wait, reward, upgrade, visual/status change.
5. Economy: currencies, sources, sinks, pacing, unlocks.
6. UX map: screens, tabs, primary CTA, blocked states, feedback.
7. Visual proof: fake shot first, runtime asset pack only when needed.
8. Ref pack: 3-7 refs with borrow/avoid/copy-risk.
9. Risk gates: fun, UX, production, tech, asset/legal.
10. Handoff: file order, commands, test plan, next implementation prompt.
