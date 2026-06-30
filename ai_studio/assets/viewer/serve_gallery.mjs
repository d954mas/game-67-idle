#!/usr/bin/env node
// Serve an asset gallery built with `build_review --ref`: the gallery dir at /,
// and the asset library at /lib/ - so the page references library files in place
// (no media copy) for huge libraries.
//
//   node ai_studio/assets/viewer/serve_gallery.mjs --gallery tmp/lib-gallery --lib <libraryRoot> --port 8910
import { createServer } from "node:http";
import { createReadStream, existsSync, statSync } from "node:fs";
import { join, resolve, normalize } from "node:path";
import { mimeType } from "../../core_harness/tool_lib/mime.mjs";

function parseArgs(argv) {
  const a = { gallery: "tmp/lib-gallery", lib: "", port: 8910 };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    if (k === "--gallery") a.gallery = argv[++i];
    else if (k === "--lib") a.lib = argv[++i];
    else if (k === "--port") a.port = Number(argv[++i]) || 8910;
  }
  if (!a.lib) throw new Error("missing --lib <library root>");
  return a;
}

const a = parseArgs(process.argv.slice(2));
const GALLERY = resolve(a.gallery);
const LIB = resolve(a.lib);

// Resolve a request path into [rootDir, relPath], confined to that root.
function resolveTarget(urlPath) {
  let p = decodeURIComponent(urlPath.split("?")[0]);
  if (p === "/" || p === "") return [GALLERY, "index.html"];
  if (p.startsWith("/lib/")) {
    const rel = normalize(p.slice(5)).replace(/^(\.\.[\\/])+/, "");
    return [LIB, rel];
  }
  const rel = normalize(p.replace(/^\/+/, "")).replace(/^(\.\.[\\/])+/, "");
  return [GALLERY, rel];
}

const server = createServer((req, res) => {
  const [root, rel] = resolveTarget(req.url || "/");
  const file = join(root, rel);
  if (!file.startsWith(root) || !existsSync(file) || !statSync(file).isFile()) {
    res.writeHead(404); res.end("not found"); return;
  }
  res.writeHead(200, { "content-type": mimeType(file), "access-control-allow-origin": "*" });
  createReadStream(file).pipe(res);
});
server.listen(a.port, () => {
  console.log(`serving gallery ${GALLERY}`);
  console.log(`        library ${LIB} at /lib/`);
  console.log(`  http://localhost:${a.port}/`);
});
