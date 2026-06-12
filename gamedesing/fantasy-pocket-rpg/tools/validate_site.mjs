import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import { createServer } from "node:http";
import { extname, join, normalize, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));

const mime = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".png": "image/png",
};

function requestPath(urlPath) {
  const decoded = decodeURIComponent(urlPath.split("?")[0]);
  const relative = decoded === "/" ? "index.html" : decoded.replace(/^\/+/, "");
  const safe = normalize(relative);
  const full = resolve(join(root, safe));
  return full.startsWith(root) ? full : null;
}

function makeServer() {
  return createServer(async (request, response) => {
    const filePath = requestPath(request.url || "/");
    if (!filePath) {
      response.writeHead(403);
      response.end("Forbidden");
      return;
    }

    try {
      const info = await stat(filePath);
      const finalPath = info.isDirectory() ? join(filePath, "index.html") : filePath;
      response.writeHead(200, {
        "content-type": mime[extname(finalPath)] || "application/octet-stream",
        "cache-control": "no-store",
      });
      createReadStream(finalPath).pipe(response);
    } catch {
      response.writeHead(404, { "content-type": "text/plain; charset=utf-8" });
      response.end("Not found");
    }
  });
}

async function fetchOk(baseUrl, path, expectedType) {
  const response = await fetch(`${baseUrl}${path}`);
  if (!response.ok) {
    throw new Error(`${path} returned ${response.status}`);
  }
  const type = response.headers.get("content-type") || "";
  if (expectedType && !type.includes(expectedType)) {
    throw new Error(`${path} content-type ${type}, expected ${expectedType}`);
  }
  const body = await response.arrayBuffer();
  return { response, body };
}

async function main() {
  const server = makeServer();
  await new Promise((resolveListen) => server.listen(0, "127.0.0.1", resolveListen));
  const address = server.address();
  const baseUrl = `http://127.0.0.1:${address.port}`;

  try {
    const htmlResult = await fetchOk(baseUrl, "/", "text/html");
    const html = new TextDecoder().decode(htmlResult.body);
    for (const needle of [
      "Fantasy Pocket RPG",
      "fake-shot-ruins-background.png",
      "camp-preparation-background.png",
      "data/balance.json",
      "data/ui_flow.json",
      "First Combat",
      "Long-Term Progression",
      "Content Model",
      "Iteration Roadmap",
      "Implementation Scope",
    ]) {
      if (!html.includes(needle)) {
        throw new Error(`index.html missing ${needle}`);
      }
    }

    await fetchOk(baseUrl, "/site.css", "text/css");
    await fetchOk(baseUrl, "/site.js", "text/javascript");
    await fetchOk(baseUrl, "/data/balance.json", "application/json");
    await fetchOk(baseUrl, "/data/ui_flow.json", "application/json");
    await fetchOk(baseUrl, "/data/combat.json", "application/json");
    await fetchOk(baseUrl, "/data/progression.json", "application/json");
    await fetchOk(baseUrl, "/data/content_model.json", "application/json");
    await fetchOk(baseUrl, "/data/roadmap.json", "application/json");
    await fetchOk(baseUrl, "/data/asset_manifest.json", "application/json");
    await fetchOk(baseUrl, "/art/fake-shot-ruins-background.png", "image/png");
    await fetchOk(baseUrl, "/art/camp-preparation-background.png", "image/png");

    console.log(`Fantasy Pocket RPG visual GDD site is valid at ${baseUrl}/`);
  } finally {
    if (typeof server.closeAllConnections === "function") {
      server.closeAllConnections();
    }
    await new Promise((resolveClose) => server.close(resolveClose));
  }
}

main().catch((error) => {
  console.error(`FAIL: ${error.message}`);
  process.exit(1);
});
