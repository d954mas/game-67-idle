import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";
import fs from "node:fs/promises";

const editorDir = path.dirname(fileURLToPath(import.meta.url));
const designDir = path.resolve(editorDir, "..");
const gameDir = path.resolve(designDir, "..");
const dataDir = path.join(designDir, "data");
const assetDir = path.join(gameDir, "assets");
const requestedPort = Number(process.argv[2] || process.env.PORT || 5191);

const contentTypes = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".webp": "image/webp",
  ".gif": "image/gif",
  ".svg": "image/svg+xml; charset=utf-8"
};

const server = http.createServer(async (request, response) => {
  try {
    const url = new URL(request.url || "/", `http://${request.headers.host || "127.0.0.1"}`);

    if (request.method === "GET" && url.pathname === "/api/content") {
      return sendJson(response, 200, await loadContent());
    }

    if (request.method === "GET" && url.pathname === "/favicon.ico") {
      response.writeHead(204);
      response.end();
      return;
    }

    if (request.method === "POST" && url.pathname === "/api/save") {
      const body = await readJsonBody(request);
      await saveContentFile(body.file, body.data);
      return sendJson(response, 200, { ok: true, file: body.file });
    }

    if (request.method === "GET") {
      return await serveStatic(url.pathname, response);
    }

    return sendJson(response, 405, { error: "Method not allowed" });
  } catch (error) {
    return sendJson(response, 500, { error: error.message || String(error) });
  }
});

server.listen(requestedPort, "127.0.0.1", () => {
  console.log(`RB Dark RPG content editor: http://127.0.0.1:${requestedPort}/`);
});

async function loadContent() {
  const manifest = await loadJsonFile("content_manifest.json");
  const files = { "content_manifest.json": manifest };
  for (const file of manifest.load_order || []) {
    files[file] = await loadJsonFile(file);
  }
  return { manifest, files };
}

async function loadJsonFile(file) {
  const target = getDataPath(file);
  const text = await fs.readFile(target, "utf8");
  return JSON.parse(text);
}

async function saveContentFile(file, data) {
  const allowed = await getAllowedFiles();
  if (!allowed.has(file)) {
    throw new Error(`File is not allowed: ${file}`);
  }
  const target = getDataPath(file);
  const json = `${JSON.stringify(data, null, 2)}\n`;
  JSON.parse(json);
  await fs.writeFile(target, json, "utf8");
}

async function getAllowedFiles() {
  const manifest = await loadJsonFile("content_manifest.json");
  return new Set(["content_manifest.json", ...(manifest.load_order || [])]);
}

function getDataPath(file) {
  const safeFile = path.basename(String(file || ""));
  if (!safeFile || safeFile !== file || !safeFile.endsWith(".json")) {
    throw new Error(`Invalid data file: ${file}`);
  }
  return path.join(dataDir, safeFile);
}

async function readJsonBody(request) {
  const chunks = [];
  let size = 0;
  for await (const chunk of request) {
    size += chunk.length;
    if (size > 5 * 1024 * 1024) {
      throw new Error("Request body is too large");
    }
    chunks.push(chunk);
  }
  const text = Buffer.concat(chunks).toString("utf8");
  return JSON.parse(text || "{}");
}

async function serveStatic(urlPath, response) {
  const pathname = decodeURIComponent(urlPath === "/" ? "/index.html" : urlPath);
  let root = editorDir;
  let relative = pathname.slice(1);

  if (pathname.startsWith("/data/")) {
    root = dataDir;
    relative = pathname.slice("/data/".length);
  } else if (pathname.startsWith("/game-assets/")) {
    root = assetDir;
    relative = pathname.slice("/game-assets/".length);
  }

  const target = path.resolve(root, relative);
  if (!isInside(root, target)) {
    return sendJson(response, 403, { error: "Forbidden" });
  }

  let data;
  try {
    data = await fs.readFile(target);
  } catch (error) {
    if (error.code === "ENOENT") {
      return sendJson(response, 404, { error: "Not found" });
    }
    throw error;
  }
  const contentType = contentTypes[path.extname(target)] || "application/octet-stream";
  response.writeHead(200, { "Content-Type": contentType });
  response.end(data);
}

function sendJson(response, status, payload) {
  response.writeHead(status, { "Content-Type": "application/json; charset=utf-8" });
  response.end(`${JSON.stringify(payload, null, 2)}\n`);
}

function isInside(root, target) {
  const relative = path.relative(root, target);
  return relative === "" || (!relative.startsWith("..") && !path.isAbsolute(relative));
}
