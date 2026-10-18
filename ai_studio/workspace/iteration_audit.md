# Studio iteration audit

Snapshot: 2026-10-18. Owner: Studio maintenance, T0461 under E022.

## Scope and evidence

All eleven catalog games and three registered derivative worktrees were
reviewed using their existing decomposition, recent Git history, linked task
logs and durable design/technical notes. Each checkout owns a private
`design/studio-iteration-audit.md` with its sources, coverage and findings.
Recent-history samples do not cover every historical commit. Two derivative
checkouts share an identical tracked snapshot; their common ancestry is not
counted as independent implementations or duplicated effort.

The aggregate profiling window covers 2026-06-20 through 2026-10-18: 765 Codex
and 30 Claude session files, about 149.9 MB of retained tool output, and about
116 million parser-observed truncated output tokens. Research accounts for
85.6 MB of the retained output. These are telemetry observations, not billing
totals. Inherited context, overlapping sessions, classifier heuristics and
different failure reporting make cross-harness efficiency comparisons unsafe.
Summed tool durations are not elapsed human work time. Git additions include
copies and inherited histories, so churn is not a labor estimate.

Raw profiles, command strings and performance artifacts remain under ignored
`tmp/studio-refresh/`. No private game identifiers or paths are promoted here.

## Repeated constraints

| Symptom | Evidence scope and cost | Cause and owner | Priority |
| --- | --- | --- | --- |
| Demonstration systems must be removed before game work | At least two games report starter/build/state cleanup; elapsed effort unknown | Starter owns architecture, shared features own invariants, games own content | P1 |
| A capture shows removed content or an unchanged old executable | Two game iteration records; another records misleading metrics; rework duration unknown | Capture process identity alone does not bind a recording to current sources; Runtime Automation | P1 |
| Source, pack and release contracts drift after engine or asset changes | At least two game records and the shared viewer fixture | Generated-format consumers and fixtures need explicit current-version validation; Assets and build tooling | P1 |
| Local tests are mistaken for phone, earned-flow or live-portal proof | At least four game reviews explicitly leave device/portal evidence open | Evidence scope must match the claim; Runtime Automation and release workflow | P1 |
| Save/account/visibility lifecycle needs repeated fixes | Three games; one records four sequential cloud/SDK fixes; elapsed effort unknown | Platform capabilities and save policy need explicit boundaries; platform-sdk/game-state | P2 |
| Default validation blocks short iterations | One historical game gate around 135 s, including a 122.8 s test; current Studio workspace domain around 316 s | Expensive subprocess fixtures and broad owner domains; build/test tooling | P2 |
| Duplicate worktrees obstruct discovery or inflate history counts | Three registered derivatives; two share the same tracked snapshot | Identity differs from release metadata; Workspace | P1 |
| External private research storage conflicts with repository-only containment | One existing Canvas mount blocked the real commit guard after containment was tightened | Canvas already permits external game stores; Workspace needs an explicit private storage boundary | P1 |
| Antivirus pauses a build fingerprint helper | Lead-provided CyberCapture result; four checked-out games contain native helper compilation, with one source hash matching the reported executable name | Game-local tooling builds unsigned, source-versioned executables even though a shared cache already exists; Runtime Automation | P1 |
| Profiling itself rereads large transcripts | Full baseline report around 172 s; metadata sample reads 119,948,396 bytes for eight profiles | Metadata discovery reads full files, and turn accounting reparses them; Core Harness profiling | P2 |

Frequencies are confirmed lower bounds within inspected evidence, not complete
incidence statistics. Historical frame/startup timings remain in private notes
with their devices and conditions. Missing timing evidence is unknown.

## Improvements in this maintenance sequence

- The blank starter and independent feature consumer remove mandatory demo
  content from new games. Test discovery now follows CMake regeneration.
- Registered derivative roots are audited without inventing duplicate catalog
  games. Identity checks retain Git registration, common-directory and physical
  path guards, while allowing different release versions and download budgets.
- Asset fixtures and the viewer use the engine's current pack/atlas contracts.
  Preview-art license records and integrity coverage have explicit owners.
- Reference source-to-runtime proof uses the independent consumer. A real
  rebuild/launch returned matching C and schema fixtures with
  `freshnessClaim:true`; the guard rejected a separate run when sources changed.
- Profiling now reads the first metadata record through the existing streaming
  reader and computes user-turn spans during its normal transcript parse.
  Eight real profiles retain identical cwd identities while metadata reads
  fall from 119,948,396 to 524,288 bytes. Alternating warm-cache samples are a
  microbenchmark, not a promise of the same speedup for the complete report.
- Native capture rebuilds the canonical game target before launch and records
  its CMake configuration, source fingerprint and executable hash. Configuration,
  source or binary drift invalidates the proof. The actual shared consumer
  passed rebuild and final validation in 119.7 s while other scans ran; full
  content hashing adds overhead. This is native build evidence; device, portal
  and release claims still need their own runs. Checkpoints cannot detect an
  adversarial swap-and-restore between checks.
- Privacy reads staged blobs in bounded Git batches instead of spawning one
  process per file. A 242-file fixture fell from 8,832.8 ms to 119.9 ms; actual
  repository scans fell from 966,690 ms to 162,842 ms before resolving the same
  known public-asset relocation findings. Conditions include cache and concurrent
  work; this is observed scan time, not a promised universal speedup. Replacement
  refs cannot substitute scanned bytes. Final acceptance retains fail-closed
  privacy guards, independent review and the complete Studio gate.
  The first successful real commit, including its stronger final guard and Git
  write, took 122,288 ms. A later text-only commit passed the narrow guard in
  about 5 s; these operations have different scan scopes.
- External private Canvas projects have an explicit machine-local registration
  owned by Canvas configuration. Required private-byte comparisons enumerate
  the registered store independently of Git or a mounted directory link.
  Different mount targets and nested escapes fail closed. Scoped game checks
  ignore other games' valid registrations. No private paths enter tracked
  configuration, and text-only changes retain the existing narrow scan scope.
- Four game build-record consumers now use the shared Node.js tree hasher.
  They no longer compile or launch source-versioned fingerprint executables.
  Selection policy and record schemas remain game-owned; standalone roots use
  their JavaScript fallback. This removes the reported CyberCapture trigger
  without antivirus exceptions. Other executable checks remain outside this
  claim; moving the old cache alone would not have removed the trigger.
  On one real current-source record with eleven inputs, shared Node and local
  JavaScript produced identical records in 7,797 ms and 7,001 ms respectively.
  This is a compatibility measurement, not a speedup claim; the old executable
  was deliberately not launched again. Native capture now uses a source-only
  record where the game otherwise requires a web compile profile.
- The blank starter's web release substitutes a real SDK release Boolean and
  acknowledges a restored frame only after presentation. Browser proof covers
  an empty initial frame and two WebGL recovery cycles; ordinary games retain
  non-empty frame checks. No engine changes were needed.

## Fast iteration follow-up

The observed 122,288 ms operation was a real commit including the deep privacy
guard, not the cost of every game build. Profiling separated that guard from
web source fingerprints and test-runner configuration.

| Operation | Earlier observation | Current observation |
| --- | --- | --- |
| Text-only privacy preflight | 5.4 s | 2.4 s; a real commit including its mandatory hook took 1.9 s |
| Deep privacy comparison with a changed public binary | 145.0 s | 23.0 s, with the same four violations |
| Bytes read by that binary probe, filesystem plus Git output | 18.04 GB | 4.99 MB |
| One representative warm web build | 18.0 s with source proof | 4.5 s as a local non-proof preview |
| Final shared preview CLI on that game | No identical baseline | 7.4 s including catalog/path validation and the game build plan |
| Focused native test / regular two-test core run | No comparable baseline | 1.94 s / 2.21 s |

These are local observations, affected by caches and concurrent work, not
latency guarantees. Binary scans now filter candidates by size before reading
their bytes, batch waiver lookups and walk each private tree once. Matching
bytes still receive full hashes; immutable Git metadata, file identity and
physical containment checks remain enforced. No antivirus exclusion is needed.

The shared preview adapter uses each selected game's own build plan without
the two full source-tree hashes. It writes an explicit non-proof record and
clears the compiled fingerprint. Release packaging rejects previews. Actual
incremental compilation remains required; first builds and broad header edits
can take longer. A real browser run showed rendered content with no runtime
exceptions; this is local preview evidence, not capture or release proof.

The starter and independent feature consumer skip unrelated Node suites for
`test --only`, and existing Ninja builds regenerate configuration only when
needed before discovering CTest names. Ordinary test and release modes retain
their checks. The documented inner loop is preview plus the smallest relevant
test; owner checks run at integration and the full Studio gate at acceptance/CI.
Focused contract tests and independent security/release reviews validate this
follow-up; the earlier complete Studio gate is recorded separately below.

## Decisions and deferred boundaries

The generic numeric limb solver and event-to-portal mapper belong in shared
features. Rigs, balance, progression content, custom render effects and account
merge policy do not become generic merely because two games use similar words.

Frame ownership and measured ablations are useful patterns. One historical
solver investigation reduced call counts without materially reducing frame
time; an assumed multiplier was disproved. Shared optimization therefore needs
measured target-device evidence, not only a smaller counter.

Engine rendering, missing-glyph diagnostics and HTTP stalls remain engine
issues with their own issue/PR route. Source inspection cannot establish a
phone GPU budget or prove live SDK behavior. No engine workaround or invented
runtime result closes those gaps.

Further candidates are transactional platform/save scenarios, workspace save
isolation, source-export reproducibility, compact asset previews and narrower
changed-file validation. These need their own concrete contract and before/after
proof. They are recommendations in this audit, not automatically created tasks.

## Verification

`node ai_studio/studio.mjs verify --full --json` passed after implementation
commit `60bc8ebde` in 224,234 ms. Harness, workspace, architecture, shell,
assets, work-management, runtime, features and template-release all passed;
design has no deterministic technical checks. The workspace domain took
224.1 s, feature checks 159.5 s, and template release 41.7 s while domains ran
concurrently. These are observations, not fixed performance budgets.

The runtime owner discovers its Node suites from the owning directory, so new
tests cannot silently remain outside the gate. The HTTP functional scenario
uses one bounded integration deadline while retaining all security assertions;
it does not impose a five-second latency SLA under concurrent build load.

The accepted workspace retained pre-existing asset-manifest edits outside this
task's commits. Private game changes remain in their owning working copies;
they were not bulk-committed with Studio. Full and focused reports, source-only
record equality and native/browser evidence remain under ignored
`tmp/studio-refresh/`. T0461's closure log records the commands and review
decisions. These checks do not claim that all games were rebuilt, played or
tested on devices or portals during the audit.
