---
name: nt-app-tunnel
description: "Use when the lead wants to view, test, pick, or play a local AI Studio surface, asset viewer export, static page, or wasm game build from a phone or another device by exposing a self-contained directory through a temporary public cloudflared quick tunnel. Public and unauthenticated while running; never serve secrets or private sources."
---

# NT App Tunnel

Serve a local directory and expose it on a public URL so the lead can open it on
a phone or another device.

## Run

One background command prints the URL, then holds the tunnel:

```powershell
node ai_studio/studio_shell/tunnel/serve_tunnel.mjs --dir <dir-to-serve> [--port <n>]
```

It auto-fetches `cloudflared` to
`tmp/ai_studio/studio_shell/tunnel/bin/` on first use, starts a static server
for `<dir>`, and prints:

```text
TUNNEL_URL https://<x>.trycloudflare.com
```

Read that line from the task output and give the URL to the lead. Stop the
background task to tear everything down.

Examples:

```powershell
node ai_studio/studio_shell/tunnel/serve_tunnel.mjs --dir tmp/asset-review-ll
node ai_studio/studio_shell/tunnel/serve_tunnel.mjs --dir <wasm-build-dir>
```

## What To Serve

- Asset Viewer static export or picker output.
- A self-contained wasm game build directory.
- Any self-contained static page.

## Rules

- The URL is public and unauthenticated while the command runs.
- Do not serve secrets, source trees, private assets, or account-gated files.
- Serve self-contained content with relative URLs. `file://` references will not
  load on another device.
- Tear down the background task when the lead is done viewing.
