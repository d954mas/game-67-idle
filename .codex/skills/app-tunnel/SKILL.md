---
name: app-tunnel
description: "Use when the lead wants to view, test, pick, or play something from a phone or another device: share a local page/app/asset-viewer/game build over a public URL (ngrok-style). Serves a directory and exposes it via a cloudflared quick tunnel. Public + unauthenticated while running — no secrets."
---

# App Tunnel

Serve a local directory and expose it on a public URL so the lead can open it on
a phone (view a gallery, pick assets, play a wasm game build, test a screen).

## Run

One background command (it prints the URL, then holds the tunnel):

    node tools/serve_tunnel.mjs --dir <dir-to-serve> [--port <n>]

It auto-fetches `cloudflared` to `tools/bin/` on first use (no account needed),
starts a static server for `<dir>`, and prints `TUNNEL_URL https://<x>.trycloudflare.com`.
Read that line from the task output and give the URL to the lead. Stop the
background task to tear everything down.

Examples:

    node tools/serve_tunnel.mjs --dir tmp/asset-review-ll     # asset viewer / pick
    node tools/serve_tunnel.mjs --dir build/game_seed/wasm-debug   # play the wasm game

## What to serve

- Asset viewer: `ai_studio/assets/asset_viewer/build_review.mjs` output dir (pick from phone).
- A wasm game build dir (self-contained index.html + .wasm/.js/.data) → play remotely.
- Any self-contained static page.

## Rules

- The URL is PUBLIC and UNAUTHENTICATED while the task runs. Do not serve secrets,
  source, or private data. Quick tunnels are ephemeral (a new URL each run).
- Serve SELF-CONTAINED content: relative URLs only. `file://` references (e.g. the
  asset viewer's local previews) will not load on a remote device — copy media into
  the served dir and reference it relatively, or use CDN assets (e.g. model-viewer).
- Tear down the background task when the lead is done viewing.
- Cross-harness: run the command as a background process in either harness; read
  the printed `TUNNEL_URL`.
