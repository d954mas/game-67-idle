# AI Pipeline Retrospective - 2026-06-13

Scope: long 67 World AI-assisted development session across concept, reference
study, generated art, native PC gameplay, balance, release packaging, and
pipeline rules.

Evidence inspected: `AGENTS.md`, `AI_PIPELINE.md`, `AI_PIPELINE_ITERATION_LOG.md`,
`tasks/STATUS.md`, active task logs, `release_candidate_audit_v29_manual_55.json`,
package smoke evidence, child-test readiness screenshots, and latest native
framebuffer screenshots.

## 1. What Was Done

- A concrete active concept was selected: `67 World`, a child-friendly meme
  merge/evolution game inspired by the broad Cow Evolution loop. Every
  collectible is a 67 variant.
- Native PC became the primary harness after an initial wrong web detour.
- The game moved from placeholder screen to a native playable loop:
  `TAP BOX`, spawn, merge matching 67s, unlock variants, earn currency, buy
  upgrades, recover from a full board with `FREE SLOT`.
- Progression expanded to 30 variants through Cosmic 67.
- One-hour automated progression was tuned to reach Cosmic 67 at about
  57.19 minutes through native C runtime actions.
- FTUE/tutorial cues, upgrade states, collection drawer, stuck-board recovery,
  audio cues, portrait layout, save isolation, and packaging flows were added.
- Generated art and runtime asset packs were introduced, including field tiles,
  fence, HUD/catalog panels, characters, and repaired sprite/keying paths.
- Release packaging became fairly complete: branded `67-world.exe`, package
  zip, manifest, checksums, self-check, start menu, child-test launchers,
  parent guide, acceptance kit, report recorder, validator, export bundle, and
  release audit.
- Process rules were improved: native-PC-first gate, art-first gate, art job
  scaffold, reference deconstruction gate, Reference Lock, Reference Digest,
  Source Ladder, Reference Evidence Board, and now `chat-session-reflection`.

Current hard truth: automated release gates pass, but `release_ready=false`
because real manual child-test/user acceptance is still missing. Also, the
latest screenshots still do not meet the user's visual-quality bar.

## 2. Where The Most Time Went

### 1. Wrong web detour

- Symptom: A web prototype path was created while the project expected native
  PC as the primary harness.
- Cause: I optimized for fast/pretty UI instead of obeying `AGENTS.md` and the
  user's PC-build rule.
- Category: agent behavior and tool misuse.
- Faster path: restate "native PC is the harness", improve `src/main.c`, and
  capture native screenshots first.

### 2. Shallow Cow Evolution understanding

- Symptom: Early gameplay/UI did not read like Cow Evolution to the user.
- Cause: I extracted genre mechanics before proving screen grammar: field,
  first action, object placement, reward location, and UI hierarchy.
- Category: weak reference study and premature implementation.
- Faster path: build a deconstruction with gameplay frames, timestamps, current
  native mismatch, and next proof before coding.

### 3. Visual work started as placeholder polish

- Symptom: I improved rectangles, panels, and procedural shapes when the user
  asked for generated art and reusable UI.
- Cause: I treated visual quality as render polish instead of asset pipeline
  work.
- Category: misunderstanding the task.
- Faster path: create art job -> generate candidates -> crop/slice manifest ->
  pack -> native screenshot proof.

### 4. Manual art slicing and keying

- Symptom: Crops were wrong, assets had bad alpha, some pieces looked cut,
  contaminated, or visually incoherent.
- Cause: Generated sheets were integrated without enough contact-sheet QA and
  key-color planning. Green chroma conflicted with green field assets.
- Category: missing asset QA tooling.
- Faster path: use magenta key for green-heavy sheets, require edge/alpha bbox
  validation, and generate labeled contact sheets before packing.

### 5. Framebuffer capture bug

- Symptom: Visual evidence looked wrong while tests still passed.
- Cause: PPM parsing consumed a pixel byte as whitespace and fell back to
  window capture, hiding the real evidence source problem.
- Category: automation tool bug.
- Faster path: validate screenshot dimensions/source immediately and fail if
  fallback is used unexpectedly.

### 6. Stale project rules

- Symptom: `AGENTS.md` still said no active concept while `tasks/STATUS.md`
  already had 67 World as active.
- Cause: source-of-truth drift after the concept became real.
- Category: context management failure.
- Faster path: update project rules in the same session where a concept is
  accepted.

### 7. Packaging hardening before visual quality was stable

- Symptom: Many tasks hardened release packaging, acceptance reports, hashes,
  launchers, and audits while the visual surface was still not good enough.
- Cause: Automated release gates were easier to define than visual taste gates.
- Category: planning/order problem.
- Faster path: set a visual acceptance gate first: no tile seams, readable text,
  coherent 2D composition, no bad crops, no flipped assets.

### 8. Repeated broad validation after tiny changes

- Symptom: Package smoke, readiness, and release audit were rerun repeatedly
  for small packaging/doc edits.
- Cause: release audit binds hashes, so every package file change invalidates
  downstream evidence.
- Category: validation strategy friction.
- Faster path: batch packaging changes, then run one smoke/readiness/audit
  chain. Use narrower static checks before the full chain.

### 9. `tasks/STATUS.md` became too large

- Symptom: The live status file contains a huge evidence history and is slow to
  scan.
- Cause: I used it as a running changelog instead of a compact index.
- Category: context hygiene failure.
- Faster path: keep only current gate, latest evidence, blockers, and next
  priorities in `STATUS.md`; put history in task logs.

### 10. Manual acceptance remained external

- Symptom: The release audit still cannot mark release ready.
- Cause: real child-test/user acceptance requires a human session.
- Category: external validation gap, but also planning issue.
- Faster path: once automated gates were close, package the test kit and stop
  expanding automation until a real child-test can happen.

## 3. Where I Was Inefficient Or Wrong

- I used web when the project needed native PC. This was directly against the
  user's workflow.
- I treated "not a game" and "not like Cow Evolution" as implementation
  feedback before doing a strong reference mismatch audit.
- I over-relied on pixel-health/nonblank checks. They prove rendering, not
  beauty or readability.
- I did not inspect latest screenshots early enough. The current package
  screenshot still has obvious tile seams, weird 2D fence posts, hard-to-read
  text, and clutter.
- I let many tasks sit in `review`, creating the illusion of progress while
  the actual product-quality gate was still unresolved.
- I created strong packaging evidence while the product still had visual
  release blockers.
- I corrected process rules reactively after mistakes instead of starting with
  the right gate.
- I spent too long making validators around manual reports before asking
  whether the game was visually acceptable enough to test with children.
- I allowed `STATUS.md` to bloat instead of preserving a crisp current state.
- I did not create a standard contact-sheet/asset-edge QA tool early enough.

## 4. Tool Use Analysis

### Terminal and file search

- Useful: `rg`, `Get-Content`, taskboard CLI, and targeted commands made it
  possible to reconstruct state and validate changes.
- Weak use: I often read too much status/log history instead of a smaller set
  of current files.
- Improvement: start each session from `AGENTS.md`, a compact `STATUS.md`, and
  one active task. Avoid broad status dumps.

### DevAPI and native screenshots

- Useful: DevAPI scenarios gave repeatable proof for FTUE, progression, audio
  cue counts, portrait layout, package smoke, and one-hour progression.
- Weak use: screenshot health checks were too quantitative. They did not catch
  "ugly", "stitched", "unreadable", or "bad 2D fence".
- Improvement: add a visual-audit scenario/checklist that fails on tile seams,
  upside-down/cropped assets, tiny text, mixed style, and visual clutter.

### Image generation and asset pipeline

- Useful: generated art entered runtime packs, and pack/cache paths became
  explicit.
- Weak use: candidate selection and crop validation were too manual. I did not
  inspect assets at gameplay scale enough before integrating.
- Improvement: use art jobs, contact sheets, alpha/edge validators, and
  native-scale screenshots before touching release packaging.

### Release audit and package smoke

- Useful: the package is now much harder to ship broken by accident.
- Weak use: I kept strengthening release automation while product visuals were
  still not accepted by the lead.
- Improvement: release audit should include a manual/human visual acceptance
  blocker, not only screenshot health.

### Skills

- Useful: skills helped encode repeated corrections: native PC gate, visual
  art direction, asset pipeline, reference study, and now reflection.
- Weak use: skills were created after failures, not before the first risky
  implementation.
- Improvement: before long work, choose the relevant skills and state their
  gates.

### Web/browser

- Bad use: web prototype work was not needed and violated project direction.
- Improvement: ban web for playable work unless current user message explicitly
  asks for it.

### Multi-agent/parallel work

- Weak use: parallelism was discussed more than operationally used.
- Improvement: only split independent lanes: reference research, asset QA,
  runtime integration, and verification. Do not parallelize ref-dependent
  implementation before the reference digest exists.

## 5. Context Problems

- I forgot the native PC rule and had to correct it after user feedback.
- I underweighted "generated visual and UI" and over-weighted "get something
  playable".
- I lost the user's art requirements: reusable UI, slice9, separate board
  parts, no baked buttons, no one big image.
- I repeatedly had to rediscover whether Cow Evolution was actually studied.
- `AGENTS.md` drifted behind the active concept, causing avoidable ambiguity.
- `tasks/STATUS.md` stopped being short enough to serve as fast current state.
- The release goal mixed two truths: automation was strong, but visual/user
  acceptance was still not done.
- The manual child-test blocker stayed visible, but the visual acceptance
  blocker was not made equally formal.

Rules that should have stayed pinned:

- Native PC first.
- Art-first for visual-quality requests.
- Reference study before implementation.
- Durable task/status evidence.
- Generated UI must be reusable.
- Human visual acceptance outranks automated pixel health.

## 6. Planning Problems

- The order should have been: reference deconstruction -> visual target -> art
  job -> native vertical slice -> visual review -> progression/balance ->
  packaging. I mixed packaging hardening into the visual-quality phase.
- The first vertical slice should have been one beautiful native screen: first
  60 seconds, with clean field, readable CTA, two 67s, merge reward, and no
  visible seams.
- Scope was too broad: "make release-ready game" caused many parallel concerns
  to be touched before the core visual/product proof was accepted.
- Definition of Done was missing for visual quality. Pixel health is not a DoD.
- Intermediate checkpoints should have been "lead accepted screenshot" gates,
  not only scenario/report gates.
- Packaging tasks became too granular and numerous; they were correct but
  created process overhead.

## 7. Product Quality Problems

Director/reviewer view:

- Latest native screenshot is not release-quality visually.
- Field tiles show hard seams and repeated squares.
- Fence posts/stakes look strange in 2D and dominate the composition.
- Some assets look pasted on top rather than composed into one art direction.
- Text is too pixelated and cramped in several HUD/CTA areas.
- UI hierarchy is busy: top HUD, central sign, crate, tap ring, field, bottom
  collection all compete.
- The package is technically strong, but the product still fails the user's
  visual expectation.
- Manual child-test is still missing, so release readiness is not proven.
- Real audio pleasantness is not manually validated with children.
- The one-hour loop is automated-balanced, but fun and comprehension over 55+
  minutes are unproven.

Highest-priority product fixes:

1. Replace or redesign the field background to remove visible seams.
2. Replace the fence/stakes with a simpler flat 2D border or softer world
   frame.
3. Rebuild CTA/HUD text treatment for readability at desktop and portrait size.
4. Run a human visual review gate before more release packaging work.
5. Prepare for real child-test only after the screen is visually acceptable.

## 8. Improved Workflow For The Next Long Cycle

Start of session:

1. Read `AGENTS.md`, compact `tasks/STATUS.md`, and current active task.
2. State current harness, current blocker, and current proof target.
3. Open latest native screenshots before making visual claims.
4. Pick relevant skills and name their gates.

Task formulation:

1. Write one working scope.
2. Write DoD with product proof and technical proof.
3. For visual tasks, include a human-readable visual checklist.
4. For reference tasks, require Evidence Board and Digest before coding.

Implementation:

1. Build one vertical slice.
2. Validate in native PC first.
3. Inspect screenshots manually.
4. Only then broaden to balance/package/release.

Review:

1. Run automated tests.
2. Run visual review against the screenshot.
3. Compare against user complaints and reference digest.
4. Record `done/problems/next` in task log.

Context preservation:

1. Keep `STATUS.md` short.
2. Put detailed history in task logs.
3. Use `AI_PIPELINE_ITERATION_LOG.md` only for reusable process lessons.
4. Do not let chat memory be the only source of decisions.

## 9. Prompt/System Work Changes

Add:

- Before visual work, inspect latest native screenshot and name the top 3
  visual blockers.
- For any "beautiful/release-quality" request, create a visual DoD: readable
  text, no seams, no bad crops, no flipped assets, coherent 2D composition,
  gameplay understandable in one screenshot.
- Automated pixel health is insufficient for visual acceptance.
- Packaging/release hardening must not outrun product visual acceptance.
- If the user says "ugly/unreadable/not understandable", stop implementation
  and do a visual review before coding.
- Keep `STATUS.md` as a compact index; do not paste long histories.
- Use native PC unless explicitly allowed otherwise.
- Use reference Evidence Board before implementing ref-driven work.
- Use art job/contact sheet/edge QA before runtime packing.
- End each long cycle with `done/problems/next` and a short process lesson if
  a recurring mistake appeared.

Ban:

- No web prototype for playable work without explicit current permission.
- No claiming visual quality from nonblank/pixel-health checks.
- No final-art integration from generated sheets without crop/edge/alpha QA.
- No "studied ref" claim without source frames and mismatch audit.
- No release-ready claim without real manual child-test/user acceptance.

## 10 Main Improvements For The Next Cycle

1. Start with native screenshot review, not code.
2. Make visual DoD explicit and human-readable.
3. Fix the field seams and weird fence before more release hardening.
4. Keep native PC as the only gameplay harness unless explicitly changed.
5. Use Reference Evidence Board before ref-driven implementation.
6. Use art jobs plus contact sheets before pack integration.
7. Batch package changes before running full smoke/readiness/audit.
8. Keep `STATUS.md` short; move history to task logs.
9. Treat user visual rejection as a blocker, not a polish note.
10. Do not call the game release-ready until automated gates, visual acceptance,
    and real child-test acceptance all pass.
