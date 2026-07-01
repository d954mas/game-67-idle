# App Tunnel

Temporary public tunnel for self-contained AI Studio surfaces and local game
builds.

Use it only when the lead needs to open a local static directory from another
device:

```powershell
node ai_studio/studio_shell/tunnel/serve_tunnel.mjs --dir <dir-to-serve> [--port <n>]
```

The command starts a local static server, runs a cloudflared quick tunnel, and
prints `TUNNEL_URL <url>`.

The URL is public and unauthenticated while the command runs. Do not serve
secrets, source trees, private assets, or account-gated files.

The downloaded cloudflared binary is cached under
`tmp/ai_studio/studio_shell/tunnel/bin/`, which is ignored by git.
