# Reflection Output Rules

Load this reference when writing the retrospective, especially a deep review of
a long session.

## Output Shape

For deep retrospectives, cover these sections in order:

1. What was done.
2. Where the most time went.
3. Where the agent got stuck or behaved poorly.
4. Tool and automation audit.
5. Context problems.
6. Planning problems.
7. Product/result quality problems.
8. Improved workflow for the next cycle.
9. Prompt/system-rule changes.
10. Top 10 improvements.

For each major problem, separate `symptom`, `cause`, and `faster path`. Do not
invent concrete examples; if evidence is missing, say so.

## Output Rules

- Be specific and self-critical.
- Use concrete examples from files, reports, screenshots, tasks, commands, or
  user corrections.
- Clearly label weak evidence, broken profiler coverage, and unknown time
  intervals.
- Mark missing evidence as likely or unknown.
- Distinguish product problems, pipeline problems, and agent behavior problems.
- Prefer small rule/tool changes over vague advice.
- Do not mark the project goal complete from a retrospective.
- Keep raw telemetry and generated reflection artifacts in
  `tmp/session_profiles/` unless the lead asks to preserve them.

## Workflow Audit Checklist

Audit:

- tool use: terminal, search, tests, generation, browser/runtime tools, and
  automation;
- context management: large reads, stale source-of-truth, rediscovered
  decisions, compaction risk;
- planning: broad scope, missing vertical slice, weak done criteria, late
  validation, over-validation;
- product quality separately from pipeline quality and agent behavior.

End with the highest-leverage process changes. For deep reviews, include the top
10 fixes for the next cycle.

## Durable Capture

If the reflection yields reusable process lessons, add a short dated entry to
`AI_PIPELINE_HISTORY.md`. If it yields actionable project work, create or update
tasks in `tasks/`. Do not bury work only inside the retrospective.
