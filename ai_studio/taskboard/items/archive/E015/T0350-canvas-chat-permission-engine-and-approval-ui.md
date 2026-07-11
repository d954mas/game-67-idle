---
id: T0350
title: Canvas Chat permission engine and approval UI
status: done
project: P001
epic: E015
priority: P0
tags: [canvas, chat, security, permissions]
created: 2026-07-10
updated: 2026-07-11
---

## What

Replace Canvas Chat's prompt-only permission claim with an enforceable browser
approval boundary. The user must be able to grant a requested capability in the
chat window before the operation executes.

## Done when

- [x] Canvas-owned permission states and transitions are explicit, fail closed,
      and covered by tests for allow, deny, cancel, expiry, and stale requests.
- [x] The chat UI displays the exact requested capability/scope and can approve
      or deny it without leaving the conversation.
- [x] Mutations and permission responses require a per-launch random token plus
      validated Origin, Host, and content type.
- [x] Permission prompts preserve the exact app-server capability/tool request;
      this task does not invent a narrower typed-tool-only policy.
- [x] Existing journaled Canvas operations retain behavior and attribution.

## Open questions

## Log

- 2026-07-10: Supersedes the permission paragraph in `T0242`; transport changes
  are intentionally deferred to `T0351`.
- 2026-07-10: Final transcript audit removed unapproved blanket bans on raw shell
  and destructive requests. Safety comes from the real permission boundary and
  exact scope shown to the user.
- 2026-07-10: Checkpoint: current production chat still launches codex exec --dangerously-bypass-approvals-and-sandbox; its permission rule is prompt text plus a post-response denied-verb tripwire, and the browser has no approval state or endpoint. Starting only the Canvas-owned permission/security/UI boundary; T0351 retains app-server transport migration ownership.
- 2026-07-11: Feasibility review confirmed the legacy `codex exec` transport cannot pause for browser approval. T0350 therefore installs the complete Canvas-owned boundary and fails the legacy production transport closed before spawn; T0351 owns restoring production execution through app-server.
- 2026-07-11: TDD RED exercised 21 permission/security/UI cases with 5 expected failures. GREEN added explicit allow, deny, cancel, expiry, stale and ABA-safe transitions; exact opaque request preservation; per-launch token plus Origin/Host/content-type enforcement; and approval UI bound to the originating store, project, and turn.
- 2026-07-11: Verification: chat tests 74/74 pass; Canvas suite 732 pass, 2 environment skips, 0 fail; syntax checks, Taskboard validation, and scoped `git diff --check` pass.
- 2026-07-11: Runtime browser evidence at 1440x900 and 390x844: bootstrap returned 200 with a 43-character launch token, unprotected mutation returned 403, hostile request text created 0 image nodes, Approve/Deny remained visible, Approve emitted `allow`, and body width stayed within each viewport.
- 2026-07-11: Independent review converged in three cycles. Cycle 1 fixed the allow/cancel race, mutable request snapshot, terminal retention, and project/store/turn binding. Cycles 2-3 reported 0 HIGH and 0 actionable MEDIUM/LOW across architecture, correctness, ownership, tests, process, performance, and context cost.
- 2026-07-11: Quality: QTECH_001=pass; QCLR_001=pass; QCLR_002=pass; evidence: focused permission/security tests plus live desktop and tall-phone approval-state inspection with measured bounds.
- 2026-07-11: Closed after permission boundary, security, UI, regression, browser, and independent review evidence passed.
