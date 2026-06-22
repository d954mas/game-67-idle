# Subagent Orchestration Playbook (game-67-idle)

Operating principle: **single-agent by default; acceptance gates the work product, not the delegation; the lead is the backstop.** Subtract before you add. This playbook tells the lead how to actually run delegation day-to-day with the lean tools already kept (`subagent-packet-template`, advisory `subagent-packet-check`, `orchestration-template`/`orchestration-check`/`orchestration-bootstrap`, the passive profiler `node tools/ai.mjs status`, and the real OUTPUT gates `validate` / tests / product-gate).

---

## 1. When to delegate (decision tree)

Apply in seconds. Start at the top; first match wins.

```
Is this the immediate blocker, ambiguous-scope, or a quick targeted edit?  -> SINGLE-AGENT (stay in main thread)
Does step B need step A's full output (a pipeline)?                        -> SINGLE-AGENT (it's sequential, not parallel)
Will it WRITE coupled files (mechanic + state + UI + render together)?     -> SINGLE-AGENT (worst multi-agent fit)
Otherwise, is it READ-ONLY or an isolated artifact the lead integrates,
  AND independent of siblings (no shared mutable state),
  AND hits a real threshold (~10+ files to read, OR 3+ disjoint pieces,
    OR it would flood the lead's context, OR needs a cheaper model)?       -> DELEGATE
Anything else                                                              -> SINGLE-AGENT
```

If you can't write a self-contained packet without a sibling's in-flight context, the work isn't separable — don't split it. The dominant failure here is **over-delegation, not under-delegation** ([Anthropic shipped a real bug spawning 50 subagents for simple queries](https://www.anthropic.com/engineering/multi-agent-research-system)).

**Good candidates (read-heavy, independent, summary-back):**
- Asset/source/license research feeding `game-texture-generation` / `design-source-knowledge`.
- Raster generation + verify-by-size via `delegated-image-generation` (isolated, verbose, cheap-model friendly).
- Codebase exploration of disjoint subsystems — e.g. one agent traces `src/` runtime asset loading, one maps `state/` + `tools/state_codegen`, one maps DevAPI — each returns a short brief.
- Independent review of one finished artifact from different angles (readability / state-schema correctness / art-direction diff). These never conflict because none of them writes — highest-value, lowest-risk multi-agent use for a solo lead.

**Bad candidates (keep single-agent):**
- The first-screen coupled slice from AGENTS.md (one location, primary path, next action, progress, locks) — mechanic+state+UI+render touch the same files and must agree.
- Any overlapping write to the same task file, runtime module, generated runtime pack, or hot doc (`AGENTS.md`, `AI_PIPELINE.md`, `tasks/STATUS.md`).
- Game-state mutation, UI layout, balance changes that must stay consistent. [Most coding tasks have fewer truly parallelizable pieces than research](https://www.anthropic.com/engineering/multi-agent-research-system); forcing parallelism creates integration debt (the "Flappy Bird background on a Mario level" failure, [Cognition](https://cognition.ai/blog/dont-build-multi-agents)).

---

## 2. How to run it (the loop)

```
SCOPE -> PACKET -> SPAWN -> COMPRESSED RETURN -> INTEGRATE + GATE -> ARCHIVE
```

1. **Scope.** Decide go/no-go via §1. If substantial, drop one line in the task `## Log`: `- orchestration: used ...` or `- orchestration: not needed - small scope: ...`. This is a *thinking* prompt, not a proof gate.

2. **Packet.** Generate the shape, fill it, optionally lint:
   ```bash
   node tools/ai.mjs subagent-packet-template               # or: node tools/taskboard/cli.mjs subagent-packet-template
   node tools/ai.mjs subagent-packet-check --file packet.txt --json   # advisory shape lint
   ```
   For inline prompts, copy the template shape directly. Every packet carries the 4 load-bearing slots — **objective / output format / tools-and-sources / boundaries** — without which agents "duplicate work, leave gaps, or fail to find necessary information" ([Anthropic](https://www.anthropic.com/engineering/multi-agent-research-system)). Subagents start with **fresh context** — they don't see your conversation, files, or invoked skills, so restate any rule that matters (e.g. "active concept is Dragon Grove; ignore ember-road").

3. **Spawn — pick the primitive per task:**
   - **Main conversation** — back-and-forth, shared context, quick change. (default)
   - **Claude Agent tool** — 1–3 self-contained workers returning a summary; restrict tools (read-only researchers get `Read, Grep, Glob`). [Claude Code subagents](https://code.claude.com/docs/en/sub-agents).
   - **`/fork`** — when a side task needs the current conversation as background (shares prompt cache, inherits history, returns only its final message).
   - **Workflow tool** — big read-heavy fan-out (the codebase-map / asset-intake recipes in §3).
   - **Codex `spawn_agent`** — when you want the Codex harness (e.g. its imagegen path).
   - Trigger parallel reads in one message: *"Map the src/ asset loader, state codegen, and DevAPI in parallel using separate read-only subagents."* Issue independent Read/Grep/Glob in a single turn, not one at a time.

4. **Compressed structured return.** The packet must specify the return shape. A non-fork subagent returns **only its final message**; make it the `handoff:` block (findings / files / commands-evidence / risks / owner-action / not-done). For high-volume output, use the artifact pattern: worker writes the full output to a file and returns a lightweight pointer, so heavy tokens never hit the orchestrator ([Anthropic appendix](https://www.anthropic.com/engineering/multi-agent-research-system)).

5. **Integrate + gate the OUTPUT.** The lead verifies current files before copying any subagent claim into task/status/docs. Run the real gate for the work type:
   - Routine code: `node tools/ai.mjs validate` or focused tests.
   - Product/visual/native-playable: product gate with screenshot + fake-shot judgment; lead accept/reject.
   - Risky/irreversible: lead approves before it lands.
   - Fuzzy research/review with no deterministic check: one LLM-judge call against a rubric (accuracy / citations / completeness / source quality) — [Anthropic's most-reliable verifier](https://www.anthropic.com/engineering/multi-agent-research-system). Don't add a judge where a test suffices.

6. **Archive.** Close the task; the profiler record stays as passive telemetry. No closeout proof artifact.

**Debugging a disappointing worker:** re-read its transcript ("think like your agents") *before* editing its prompt — blind edits fix the wrong thing. The common real failures are continuing after it had enough results, overly specific search queries, and wrong tool choice.

---

## 3. Packet & return recipes

Copyable. Fill the angle-bracket parts. `allowed files` must be bounded repo-local paths/patterns (exact files, final-segment globs like `tasks/active/T*.md`, scoped recursive globs like `tools/state_codegen/**`) — never absolute paths, `..`, broad root globs, or "all files".

### A. Asset / source / license research (1 agent, cheap model, read-only)
```text
objective: Find 1 CC0/CC-BY dragon sprite suitable for Dragon Grove merge-3 tiles; report license + provenance + integrity, do not import.
allowed files: gamedesign/sources/**, gamedesign/knowledge/**   (read), tmp/** (write notes only)
forbidden files: src/**, state/**, any runtime pack, hot docs
project boundary: active concept = Dragon Grove; Y-up; real legal assets only; runtime uses project-local copies.
tool-use guard: prefer primary/authoritative sources over SEO content farms; verify each license URL; keep evidence read-only.
expected output: handoff block; candidate list <=5 with URL + license + provenance + integrity check.
evidence command or artifact: tmp/dragon-sprite-candidates.md
stop condition: 5 viable candidates found OR 8 sources checked with none viable.
handoff: findings / files / commands-evidence / risks / owner-action(import decision is the lead's) / not-done
```

### B. Codebase exploration of disjoint subsystems (2–3 parallel, read-only, Haiku/Sonnet)
Spawn one per subsystem in a single message. Per-agent packet:
```text
objective: Map how <subsystem> works and its public entry points; produce a <=200-word brief, no edits.
allowed files: <e.g. src/**>  (read only — Read, Grep, Glob)
forbidden files: everything else; make no edits
tool-use guard: verify paths with rg --files before reading; window large files with Select-Object -Skip/-First.
expected output: brief = entry points (file:line) + data flow + 3 risks/unknowns.
evidence command or artifact: inline handoff (small)
stop condition: entry points + flow identified, or 10 files read.
```
Lead concatenates the 2–3 briefs and synthesizes — no agent writes, so they can't conflict.

### C. Independent review of a finished artifact (1–3 parallel, read-only, fresh slate)
```text
objective: Review <file/feature> for <ONE axis: readability | state-schema correctness | art-direction match>. Verdict + concrete issues only.
allowed files: <the artifact + its direct deps>  (read only)
forbidden files: no edits anywhere
project boundary: judge against <art_contract / state schema / AGENTS.md first-screen rules>; you are a fresh reviewer, not the builder.
expected output: verdict (pass/concerns/fail) + issue list (file:line + one-line fix), <=10 bullets.
evidence command or artifact: inline handoff
stop condition: artifact fully reviewed on the one assigned axis.
```
Lead reconciles verdicts; reviewer PASS is **input, not a gate**.

### D. Parallel texture/icon generation (1 per asset, isolated, cheap model)
```text
objective: Generate <asset> per delegated-image-generation; verify-by-size; propose a project-local path, do not wire into runtime.
allowed files: tmp/gen/**, scripts/codex_imagegen.sh (read)
forbidden files: src/**, generated runtime pack, any manifest, hot docs
project boundary: active concept = Dragon Grove visual direction; placeholder is debug-only.
tool-use guard: verify output file exists and dimensions match the request before returning.
expected output: handoff with artifact path + dimensions + a one-line fake-shot self-judgment.
evidence command or artifact: tmp/gen/<asset>.png
stop condition: one verified asset produced OR 3 generation attempts fail.
```
Each writes only its own `tmp/gen/<asset>.png` — provably disjoint. Lead does cutting/manifesting/runtime-wiring (those are hot/coupled).

---

## 4. Cost & safety discipline

- **Single-agent default.** [Find the simplest solution; only add complexity when needed](https://www.anthropic.com/engineering/building-effective-agents). Linear single-thread "gets you surprisingly far in reliability" ([Cognition](https://cognition.ai/blog/dont-build-multi-agents)).
- **The 15x reality.** Agent ≈ 4x chat tokens; multi-agent ≈ 15x; [token usage alone explains ~80% of performance variance](https://www.anthropic.com/engineering/multi-agent-research-system). Decide economics **before** spawning: multi-agent only pays when task value > 15x cost AND the work is genuinely parallel. Routine feature iteration: almost always single-agent.
- **Effort dial (size the fan-out):** fact-find = 1 agent / 3–10 tool calls; comparison or disjoint multi-file read = 2–4 agents / 10–15 calls each; 5+ only for a true multi-context-window job. Start one rung lower than you think; climb only when the lower rung visibly underdelivers.
- **Hard caps (you are the circuit breaker — the published architectures have none):** depth = 1, flat only — **subagents never spawn subagents** (recursive fan-out is the primary cost blowup, can multiply cost 10x *on top of* 15x). Concurrency ≤ ~6 parallel threads. Set a per-run token/$ budget up front.
- **Parallel-read / serial-write.** Fan out reads freely; never let two agents write coupled or overlapping files. "Actions carry implicit decisions, and conflicting decisions carry bad results."
- **Capability boundary > prose boundary.** Restrict with tools (`tools: Read, Grep, Glob`) instead of instructing "don't edit." The harness enforces it for free.
- **Model tiering.** Lead on the strong model; Haiku for search/exploration, Sonnet for analysis. [Lead=Opus + subagents=Sonnet beat single-agent Opus by 90.2%](https://www.anthropic.com/engineering/multi-agent-research-system) on Anthropic's eval.
- **Stop conditions in every packet.** Workers over-run without an explicit "stop when X" (continuing after enough results is a top observed failure).
- **Lead-as-backstop risk tiers:**
  - *Low* (research brief, exploration map, review verdict) — lead reads + `validate`/tests.
  - *Medium* (generated asset, isolated code artifact) — product gate / focused test before integrate.
  - *High* (anything irreversible: deletes, commits, pack regen, schema migration) — lead executes or explicitly approves; never delegated to land autonomously.

---

## 5. Worth adding (prioritized, friction-justified)

Each tied to a real repeating friction. Add only when the friction actually appears — a preset/view/recipe, never a required step.

1. **Packet presets (small, ~½ day) — add when you retype the same packet 3rd time.** Ship 2–3 fill-in presets on the existing command, e.g. `node tools/ai.mjs subagent-packet-template --preset texture-research|codebase-map|visual-review`, reusing `subagentPacketTemplate()` in `tools/taskboard/cli.mjs`. Zero new gate, just less typing. Maps to recipes A/C above.
2. **Subagent telemetry line (small, ~½ day) — add when "I don't know what ran this session" recurs.** The profiler currently records only shell/command tool calls (`tools/ai_profile/hook_record.mjs` exits early on non-command tool calls), so Agent/Task spawns are invisible. Add a tiny non-command branch recording subagent tool name + objective first line, then surface `Subagents this session: N (objectives...)` in `tools/ai_profile/status.mjs`. **Strictly diagnostic — never an acceptance condition.**
3. **Two Workflow recipes (medium, ~1 day) — add when the same read-heavy fan-out recurs.** A `codebase-map` recipe (N disjoint read-only Explore-style agents, one per subsystem, concatenate briefs) and an `asset-intake` recipe (research license → generate/download → verify-by-size → propose project-local copy). Invocable recipes, not mandatory steps. Recipes B and D above.
4. **A reusable read-only researcher agent file (tiny) — ONLY after you spawn the same researcher repeatedly.** A `.claude/agents/researcher.md` with `tools: Read, Grep, Glob` and `model: haiku`. One-offs stay inline; do not pre-build an agent zoo.

If none of these frictions recur, add none of them. That is the correct outcome.

---

## 6. DO NOT add (regression guardrails)

These re-create the just-removed proof layer. Each one gates the *delegation* instead of the *work product* — the exact ceremony to stay deleted:

- **Machine proof-of-delegation** — any check that a subagent *was* spawned, or counts of spawns required to pass.
- **Per-task evidence JSON / closeout evidence artifacts** — acceptance is `validate`/tests/product-gate on the output, never a recorded proof that delegation happened.
- **Workflow-manifest gates** — the large-workflow plan is cold planning state for resume; never required for closeout, never validated as proof.
- **Required reviewer-PASS block** — reviewer verdicts are *input* to lead judgment, not a hard gate; the lead is the backstop.
- **Task-id / file-count thresholds that *block*** — the §1 thresholds are advisory go/no-go heuristics, never enforced acceptance conditions.
- **Transcript counting / process-policing eval** — judge final state, not how the worker did it. The moment you audit *how* delegation was split, delete that check.
- **Turning any §5 ergonomic (preset / telemetry line / recipe) into a mandatory step** — ergonomics that become required steps regress straight back to ceremony.

Rule of thumb: if a new check inspects the *delegation* rather than the *deliverable*, it is the proof layer coming back — cut it.

---

Operationalizes `docs/ai-pipeline/subagent-protocol.md` (the lean method). Tooling: `tools/taskboard/cli.mjs` (packet/orchestration commands), `tools/ai_profile/status.mjs` + `tools/ai_profile/hook_record.mjs` (passive profiler; the section 5.2 telemetry add-point).
