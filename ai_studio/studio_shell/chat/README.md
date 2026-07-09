# Chat (T0242)

The AI chat panel on the canvas page: type a request (Russian or English), an
agent performs it on the live project, the reply names the journal entries it
created. See `tmp/design_T0242_chat_panel_2026-07-03.md` (+ REVISIONS R1/R2/R3)
for the full design; this file is the module map + the two rules that must
survive every future change to it.

## Owner and boundary

This module is a **Studio Shell concern that consumes canvas ops** — like the
canvas page itself, it never gets a privileged shortcut into `project.json`.

- **Parity law: mutations happen ONLY through a spawned agent driving the
  canvas CLI (`ai_studio/assets/canvas/cli.mjs`), never a parallel path.**
  `chat/agent.mjs` prints the CLI's own bare-invocation help + the driving
  contract into the prompt and tells the agent to run
  `node ai_studio/assets/canvas/cli.mjs <verb> <projectId> [flags]` for
  everything; for private Canvas stores the context digest also requires
  `--store game:<id>` on every CLI command. This is the same command a
  terminal-driven agent runs today, so the "one ops layer" invariant holds by
  construction. This module's own code
  (`context.mjs`) imports `ops.getProject` / `ops.listHistory` **read-only**,
  for the context digest and the seq-range math — it never imports or calls
  any mutating op.
- If a future change makes chat call a canvas op directly (bypassing the CLI
  subprocess) for "efficiency," that breaks the parity law — don't. The whole
  point of routing through the CLI is that the page, a terminal agent, and the
  chat agent are provably driving the exact same surface.

## The session model

One chat **conversation** = one **codex session** (design R3, verified live
2026-07-03: `codex exec resume <SESSION_ID>` gives real continuity — see
`agent.mjs`'s own header comment for the exact invocation shapes and the
verification transcript).

- **First message of a conversation**: `codex exec --json --output-last-message
  <file> "<prompt>"` (no session yet) — the prompt carries the full driving
  contract + context digest + the user's message (`buildFirstTurnPrompt`).
  codex's `--json` stream's first line is `{"type":"thread.started",
  "thread_id":"<uuid>"}`; that `thread_id` **is** the session id.
- **Every later message**: `codex exec resume <sessionId> --json
  --output-last-message <file> "<message>"` — a short-lived process per
  message either way, so **Cancel kills that one message's subprocess only**;
  the session lives in codex's own on-disk session store and survives a
  cancel (`api.mjs` persists a captured session id even off the error path).
- **Clear context** (`POST .../clear`) mints a brand-new conversation: the
  current `chat/transcript.jsonl` is **archived by rename** (never deleted)
  and `chat/state.json`'s `sessionId` resets to `null`, so the next message
  spawns a fresh `codex exec` with no memory of the old session.
- `chat/transcript.jsonl` is **PANEL DISPLAY ONLY** — an append-only log under
  the selected Canvas project's own store directory, including private
  `games/<id>/.ai_studio/canvas/projects/<project>/chat/`, of
  `{role, text, seqRange?, at}` the page reads back on open/reload so the chat
  stream survives a page refresh. It is never replayed into the prompt and it
  is not the model's memory; the codex session is. The two are bridged
  factually by `seqRange`: each assistant turn records the journal
  `[head_before, head_after]` its own turn produced, so a later turn (or a
  human re-reading the panel) can point at exactly what ops happened, instead
  of trusting the model's recollection.

## Trust boundary

This API is mounted next to `/api/canvas/` on the **same 127.0.0.1-only**
Studio Shell server (`server.mjs`) — **not a weaker boundary, the same one**.
It must stay that way, because this surface is more dangerous than the rest of
Studio Shell: `agent.mjs`'s default transport spawns codex with
`--dangerously-bypass-approvals-and-sandbox -C <repoRoot>`, i.e. an
**unsandboxed shell-capable process**, on every message. That is the same
trust level as the lead's own terminal — acceptable single-user-localhost, not
acceptable if this port were ever reachable from anything else.

Permission line (design R2, stated in `agent.mjs`'s `buildDrivingContract`):
the agent may perform **any journaled, undoable** canvas operation the lead
asks for. It must refuse project deletion (`cli.mjs delete` — a `.trash` move
that happens *outside* the journal, not undoable) and refuse touching any file
outside the project's own store directory. Enforcement is honest, not a hard
sandbox: (a) the prompt states the rule and instructs refusal, (b)
`checkDeniedVerbs` post-checks the reply text for the denied verb and returns
loud `flags` the panel renders in red — a tripwire, not a guarantee.

## File map

- `context.mjs` — `buildChatContext()` (pure, read-only digest over
  `ops.getProject`/`ops.listHistory`: selection refs + counts + head, bounded
  by selection size, never full project JSON; private refs use
  `canvas://game/<gameId>/<projectId>/...` without project/object name tails)
  + the per-project `chat/` store
  (`transcript.jsonl` append/read, `state.json` session id read/write,
  `clearConversation()`'s archive-and-reset).
- `agent.mjs` — the codex spawn seam. Pure prompt/argv builders
  (`buildDrivingContract`, `buildContextDigestText`, `buildFirstTurnPrompt`,
  `buildResumeMessage`, `buildFirstTurnCommand`, `buildResumeCommand`,
  `extractSessionId`, `checkDeniedVerbs`) plus the injectable `transport` seam
  `runChatTurn({ context, message, sessionId, transport, onChild })` defaults
  to `runCodexTransport` — the one un-unit-tested edge (codex never runs in
  the suite; tests inject a fake transport, mirroring
  `tools/dual_plate_generate.mjs`'s `generatePlate`).
- `api.mjs` — `createChatApi(root, { transport })`, the HTTP/SSE adapter
  mounted on `/api/chat/` by `server.mjs`. It accepts
  `x-ai-studio-store: game:<id>` for private Canvas stores; `?store=`/`?game=`
  remain manual legacy fallbacks, while the browser keeps store out of visible
  paths:
  - `POST /api/chat/projects/<id>/message {text, selection?}` → SSE
    `progress` → optional `op-committed {seqRange}` → `final {text,
    sessionId, seqRange, flags}` | `error {message}`. Exactly one of
    `final`/`error` ends every stream; the HTTP response itself is always 200
    (errors travel *inside* the stream, per the SSE contract comment at the
    top of the file).
  - `POST /api/chat/projects/<id>/cancel` — SIGTERMs the tracked child of the
    project's current in-flight turn, if any.
  - `GET /api/chat/projects/<id>/transcript` — the parsed `transcript.jsonl`;
    always 200 (a project with no chat yet is `{transcript: []}`, not a 404).
  - `POST /api/chat/projects/<id>/clear` — `clearConversation()`.
- `../../assets/canvas/site/chat_panel.js` — the page panel (owned by the
  canvas module's `site/`, not this dir, but it is this API's only consumer):
  a collapsible right-side column mirroring the history palette's
  toggle/localStorage/hidden-by-default state, streaming SSE via `fetch` +
  `ReadableStream` (no `EventSource`, since that is GET-only), auto-attaching
  the current canvas selection as `selection` refs, sending private store scope
  through `x-ai-studio-store` headers, and calling the page's
  existing `reloadProject()` on every `op-committed`/`final` with a
  `seqRange` — it never applies a mutation itself, only re-reads.
- `tests/` — `node:test` suites for `context.mjs`, `agent.mjs`, and
  `api.mjs` (fake transport throughout — codex never spawns in CI/local test
  runs).
