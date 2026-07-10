---
id: T0350
title: Canvas Chat permission engine and approval UI
status: backlog
project: P001
epic: E015
priority: P0
tags: [canvas, chat, security, permissions]
created: 2026-07-10
updated: 2026-07-10
---

## What

Replace Canvas Chat's prompt-only permission claim with an enforceable browser
approval boundary. The user must be able to grant a requested capability in the
chat window before the operation executes.

## Done when

- [ ] Canvas-owned permission states and transitions are explicit, fail closed,
      and covered by tests for allow, deny, cancel, expiry, and stale requests.
- [ ] The chat UI displays the exact requested capability/scope and can approve
      or deny it without leaving the conversation.
- [ ] Mutations and permission responses require a per-launch random token plus
      validated Origin, Host, and content type.
- [ ] Permission prompts preserve the exact app-server capability/tool request;
      this task does not invent a narrower typed-tool-only policy.
- [ ] Existing journaled Canvas operations retain behavior and attribution.

## Open questions

## Log

- 2026-07-10: Supersedes the permission paragraph in `T0242`; transport changes
  are intentionally deferred to `T0351`.
- 2026-07-10: Final transcript audit removed unapproved blanket bans on raw shell
  and destructive requests. Safety comes from the real permission boundary and
  exact scope shown to the user.
