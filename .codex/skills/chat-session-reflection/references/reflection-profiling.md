# Reflection Profiling

Load this reference for AI workflow review, profiler review, analytics review,
requested retrospective reviews, or any reflection where time-spend claims need
profiling evidence.

Profiling is passive by default. For normal retrospectives, start with:

```powershell
node tools/ai.mjs status
```

Use it to name unresolved failures, slowest recorded work, largest context
input, long manual/research/review gaps, and whether normal work needs any
profile action.

For AI workflow, profiler, analytics, or requested retrospective reviews, run:

```powershell
node tools/ai.mjs status --verbose
node tools/ai.mjs status --require-current-scope-usable
```

If the guard fails, do not repair stale artifacts, stale bundles, drafts,
reviews, follow-ups, or baselines by default. Say what is missing, use available
evidence, and mark time-spend claims as partial or unknown.

For long Codex sessions with suspected missing failures, run:

```powershell
node tools/ai.mjs import-codex-session
```

Do this before trusting status for failure analysis. Use:

```powershell
node tools/ai.mjs reflect
```

for a short closeout artifact. Add `--gap-checkpoint` only when the user
explicitly wants the pre-reflection gap recorded. The old `reflect --deep` chain
is retired.

Use low-level `tools/ai_profile/*` only when debugging profiling itself or when
`tools/ai.mjs status --verbose` identifies a specific artifact that must be
inspected.
