# AI Observability Tooling Decision Gate

Purpose: decide when an AI-first project should keep using the local
`tools/ai_profile/` JSONL profiler, when it should run a bounded external
observability/evaluation pilot, and when an external system is a credible
adoption candidate.

The default is local-first. A tool that needs accounts, keys, servers, SDK
configuration, or manual trace cleanup must earn its place by reducing
reflection/debug time or enabling a workflow that local JSONL cannot provide.

## What Current Tools Converge On

Checked 2026-06-13:

- OpenTelemetry has GenAI semantic conventions and has moved the GenAI docs
  into the semantic-conventions repository:
  https://opentelemetry.io/docs/specs/semconv/gen-ai/
  and
  https://github.com/open-telemetry/semantic-conventions/tree/main/docs/gen-ai
- LangSmith frames the workflow as tracing, production monitoring, feedback,
  evaluation, dashboards, automations, and export:
  https://docs.langchain.com/langsmith/observability
- Arize Phoenix combines traces, evaluation, prompt iteration, datasets, and
  experiments, and accepts traces through OpenTelemetry/OTLP:
  https://arize.com/docs/phoenix
- Langfuse emphasizes LLM application tracing: prompts, model responses, token
  usage, latency, tools/retrieval steps, sessions, costs, evals, dashboards,
  and async batching:
  https://langfuse.com/docs/observability/overview
- Braintrust organizes the workflow as instrument, observe, annotate,
  evaluate, deploy, with traces, logs, human review, datasets, experiments, and
  monitoring:
  https://www.braintrust.dev/docs

Common pattern: traces/spans/events are the capture layer; dashboards,
annotation, datasets, evaluations, experiments, and monitoring are the
analysis/product layer. For our AI-development pipeline, the local JSONL
profile should cover the capture layer first. External tools are useful when
the analysis/product layer is needed by a team or by production-like AI app
work.

## Local-First Baseline

Stay with `tools/ai_profile/` as the default when:

- one local agent is working in one repo;
- reflection needs command timing, validation repeats, context inputs, work
  items, failures, and evidence paths;
- raw telemetry must remain local and ignored;
- the bottleneck is agent discipline, scope, validation batching, or context
  hygiene;
- a ready-made tool would require setup before we have a concrete dashboard,
  eval, annotation, or production monitoring need.

The local profiler must keep collecting these fields with low friction:

- `work_item`, `iteration`, `phase`, `category`;
- `intent`, `result`, `value`;
- `duration_ms`, `tools`, `commands`;
- `context_inputs`, `files_read`, `files_written`;
- validation scope, failure/recovery state, evidence paths;
- closeout summary, review JSON, and follow-up drafts.

## External Pilot Triggers

Run a bounded external observability pilot only when at least one trigger is
concrete:

- multiple humans/agents need a shared trace dashboard;
- human review, labels, or annotations are part of the workflow;
- datasets/experiments/evals must compare agent or model changes over the same
  inputs;
- production AI app telemetry, cost tracking, latency, prompt/model versions,
  or online evals are required;
- OTLP/OpenTelemetry export is needed for a wider observability stack;
- local JSONL review cannot answer an important repeated question without
  manual reconstruction.

The pilot is side-by-side. It does not replace `tmp/session_profiles/*.jsonl`
until the local profile can be exported or the external tool proves a repeated
time saving.

## Adoption Guardrails

Do not adopt an external observability system if:

- it requires secrets, accounts, containers, paid workspace setup, or SDK
  wiring before the first useful question is known;
- it would capture prompts, screenshots, child-test notes, or proprietary code
  outside the machine without an explicit privacy decision;
- it only duplicates local summary/review/follow-up outputs;
- it makes the agent spend more time formatting traces than shipping the game
  or improving the pipeline.

An external system can become an adoption candidate only after a pilot shows at
least one of:

- lower reflection/debug time for the same session type;
- better cross-agent/human review than local markdown/JSON can provide;
- reusable datasets/evals/experiments that prevent regressions;
- useful production-style monitoring that the project actually needs.

## CLI Gate

Use the gate before adding an external tool to a project:

```powershell
node tools/ai_profile/observability_gate.mjs --need human-review --need datasets --team small --setup-cost medium --sensitivity medium
```

It returns one of:

- `local_jsonl_only`: keep improving the local profile;
- `external_pilot`: run a bounded side-by-side pilot;
- `external_adoption_candidate`: pilot evidence exists and setup/privacy cost
  is acceptable.

The output is intentionally conservative. It prevents "install a platform"
from becoming the default answer to a reflection gap.

## Portable Rule

Every future project gets this rule with the portable AI base:

1. Start local JSONL profiling when the session is long or reflection-heavy.
2. Use the gate when someone proposes LangSmith, Phoenix, Langfuse,
   Braintrust, OpenTelemetry export, or another tracing/eval platform.
3. Keep raw telemetry in ignored scratch paths.
4. Promote only reusable lessons, rules, tooling, and task files to git.
