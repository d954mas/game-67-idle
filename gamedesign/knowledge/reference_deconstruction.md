# Reference Deconstruction

Goal: prevent shallow "genre understood" research from driving implementation.
A reference is implementation-ready only when it explains the player-facing
screen grammar, timing, rewards, and mismatch against the current build, not
just the feature list.

## Required When

Use this gate before implementation when:

- the user names a specific reference game, app, ad, toy, meme, or visual style;
- the task asks for gameplay "like", "in the style of", or "similar to" a
  reference;
- the current screenshot is rejected as "not a game", "not like the ref",
  unclear, ugly, or visually incoherent;
- a feature depends on player expectations from an existing genre.

This is a stop rule, not a documentation preference. If the reference drives
gameplay, UI, economy, balance, or art direction, implementation waits until a
durable deconstruction exists or a clearly scoped exception is recorded.

## Study Modes

Choose and state the mode before research. The mode controls how much evidence
is enough and what implementation it can unlock.

### Quick Check

Use for a narrow UI label, one visual motif, one ad/meme pattern, or a small
screen-composition question.

Minimum output:

- 1-2 source links or local paths;
- checked date;
- 3 concrete visible observations;
- borrow / avoid / copy-risk;
- one sentence explaining the small implementation decision.

Limits:

- does not justify core gameplay, economy, balance, progression pacing, or
  broad art-direction changes;
- if the observation changes the main loop or first screen, escalate to
  central deconstruction.

### Central Deconstruction

Use when the user names a reference as the gameplay, UI, economy, balance, or
art-direction driver, or when the current build is rejected as not matching the
reference.

Minimum output:

- source quorum: official/store/trailer visuals, gameplay footage/walkthrough
  or long screenshot sequence, and current native build capture;
- source matrix with quality, checked date, what each source proves, and what
  remains uncertain;
- observation ledger with at least 5 visible beats;
- first screen, first input, first 10 seconds, first 60 seconds, and 1-5 minute
  loop;
- screen grammar, mechanics/economy/balance notes, visual composition;
- borrow / avoid / copy-risk;
- mismatch audit against current native build;
- exact next native screenshot, scenario, or capture that proves the
  translation.

### Deep Deconstruction

Use when the reference controls first-session pacing, one-hour balance,
retention pressure, monetization pressure, child-test UX, or release-critical
readability.

Minimum output:

- everything from central deconstruction;
- 2+ gameplay sources or one long source with enough timestamps to cover
  repeated play;
- explicit timing/balance table for visible timers, currency changes, object
  counts, prices, blockers, recovery, and unlock cadence;
- child-safety/tone audit when the target audience includes children;
- implementation risk list and validation plan.

Do not start coding from a quick check when the change affects loop, economy,
balance, primary controls, or release-critical UX. Escalate the mode first.

## Minimum Evidence

Collect enough evidence to describe what the player actually sees and does:

- official/store page screenshots or trailer when available;
- at least one gameplay video, walkthrough, or long screenshot set when the
  interaction model matters;
- one visual reference source when art direction matters;
- one review, guide, wiki, or community source only when it clarifies real
  player friction or loop pacing.

If video is unavailable, state that plainly and use screenshots plus written
walkthroughs. Do not claim video-derived behavior without video evidence.

Do not rely on memory for a central reference. If sources cannot be accessed,
ask the user for screenshots/video or record a clearly scoped memory-only
exception before implementation.

Minimum useful packet for a central gameplay reference:

- 2+ visual sources, preferably official/store screenshots plus video frames;
- 1 gameplay video, walkthrough, or long screenshot sequence for timing;
- 1 guide/review/wiki only when it clarifies pacing, friction, or balance;
- current build screenshot or capture path for the mismatch audit.

## Source Ladder

Study sources in this order before drawing conclusions:

1. User-provided material: screenshots, videos, links, notes, or taste
   corrections from the lead.
2. Official/store/trailer visuals: official screenshots, trailer frames, store
   pages, studio posts, or platform pages.
3. Raw gameplay evidence: gameplay video, walkthrough, stream capture, or a
   long screenshot sequence that shows actions and responses.
4. Supporting interpretation: guides, reviews, lectures, deconstructions, wiki
   pages, community posts, and balance notes.

Central and deep studies must include a Source Ladder section in the durable
doc. For each source, record:

- link or local path;
- source role from the ladder above;
- checked date;
- watched/read scope: timestamps, frames, pages, or screenshot ids;
- what it proves;
- what it cannot prove;
- evidence type: observation evidence or secondary interpretation.

Use the ladder to prevent shallow research:

- Do not rely on search snippets, thumbnails, or memory as source evidence.
- Do not let a guide, review, lecture, or deconstruction replace observed
  gameplay frames for first-screen, first-input, loop, control, reward, or UI
  hierarchy claims.
- If the available material stops at store screenshots or secondary summaries,
  mark the study as a quick check or source-packet incomplete. Ask for user
  material or record a narrow exception before implementation.

Do not write "studied" in a task, final answer, or implementation handoff until
that packet is present in the durable doc. If only memory, store screenshots,
or a second-hand summary are available, write the narrower truth: "quick check
only", "source packet incomplete", or "memory-only exception".

## Reference Evidence Board

Central and deep references need an evidence board before conclusions. The
board is a compact set of cited frames, screenshots, or local captures that
prove the player-facing loop. It prevents "I watched enough" from becoming an
unverifiable claim.

Minimum board for a central/deep gameplay reference:

- first screen before the player acts;
- first input target and how it is signaled;
- visible response after that input;
- reward feedback location: coins, counter, animation, sound cue implication,
  unlock, or modal;
- upgrade, shop, collection, progression, or status UI;
- friction/blocked state: full board, waiting, price too high, cooldown,
  modal interruption, missing space, or recovery;
- one raw gameplay video/walkthrough segment, stream capture, or long
  screenshot sequence for timing and repeated actions;
- one guide, review, lecture, wiki, community note, or deconstruction only
  when the implementation uses balance, pacing, player-friction, retention, or
  monetization claims.

For each item, record:

- link or local path;
- timestamp, frame id, page, or screenshot id;
- checked date;
- source role from the Source Ladder;
- what this item proves;
- what it cannot prove.

If fewer than six player-facing frames/screenshots can be cited, mark the
evidence board incomplete. A quick check may still answer a tiny visual or UI
question, but central gameplay, economy, balance, primary UI, or final art
implementation remains locked unless the user approves a narrow exception.

## Auditable Deliverable Rule

The output of reference study is a durable artifact, not confidence in chat.
For a central gameplay reference, create or update a deconstruction doc in the
active design folder before implementation continues. The doc must let a later
agent or the user audit the work without replaying the conversation.

Required source matrix:

- source title/link or local path;
- source quality: user-provided, official/store/trailer, gameplay footage,
  walkthrough/guide, review/community, deconstruction/analysis, or memory-only
  exception;
- checked date;
- what the source proves: visual layout, first input, timing, economy,
  reward feedback, UI hierarchy, balance, player friction, or tone;
- what the source does not prove or leaves uncertain.

Required study evidence:

- at least one gameplay video, walkthrough, or long screenshot sequence when
  interaction matters;
- official/store/trailer visuals when available, to avoid learning only from
  second-hand summaries;
- guide, review, wiki, lecture, or deconstruction only as supporting evidence,
  not as a replacement for observed gameplay;
- current native build capture path for mismatch comparison.

If the user asks whether the reference was actually studied, answer by naming
the deconstruction doc and the concrete observations inside it. If the doc does
not answer the challenge, implementation remains blocked until the study is
improved.

Each non-trivial conclusion in the doc should carry an evidence label:

- `observed`: directly visible in a screenshot, video timestamp, walkthrough
  step, or local capture;
- `inferred`: reasonable design inference, but not directly visible;
- `user-provided`: stated by the user or provided in user material;
- `unknown`: not proven yet.

Only observed or user-provided claims can unlock implementation by default.
Inferred claims need a clearly marked risk, and unknown claims remain research
gaps.

The answer must be specific enough to cite at least three observed facts from
timestamps, frames, or screenshots, and one mismatch against the current native
build. If the agent cannot do that from the doc, it has not studied the
reference for implementation purposes.

## Study Protocol

Use this order before coding reference-driven gameplay, UI, economy, or art:

00. **Run Reference Intake.** When the user names a reference or says the
    current build does not match it, write the intake before defending,
    redesigning, coding, or generating final art:
    - exact reference question: loop, first screen, UI hierarchy, economy
      pacing, art direction, humor/tone, or rejected mismatch;
    - study mode: quick check, central deconstruction, or deep deconstruction;
    - durable deconstruction doc path;
    - required source packet;
    - current native capture path or capture plan;
    - no-coding/no-final-art boundary;
    - first proof screenshot/scenario.
    Every later claim must be labeled as observed, inferred, user-provided, or
    unknown. Observed claims need a source timestamp, frame, screenshot, or
    local path. Inferred and unknown claims cannot drive implementation unless
    the doc records a narrow exception accepted by the user.
0. **Set the Reference Lock.** State the study mode, exact reference question,
   deconstruction doc path, required source packet, current native capture path
   or capture plan, no-coding/no-final-art boundary, and expected native
   screenshot/scenario proof. If the lock cannot name the artifact path and
   proof, do not start implementation.
1. **Collect sources.** Prefer official/store screenshots or trailer, gameplay
   video/walkthrough, long screenshot set, and one guide/review/wiki only when
   it explains real pacing or player friction. Record the Source Ladder before
   conclusions: user-provided material, official/store/trailer visuals, raw
   gameplay evidence, and supporting interpretation. If the ladder lacks raw
   gameplay evidence for interaction claims, the study is not ready for central
   gameplay implementation unless the user approves a narrow exception.
2. **Observe before inferring.** Write what appears on screen in the first 10
   seconds, first 60 seconds, and 1-5 minute loop before naming systems.
3. **Extract screen grammar.** Name the main play space, camera/framing,
   object spawn area, click/touch targets, reward location, secondary UI, modal
   cadence, feedback effects, and object scale hierarchy.
4. **Extract mechanics and balance.** Capture visible object counts, timers,
   merge/combine rules, currencies, sources, sinks, unlock cadence, blockers,
   offline/session rhythm, and fail/recovery states when visible.
5. **Translate, do not clone.** Write borrow, avoid, and copy-risk notes before
   choosing implementation details.
6. **Audit the current build.** Compare against a current screenshot or capture
   and list concrete mismatches that the next code/art pass must fix.
7. **Name the proof.** End with the next implementation pass and the exact
   screenshot, DevAPI scenario, or native capture that will prove the reference
   translation worked.

For live games or rapidly changing references, include the date checked.

## Agent Action Sequence

When a user says "make it like X" or challenges that the current result is not
like the reference, run this sequence before more implementation:

0. **Run Reference Intake.** State what the ref must prove, selected study
   mode, doc path, source packet, current native capture plan/path,
   no-coding/no-final-art boundary, and first proof screenshot/scenario. Do not
   defend the current build or start a new implementation pass before this is
   written.
1. **Set the Reference Lock.** Name the mode, exact question, doc path, source
   packet, current native capture plan, boundary that blocks coding/final art,
   and the expected proof screenshot/scenario.
2. **State the reference question.** Name what the ref must answer: gameplay
   loop, first screen, UI hierarchy, economy pacing, art direction, humor/tone,
   or a specific rejected mismatch.
3. **Collect a source packet.** Gather links or local paths for official/store
   visuals, gameplay footage/walkthrough or long screenshot sequence, useful
   guide/review/deconstruction if needed, and the current native capture.
4. **Record evidence, not confidence.** Write checked date, source quality,
   what each source proves, and what remains uncertain. If internet or source
   access fails, stop for user material or record a narrow memory-only
   exception.
5. **Transcribe the player view.** Fill at least 5 visible beats before
   conclusions: screen state, player action, visible response, reward/UI
   feedback, and only then inferred meaning.
6. **Extract systems from the transcript.** Derive loop, screen grammar,
   object counts, timers, currencies, sources/sinks, upgrade cadence,
   blockers, recovery, and visual composition. Mark unsupported details as
   inferred.
7. **Translate safely.** Write borrow, avoid, and copy-risk. Do not copy names,
   screenshots, exact characters, monetized pressure, or protected UI/asset
   shapes.
8. **Compare to our build.** Put the current native screenshot/capture path in
   the doc and list concrete mismatches: what is missing, unclear, too slow,
   too cluttered, too static, or visually off.
9. **Name the next proof.** End with one implementation pass and the exact
   native screenshot, DevAPI scenario, simulator, or capture that will prove it.

Implementation may start only after step 9 exists. If the next agent cannot
name three observed facts from the reference and one current-build mismatch, it
has not studied the reference.

## Observation Ledger Rule

Before summarizing a central reference, write a visible-action ledger. This is
the part that prevents shallow "I know the game" reasoning.

Minimum ledger:

| Beat | Source timestamp/frame | Visible screen state | Player action | Visible response | Reward/UI feedback | Inferred meaning |
|---|---|---|---|---|---|---|
| 1 |  |  |  |  |  |  |
| 2 |  |  |  |  |  |  |
| 3 |  |  |  |  |  |  |
| 4 |  |  |  |  |  |  |
| 5 |  |  |  |  |  |  |

Rules:

- Fill `Visible screen state`, `Player action`, `Visible response`, and
  `Reward/UI feedback` before `Inferred meaning`.
- Use concrete verbs: tap, drag, merge, wait, collect, buy, unlock, close,
  open, clear, return.
- Include failed or blocked beats when visible: full board, waiting, price too
  high, modal, missing space, cooldown, or forced upgrade.
- If the reference is a video, use timestamps. If it is screenshots, use frame
  numbers or local filenames.
- If fewer than 5 beats can be observed, mark the evidence gap and do not
  implement ref-driven behavior without user approval or a narrow
  memory-only exception.

## Four-Pass Reference Study Method

Use this method when the reference is central to gameplay, UI, economy,
balance, or art direction. Do not skip directly from sources to system design.

### Pass 1: Source Packet

Collect and label the evidence before interpreting it:

- source links or local paths;
- source quality: user-provided, official/store/trailer, gameplay footage,
  walkthrough/guide, review/community, or memory-only exception;
- checked date;
- what each source proves and what it does not prove;
- current native build screenshot/capture path for comparison.

For central gameplay refs, the packet should include official/store/trailer
visuals, gameplay video/walkthrough or a long screenshot sequence, and the
current build capture. If that quorum is impossible, stop and ask for material
or record a narrow memory-only exception.

### Pass 2: Player Transcript

Write only what a player can see or do. Do not name systems yet.

- first screen: visible objects, UI, camera/framing, empty/filled areas;
- first input: exact tap/click/drag target and how it is signaled;
- visible response: object spawn, merge, movement, reward, number change,
  animation, sound implication, modal, unlock, or blocker;
- first 10 seconds: action/response sequence;
- first 60 seconds: repeated actions, first reward, first friction;
- 1-5 minute loop: what repeats, what changes, what blocks progress, what asks
  the player to upgrade, wait, clear space, or return.

Bad transcript: "idle merge economy with upgrades." Good transcript: "The
field is full screen; player taps a crate at lower right; a new creature
appears on the field; dragging two matching creatures creates a higher-tier
creature; coins pop near the merged object and the top coin counter increases."

### Pass 3: Systems Extraction

After the transcript, extract the design:

- screen grammar: main play space, spawn area, object slots/free placement,
  reward location, primary controls, secondary UI, modal cadence;
- mechanics: spawn, combine/merge, collection, clearing/recycling, timers,
  offline/session rhythm, fail/recovery states;
- economy: currencies, sources, sinks, price growth, upgrade cadence, blockers;
- balance clues: visible object counts, wait times, upgrade thresholds,
  compounding pace, first-session target;
- visual composition: scale hierarchy, silhouettes, density, feedback effects,
  background/field treatment, child readability.

Mark every inferred detail as inferred when it is not directly visible.

### Pass 4: Translation Gate

Finish with implementation decisions tied to proof:

- borrow: behavior, screen grammar, pacing, UI feedback, or visual structure to
  adapt;
- avoid: confusing parts, monetization pressure, adult/unsafe tone, clutter,
  or anything outside the current audience;
- copy-risk: names, exact characters, UI layouts, monetized flows, screenshots,
  or asset shapes that must not be cloned;
- current-build mismatch: concrete differences between the native screenshot
  and the reference transcript;
- next pass: one scoped gameplay/UI/art change;
- proof: exact native screenshot, DevAPI scenario, simulator, or capture that
  will prove the translation.

If the translation gate is vague, implementation stays blocked. The next action
is to improve the deconstruction, not to code.

## Reference Study Rule

The rule is: observe like a player before designing like a developer.

For a central gameplay reference, do not start implementation until the durable
study contains:

- source quorum: official/store/trailer visuals, gameplay video/walkthrough or
  long screenshot sequence, and a current build screenshot for comparison;
- first-screen observation: what is visible before the player acts;
- first input: what the player taps/clicks/drags and how that target is
  presented;
- visible response: spawn, merge, reward, animation, UI count, unlock, modal,
  or sound/feedback implied by the footage;
- screen grammar: play field, object location, reward location, camera/framing,
  primary UI, secondary UI, and modal cadence;
- pacing: what repeats in the first minute and what changes by minute five;
- translation: borrow, avoid, copy-risk, current-build mismatch, and next proof
  screenshot/scenario.

Not enough:

- "I know this genre";
- store description without gameplay evidence;
- a mechanics list without screen layout and first actions;
- visual mood words without actual screenshots or frames;
- coding from memory when the named reference is central.

If the source quorum cannot be met, ask for user-provided material or record a
memory-only exception with narrow scope before coding.

## Reference Study Definition Of Ready

Before coding gameplay/UI/economy/balance or generating final art from a named
reference, the reference study is ready only when all of these are true:

- the study mode is stated and strong enough for the requested change;
- the durable doc path exists in the active design folder;
- the Reference Lock states the question, source packet, current native capture
  plan/path, no-coding/no-final-art boundary, expected proof, and unlock
  condition;
- the source matrix includes links or local paths, checked dates, source
  quality, what each source proves, and what remains uncertain;
- a central/deep gameplay ref has observed gameplay evidence: gameplay footage,
  walkthrough, or a long screenshot sequence, plus official/store/trailer
  visuals when available;
- the observation ledger has at least 5 visible beats or a recorded evidence
  gap approved by the user or scoped as a narrow memory-only exception;
- first screen, first input, visible response, reward feedback location, and
  primary/secondary UI hierarchy are written before system conclusions;
- borrow, avoid, and copy-risk are explicit;
- the current native build capture is named and compared against the reference;
- the next implementation pass is scoped to one change and tied to an exact
  native screenshot, DevAPI scenario, simulator, or capture proof.

If any item is missing, the ready state is closed. The agent must say "reference
study is not ready for implementation", name the missing evidence, and continue
with source gathering/deconstruction or ask for user material. Do not bypass
this by calling the change a small polish pass if it affects the main loop,
first screen, primary controls, economy, balance, art direction, or
release-critical readability.

## Reference Digest Rule

The study is not only for the agent. Before coding or generating final art from
a named reference, write a short Reference Digest in chat, the task log, or the
deconstruction doc. This makes the lead able to challenge the research before
time is spent on implementation.

Required digest:

- mode: quick check, central deconstruction, or deep deconstruction;
- sources actually checked: links or local paths plus checked date;
- 3-5 observed facts: visible screen/action/response facts, not genre labels;
- current-build mismatch: one concrete difference from the current native
  capture;
- translation: borrow, avoid, and copy-risk;
- proof: exact native screenshot, DevAPI scenario, simulator, or capture for
  the next pass.

If the digest cannot be written from the durable artifact, the reference study
is not ready for implementation. Name the missing evidence and continue source
gathering, deconstruction, or ask the user for material.

## Parallel Reference Work Rule

Parallelism is allowed for research throughput, not for bypassing the gate.

Allowed in parallel:

- source search and source packet collection;
- frame capture, screenshot gathering, and timestamp notes;
- visible-action ledger transcription;
- current native screenshot capture for mismatch comparison;
- unrelated setup or tooling that does not decide gameplay, UI, economy,
  balance, or final art from the reference.

Not allowed in parallel:

- implementing the loop, first screen, controls, economy, balance, UI hierarchy,
  or final art that the reference is supposed to define;
- generating final reference-driven art before the Reference Digest exists;
- claiming implementation is "based on the ref" when the durable doc was
  finished after the code/art;
- fitting the deconstruction around an already-made implementation.

If research and implementation were accidentally started together, stop the
implementation lane. Finish the deconstruction, write the digest, compare the
existing build honestly as a current-build mismatch, then choose the next
scoped pass and proof.

## Deconstruction Template

Write a short durable doc before implementation:

```markdown
# [Reference] Deconstruction

## Sources Checked
- [source quality] [link/date] - what it proves
- current build capture: [path/date]

## Source Ladder
- user-provided material:
- official/store/trailer visuals:
- raw gameplay evidence:
- supporting interpretation:
- source gaps / exceptions:

## Reference Lock
- mode:
- reference question:
- durable doc path:
- required source packet:
- current native capture path or capture plan:
- no-coding/no-final-art boundary:
- expected proof screenshot/scenario:
- unlock condition:

## Definition Of Ready Checklist
- [ ] mode matches implementation risk
- [ ] source matrix is filled
- [ ] evidence board has 6 cited player-facing frames/screenshots or an approved gap
- [ ] gameplay footage/walkthrough or long screenshot sequence exists, or gap is approved
- [ ] current native capture exists or capture plan is explicit
- [ ] observation ledger has 5 visible beats before conclusions
- [ ] borrow / avoid / copy-risk are explicit
- [ ] current-build mismatch is written
- [ ] next native proof is named

## Pass 1: Source Packet
- source quorum:
- missing/uncertain evidence:

## Reference Evidence Board
| Item | Source/timestamp/frame | What it proves | What it cannot prove |
|---|---|---|---|
| first screen |  |  |  |
| first input |  |  |  |
| visible response |  |  |  |
| reward feedback |  |  |  |
| upgrade/progression UI |  |  |  |
| friction/blocked state |  |  |  |

## Pass 2: Player Transcript
- first screen:
- first input:
- visible response:

## First 10 Seconds
1. ...

## First 60 Seconds
1. ...

## 1-5 Minute Loop
- repeated actions:
- pacing/blockers:
- upgrade/reward cadence:
- session/offline rhythm:

## Core Loop
- generate:
- interact:
- resolve:
- reward:
- progress:

## Pass 3: Systems Extraction

## Screen Grammar
- camera/framing:
- primary play space:
- spawn/object area:
- where player touches/clicks:
- where rewards appear:
- primary UI:
- secondary UI:

## Mechanics / Balance Notes
- object counts:
- timers:
- currencies:
- sources:
- sinks:
- unlocks:
- fail/recovery:

## Visual Composition
- dominant shapes:
- scale hierarchy:
- HUD density:
- feedback effects:

## Pass 4: Translation Gate

## Borrow / Avoid / Copy-Risk
- borrow:
- avoid:
- copy-risk:

## Mismatch Audit Against Current Build
- current screenshot:
- mismatch:
- required change:

## Implementation Gate
- next code/art pass:
- evidence screenshot/scenario:

## Reference Digest
- mode:
- sources checked:
- observed facts:
- current-build mismatch:
- borrow:
- avoid:
- copy-risk:
- next native proof:
```

## Hard Rules

- A feature list is not enough. Capture screen grammar: where objects live,
  what the player manipulates, where rewards appear, and what the UI supports.
- "I studied the ref" is only valid when the deconstruction contains source
  links/paths, checked dates, source quality, observed timestamps/frames, and
  current-build mismatch. Otherwise state the gap before coding.
- Do not implement from memory when a named reference is central to the task.
- Do not treat store copy as mechanics proof. Use screenshots, videos, or
  guides for interaction claims.
- Do not proceed from "I watched/read enough" unless the deconstruction names
  concrete first actions, screen layout, reward location, and current-build
  mismatches.
- Do not proceed from memory, thumbnails, snippets, or a video title. For
  central/deep refs, cite the Reference Evidence Board frames/timestamps first.
- Do not rely on search snippets, thumbnails, or memory as proof that a
  reference was studied.
- Do not let secondary summaries replace raw gameplay evidence for the main
  loop, first screen, control, reward, or UI hierarchy.
- Do not start changing gameplay, UI, economy, balance, or art for a named
  reference until the deconstruction names the next proof screenshot/scenario.
- Do not run reference-driven implementation in parallel with the reference
  study. Parallel lanes may gather evidence, but implementation/final art waits
  for the digest, mismatch audit, and proof target.
- Do not skip the Reference Lock. If implementation starts without a named
  mode, doc path, source packet, native mismatch plan, and proof target, stop
  and create the lock before continuing.
- Always include borrow/avoid/copy-risk so the result is inspired, not copied.
- Always include a mismatch audit against the current build before continuing
  after negative user feedback.
- If the reference is visual, the accepted visual target must include actual
  screenshots or frames, not only verbal style notes.

## Validation

Before implementation starts, the agent should be able to answer:

1. What is the main play space?
2. What does the player touch/click in the first 10 seconds?
3. What changes on screen after the action?
4. Where does reward feedback appear?
5. What UI is primary, and what UI is secondary?
6. What must we avoid copying?
7. How does the current build differ from the reference?

If any answer is vague, research is not implementation-ready.
