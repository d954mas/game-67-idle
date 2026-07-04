# T0242 — AI chat panel on the canvas page (design)

Design-only doc. Changes no production files. Grounds every claim in the code read
2026-07-03. The deliverable is this doc + the increment plan.

## The idea, in one line

Embed the loop that already exists (the lead pastes a `canvas://` ref into a terminal
agent, the agent drives `cli.mjs`) *into the page*: select node(s), type a request in
Russian, an agent performs it through the **same ops layer**, and the reply names the
journal entries it created.

The whole feature is a thin transport around parts that are already built:
- The agent surface exists — `ai_studio/assets/canvas/cli.mjs` is the full verb surface,
  self-documenting when run bare (`cli.mjs:163-216`), attributed as `agent` for the
  🤖 history marker (`cli.mjs:739-745`).
- The selection exists — `state.selectedIds` / `state.selectedGroupId` /
  `state.selectedGroupIds` on the page (`site/app.js:15-71`).
- The ref format exists — Copy ID emits `canvas://<project>/element/<id> — …`
  (`site/context_menu.js:88-155`; documented `README.md:47-62`).
- The live-project guard exists — T0234 `--expect-head` on undo/redo/history-jump
  (`cli.mjs:664-704`, `README.md:942-966`).
- Out-of-band refresh exists — `reloadProject()` is the "a change I didn't make just
  landed" resync path (`site/app.js:449-473`).
- A headless-agent spawn precedent exists — `codex exec …` in
  `.codex/skills/nt-asset-image-generation/scripts/codex_imagegen.sh:45`, and the
  injectable-generator seam in `tools/dual_plate_generate.mjs:98-119`.

So T0242 adds ~4 small modules and one page panel. No new agent loop, no parallel
op implementation.

---

## 1. Backend: what process answers the chat

**Decision: (a) spawn a headless agent CLI per message, `codex exec` as the default
backend, behind an injectable transport seam.** Reject (b) long-lived process and (c)
external tool-calling API.

Why (a):
- **Zero new agent-loop code.** codex exec already *is* an agent loop that runs shell
  commands. Priming it to run `node ai_studio/assets/canvas/cli.mjs …` reuses the exact
  transport a terminal agent uses today — the one-ops-layer law holds by construction
  (`README.md:4-24`; the CLI never bypasses `ops.mjs`).
- **Matches the proven precedent on this box.** `codex exec
  --dangerously-bypass-approvals-and-sandbox --skip-git-repo-check -C <repo> "<prompt>"`
  is the working headless spawn (`codex_imagegen.sh:45`); the T0238 dual-plate flow
  already spawns codex-backed work behind an injectable seam
  (`dual_plate_generate.mjs:107-119`). This design is the same shape, one level up.
- **Sidesteps the two environment traps.** codex uses ChatGPT OAuth (no metered API
  key) and its own transport, which already works past the Avast TLS MITM that breaks
  raw node/python TLS (memory: *Avast TLS MITM*; the imagegen path is unblocked). A
  node-side streaming API client (option c) would hit exactly that wall *and* need an
  API key *and* re-implement tool dispatch — three new problems.

Why not (b) long-lived process: a resident per-project agent means building a
stdio/socket protocol, lifecycle, and crash recovery around an interactive `codex`/
`claude` session — a second harness, and a second writer holding project state against
the single-writer append model (`README.md:855-862`). High new-code, brittle, against
the repo's "subtract not add" stance.

Why not (c) external tool-calling API: needs an API key the box does not use, trips
Avast TLS, and re-implements the agent loop in node (the biggest parity risk — a second
place that "knows how to drive the canvas").

**Cost of (a), accepted honestly:**
- *Cold start ~seconds/message* (codex boot + first token). Acceptable: the UX is "type
  a command, watch the AI work", the same feel as the terminal loop. Not a chat that
  needs sub-second turns.
- *No in-process memory across messages.* v1 is single-turn (see §5), so this is a
  non-issue; v2 replays a compact transcript in the prompt (we build the prompt) or uses
  session resume — an additive change, not a redesign.
- *Cost:* one codex agent run per message — free on the ChatGPT plan but rate-limited;
  surface rate-limit errors into the chat as loud errors.

**The seam (this is the real design commitment, not the CLI choice):** `agent.mjs`
exposes `runChatTurn({ context, message, transport })` where `transport({ prompt, cwd,
onEvent }) -> {exitCode}` is injectable, defaulting to the codex-exec spawn. Tests inject
a fake transport that emits canned events and runs canned CLI ops — **codex never runs in
the suite**, exactly like the T0238 generator fake (`dual_plate_generate.mjs:98-106`).
Swapping codex → `claude -p` is then a one-module change, which is why "codex vs claude"
is a *lead preference*, flagged in Open Questions, and not something the architecture
depends on.

---

## 2. Context passing: what rides with the message

**Page → backend (small, bounded):** `{ message, projectId, selection: [refs] }` where
each ref is the exact Copy ID string the page already builds from `state.selectedIds` +
`state.selectedGroupIds` (+ the region ref when in region-edit), e.g.
`canvas://<pid>/element/<eid> — project "…", element "…"`
(`site/context_menu.js:97-153`). Reuse that formatter; do not invent a second shape.

**Backend → agent prompt: a lean context DIGEST, never the full project JSON.** The
project JSON can be large; stuffing it in the prompt is wasteful and the agent can pull
exactly what it needs. `context.mjs` builds:

```
{ projectId, title,
  selection: [ { ref, id, kind: "element"|"group"|"region", type: "image"|"text",
                 name, w, h, groupId } ],   // resolved from ops.getProject, read-only
  counts: { elements, groups },
  head }                                     // current history_seq, the expect-head seed
```

This is bounded by *selection size*, not project size. The digest is built by importing
`ops.getProject` / `listHistory` directly (read-only, no journal, no actor) — the same
data the page's API GET returns. **Full state stays on demand:** the prompt tells the
agent it can run `node cli.mjs show <projectId>` for the complete picture (the skill
already teaches this — `nt-canvas-operations/SKILL.md:28-33`). So: *selection summary +
counts + head in the prompt; full project one CLI call away.* That is the
"element-summary vs full JSON" answer.

**The prompt** `agent.mjs` builds (pure, unit-tested) contains, in order:
1. The task framing: "You drive a live canvas project through its CLI. Do what the user
   asks to the selected objects."
2. The digest (above) + the selection refs verbatim.
3. The driving contract, inline (robust even if codex does not auto-load the skill):
   the CLI path, "run it bare to see every verb", the resolve-ref workflow, and the
   **T0234 rule** — before any undo/redo/history-jump, run `history-list`, read `head`,
   pass `--expect-head <head>` (`cli.mjs:664-704`, `SKILL.md:51-67`). Point at
   `.codex/skills/nt-canvas-operations/SKILL.md` for detail.
4. The user's message text (Russian, verbatim).

NL understanding lives in the model; capability discovery lives in the self-documenting
CLI (`cli.mjs:163-216`). We do **not** hardcode NL→op mappings ("чёрную версию" →
which op) — that is the agent's job and the whole point.

---

## 3. Safety / concurrency (the sharp part)

The page is LIVE while the agent works; T0234 exists because of exactly this
(`README.md:942-955`). Honest collision story:

- **History ops (undo/redo/jump) are fully guarded — for free.** The CLI *refuses*
  without `--expect-head`, and refuses loudly on a stale head, writing nothing
  (`cli.mjs:664-704`). The chat agent inherits this because it drives the CLI. The prompt
  reinforces "re-read head right before navigating." This is the incident T0234 fixed;
  the chat does not reopen it.
- **Forward ops (add/patch/alpha/…) have no head-guard today, and v1 does not add one.**
  Each CLI op reads `project.json` fresh at execution and commits an atomic O_APPEND
  journal line (`README.md:855-862`), so cross-writer exposure is only the
  read-modify-write window of *one* op — ms for metadata ops, seconds for a python op
  (alpha). If the user and the agent touch the *same* element inside that window, one
  writer's change can be clobbered in the live view — but it is preserved in the journal
  and restorable by undo. This is the same risk the terminal-agent loop already carries
  today; it is acceptable for a local single-user tool and is *not* worth optimistic
  locking on every op in v1. State it in the panel: "the project is live — your edits and
  the AI's both land; undo if they collide."
- **Prefer additive/undoable ops.** The prompt biases the agent toward
  new-element/undoable ops (e.g. `alpha-dual-generate` mints a *new* element beside the
  source, source untouched — `README.md:663-675`) and away from `history-jump` unless the
  user explicitly asks to time-travel.
- **Long ops (minutes).** `alpha-dual-generate` spawns codex image generation
  (`README.md:614-682`) — the chat turn can run for minutes. The subprocess streams
  stdout; the backend surfaces coarse progress into the chat ("running
  alpha-dual-generate…"), mirroring `runLongOp`'s progress-toast contract
  (`README.md:1244-1258`). The user keeps editing meanwhile.
- **Cancel is safe.** Killing the subprocess (SIGTERM) mid-turn may leave N of M ops
  applied — but every op is atomic + journaled + undoable, so the panel reports
  "cancelled — N ops already applied, undo if needed." No half-written project state.
- **Refresh after out-of-band changes:** the page must NOT use `applyMutation` (that is
  for op responses the page itself made). It uses the existing `reloadProject()` (full
  GET + `/history`) — which is precisely the "a change I didn't originate just landed"
  path (`site/app.js:449-473`). Trigger it on each streamed op-committed event and on
  turn-end. A dead-simple fallback: poll `GET …/history-list` every ~2s while a turn is
  active and `reloadProject()` when `head` advances (head is monotonic). No new refresh
  primitive is needed.

---

## 4. UI

**Placement — a collapsible right-edge panel, toggled from the top bar, mirroring
`history_panel.js` exactly** (`README.md:1177-1187`): hidden by default, open-state in
`localStorage` (view-state, never journaled), a top-bar "AI"/"Chat" button + a keyboard
toggle. Rejected: a bottom drawer (steals canvas height and collides with the
bottom-right toast stack, `README.md:1217-1235`). The inspector keeps the right side; the
chat panel slides in as a sibling column / overlay on the right, so both can be open.

**Message stream:**
- User message bubbles + agent reply bubbles.
- Each agent reply carries an **auditability block**: the journal seq range the turn
  created (`head_before → head_after`), listing each op as a row (op name + target name),
  computed from the head delta — this satisfies "each reply lists the journal entries it
  created." Rows can deep-link the History panel (`jumpHistory` already backs it).
- **Errors land in the chat as red messages**, never swallowed — the same contract as the
  error toast (`README.md:1228-1230`). A loud CLI/tool error (e.g. the dual-plate pair
  gate refusal, `README.md:594-599`) travels verbatim into the chat.

**Streaming:** v1 = coarse progress + final summary (the long-op case *requires* at least
progress). Full token-by-token streaming of the agent's reasoning is v2.

**Cancel/kill:** a Cancel button on the in-flight turn SIGTERMs the subprocess; the panel
disables Send while a turn runs (or queues — but v1 can simply reject a second Send with
"a turn is already running").

**Transport:** `POST /api/chat/projects/<id>/message` returns an SSE stream (progress
events, op-committed events, final summary, error); `POST …/cancel` kills the turn. SSE
reuses the subprocess stdout the backend already holds — no polling machinery required
(the poll is only the fallback).

---

## 5. v1 scope + the cut line

**v1 — single-turn command on the current selection:**
- One message → the agent plans → executes existing ops via the CLI → replies with a NL
  summary + the journal seq range.
- Selection is auto-attached (element/group/region refs). Nothing selected → the agent
  acts on the project (or asks).
- IN: acting on the selection with **existing** ops — alpha / alpha-dual /
  alpha-dual-generate, align/distribute/center ("выровняй по центру экрана" →
  `nodes-align`), rename, hide, duplicate, group, add-text. The lead's examples
  ("сделай чёрную версию", "дуал путь") map to the alpha family — the agent decides
  which, from the self-documenting CLI.
- One subprocess per message; reply names ops + seq range; errors surface loudly.

**Cut OUT of v1 (v2+):**
- **Multi-turn memory** — v1 is stateless single-turn (keeps the prompt tiny and the
  collision window short). v2 replays a compact transcript / uses session resume.
- **Proactive suggestions** — the agent only responds to an explicit message.
- **Generation-orchestration conversations** — "generate me 3 buttons and lay them out"
  pulls in the whole asset-generation pipeline (minutes-long codex image calls, the
  generate-on-magenta/green-bg law — memory: *Canvas 2D conveyor*). v1 allows
  generation *only* through an existing single op the agent invokes (e.g.
  `alpha-dual-generate`), not open-ended art creation.
- **Full token streaming** of agent reasoning.

The cut line in one sentence: **v1 acts on what is selected using ops that already exist,
one message at a time; anything that needs conversation state or net-new art generation
is v2.**

---

## 6. Increments (fast-worker sized), file ownership, tests

New module dir `ai_studio/studio_shell/chat/` — chat is a **shell** concern that *consumes*
the canvas ops (like the page does), so it lives beside the other shell-mounted APIs
(`server.mjs:11-28`), keeping the canvas module clean. Read-only digest imports
`ops.getProject`; all *mutations* go through the spawned CLI (actor=agent). Parity intact.

**Inc 1 — context digest builder** · `chat/context.mjs` (+ test)
`buildChatContext({ projectId, selection }) -> { projectId, title, selection[], counts,
head }` over `ops.getProject` + `listHistory`, plus a server-side ref formatter matching
`context_menu.js:97-153`. Pure/read-only. `node:test` on a fixture project: assert digest
shape, ref strings, resolved names/types, `head`. *Owner: fast-worker.*

**Inc 2 — agent spawn seam** · `chat/agent.mjs` (+ test)
`runChatTurn({ context, message, transport })`: pure prompt builder (digest + refs +
inline driving contract + T0234 rule + user text) and the injectable `transport` seam
(default = codex-exec spawn `codex exec --dangerously-bypass-approvals-and-sandbox
--skip-git-repo-check -C <repoRoot>`, streaming stdout). Test injects a fake transport
(codex never runs): assert prompt construction and that ops flow through the CLI. Mirrors
`dual_plate_generate.mjs:88-119`. *Owner: fast-worker.*

**Inc 3 — chat HTTP/SSE adapter** · `chat/api.mjs` + mount in `server.mjs` (+ test)
`POST /api/chat/projects/<id>/message` opens SSE (progress / op-committed / final /
error); `POST …/cancel` kills the turn. Records `head` before, streams events, computes
the seq range from `head` after. Mount next to `/api/canvas/` (`server.mjs:149-152`); bind
stays 127.0.0.1 (local-only trust boundary for the unsandboxed agent). Test hits the
handler with the fake transport: assert SSE event sequence, seq-range, error surfacing.
*Owner: fast-worker.*

**Inc 4 — page chat panel** · `site/chat_panel.js` + top-bar/`canvas.js` wiring
Collapsible right panel following `history_panel.js` (toggle button, `localStorage` open
state, refresh-bus render). Auto-attach selection → refs from `state.selectedIds` /
`state.selectedGroupIds` (`app.js:15-71`); render the stream + audit block + errors;
`reloadProject()` on head-change and turn-end; Cancel button. Manual browser verify
(headless page tests are not the norm here). *Owner: fast-worker.*

**Inc 5 — docs + task log** · `chat/README.md` (owner/boundary/parity note: chat drives
the canvas CLI, never a parallel path) + T0242 log update. *Owner: fast-worker.*

**Headless-testable seam summary:** context digest (pure over ops), prompt builder
(pure), SSE adapter (fake transport). The codex subprocess is the one un-unit-tested edge
— same stance as `generatePlate` (`dual_plate_generate.mjs:98-106`).

---

## Open questions for the lead (load-bearing only)

1. **Which CLI backs v1 — `codex exec` (default here; proven headless + OAuth +
   Avast-safe) or `claude -p` (the lead's "попросить тебя" continuity, but a node CLI
   that needs the Avast TLS workaround)?** The seam makes it a one-module swap; pick the
   default to build against.
2. **May the chat CREATE new objects in v1, or only ACT on the selection?** Recommendation:
   allow additive ops that need no net-new art (add-text, duplicate, align, group, and the
   single-op `alpha-dual-generate`); defer open-ended "generate new art and place it" to
   v2. Confirm the line.
3. **Priority vs the T0238 / T0207 / T0239 queue** (already flagged on the task) — lead to
   place it.

---

## REVISION R1 (2026-07-03, lead) — history + clear context

Lead: "у чата нет истории? а что если я хочу поработать? я бы хотел историю с
возможностью почистить контекст". The single-turn v1 cut is REVISED:

- CHAT HISTORY is persistent PER PROJECT: a server-side transcript
  (chat/history jsonl next to the project store or under the shell's own
  state dir — implementer picks, path documented), reloaded into the panel on
  page open. The panel shows the full stream across page reloads.
- CONTEXT: each spawn receives the last N turns (bounded window, N~10-20
  turns or a byte budget — implementer measures against codex prompt limits;
  older turns drop off silently, the UI still shows them). The spawn-per-
  message backend stands (no long-lived process); history is REPLAYED into
  the prompt, not held in a resident agent. Each agent reply also records the
  journal seq range so a later turn can reference "what you did before"
  factually rather than from model memory.
- CLEAR CONTEXT button: starts a NEW conversation - archives the current
  transcript (rotate the jsonl, keep on disk, cheap) and empties the panel +
  the context window. Explicit, loud, undo-free (archives are kept).
- Increment impact: history store + window builder join increment 1
  (context.mjs); the Clear button joins increment 4 (panel). Increment count
  unchanged.

## REVISION R2 (2026-07-03, lead) — backend + permissions, EXPLICIT

Lead's exact decisions:
1. BACKEND = `codex exec`. Stated explicitly per the lead's ask ("кодекс ок,
   только напиши явно"): the chat panel spawns `codex exec` per message —
   ChatGPT OAuth creds from ~/.codex/auth.json, codex's own transport (safe
   against the Avast TLS MITM), invocation shape mirrors
   codex_imagegen.sh. `claude -p` is NOT the build target; the transport seam
   still exists (tests inject a fake), but no claude branch is built.
2. PERMISSIONS: the chat agent may perform ANY operation the lead asks for
   EXCEPT destructive ones. The line is UNDOABILITY: every journaled op
   (add/patch/move/align/slice/alpha/generate/undo/redo/history-jump — all
   recoverable via the journal) is allowed; ops that cannot be undone from
   inside the project are DENIED — the named one is PROJECT DELETION
   (`delete <id>`; .trash move happens outside the journal). Same class:
   anything touching files outside the project store. Enforcement, honestly
   stated: (a) the system prompt states the rule explicitly and instructs
   refusal; (b) the chat backend post-checks the transcript for the denied
   verbs and flags loudly in the panel if one ran (codex exec has shell
   access - a hard sandbox is not claimable in v1; same trust level as the
   lead's own terminal, single-user localhost).
Queue: after T0239 recipe-card increments (lead did not bump it).

## REVISION R3 (2026-07-03, lead challenge) — codex SESSIONS instead of history replay

Lead: "почему просто не поднять отдельного клиента для чата?" — the instinct
is right; R1's replay-the-window design is the clunky version. Verified on
the box: `codex exec resume <SESSION_ID>` (or --last) exists.

REVISED: one chat CONVERSATION = one CODEX SESSION.
- First message of a conversation: plain `codex exec` -> capture the session
  id from codex's own session store.
- Every following message: `codex exec resume <session_id> "<message>"` —
  still a short-lived process per message (cancel = kill THAT run only), but
  the conversational context lives in codex's session, no history replay, no
  token re-spend, true continuity.
- CLEAR CONTEXT = mint a new session id (old codex session + our transcript
  archive stay on disk).
- Our per-project transcript jsonl (R1) remains for PANEL DISPLAY ONLY — the
  model's memory is the codex session, the panel's memory is the jsonl; the
  journal seq ranges per reply still bridge the two factually.
- A truly RESIDENT client (daemon) stays rejected: resume gives the same
  continuity with none of the lifecycle pain (watchdog/restart, a
  permanently-open shell-capable agent parked next to the server, cancel
  killing the whole client instead of one message).
- Fallback if resume proves flaky in practice: R1's bounded replay window.
