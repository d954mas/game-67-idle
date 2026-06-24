# AI Pipeline History

Compact log of durable, still-live process decisions. The dated per-session
retrospectives were removed in the 2026-06-24 cleanup (recoverable from the git tag
`pre-history-cleanup-2026-06-24`); their lessons are already encoded in `AGENTS.md`,
the skills, and `REFACTOR_PLAN.md`. Add a new entry only for a reusable decision
that is not captured elsewhere.

## Standing rule

If AI tooling creates work that does not directly help answer "what should we
build, change, or verify next in the game?", simplify the tool or move the behavior
behind an explicit deep mode. The harness repeatedly grew too heavy and had to be
cut back: **subtract before you add**, and the agent only pays for always-loaded
context, so cut hot docs and during-work blocking gates first — not on-demand LOC.

## External AI Observability Decision Criteria

The project profiles AI sessions with the local `tools/ai_profile/` JSONL profiler
and stays local-first. Do not add an external tracing/eval platform (LangSmith,
Phoenix, Langfuse, Braintrust, OpenTelemetry export, etc.) just because reflection
needs more data. Run a bounded side-by-side external pilot only when a concrete
trigger exists: multiple humans/agents need a shared trace dashboard; human
review/labels/annotations are part of the workflow; datasets/experiments/evals must
compare agent or model changes over the same inputs; production AI app
telemetry/cost/latency or online evals are required; OTLP export is needed for a
wider stack; or local JSONL review cannot answer an important repeated question
without manual reconstruction. Reject a tool that needs accounts, keys, servers, or
SDK wiring before the first useful question is known, that would capture
prompts/screenshots/child-test notes off the machine without an explicit privacy
decision, or that only duplicates local summary/review/follow-up outputs. A pilot
earns adoption only after it shows lower reflection/debug time, better
cross-agent/human review than local markdown/JSON, reusable datasets/evals that
prevent regressions, or production monitoring the project actually needs. Local
JSONL in `tmp/session_profiles/` stays the baseline evidence source unless the lead
explicitly changes that rule.
