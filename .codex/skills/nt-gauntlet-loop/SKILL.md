---
name: nt-gauntlet-loop
description: Run an autonomous Gauntlet Loop for ambitious player-facing game work. Use when the user asks to gauntlet, keep improving, reach reference quality, fan out subagents, compare against another game or visual target, or continue until independent critics approve. Orchestrates builder subagents and separate fresh critics without ultracode or GSD.
---

# NT Gauntlet Loop

Turn one ambitious game-quality goal into parallel builder loops with independent critics.

## Set the bar

1. Read the repository instructions and the relevant game sources.
2. Restate the goal in one sentence.
3. Select the strongest concrete references available: shipped games, supplied images or video, accepted project targets, and current runtime captures.
4. Capture the current result before changing it.
5. Define success as an observable comparison, not “looks good for AI.”

Do not start from a vague quality claim when no artifact can be inspected. Obtain or create comparable evidence first.

## Preserve the rules

Create one invariant packet and include it verbatim in every builder and critic assignment:

```text
Goal:
Reference bar:
Player-visible success:
Hard project rules:
Owned scope:
Evidence to produce:
```

Include applicable `AGENTS.md` rules and the exact reference paths. Subagents do not inherit unstated assumptions. Re-read this skill and the invariant packet after any context reset.

Never invoke ultracode or a GSD workflow as part of this skill.

## Fan out

Break the goal into the smallest independently improvable parts. Prefer player-visible seams such as composition, environment, characters, animation, interaction feel, effects, UI, audio, performance, or a specific gameplay beat.

Spawn builder subagents only for parts that can progress independently. Give each builder explicit file or subsystem ownership. Tell every builder that other agents share the repository, to preserve unrelated changes, and to return:

- what changed;
- runnable or viewable evidence;
- checks run;
- remaining weaknesses.

Keep coupled work with one builder. Do not split merely to increase agent count.

## Run each gauntlet

For every part:

1. Let the builder produce the strongest version it can and capture the actual result.
2. Spawn a separate fresh critic with read-only responsibility. Do not give it the builder’s rationale or self-review.
3. Give the critic the reference and current result under matched conditions. For visual work, require side-by-side inspection at the same viewport, camera, state, and scale. Use neutral labels when practical.
4. Require the critic to return:
   - which result wins and why;
   - the largest visible or experiential gap;
   - concrete evidence, not taste-only language;
   - the next smallest change most likely to close the gap.
5. If the project result loses, send the finding to the builder, recapture the result, and ask a fresh critic to judge the new evidence.
6. Continue until the project result matches or beats the reference for that part, the user stops the run, or a genuine blocker requires the user.

A builder cannot approve its own work. Tests cannot substitute for seeing player-facing output. A critic cannot pass work it did not inspect.

## Integrate the wave

After independently improved parts land, inspect the whole experience. Use one fresh integration critic to find inconsistencies, regressions, and seams between individually strong parts. Fix those without redesigning accepted work.

Run the game’s real checks and replay the target experience. Compare the final result with the baseline and references.

## Stop honestly

Do not stop because the work is merely better, the diff is large, or a builder says it is done. Stop only when:

- independent critics judge every scoped part at the bar;
- the integrated experience still holds together;
- required runtime and mechanical checks pass;
- evidence is current;
- or the user stops the run or must resolve a real blocker.

Report the achieved result, evidence, critic verdicts, and any surviving gap. Never call an uninspected result “perfect.”

## Invocation example

```text
Use $nt-gauntlet-loop on the trolley game’s first five minutes. Fan out builders by independent player-facing seam, compare runtime captures against the accepted references, use a separate harsh visual critic after every attempt, and keep iterating until the critics prefer our result. Do not use ultracode or GSD.
```

