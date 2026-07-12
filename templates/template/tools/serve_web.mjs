#!/usr/bin/env node
// Local static server for a wasm game build (one command from the done-when).
// Self-contained on purpose (inline MIME, stdlib http only) so a copied game
// stays portable if it is ever moved out of the repo -- no ai_studio import.
//
//   node tools/serve_web.mjs [--preset wasm-release|wasm-debug|wasm-devapi-debug] [--target local|itch|poki|yandex|playgama] [--port N] [--dir <bin>]
//
// Serves build/<preset>/bin/ over http://127.0.0.1:<port>/. game.wasm is served
// as application/wasm so emscripten's streaming compile does not fall back.
import { createServer } from "node:http";
import { createReadStream, existsSync, statSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, extname, isAbsolute, join, normalize, resolve, sep } from "node:path";

const GAME_DIR = dirname(dirname(fileURLToPath(import.meta.url)));

// Inline MIME table (deliberately NOT importing ai_studio/.../mime.mjs: keeps a
// copied game self-contained). Unknown -> octet-stream.
const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript",
  ".wasm": "application/wasm",
  ".data": "application/octet-stream",
  ".ntpack": "application/octet-stream",
  ".json": "application/json",
  ".css": "text/css",
  ".png": "image/png",
  ".wav": "audio/wav",
  ".mp3": "audio/mpeg",
  ".ogg": "audio/ogg",
};
const mimeType = (p) => MIME[extname(p).toLowerCase()] || "application/octet-stream";

const DEFAULT_PORT = { "wasm-release": 8080, "wasm-debug": 8080, "wasm-devapi-debug": 8081 };

function parseArgs(argv) {
  const a = { preset: "wasm-release", target: "local", port: 0, dir: "" };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    if (k === "--preset") a.preset = argv[++i];
    else if (k === "--target") a.target = argv[++i];
    else if (k === "--port") a.port = Number(argv[++i]) || 0;
    else if (k === "--dir") a.dir = argv[++i];
    else throw new Error(`unknown option: ${k}`);
  }
  return a;
}

function requestRel(urlPath) {
  let p;
  try {
    p = decodeURIComponent(String(urlPath || "/").split("?")[0] || "/");
  } catch {
    return null;
  }
  if (p === "/" || p === "") return "index.html";
  const rel = normalize(p.replace(/^[/\\]+/, ""));
  if (!rel || rel === "." || rel.startsWith("..") || isAbsolute(rel)) return null;
  return rel;
}

function main() {
  const a = parseArgs(process.argv.slice(2));
  if (!["local", "itch", "poki", "yandex", "playgama"].includes(a.target)) {
    throw new Error(`unknown target: ${a.target}`);
  }
  const buildName = a.target === "local" ? a.preset : `${a.preset}-${a.target}`;
  const root = resolve(a.dir || join(GAME_DIR, "build", buildName, "bin"));
  const port = a.port || DEFAULT_PORT[a.preset] || 8080;

  if (!existsSync(join(root, "assets", "game.ntpack"))) {
    console.warn(`WARN: ${join(root, "assets", "game.ntpack")} missing -- run node tools/build_web.mjs --preset ${a.preset} first`);
  }

  const server = createServer((req, res) => {
    const rel = requestRel(req.url || "/");
    if (rel === null) { res.writeHead(404); res.end("not found"); return; }
    const file = resolve(root, rel);
    if (file !== root && !file.startsWith(root + sep)) { res.writeHead(404); res.end("not found"); return; }
    if (!existsSync(file) || !statSync(file).isFile()) { res.writeHead(404); res.end("not found"); return; }
    res.writeHead(200, { "content-type": mimeType(file) });
    const stream = createReadStream(file);
    stream.on("error", () => { res.destroy(); });
    stream.pipe(res);
  });
  server.on("error", (err) => {
    const hint = err.code === "EADDRINUSE"
      ? `port ${port} is already in use (another serve_web? pass --port)`
      : err.message;
    console.error(`serve_web: ${hint}`);
    process.exit(1);
  });
  server.listen(port, "127.0.0.1", () => {
    console.log(`serve_web: http://127.0.0.1:${port}/ (preset ${a.preset}, target ${a.target}, dir ${root})`);
  });
}

main();
