---
name: nt-app-tunnel
description: "Expose a local preview through a temporary public URL when the user explicitly requests and authorizes public exposure."
---

# NT App Tunnel

Expose a selected static preview so the lead can open it on another device.

## Exposure Boundary

Use only for an explicit tunnel/public URL request. A request to run, show,
test, play, or open on PC authorizes a local launch only.

Before launch, establish that the lead authorizes a public, unauthenticated URL
for the selected content. Explain and ask if that scope is not already explicit;
reuse existing confirmation for the same content and exposure. Skill selection
alone is not confirmation.

## Serve And Verify

Choose a self-contained static export or wasm build with relative URLs.
Do not expose source trees, secrets, private assets, or account-gated files.
Check the selected directory's contents before serving it.

```powershell
node .codex/skills/nt-app-tunnel/scripts/serve_tunnel.mjs --dir <dir-to-serve> [--port <n>]
```

Run in the background. The command obtains `cloudflared` if needed, starts the
static server, and holds the tunnel. Read its `TUNNEL_URL https://...` output;
verify the intended preview and its resources load before sharing the URL.
`file://` references cannot load remotely.

Report the URL and how to stop the background task. Tear it down when the lead
is done viewing, or when an error leaves the requested preview unusable.
