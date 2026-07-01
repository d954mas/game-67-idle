// Unified AI Studio local server. Prefer start_site_windows.ps1 for browser use on Windows.
//
// Foreground debug:
//   node ai_studio/studio_shell/server.mjs
//   node ai_studio/studio_shell/server.mjs 8780

import { createReadStream, existsSync, mkdirSync, statSync, writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { extname, join, normalize, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { createTaskboardApi } from "../taskboard/api.mjs";
import { findRoot } from "../taskboard/lib.mjs";
import { createAssetViewerApi, resolveAssetViewerGalleryPath } from "../assets/viewer/api.mjs";
import { loadQualityCatalog } from "../quality/catalog.mjs";

const repoGuess = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const root = findRoot(repoGuess);
const aiStudioRoot = join(root, "ai_studio");
const taskboardPublic = join(aiStudioRoot, "taskboard", "public");
const assetViewerRoot = join(aiStudioRoot, "assets", "viewer");
const assetPreviewRoot = join(aiStudioRoot, "assets", "storage", "previews");
const port = Number.parseInt(process.argv[2] || process.env.AI_STUDIO_PORT || "8765", 10);
const handleTaskboardApi = createTaskboardApi(root);
const handleAssetViewerApi = createAssetViewerApi(root);
const stateDir = join(root, "tmp", "ai_studio");
const pidFile = join(stateDir, `studio_shell_${port}.pid`);

const mime = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".md": "text/plain; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".webp": "image/webp",
  ".gif": "image/gif",
  ".glb": "model/gltf-binary",
  ".gltf": "model/gltf+json; charset=utf-8",
  ".hdr": "image/vnd.radiance",
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

  if (pathname === "/asset_viewer" || pathname === "/asset_viewer/" || pathname === "/viewer" || pathname === "/viewer/") {
    return join(assetViewerRoot, "index.html");
  }
  if (pathname === "/asset_viewer/studio_env.hdr" || pathname === "/viewer/studio_env.hdr") {
    return join(assetPreviewRoot, "studio_env.hdr");
  }
  if (pathname.startsWith("/asset_viewer/gallery/") || pathname.startsWith("/viewer/gallery/")) {
    return resolveAssetViewerGalleryPath(root, pathname);
  }
  if (pathname.startsWith("/asset_viewer/")) {
    return safeResolve(assetViewerRoot, pathname.slice("/asset_viewer/".length));
  }
  if (pathname.startsWith("/viewer/")) {
    return safeResolve(assetViewerRoot, pathname.slice("/viewer/".length));
  }

  if (pathname === "/quality" || pathname === "/quality/") {
    return join(aiStudioRoot, "quality", "index.html");
  }
  if (pathname.startsWith("/quality/")) {
    return safeResolve(join(aiStudioRoot, "quality"), pathname.slice("/quality/".length));
  }

  if (pathname.startsWith("/ai_studio/")) {
    return safeResolve(root, pathname.slice(1));
  }

  return null;
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

function serveJson(res, value) {
  res.writeHead(200, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(value));
}

const server = createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);
  if (url.pathname.startsWith("/api/")) {
    if (url.pathname === "/api/quality-checks") {
      serveJson(res, loadQualityCatalog(root));
      return;
    }

    handleAssetViewerApi(req, res, url).then((handled) => {
      if (!handled) handleTaskboardApi(req, res, url);
    });
  } else {
    serveStatic(req, res, url);
  }
});

server.listen(port, "127.0.0.1", () => {
  mkdirSync(stateDir, { recursive: true });
  writeFileSync(pidFile, `${process.pid}\n`, "utf8");
  console.log(`ai_studio: http://127.0.0.1:${port}/  (repo: ${root})`);
});
