#!/usr/bin/env node
// Serve a local directory and expose it on a public URL via a cloudflared quick
// tunnel — so the lead can view/test/play an app from a phone or another device.
//
// Run as a BACKGROUND task; it prints `TUNNEL_URL <https://...trycloudflare.com>`
// once ready, then stays alive holding the tunnel. Stop the background task to
// tear it down (kills cloudflared + closes the server).
//
//   node tools/serve_tunnel.mjs --dir tmp/asset-review-ll
//   node tools/serve_tunnel.mjs --dir build/game_seed/wasm-debug --port 8910
//
// Caveats: the URL is PUBLIC and unauthenticated while running — do not expose
// secrets. Quick tunnels are ephemeral (new URL each run). Serve self-contained
// content (relative URLs / wasm build dir); file:// references won't work remotely.
import { createServer } from "node:http";
import { createReadStream, existsSync, mkdirSync } from "node:fs";
import { stat } from "node:fs/promises";
import { spawn } from "node:child_process";
import { join, resolve, normalize, extname } from "node:path";
import { get } from "node:https";

const TYPES = {
  ".html": "text/html; charset=utf-8", ".js": "text/javascript", ".mjs": "text/javascript",
  ".css": "text/css", ".json": "application/json", ".wasm": "application/wasm",
  ".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".gif": "image/gif",
  ".webp": "image/webp", ".svg": "image/svg+xml", ".ico": "image/x-icon",
  ".ttf": "font/ttf", ".otf": "font/otf", ".woff": "font/woff", ".woff2": "font/woff2",
  ".wav": "audio/wav", ".mp3": "audio/mpeg", ".ogg": "audio/ogg",
  ".glb": "model/gltf-binary", ".gltf": "model/gltf+json", ".obj": "text/plain",
  ".data": "application/octet-stream", ".mem": "application/octet-stream",
};

function parseArgs(argv) {
  const a = { dir: "", port: 0, bin: "" };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${arg}`);
    i += 1;
    if (arg === "--dir") a.dir = next;
    else if (arg === "--port") a.port = Number(next);
    else if (arg === "--bin") a.bin = next;
    else throw new Error(`unknown option: ${arg}`);
  }
  if (!a.dir) throw new Error("missing --dir <directory to serve>");
  if (!a.port) a.port = 8700 + Math.floor((process.pid % 200));
  return a;
}

function cloudflaredTarget() {
  const p = process.platform;
  const arch = process.arch === "arm64" ? "arm64" : "amd64";
  if (p === "win32") return { name: "cloudflared.exe", url: `https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-windows-${arch}.exe` };
  if (p === "linux") return { name: "cloudflared", url: `https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-${arch}` };
  if (p === "darwin") return { name: "cloudflared", url: `https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-darwin-${arch}.tgz` };
  throw new Error(`unsupported platform: ${p}`);
}

function download(url, dest, redirects = 0) {
  return new Promise((res, rej) => {
    if (redirects > 6) return rej(new Error("too many redirects"));
    get(url, { rejectUnauthorized: false }, (r) => {
      if (r.statusCode >= 300 && r.statusCode < 400 && r.headers.location) {
        r.resume();
        return res(download(r.headers.location, dest, redirects + 1));
      }
      if (r.statusCode !== 200) return rej(new Error(`download ${r.statusCode} for ${url}`));
      const out = createReadStream ? null : null; // placeholder
      import("node:fs").then(({ createWriteStream }) => {
        const ws = createWriteStream(dest);
        r.pipe(ws);
        ws.on("finish", () => ws.close(() => res(dest)));
        ws.on("error", rej);
      });
    }).on("error", rej);
  });
}

async function ensureCloudflared(binArg) {
  if (binArg && existsSync(binArg)) return binArg;
  const { name, url } = cloudflaredTarget();
  const binDir = resolve("tools/bin");
  const dest = join(binDir, name);
  if (existsSync(dest)) return dest;
  // fall back to a tmp copy from an earlier manual download if present
  if (existsSync(resolve("tmp", name))) return resolve("tmp", name);
  if (!existsSync(binDir)) mkdirSync(binDir, { recursive: true });
  process.stderr.write(`downloading cloudflared -> ${dest}\n`);
  await download(url, dest);
  if (process.platform !== "win32") {
    const { chmodSync } = await import("node:fs");
    chmodSync(dest, 0o755);
  }
  return dest;
}

function startServer(dir, port) {
  const root = resolve(dir);
  const server = createServer(async (req, res) => {
    try {
      let rel = decodeURIComponent(req.url.split("?")[0]);
      if (rel === "/" || rel === "") rel = "/index.html";
      const full = normalize(join(root, rel));
      if (!full.startsWith(root)) { res.writeHead(403); return res.end("forbidden"); }
      const st = await stat(full).catch(() => null);
      const target = st && st.isDirectory() ? join(full, "index.html") : full;
      if (!existsSync(target)) { res.writeHead(404); return res.end("not found"); }
      res.writeHead(200, { "content-type": TYPES[extname(target).toLowerCase()] || "application/octet-stream", "cache-control": "no-cache" });
      createReadStream(target).pipe(res);
    } catch (e) {
      res.writeHead(500); res.end(String(e.message));
    }
  });
  return new Promise((ok) => server.listen(port, () => ok(server)));
}

async function main() {
  const a = parseArgs(process.argv.slice(2));
  if (!existsSync(resolve(a.dir))) throw new Error(`--dir not found: ${resolve(a.dir)}`);
  const bin = await ensureCloudflared(a.bin);
  await startServer(a.dir, a.port);
  process.stderr.write(`serving ${resolve(a.dir)} on http://localhost:${a.port}\n`);

  const cf = spawn(bin, ["tunnel", "--url", `http://localhost:${a.port}`], { stdio: ["ignore", "pipe", "pipe"] });
  let printed = false;
  const scan = (buf) => {
    const m = String(buf).match(/https:\/\/[a-z0-9-]+\.trycloudflare\.com/);
    if (m && !printed) { printed = true; process.stdout.write(`TUNNEL_URL ${m[0]}\n`); }
  };
  cf.stdout.on("data", scan);
  cf.stderr.on("data", scan);
  cf.on("exit", (code) => { process.stderr.write(`cloudflared exited (${code})\n`); process.exit(code || 0); });

  const shutdown = () => { try { cf.kill(); } catch {} process.exit(0); };
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}

main().catch((e) => { console.error(e.message); process.exit(1); });
