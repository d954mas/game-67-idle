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

2. **Packet.** Generate a ready packet from a preset (§3), or the blank shape, then optionally lint:
   ```bash
   node tools/ai.mjs subagent-packet-template --preset codebase-map --targets src/world/**,state/world/**
   node tools/ai.mjs subagent-packet-template --preset asset-research   # or review|texture-gen|asset-intake
   node tools/ai.mjs subagent-packet-template                            # blank shape
   node tools/ai.mjs subagent-packet-check --stdin --json               # advisory lint, one packet at a time
   ```
   Presets emit bounded, lint-valid, **harness-neutral** packets. Every packet carries the 4 load-bearing slots — **objective / output format / tools-and-sources / boundaries** — without which agents "duplicate work, leave gaps, or fail to find necessary information" ([Anthropic](https://www.anthropic.com/engineering/multi-agent-research-system)). Workers start with **fresh, isolated context in BOTH harnesses** — they don't see your conversation, files, or invoked skills — so restate any rule that matters (e.g. "active concept is `<game-id>`; ignore archived prototypes"; "real raster only, no drawing code").

3. **Spawn — pick the primitive per task (the packet is the same; only the spawn wrapper differs — see §3b):**
   - **Main conversation** — back-and-forth, shared context, quick change. (default)
   - **Parallel fan-out (the context-isolation + parallelism win)** — N independent workers at once. Claude: *"Research X, Y, and Z in parallel using separate subagents, then synthesize."* Codex: *"Spawn one agent for X, one for Y, one for Z; wait for all; summarize."* 3–5 per batch.
   - **Large scripted fan-out** — Claude `ultracode` workflow (`parallel()`/`pipeline()`); Codex `spawn_agents_on_csv`; or a portable `for … & wait` loop over `codex exec -o` / `claude -p` (§3b).
   - **`/fork`** (Claude) — when a side task needs the current conversation as background (shares cache, inherits history, returns only its final message).

4. **Compressed structured return.** The packet must specify the return shape. A non-fork subagent returns **only its final message**; make it the `handoff:` block (findings / files / commands-evidence / risks / owner-action / not-done). For high-volume output, use the artifact pattern: worker writes the full output to a file and returns a lightweight pointer, so heavy tokens never hit the orchestrator ([Anthropic appendix](https://www.anthropic.com/engineering/multi-agent-research-system)).

5. **Integrate + gate the OUTPUT.** The lead verifies current files before copying any subagent claim into task/status/docs. Run the real gate for the work type:
   - Routine code: `node tools/ai.mjs validate` or focused tests.
   - Product/visual/native-playable: product gate with screenshot + fake-shot judgment; lead accept/reject.
   - Risky/irreversible: lead approves before it lands.
   - Fuzzy research/review with no deterministic check: one LLM-judge call against a rubric (accuracy / citations / completeness / source quality) — [Anthropic's most-reliable verifier](https://www.anthropic.com/engineering/multi-agent-research-system). Don't add a judge where a test suffices.

6. **Archive.** Close the task; the profiler record stays as passive telemetry. No closeout proof artifact.

**Debugging a disappointing worker:** re-read its transcript ("think like your agents") *before* editing its prompt — blind edits fix the wrong thing. The common real failures are continuing after it had enough results, overly specific search queries, and wrong tool choice.

---

## 3. Packet presets (implemented)

`subagent-packet-template --preset <name>` emits ready, bounded, lint-valid, **harness-neutral** packets — game-agnostic; fill the `<…>` parts. Parallel presets emit one packet per `--targets` entry; spawn them concurrently (§3b).

| Preset | Mode | Use | `--targets` |
|---|---|---|---|
| `codebase-map` | parallel | map disjoint subsystems, read-only | one bounded scope per worker |
| `review` | parallel | independent review, one axis per worker | axes, e.g. `correctness,readability,scope` |
| `asset-research` | single | CC0/CC-BY source + license + provenance | — |
| `texture-gen` | parallel | generate raster art (Codex imagegen → `agy` fallback) | one asset name per worker |
| `asset-intake` | sequential | research → generate → verify + propose | — |

```bash
node tools/ai.mjs subagent-packet-template --preset codebase-map --targets src/world/**,state/world/**
node tools/ai.mjs subagent-packet-template --preset texture-gen  --targets hero-idle,coin
node tools/ai.mjs subagent-packet-template --preset              # list presets
```

`allowed files` in every emitted packet is bounded (exact files, final-segment globs `dir/*.ext`, or scoped recursive globs `a/b/**` — never absolute paths, `..`, root globs like `src/**`, or "all files"). The return is the `handoff:` block (findings / files / commands-evidence / risks / owner-action / not-done); for bulky output the worker writes a file and returns a pointer. A review verdict is **input to lead judgment, not a gate**. Add a new preset only when you retype the same packet a 3rd time.

## 3b. Cross-harness fan-out (Claude + Codex)

The **packet body is the portable unit; only the thin spawn wrapper differs.** Never name a harness/tool/model inside the packet; use absolute paths (a worker's `cd` does not persist); declare tool policy as prose ("read-only: do not write or edit") so it holds even if the wrapper forgets to restrict tools.

| Capability | Claude Code | Codex CLI |
|---|---|---|
| Worker primitive | Agent tool; reusable `.claude/agents` defs (MD+YAML) | `spawn_agent` (in-session) / `codex exec` (headless); `.codex/agents` defs (TOML) |
| Fan out N parallel | N Agent calls in one turn: *"…in parallel using separate subagents"* | *"Spawn one agent per item, wait for all, then summarize"* |
| Large scripted fan-out | `ultracode` workflow → `parallel()` / `pipeline()` | `spawn_agents_on_csv` (one worker per row) |
| Portable script fan-out | `for i; do claude -p "$pkt" >out_$i & done; wait` | `for i; do codex exec -s read-only -o out_$i "$pkt" & done; wait` |
| Worker model | frontmatter `model:` / env `CLAUDE_CODE_SUBAGENT_MODEL` | TOML `model` / `codex exec -m <model>` |
| Read-only worker | `tools: Read, Grep, Glob` (or built-in **Explore**) | `sandbox_mode = "read-only"` / `codex exec -s read-only -a never` |
| Concurrency / depth | ~10–16 concurrent; nesting depth 5 | `[agents] max_threads = 6`, `max_depth = 1` (keep at 1) |
| Return to parent | final assistant message (in-process) | consolidated response / `-o` file / CSV `result_json` |

Enable Codex multi-agent first in `~/.codex/config.toml` → `[features] multi_agent = true` (leave `max_depth = 1` — recursive fan-out is a cost bomb). Both harnesses give each worker a **fresh, isolated context** and return only its final summary — that is the context-isolation win. Many *verbose* returns re-flood the parent in either harness: enforce the compact handoff and push bulky evidence to files.

### Image generation (Codex primary, Antigravity fallback)

Image gen is an external shell capability, **not** a subagent — invoke it the same way under either harness, via the `delegated-image-generation` skill:
- **Primary — Codex imagegen** (gpt-image-2): `.codex/skills/delegated-image-generation/scripts/codex_imagegen.sh`. Real output ≈ MBs; a code-drawn fake is < ~200KB.
- **Fallback — Antigravity `agy`**: `agy -p "use your built-in image generation to create one real raster image … save the PNG to <abs path>; do not write or run any drawing code"` (wrap in a pseudo-TTY under non-TTY).
- **Verify by file existence + size + eyeball, never by transcript** — the CLI fakes itself with a weak prompt by drawing via code. Size is a fake-detector, not quality proof; run the visual gate on the assembled screen.

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

1. **Packet presets — DONE.** `subagent-packet-template --preset codebase-map|review|asset-research|texture-gen|asset-intake [--targets …]` (§3). Harness-neutral, lint-valid, game-agnostic. Add a new preset only when you retype the same packet a 3rd time.
2. **Subagent telemetry — DONE.** The passive profiler now records Claude Agent/Task and Codex spawn_agent calls (new `Agent`/`spawn_agent` PostToolUse matchers route to `tools/ai_profile/hook_record.mjs`), and `node tools/ai.mjs status` shows `Subagents delegated: N (by type)` plus a `## Subagents Delegated (advisory)` list of objectives. The lead reads this to report what it delegated. **Strictly diagnostic — never an acceptance condition.** (New hook matchers take effect on the next session.)
3. **A saved scripted fan-out (medium) — add when the same read-heavy fan-out recurs.** Claude: save a good `ultracode` run as `/<name>` (`s` in `/workflows`). Codex: a small `spawn_agents_on_csv` or `for … codex exec … & wait` wrapper. Invocable, not a mandatory step. The §3 presets already feed these.
4. **A reusable read-only researcher worker (tiny) — ONLY after you spawn the same researcher repeatedly.** Claude `.claude/agents/researcher.md` (`tools: Read, Grep, Glob`, `model: haiku`) or Codex `.codex/agents/researcher.toml` (`sandbox_mode = "read-only"`). One-offs stay inline; do not pre-build an agent zoo.

If none of these remaining frictions recur, add none of them. That is the correct outcome.

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
