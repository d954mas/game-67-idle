---
name: nt-gauntlet-loop
description: "Run repeated building and independent criticism when the user asks for a gauntlet, reference-quality comparison, or improvement until critics approve."
---

# NT Gauntlet Loop

Improve the requested game-quality goal against concrete references, using
independent criticism to judge the result.

## Set the bar

Use the request, accepted project targets, and relevant sources to define the
scope and an observable quality bar. References may be shipped games, supplied
images/video, or accepted captures. Reuse current matched evidence; capture a
new baseline when existing evidence is stale or incomparable.

Do not start from a vague quality claim when no artifact can be inspected. Obtain or create comparable evidence first.

## Preserve the rules

Give builders and critics the goal, reference paths, acceptance criteria,
applicable project rules, owned scope, and evidence required. Preserve this
working state across context resets; recover missing constraints before
continuing.

Never invoke ultracode or a GSD workflow as part of this skill.

## Choose the work split

Keep coupled work with one builder, which may be the lead agent. Delegate
independent parts when parallel work reduces time or review risk. Give each
builder file/subsystem ownership and tell it to preserve other agents' changes.
Require changes, runnable/viewable evidence, checks, and remaining weaknesses
in its return.

## Run each gauntlet

1. Build the scoped result. Capture changed player-visible output; use targeted
   checks for internal changes.
2. Use a separate read-only critic to inspect the reference and current result.
   Keep the builder's rationale and self-review out of the critic's packet.
   Compare visuals side by side at matching viewport, camera, state, and scale;
   use neutral labels when practical.
3. Ask which result wins, the largest gap, concrete evidence, and the smallest
   change likely to close it.
4. Address the gap and obtain a fresh judgment of changed evidence. Recapture
   when the change affects the judged pixels or interaction. If attempts stop
   improving the result, revise the approach rather than repeat unchanged work.
5. Continue toward the agreed bar within the user's scope and any supplied
   budget. Report an unmet bar honestly if the budget ends or a blocker remains.

A builder cannot approve its own work. Tests cannot substitute for seeing player-facing output. A critic cannot pass work it did not inspect.

## Integrate the wave

When independently changed parts are combined, use an independent integration
critic to inspect the whole experience for inconsistencies and regressions.
Fix those while preserving accepted work.

Run the game’s fast gate and the targeted checks for changed subsystems. Replay
and capture the target experience only for player-visible work. Run the full
release gate once at a release boundary or when the user requests it.

## Stop honestly

Do not stop because the work is merely better, the diff is large, or a builder says it is done. Stop only when:

- independent critics judge every scoped part at the bar;
- the integrated experience still holds together;
- required runtime and mechanical checks pass;
- evidence is current;
- or the user stops the run, a supplied budget ends, or a real blocker needs
  the user's decision.

Report the achieved result, evidence, critic verdicts, and any surviving gap. Never call an uninspected result “perfect.”

