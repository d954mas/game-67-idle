---
type: Source Notes
title: Idle / Incremental Game Design Sources
description: Raw source notes for reusable idle and incremental game design knowledge.
tags: [sources, idle, incremental, economy, prestige]
timestamp: 2026-06-16T00:00:00Z
---

# Sources - Idle / Incremental Game Design (checked 2026-06-16)

Reusable source notes for idle/incremental design. Each claim is labeled:
`observed` (read in the source), `secondary`, or `inferred`.

Project-specific application belongs in project wiki/reference files. Current
The former Voxelheim application was removed with the stopped prototype; use
git history if that project-specific application is needed for comparison.

## Source Matrix

| Source | Link | Quality | Covers |
|---|---|---|---|
| Pecorella, "Quest for Progress: The Math and Design of Idle Games" (GDC Europe 2016) | gdcvault.com/play/1023876; PDF: media.gdcvault.com/gdceurope2016/presentations/Pecorella_Anthony_Quest%20for%20Progress.pdf; slides: slideshare.net/slideshow/quest-for-progress-gdc-europe-2016/65405507 | GDC talk (authoritative) | idle math: cost/production formulas, prestige tuning, generator relevance, offline caps |
| "Idle Games: The Mechanics and Monetization of Self-Playing Games" (GDC) | gdcvault.com/play/1022065 | GDC talk | core + meta loop, retention, F2P viability |
| Eric Guan, "Idle Game Design Principles" | ericguan.substack.com/p/idle-game-design-principles | dev essay | reengagement cadence, currency/playstyle, growth curves |
| "Lessons of my first incremental game" | gamedeveloper.com/design/lessons-of-my-first-incremental-game | postmortem | data-driven economy architecture, scope, scalability triangle |
| "Incremental game" (overview) | en.wikipedia.org/wiki/Incremental_game | wiki (secondary) | genre overview, terminology |

## Key Takeaways

- **Cost vs value growth (Pecorella, Guan, observed).** Cost grows
  exponentially (`cost_next = cost_base * rate^owned`); the cost exponent must
  be steeper than the value/production exponent. Guan's example shape is cost
  x1.15 vs production x1.1. The differential is the pacing tension and the
  "one more upgrade" pull.
- **Prestige tuning (Pecorella, observed).** Optimal reset is when a prestige
  grants +50% to +200% meta-currency vs the last run. Scale meta currency with
  a fractional/sqrt exponent so meaningful prestige points come regularly.
- **Reengagement cadence (Guan, observed).** Match production-clock lengths to
  decaying session frequency; stagger timers exponentially, such as 20 minutes,
  5 hours, and 2 days, so a player feels productive at their actual check-in
  rate.
- **Offline caps (Pecorella, observed).** Egg Inc's 2h offline cap caused his
  own churn; caps that are too tight punish idlers. Be generous.
- **Player motivators (Pecorella, Quantic Foundry survey, secondary).** Idle
  players index on Completion + Power; both must feel tangible.
- **Architecture (gamedeveloper, observed).** Build the economy data-driven
  (values / formulas / happenings / interface) so balance lives in data, not
  code; automate balance estimation early. Biggest mistake: no deadline leading
  to scope creep. Scalability triangle: long playtime / content volume / fun;
  pick 2.

## Candidate Knowledge Updates

- A future reusable idle/incremental knowledge page should promote only the
  generic economy, prestige, offline, reengagement, and data-driven-balance
  rules above.
- Keep concrete game balance applications in the active project wiki.
