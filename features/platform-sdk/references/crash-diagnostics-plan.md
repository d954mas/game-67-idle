# Shared crash diagnostics: research and implementation plan

Status: implemented in reviewed pull requests; release integration awaits the
engine master pin. The plan passed independent subagent and Claude reviews.
HTTP samples remain supported; retirement requires a separate lead decision.
Research date: 2026-10-19. Engine baseline: `8eb37192`; local Emscripten: `4.0.19`.

## Implementation evidence

The selected architecture is implemented in [Studio PR #33](https://github.com/d954mas/game-67-idle/pull/33)
and [engine PR #696](https://github.com/d954mas/neotolis-engine/pull/696), which addresses
issues #691 and #694. The engine candidate is `4e6b453d`; official consumer pins
remain on `8eb37192` until the candidate is merged to engine `master`. The Studio
and game migration PRs are drafts pending that pin update and integrated proof.

- Two independent engine reviewers approved the capture/HTTP changes. Engine
  `format_and_check` and `check --push` passed; 7 source regressions and 5 optimized
  Chromium crash cases passed. The optimized test caught and fixed an absent
  abort reason: the raw record now uses `abort` when Emscripten supplies none.
- Two independent SDK reviewers approved the final operation boundaries. The
  combined SDK/runtime/storage suite passed 122 tests. Untagged asynchronous
  GamePush events remain independent lifecycle events; only synchronous events
  emitted during a vendor invocation can be assigned to that request.
- Shared reporter tests passed 15 cases; CMake installation passed 3. Template
  and consumer package/minifier suites passed 93 cases. Generated-game wiring
  passed, including a run without a global Git identity. Feature contracts
  passed for 17 modules and 2 pointers. The copied-game CLI fixtures passed 30
  tests; runtime-build suites passed 16, with 2 Windows symlink cases skipped.
- The actual optimized engine browser fixture delivered one report for caught
  abort and one for trap through the canonical shared reporter. Build, GPU,
  context, reason and the newest journal tail were retained. A throwing prior
  abort handler ran once. Engine persistence and direct reporter replay passed;
  the replay setup explicitly copied the byte-identical engine fixture record
  from its arbitrary test key into the reporter's configured `.v1` key.
- Actual consumer preview failure probes passed: missing runtime JS, an owned
  runtime script throw, missing or throwing SDK module, and missing WASM all
  retained the corresponding report and displayed fatal UI. An SDK evaluation
  failure persists the report before an error sink exists. Browser execution
  also exposed an empty preview identity and a duplicate sprite-atlas binding;
  both caller fixes passed an independent engine-contract review. The generated
  game test preserves the nonempty preview identity requirement.
  Normal preview then passed in a fresh browser with the real SDK: the loading
  overlay closed, the scene and HUD rendered, and no fatal UI or crash record
  appeared. The initial stuck-loading observation was caused by a test harness
  replacing the SDK backend; normal-mode instrumentation was corrected.
- Linux and Windows CI passed native template linking after correcting the
  executable-selected Basis provider order. The next release gate exposed an
  omitted invoked `startRuntime` loader path. Focused proof now transforms the
  caught bootstrap, runs the actual release minifier, and verifies the reopened
  ZIP. Dormant, member, arrow and dead-code calls remain rejected. Canonical
  reporter comparison permits CRLF/LF materialization differences while still
  rejecting changed executable contents; both cases have regression coverage.
- The shared reporter is 11,362 source bytes / 3,328 gzip bytes. No engine
  persistent C storage or diagnostic hot-path calls were added. A matched
  before/after linked WASM baseline was unavailable locally, so no binary-size
  delta is claimed for the crash fixture. The later
  [paired CI size report](https://github.com/d954mas/neotolis-engine/pull/696#issuecomment-6255450112)
  confirms zero gzip growth for `hello`, which does not opt into `nt_crash`.

Independent integration review approved two existing game migrations. Their
private matrix records separate PRs and the nine older consumers whose pinned
engine revisions lack `nt_crash`. Those upgrades remain explicit prerequisites;
updating this template does not update a published game or its engine pin.
Both migrated games passed local optimized Web builds and normal browser
startup. The source game also passed two sequential intentional context-loss
and recovery cycles in one page without a crash report. These builds use the
official engine pin; they are not integrated proof of the candidate engine fix.

Hosted portal delivery, physical PowerVR behavior, and a new Poki release remain
outside the completed proof. The existing Linux Studio CI also has unrelated
baseline failures in Windows-path/tool fixtures; the same failures appear in
[the preceding master run](https://github.com/d954mas/game-67-idle/actions/runs/39138573969).
New failures caused by the reporter installation gate are fixed in the affected
package fixtures rather than bypassing the gate.

## Outcome and boundaries

Every integrated game's Web build uses the same diagnostic implementation. The engine owns
the fatal runtime record; the shared web integration owns delivery and early
loader failures; each game supplies its build identity and optional context.
No game maintains a second engine log ring or scrapes console messages to find
engine crashes. Runtime assertions still fail; reporting is not recovery.

Deliver engine and Studio changes through reviewed pull requests. Keep the shared
engine checkout read-only; verify engine changes in the isolated worktree. Do not
merge pull requests or publish/rebuild a Poki release as part of this work.
No Taskboard items are created by this plan.

## Findings in this repository

1. `engine/http/web/nt_http_web.c:126,143,147` uses catch-all Promise chains around
   successful JS/WASM delivery. A completion exception can disappear; a progress
   exception can trigger a second C call with FAILED. Existing
   [issue #691](https://github.com/d954mas/neotolis-engine/issues/691) describes
   this. `FAILED` is documented for network/timeout/cancel, not program faults
   (`docs/spec/assets/async-loading.md`). A test-local execution of the original
   EM_JS reproduced all three cases. This proves a way to lose diagnostics,
   not that this boundary caused the observed HUD or PowerVR incident.
2. `engine/crash/web/nt_crash_web.c:39` deliberately accepts a RuntimeError or
   stack with WASM frames. Only global `error`/`unhandledrejection` are observed.
   A caught abort is therefore outside current coverage. New
   [issue #694](https://github.com/d954mas/neotolis-engine/issues/694) proposes an
   `onAbort` capture boundary. This is a documented contract extension, not a
   claim that the existing spec already promises onAbort capture.
3. Clay already logs its reason before asserting (`engine/ui/nt_ui.c:63`), and
   the crash sink receives it synchronously. A separate Clay collector duplicates
   engine work; the exception boundary must be fixed instead.
4. `nt_crash_set_context()` already copies up to 1023 bytes. A game can update
   its context while healthy; the recorder reads it from memory after a fault.
   No new context registry or crash-time call back into WASM is needed.
5. `features/platform-sdk/references/contract.md` currently assigns reporting to
   each game and exposes `errorSink()`. Moving transport changes that contract;
   update it, README, INSTALL and feature version together.
6. Template SDK source modules are bundled into `game.js` for release. A reporter
   installed only by importing `platform-sdk.js` disappears when `game.js` itself
   fails to load. The reporter must exist earlier in the HTML shell.
7. The template is copied when a game is created (`games/README.md`). Updating
   it does not migrate existing games. The workspace catalog currently contains
   11 canonical private game mounts; extra filesystem folders include comparison
   worktrees and must not be treated as independent games.
8. `features/platform-sdk/src/platform_sdk_web.c:43-65` catches and discards
   exceptions from eight WASM lifecycle calls. Poki ad success callbacks can
   enter the failure callback and call WASM again (`web/adapters/poki.js:140-153`).
   Its deadline wrapper also turns rejected operations into normal failures.
   Fixing only HTTP, or only the innermost callback, leaves reachable holes.
9. The release packager validates a fixed `__PLATFORM_SDK_CONFIG__` shape and
   rejects residual SDK module references (`templates/template/tools/package_web.mjs`).
   Inline source injection therefore needs packaging proof, not just CMake proof.

## What other engines do

These are architecture comparisons, not claims of equivalent browser support.

| Engine / runtime | Verified practice | Decision for Neotolis |
| --- | --- | --- |
| [Defold crash API](https://defold.com/ref/stable/crash-lua/) | Native crash dumps expose system fields, stack and bounded user fields. `load_previous()` deletes the disk dump after loading. | Reuse the bounded context idea. Keep Neotolis non-destructive loading because SDK handoff is not acknowledged delivery. Do not claim this native API proves HTML5 behavior. |
| [Godot 4.5 logging](https://docs.godotengine.org/en/4.5/tutorials/scripting/logging.html) | Desktop file logging retains crash output; custom loggers can send errors remotely. Useful native crash backtraces require matching symbols. | Separate capture from a selected destination; retain symbols by exact build identity. Avoid claiming desktop file persistence works unchanged in browsers. |
| [Unreal crash reporting](https://dev.epicgames.com/documentation/en-us/unreal-engine/crash-reporting-in-unreal-engine) | The engine creates a report; a separate optional Crash Report Client sends it. Game data is set before a crash and copied into the report. | Set context in healthy execution, then capture without calling game code. No separate uploader process or server infrastructure is needed for the browser target. |
| [Unity log callback](https://docs.unity.com/en-us/engine/7000.0/script-reference/unityengine/application/logmessagereceived) | A log event gives subscribers the message, stack and type. The documented callback runs on the main thread. | Use an existing engine record/event as the source; do not recognize fatal conditions by parsing console strings. This callback alone is not a guarantee of fatal Web crash capture. |
| [Emscripten Module API](https://emscripten.org/docs/api_reference/module.html#Module.onAbort) | `onAbort` runs on abnormal termination; it cannot recover execution. Incoming Module attributes must be retained by the build configuration. | Capture before the shell callback and test the pinned compiler plus minified release. Preserve termination semantics and the previous callback. |

The local 4.0.19 `src/preamble.js:239` calls `Module['onAbort']` before setting
`ABORT` and throwing RuntimeError. An exception from our recorder must never
prevent the original abort path. The existing shell callback must run once; its
own exception must not replace the primary abort. This is an explicit terminal-
path contract. TRAP assertions use `__builtin_trap()`, which does not invoke
`onAbort`. The abort hook covers Emscripten abort only; escaping WASM traps still
depend on correct exception boundaries and global listeners.

## Proposed ownership and data flow

```text
game build identity + precomputed context
                     |
                     v
nt_crash: log ring -> fatal record -> local storage + nt-crash event
                                               |
shell: own JS/WASM loader failure -> shared web reporter
                                               |
                              selected platform adapter / console
```

### Engine: two bounded fixes, no new public C API

- For fetch, reader.read and arrayBuffer use `.then(success, transportFailure)`.
  Keep valid transport rejection handling and request ownership; let exceptions
  from success/delivery escape. Do not classify errors with regex or couple HTTP
  to the optional crash module. Review header cleanup, cancellation, generation
  checks and exactly-once completion while making the change.
- Split the existing JS capture helper into global-event classification and
  an internal writer accepting explicit reason and stack. `onAbort` receives
  a string/number, not an Error; forwarding it to the current stack filter
  silently drops it. The abort hook uses its original reason and a fresh Error
  stack; global listeners retain the existing WASM filter.
- Chain `Module['onAbort']` when installing `nt_crash`: write first, then invoke
  the previous callback once with its original receiver/arguments. Contain both
  writer failure and a throwing previous callback so Emscripten still sets ABORT
  and throws its original RuntimeError. Use the existing once guard against later
  global events. Read current memory views after memory growth. Test this with
  the real current shell callback and with an outer caller catching abort.
  Cover both shell styles in current consumers: diagnostic forwarding and fatal
  overlay display. The overlay must appear once, after record capture.
- Preserve opt-in linkage and existing text record, storage key, log ring,
  GPU data, context and C read/clear APIs. No automatic upload, public assert hook,
  generic telemetry bus, new ring or native behavioral change. Correct the header
  summary to say clearing requires confirmed delivery, consistent with the spec.
  Update the Web contract in `docs/spec/debug/logging-errors-debugging.md`: capture
  caught Emscripten abort before the previous shell handler, contain that handler's
  exception to preserve primary termination, and keep TRAP/vendor/browser-process
  exclusions explicit. The current description of independent shell onAbort
  handling must not remain after this behavior change.

The HTTP fix guarantees no second WASM call from its failed continuation.
It does not by itself stop separately scheduled RAF, audio, SDK or network
callbacks. `nt_app` currently uses `emscripten_request_animation_frame_loop`.
Pinned Emscripten schedules the next RAF only after the frame callback returns:
a trap inside that callback stops its chain; a trap outside it leaves a pending
RAF alive. Do not claim a global runtime shutdown. Verify this locally; a general
shutdown mechanism is a separate engine
design, not a hidden responsibility of an optional crash logger.

### First-party SDK exception boundaries

Remove the empty catches around WASM lifecycle calls. Audit the whole delivery
path in `platform_sdk_web.c`, the JS facade and installed adapters: ad start/end,
pause/resume, audio/sound, init, allocation, player and leaderboard completion.
External SDK rejection/timeout remains a supported outcome; a game/WASM callback
exception must not become that outcome or trigger another WASM call.

Normalize the raw external SDK operation before attaching first-party lifecycle
and completion callbacks. Apply deadline/failure conversion only to that raw
operation; keep first-party callbacks outside it and outside downstream broad
catches. A success/failure pair on an already mixed Promise is insufficient.
Make this a stated adapter contract: supported portal failures resolve to their
existing result values; rejection after adapter processing means a program fault
and must propagate. Remove the C bridge's conversion of those rejections into
`_complete_*(failed)`. The current leaderboard contract says any rejected Promise
is `failed`; amend it and its tests rather than leaving contradictory semantics.
Normalize synchronous vendor throws only around the actual vendor call, not a
first-party callback-bearing operation. Where a supplied vendor callback can
enter WASM, retain its fault in that operation's lexical state for the whole
callback lifetime, including asynchronous invocation. While the outer first-party
operation is pending, a callback fault immediately rejects it with the original
exception, without waiting for another vendor event or deadline. Vendor rejection,
resolution or silence must not convert it into a supported outcome. Check the
fault before subsequent lifecycle/completion entry or timeout settlement; once
faulted, that operation cannot call WASM again. This is local operation ownership,
not a global fault registry or shutdown mechanism; do not match error messages.
The synchronous vendor-call catch also checks that retained fault before treating
a throw as a supported vendor failure; test a callback invoked synchronously
inside the vendor call as well as the asynchronous cases.
Timeout does not prove an ad overlay closed: preserve healthy request-correlated
late visibility and terminal cleanup, as required by the existing SDK contract.
If the first callback fault occurs after settlement, schedule one microtask throw
of the original exception instead of attempting to reject the settled Promise.
Mark that operation faulted before doing so; all of its subsequent callbacks skip
WASM. Independent portal lifecycle events do not share an ad's fault state.
Test asynchronous callback fault followed by vendor rejection, resolution and
pending-until-deadline, plus late callbacks after timeout. Test healthy late close
and a late fault separately: close still reaches C, fault escapes once without
further C entry or duplicate completion. Rejecting our pending operation
must still expose the fault when the vendor swallows the callback exception.
Do not add RuntimeError message matching, an exception registry or a dependency
from platform-sdk into private crash internals. Vendor code that itself swallows
an exception entirely outside our supplied callback boundaries remains a
documented limit, not a reason to invent recovery.

| Boundary to inventory | Required separation |
| --- | --- |
| `platform_sdk_web.c` lifecycle globals and initial state replay | Direct C exceptions escape; no empty catches or further replay after a throw. |
| C bridge init/player/ad/login/leaderboard completion and allocation | Normal adapter failure is a result; a program-fault rejection is not a second completion. |
| Adapter lifecycle calls, raw vendor Promises and deadline helpers | Convert only vendor failure/timeout; keep first-party callbacks and their exceptions outside conversion. |
| Shell imports/instantiate success callback | Loader I/O failure reports as such; runtime callback exception escapes without relabeling. |
| Diagnostic writer/transport only | Contain their own failure to preserve the primary failure; never enter WASM. |

### Shared web implementation: one source in platform-sdk

Proposed file: `features/platform-sdk/web/runtime-diagnostics.js`.
It is a small classic-script installation, embedded from that one source into
the generated HTML after immutable build config and before SDK/runtime loading.
Do not copy editable diagnostic logic into game repositories.

Use the existing config's build fingerprint without extending its strict schema.
One CMake crash prefix produces the C initialization key and the inline reporter
arguments: engine `<prefix>.v1`, web `<prefix>.pending.v1`. Default prefix comes
from the game's existing unique storage namespace plus `.crash`; preserve legacy
prefixes explicitly. Encode generated JS/C string literals safely. The shell
already knows its runtime URL and passes it as an installation argument.

The shared module has one reporting operation for explicit shell/game failures.
It subscribes to `nt-crash` and directly reads the previous text from the documented
engine localStorage key at installation. No per-game replay bridge, large WASM
stack buffer or read-back call is needed. It never calls WASM or reads private
memory. Game glue only initializes `nt_crash` and sets meaningful context.

Transport belongs here; Poki-specific readiness and pacing belong beside the
Poki adapter. Reuse `errorSink()` initially. Preserve the current proven delay
around Poki's automatic reports and the observed SDK-core readiness condition
until a replacement is verified against the actual SDK. Treat those details as
adapter observations, not a stable official delivery-acknowledgement API.

Keep four fixed fatal slots: current engine, current web, previous engine,
previous web, in that priority order. Each admits at most one record per session;
the first current engine crash wins. Defer only engine-eligible WASM errors and
shell abort candidates by one event-loop turn. If a live `nt-crash` record already
exists or arrives before that deferred flush, suppress that candidate before
persisting/sending it. Loading a previous record does not count as a live event.
This is terminal-report priority, not equality between unlike JS/engine payloads.
Pure-JS-only and explicit loader failures use their own slot without this deferral;
an unrelated later engine event never deletes an already accepted web report.
Deduplicate equal raw records within a session, not by report-kind labels.
Sanitize outgoing URLs/credentials, cap UTF-8 bytes, retain the engine header
(build/GPU/context) and newest log tail when truncating. Truncation is marked.
Keep the existing 12,000-byte outgoing cap. Poll transport readiness at most ten
times at one-second intervals per installation; new backend readiness or a new
fatal can trigger one flush without restarting an unbounded poll. Once ready,
attempt each record once; a throwing sink consumes that attempt, not the stored
record. SDK absence/failure falls back to console
without recursively feeding the collector. Other adapters keep console behavior;
do not invent unsupported upload services.

Read the raw engine value once with storage exceptions contained. Use a 65,536
UTF-16-code-unit input ceiling for stored engine text; it exceeds this baseline's
8,192-byte ring plus bounded header/stack/context. Treat a larger configured ring
as a compatibility change to check in the per-game matrix. Reject oversized or
structurally invalid stored records from upload without deleting their raw slot.
Validate only the current text envelope, not a second semantic crash parser.
Never parse unknown record text into executable data or rewrite the engine slot.
An unsupported/oversized value cannot allocate an unbounded queue. Reading a
localStorage value itself necessarily allocates that browser-owned string.

Poki has no delivery acknowledgement. Therefore neither a successful call nor
a persistent `sent` marker may suppress replay on the next launch. Original time
and build remain in the record; do not replace them with this launch's build.
Duplicates across launches are an accepted cost of retaining unconfirmed reports;
replay continues every launch until overwritten or explicitly cleared, including
after an upgrade. Triage must use the record's original build. Actual at-least-once
or exactly-once delivery is not guaranteed. A different
policy requires a deliberate reliability tradeoff, not a hidden cleanup rule.

### Early loader and pure JS failures

Before `nt_crash_init`, there is no engine log to collect. The shell reports its
own script download, WASM fetch/compile/instantiate failures to the already
installed shared module. Keep the pre-init shell `onAbort` fallback; after engine
init, the engine captures before calling it and the deferred fallback is suppressed.
The shell keeps ownership of its fatal overlay.
Catch I/O failure separately from calling the instantiated runtime's success
callback so another broad catch does not reproduce #691.

Handle failure to download/evaluate our SDK facade or selected adapter explicitly:
report a local bootstrap failure and show the fatal overlay instead of leaving
the loader pending forever. Missing/adblocked external portal SDK still follows
the adapter's existing playable fallback. Template, consumer, source game and
release bootstrap all need this distinction; static-import failure cannot be
handled by code following that failed import. Choose an explicit caught import
in preview and a verified bundled bootstrap in release; update the packager's
recognized transformation accordingly rather than hiding a module filename.

After startup, the shared module may observe exceptions attributable to the
configured runtime script; unrelated portal/extension/page errors are excluded.
Apply the engine-eligible deferral rule above; pure JS errors are not suppressed
by an unrelated engine failure. A trap in main/successCallback before crash installation
also becomes a web report without a journal. A pure JS error without a WASM frame
is a web report with
its own stack/build identity, not a fabricated engine crash. It has no promise
of an engine journal. In release, attribution to game.js includes its bundled
first-party SDK adapter. Do not expand nt_crash to every error on the page.

One bounded shared web-failure storage slot preserves these failures when the
SDK is unavailable, using a separate namespace from the engine record. It holds
no engine log ring and never stores a second copy of an engine crash. This
retains pre-runtime failure persistence without two competing owners of the
engine's storage format. This slot is needed when game.js cannot load: the
release adapter is inside that same missing file and cannot send now. Preserve
the existing outer schema `{version:1, at, kind, stack}` and 20,000-byte stored
cap, with build/time included in the bounded stack envelope. No new migration
framework: validate the outer shape/size and preserve a valid legacy stack as
opaque text, including its original identity; no private game marker parser.
The first current fatal web candidate is stored after engine deduplication,
before handoff; it may replace the previous web record, like the engine's single
slot. On reload, validate age too. Retain the existing 24-hour web-report expiry;
the engine record has no expiry. Neither slot is cleared after fire-and-forget
handoff. Storage exceptions never interrupt startup or the primary failure.

Native raw records remain untouched. Web output sanitization is not a guarantee
that arbitrary secrets written by application code are safe to log: game context
must contain bounded non-secret state, never player credentials or raw URLs.

### Game-owned data and removal inventory

Keep only initialization metadata, calls to the common
loader reporter, and context updates through `nt_crash_set_context()`.
Update context at meaningful state changes (screen, boot stage, connection state).
Remove diagnostic UI phase updates executed every frame; Clay already logs its
fatal reason synchronously. Avoid per-frame serialization of packs,
network history, GPU caps or heap data. Use fixed storage and no hot-path heap.

Remove the game's second engine log sink/ring, special Clay report, diagnostic-only
WASM refresh export and game-local pending-report implementation. Remove the
console.error scraper and fetch/stream monkeypatches only after the HTTP report
retirement/parity decision below. Port their real crash/transport regression
cases into the shared tests before deleting them. Keep game-specific tests of
its context and existing fatal/loading UI. Preserve explicit fatal asset-load
reporting through the common operation. Preserve unexpected WebGL context loss
and context-creation status with shared browser listeners and bounded nonfatal
reports; retain the deliberate context-loss test exclusion. A recovered context
loss is a diagnostic signal, not a fatal engine record.
Nonfatal reports use at most two additional pending entries, one per context-loss
or creation kind, one attempt each per session and lower priority than all fatal
slots. They are not persisted and cannot evict a fatal record. Legacy per-request
samples remain outside this new queue until their separate migration decision.

The existing low-level `http.status_failed`, `http.engine_fetch`,
`http.engine_stream` and `pack.failed` reports are not all duplicates of a crash.
Before deleting their scraper/instrumentation, inventory each trigger and its
structured replacement. Fatal pack failure stays covered at the loading failure
site. Retiring recoverable per-request portal samples is a separate proposed
scope reduction requiring the lead's decision; keep that cleanup blocked if
parity is unavailable. Do not add an engine telemetry API merely to copy those
samples into another ring. Per-request timelines/heap/pack snapshots are excluded
from the common crash recorder.

## Installation and migration

1. Fix and review the engine in `691-web-crash-boundaries`, reference issues
   #691 and #694, run required checks and open an engine PR. Shared checkout stays
   read-only. Do not remove game fallback coverage before the fixed revision is
   available and the integration proof passes. Merge/checkout changes follow the
   user's applicable authorization; an open PR is not an installed engine fix.
2. Correct the first-party SDK exception boundaries and add their focused
   adapter-to-C fault tests. Add the shared web source, contract and tests.
   Raise platform-sdk's version
   with a precise compatibility note. The public engine C surface stays stable.
   Record extraction provenance in the private source game's knowledge files;
   do not publish its identifiers or implementation-specific context fields here.
3. Wire the starter and independent feature consumer on Web: opt-in `nt_crash` link;
   initialize before window/gfx and before recoverable startup failures; derive
   keys/installation arguments from existing metadata; inline the shared source
   through configure_file without adding SDK config members.
   Add the source to CMAKE_CONFIGURE_DEPENDS so edits rebuild HTML. Existing
   feature-source fingerprints must include it. Release packaging preserves the
   inline reporter before the bundled runtime script; no extra ZIP entry or CDN.
   Add packaging validation of the executable inline block against expected
   shared source and generated arguments, not a trusted self-declared hash or
   comment marker. Reuse existing source hashing; reject a missing, stale,
   duplicated or late block. Update bootstrap recognition for caught preview
   imports and release bundling; the broad module-name check must not reject
   inert filename data. Do not weaken checks against executable leftover imports.
4. Prove a throwaway `games/new_game.mjs` game, then migrate the source game and
   remove its proven duplicates. Preserve the previous engine storage key and
   accept the bounded legacy pending schema without changing original build/time.
   Do not remove separate report kinds before the parity/retirement decision.
   Never delete unsent records as cleanup.
5. Inventory canonical games through `games.mjs list --include-private --json`.
   For each, inspect its real engine/API generation, custom shell, dependency pin
   and dirty work before changing it. Migrate source integration and pin together;
   run its focused tests and a local browser check. The other ten games currently
   have no crash initialization: this is new opt-in wiring for them, not only
   duplicate removal. Their copied package tools also need the integration gate.
   Do not bulk-update pins on
   incompatible older consumers. Comparison worktrees are not deployment targets.
6. Keep a local per-game migration/evidence matrix (private identifiers stay
   private). Only mark universal installation complete when every canonical game
   is integrated or explicitly classified with a concrete blocker. New games
   inherit the starter wiring. Existing published builds change only after their
   own rebuild/deployment; this task does not automatically publish them.

## Acceptance and review

| Check | Required evidence |
| --- | --- |
| HTTP callback fault | Original EM_JS executed with throws in progress/completion/malloc paths; stream and arrayBuffer; rejection observed, no fallback completion/re-entry. Transport rejection/cancel still finish correctly. |
| SDK callback fault | Lifecycle replay and live pause/resume/audio/sound/ad hooks, init and completion paths: exception escapes through the full facade/adapter/deadline chain, no second WASM call. SDK rejection, timeout and late resolution remain valid supported outcomes. |
| Abort capture | Real pinned-compiler browser build: string/number abort reason recorded with stack before shell callback, caught abort retained, handler called once with same receiver/args, throwing handler contained, ABORT/RuntimeError still occur, later global event deduped. Capture/storage failure cannot prevent termination. Separate TRAP test must pass via global listener, not onAbort. |
| Ring and identity | WARN/ERROR before trap, readable program and phase, GPU/build/context retained after reload and after memory growth. |
| Engine optionality | Consumer without nt_crash still builds and has no new dependency; no native behavior change. |
| Shared delivery | SDK delay/absence/throw, pacing, fixed capacities and current priority, previous plus current reports, UTF-8 cap and credential stripping, no WASM exports called. Eligible candidate suppressed by live engine event before flush; unrelated pure-JS report retained; replay does not suppress current fallback. Same raw report once per session; next launch retries unconfirmed report with original identity. No sent-marker/automatic clear. |
| Bootstrap coverage | Missing runtime JS, own SDK facade/adapter import or evaluation failure, failed WASM fetch/compile/instantiate, pre-init abort/trap and successCallback trap, own-script TypeError without WASM frame, unrelated extension/portal error excluded. Local module failure shows fatal UI; external portal SDK adblock remains playable. Preview and bundled release both tested. |
| Storage | Storage denied/full, malformed/oversized/expired web report, bounded direct engine replay with no C bridge, separate games on one origin, identical generated C/JS keys, preserved previous engine key and legacy report/build/time. No unconfirmed record cleared. |
| Template propagation | Source edit regenerates HTML and fingerprint. Packager rejects missing/stale/duplicate/late inline source, while harmless diagnostic filename data is accepted. Throwaway generated game and copied packagers work without copied reporter source. |
| Consumer behavior | Normal play, fatal overlay, unexpected WebGL loss/creation signal, two deliberate loss/recovery cycles excluded from reports, and diagnostic fault on local optimized artifact. A trap outside RAF does not cause a report storm or overwrite the first fatal. |
| Cost | Compare gzip JS/WASM and static record storage before/after, count diagnostic C-to-JS calls per frame; no new hot-path heap or reporting poll in game frame. |

Run narrow tests while editing, then owner checks and the engine's required
format/check gates. Two independent code-review lenses cover engine principles
and Web/WASM behavior after implementation. Browser proof uses the actual linked
code, not only copied JavaScript mocks. Hosted Poki delivery, physical PowerVR
and browser-process termination remain separate, explicitly unverified claims.

## Plan review procedure

Before implementation, independently review this plan with an engine-principle
reviewer, a Web/WASM/integration reviewer and Claude. Give Claude the research,
repository principles, local findings and private migration context; the user
explicitly authorized that disclosure. All reviewers are read-only. Record
specific objections, accept/refute each with evidence, revise this plan and
re-review changed contracts. Do not claim unanimous approval while a blocker
remains. The final response links this plan and reports the remaining decisions.

## Review resolutions

Initial independent verdicts: engine-principle reviewer, Web/WASM reviewer and
Claude all requested revision. Their material findings changed this plan:

- Accept: separate abort writer from WASM classification, preserve primary abort
  despite a throwing shell callback, preserve pre-init fallback, and fix SDK
  exception boundaries in addition to HTTP.
- Accept: remove per-game replay bridge and buffer; read the documented web key
  directly, derive C/JS keys once, and preserve original replay identity.
- Accept: handle local SDK-module bootstrap failure and strict release packaging;
  stop per-frame context updates; inventory nonfatal signals before deletion.
- Keep with explicit scope: onAbort is an abort-only supported boundary, not a
  replacement for global TRAP detection. The current shell has a real onAbort
  callback, so recording before it removes a present ordering dependency.
- Reject: persistent sent marker or deleting after SDK invocation. They suppress
  retries without acknowledgement and can hide undelivered records. Repeated
  reports across launches are explicit; backend deduplication is not assumed.
- Keep with bounded contract: one early-web slot, because the missing runtime
  bundle also contains the sender. Reuse the existing schema/expiry, no journal
  mirror or general retry queue.
- Accept after additional Web review: supplied portal callbacks need operation-
  local fault ownership across asynchronous invocation. Reject pending work on
  its first callback fault. Both subagents and Claude re-reviewed this contract.

Implementation source verification refined the timeout rule: the existing SDK
architecture and Playgama timeout test require late visibility until definitive
close. The Web/WASM reviewer approved preserving healthy late callbacks and using
one microtask rethrow for a callback fault after settlement; subsequent C entry
for that faulted operation is still forbidden. This preserves the existing
contract rather than treating every settled operation as a failed runtime.

Final plan verdicts on 2026-10-19: engine-principle reviewer READY, Web/WASM
reviewer READY, Claude READY after independent critique and focused re-review.
Claude's final nonblocking clarifications (immediate rejection and the synchronous
callback-fault check) are included above. These are plan verdicts, not evidence
that the future implementation or a new device build has passed testing.

Remaining lead decision before cleanup: retire recoverable per-request portal
samples, or retain their current implementation until structured parity exists.
Recommended scope is to retain fatal pack failures and browser graphics signals,
and retire per-request portal sampling; ordinary HTTP logs remain in the engine
journal. This recommendation is not authorization to delete those report kinds.
