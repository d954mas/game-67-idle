---
name: nt-app-tunnel
description: "Manually selected workflow for exposing a specific local preview, build, or static directory through a temporary public URL. Public exposure is never automatic: explain that the URL is public and unauthenticated, then ask for confirmation immediately before launch."
---

# NT App Tunnel

Serve a local directory and expose it on a public URL so the lead can open it on
a phone or another device.

## Invocation Boundary

Invoke this skill only when the lead explicitly requests a tunnel or public URL.
Requests such as "run", "launch", "open", "show", "test", "play", or "open on
PC" authorize only a local launch. They do not authorize a tunnel, upload, or
public exposure.

Before running the tunnel command, explain that the URL will be public and
unauthenticated and ask the lead for explicit confirmation. Do not treat skill
selection or an earlier general request as launch confirmation.

## Run

One background command prints the URL, then holds the tunnel:

```powershell
node .codex/skills/nt-app-tunnel/scripts/serve_tunnel.mjs --dir <dir-to-serve> [--port <n>]
```

It auto-fetches `cloudflared` to
`tmp/nt-app-tunnel/bin/` on first use, starts a static server
for `<dir>`, and prints:

```text
TUNNEL_URL https://<x>.trycloudflare.com
```

Read that line from the task output and give the URL to the lead. Stop the
background task to tear everything down.

Examples:

```powershell
node .codex/skills/nt-app-tunnel/scripts/serve_tunnel.mjs --dir tmp/asset-review-ll
node .codex/skills/nt-app-tunnel/scripts/serve_tunnel.mjs --dir <wasm-build-dir>
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
