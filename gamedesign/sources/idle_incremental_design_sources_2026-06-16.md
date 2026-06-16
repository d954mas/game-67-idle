# Sources — Idle / Incremental Game Design (checked 2026-06-16)

Reusable source notes (per AGENTS.md `gamedesign/sources/`) backing the Voxelheim
idle GDD + `data/balance.json` and a future `gamedesign/knowledge/` page. Each
claim is labeled: `observed` (read in the source) / `secondary` / `inferred`.

## Source matrix

| Source | Link | Quality | Covers |
|---|---|---|---|
| Pecorella, "Quest for Progress: The Math and Design of Idle Games" (GDC Europe 2016) | gdcvault.com/play/1023876 · PDF: media.gdcvault.com/gdceurope2016/presentations/Pecorella_Anthony_Quest%20for%20Progress.pdf · slides: slideshare.net/slideshow/quest-for-progress-gdc-europe-2016/65405507 | GDC talk (authoritative) | **the idle MATH**: cost/production formulas, prestige tuning, generator relevance, offline caps |
| "Idle Games: The Mechanics and Monetization of Self-Playing Games" (GDC) | gdcvault.com/play/1022065 | GDC talk | core + meta loop, retention, F2P viability |
| Eric Guan, "Idle Game Design Principles" | ericguan.substack.com/p/idle-game-design-principles | dev essay | reengagement cadence, currency/playstyle, growth curves |
| "Lessons of my first incremental game" | gamedeveloper.com/design/lessons-of-my-first-incremental-game | postmortem | data-driven economy architecture, scope, scalability triangle |
| "Incremental game" (overview) | en.wikipedia.org/wiki/Incremental_game | wiki (secondary) | genre overview, terminology |

## Key takeaways (observed in the sources)

- **Cost vs value growth (Pecorella, Guan).** Cost grows exponentially
  (`cost_next = cost_base * rate^owned`); the COST exponent must be steeper than
  the value/production exponent (Guan: cost ×1.15 vs production ×1.1). The
  *differential* is the pacing tension and the "one-more-upgrade" pull.
- **Prestige tuning (Pecorella).** Optimal reset is when a prestige grants
  **+50% to +200%** meta-currency vs the last run. Scale the meta currency with a
  **fractional/sqrt exponent** so meaningful prestige points come regularly.
- **Reengagement cadence (Guan).** Match production-clock lengths to decaying
  session frequency; **stagger timers exponentially** (e.g. 20min / 5h / 2day
  caps) so a player feels productive at their actual check-in rate.
- **Offline caps (Pecorella).** Egg Inc's **2h** offline cap caused his own
  churn — caps that are too tight punish idlers; be generous.
- **Player motivators (Pecorella, Quantic Foundry survey).** Idle players index on
  **Completion + Power** — both must feel tangible.
- **Architecture (gamedeveloper).** Build the economy **data-driven** (values /
  formulas / happenings / interface) so balance lives in data, not code; automate
  balance estimation early. Biggest mistake: **no deadline → scope creep**.
  "Scalability triangle": long playtime / content volume / fun — pick 2.

## Application to Voxelheim (`data/balance.json`)

- VALID: our cost growth (×1.09) < monster HP/gold growth (×1.15 / ×1.18) keeps
  the cost-vs-value differential — but verify upgrades stay affordable (Guan
  cost ×1.15 vs prod ×1.1 is the canonical shape).
- TUNE: our prestige `frost_shards = floor((stage/10)^1.5)` uses exponent **1.5
  (super-linear)** — Pecorella says use a **fractional/sqrt** exponent for
  regular prestige points; consider ~`^0.7` and target +50-200% shards/run.
- KEEP: offline cap 8h (more generous than Egg Inc's churn-causing 2h).
- DESIGN: lean into Completion (stages/bosses milestones) + Power (visible
  damage/kill-speed jumps) per the motivator data. Keep the economy in
  `balance.json` (data-driven), as gamedeveloper advises.
