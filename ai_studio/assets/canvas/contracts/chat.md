# Canvas Chat contract

Canvas owns Chat runtime, context, permissions, app-server transport, docs, and
tests under `../chat/`. Studio Shell imports the Canvas API and supplies only
the loopback host list, HTTP lifecycle, and shutdown call.

Chat context contains the current Canvas project/selection contract and drives
the same CLI/operation layer as other agents. Permission decisions are enforced
by the broker and transport; prompt text is not an authorization boundary.
Per-launch credentials and conversations remain local and are never exposed in
tracked docs or browser payloads.
