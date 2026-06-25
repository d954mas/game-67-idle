#!/usr/bin/env node
import { createReadStream, existsSync, statSync } from "node:fs";
import { createServer } from "node:http";
import { extname, join, normalize, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const port = Number.parseInt(process.argv[2] || process.env.AI_STUDIO_MAP_PORT || "8765", 10);

const contentTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".mjs", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".md", "text/markdown; charset=utf-8"],
  [".css", "text/css; charset=utf-8"],
  [".svg", "image/svg+xml"],
]);

function send(res, status, body, type = "text/plain; charset=utf-8") {
  res.writeHead(status, {
    "content-type": type,
    "cache-control": "no-store",
  });
  res.end(body);
}

function resolveRequestPath(url) {
  const pathname = decodeURIComponent(new URL(url, "http://127.0.0.1").pathname);
  const rel = pathname === "/" ? "ai_studio/architecture_map/index.html" : pathname.replace(/^\/+/, "");
  const abs = resolve(repoRoot, normalize(rel));
  if (abs !== repoRoot && !abs.startsWith(repoRoot + sep)) return null;
  return abs;
}

const server = createServer((req, res) => {
  const abs = resolveRequestPath(req.url || "/");
  if (!abs) {
    send(res, 403, "Forbidden");
    return;
  }
  if (!existsSync(abs) || !statSync(abs).isFile()) {
    send(res, 404, "Not found");
    return;
  }
  const type = contentTypes.get(extname(abs)) || "application/octet-stream";
  res.writeHead(200, {
    "content-type": type,
    "cache-control": "no-store",
  });
  createReadStream(abs).pipe(res);
});

server.listen(port, "127.0.0.1", () => {
  console.log(`AI Studio architecture map: http://127.0.0.1:${port}/ai_studio/architecture_map/index.html`);
  console.log(`Serving ${join(repoRoot, "ai_studio", "architecture_map")}`);
});
