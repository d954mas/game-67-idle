# Canvas Chat

Canvas owns the chat semantics and app-server adapter. Studio Shell only mounts
the HTTP surface and forwards its process-lifecycle shutdown hook. Chat never
receives a privileged path into `project.json`.

## Boundary

Mutations happen only through an agent driving
`ai_studio/assets/canvas/cli.mjs`. The prompt requires the agent to inspect the
CLI's current help, use the selected private store when applicable, and read a
fresh history head before navigation. `context.mjs` imports Canvas operations
only for read-only context and sequence-range calculations.

## Session and transport

One chat conversation is one Codex app-server thread. The Canvas chat adapter
owns a persistent local `codex app-server --listen stdio://` process and speaks
its newline-delimited JSON protocol. It authenticates through the installed
Codex client's managed ChatGPT subscription; API-key accounts are rejected.
`CODEX_BIN` may select an executable, while `CODEX_APP_SERVER_JS` may select a
CLI JavaScript entry point. An incompatible CLI/model pair fails loudly; the
adapter never auto-upgrades Codex or falls back to an API key.

- The first message performs `initialize`, `account/read`, `thread/start`, and
  `turn/start`. The turn input contains the full driving contract, bounded
  context digest, and user message.
- Later messages perform `thread/resume {threadId}` and `turn/start` with the
  compact current-head/selection message.
- `chat/state.json` persists exactly `thread.id` as `sessionId`; it never uses
  a similarly named thread field. A stale or mismatched thread fails loudly
  instead of silently starting a new conversation.
- Text streams from `item/agentMessage/delta` as bounded delta batches;
  `turn/completed` closes a turn.
  Cancel sends `turn/interrupt` before terminating an unresponsive process.
- Different Canvas projects may own concurrent turns on the same process;
  events, approvals, and cancel handles remain keyed to exact thread/turn ids.
- EOF, startup failure, malformed JSONL, RPC/turn timeouts, and crashes reject pending
  work. A later message lazily starts a clean app-server process. Studio Shell
  shutdown terminates the full process tree, so restarts leave no orphan.
- Clear archives the display transcript, resets `sessionId` to null, and is
  rejected while a turn is active.

`chat/transcript.jsonl` is an append-only panel display log, not model memory.
Assistant entries record the Canvas journal `[head_before, head_after]` when a
turn commits operations.

## Permission gate

Every mutating HTTP request requires the per-launch token, exact loopback
Host/Origin, and JSON content type. The production app-server transport is
approval-aware. For `item/commandExecution/requestApproval` and
`item/fileChange/requestApproval`, it passes the exact opaque `{method, params}`
request through `permission_broker.mjs` and does not answer the app-server's
request id until the decision settles. Allowed maps to `accept`, denied to
`decline`, and cancelled or expired to `cancel`; `acceptForSession` is never
used. Unknown server requests fail closed.

The prompt's denied-operation rule and `checkDeniedVerbs` are defense in depth.
The permission broker is the hard mutation gate.

## File map

- `context.mjs`: bounded read-only context, append-only transcript, persisted
  thread id, archive-and-clear lifecycle.
- `agent.mjs`: prompt builders, reply tripwire, and injectable turn seam. Its
  production default is the app-server adapter.
- `app_server.mjs`: subscription authentication, JSONL request routing,
  thread/turn continuity, streamed replies, approvals, interrupt, and process
  lifecycle.
- `permission_broker.mjs`: store/project/turn-bound one-shot decisions,
  cancellation, expiry, and stale refusal.
- `api.mjs`: authenticated HTTP/SSE routes mounted by Studio Shell. Tests may
  inject a fake approval-aware transport without starting Codex.

The API emits `progress`, optional permission events, optional `op-committed`,
and exactly one `final` or `error` event. Transcript reads return an empty array
for a fresh project. The browser sends private store scope in a header and
reloads Canvas after committed operations; it never mutates the project itself.
