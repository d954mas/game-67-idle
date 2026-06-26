// Unified AI Studio local server.
//
//   node ai_studio/studio_shell/server.mjs        -> http://127.0.0.1:8765/
//   node ai_studio/studio_shell/server.mjs 8780   -> http://127.0.0.1:8780/

import { createReadStream, existsSync, statSync } from "node:fs";
import { createServer } from "node:http";
import { extname, join, normalize, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { createTaskboardApi } from "../taskboard/api.mjs";
import { findRoot } from "../taskboard/lib.mjs";

const repoGuess = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const root = findRoot(repoGuess);
const aiStudioRoot = join(root, "ai_studio");
const taskboardPublic = join(aiStudioRoot, "taskboard", "public");
const port = Number.parseInt(process.argv[2] || process.env.AI_STUDIO_PORT || "8765", 10);
const handleTaskboardApi = createTaskboardApi(root);

const mime = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".md": "text/plain; charset=utf-8",
  ".svg": "image/svg+xml",
};

function safeResolve(base, relativePath) {
  const resolvedBase = resolve(base);
  const full = resolve(resolvedBase, normalize(relativePath));
  if (full !== resolvedBase && !full.startsWith(resolvedBase + sep)) return null;
  return full;
}

function staticPath(pathname) {
  if (pathname === "/" || pathname === "/index.html") return join(aiStudioRoot, "studio_shell", "index.html");
  if (pathname === "/home.css") return join(aiStudioRoot, "studio_shell", "home.css");
  if (pathname === "/studio_shell.css") return join(aiStudioRoot, "studio_shell", "studio_shell.css");
  if (pathname === "/studio_shell.js") return join(aiStudioRoot, "studio_shell", "studio_shell.js");
  if (pathname === "/tree.json") return join(aiStudioRoot, "tree.json");

  if (pathname === "/architecture_map" || pathname === "/architecture_map/") {
    return join(aiStudioRoot, "architecture_map", "index.html");
  }
  if (pathname.startsWith("/architecture_map/")) {
    return safeResolve(join(aiStudioRoot, "architecture_map"), pathname.slice("/architecture_map/".length));
  }

  if (pathname === "/taskboard" || pathname === "/taskboard/") {
    return join(taskboardPublic, "index.html");
  }
  if (pathname.startsWith("/taskboard/")) {
    return safeResolve(taskboardPublic, pathname.slice("/taskboard/".length));
  }

  if (pathname.startsWith("/ai_studio/")) {
    return safeResolve(root, pathname.slice(1));
  }

  return safeResolve(root, pathname.replace(/^\/+/, ""));
}

function serveStatic(req, res, url) {
  const full = staticPath(decodeURIComponent(url.pathname));
  if (!full || !existsSync(full) || !statSync(full).isFile()) {
    res.writeHead(404, { "content-type": "text/plain; charset=utf-8" });
    res.end("not found");
    return;
  }
  res.writeHead(200, { "content-type": mime[extname(full)] || "application/octet-stream" });
  createReadStream(full).pipe(res);
}

const server = createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);
  if (url.pathname.startsWith("/api/")) {
    handleTaskboardApi(req, res, url);
  } else {
    serveStatic(req, res, url);
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`ai_studio: http://127.0.0.1:${port}/  (repo: ${root})`);
});
