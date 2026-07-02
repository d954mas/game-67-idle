#!/usr/bin/env node
// Serve an asset gallery built with `build_review --ref`: the gallery dir at /,
// and the asset library at /lib/ - so the page references library files in place
// (no media copy) for huge libraries.
//
//   node ai_studio/assets/gallery/serve_gallery.mjs --gallery tmp/lib-gallery --lib <libraryRoot> --port 8910
import { createServer } from "node:http";
import { createReadStream, existsSync, statSync } from "node:fs";
import { isAbsolute, normalize, relative, resolve } from "node:path";
import { mimeType } from "../../core_harness/tool_lib/mime.mjs";
import { isMain } from "../../core_harness/tool_lib/cli.mjs";

function parseArgs(argv) {
  const a = { gallery: "tmp/lib-gallery", lib: "", port: 8910 };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) throw new Error(`missing value for ${k}`);
    i += 1;
    if (k === "--gallery") a.gallery = next;
    else if (k === "--lib") a.lib = next;
    else if (k === "--port") a.port = Number(next) || 8910;
    else throw new Error(`unknown option: ${k}`);
  }
  if (!a.lib) throw new Error("missing --lib <library root>");
  return a;
}

function requestPath(urlPath) {
  try {
    return decodeURIComponent(String(urlPath || "/").split("?")[0] || "/");
  } catch {
    return null;
  }
}

function safeRequestRel(rawPath) {
  const rel = normalize(String(rawPath || "").replace(/^[/\\]+/, ""));
  if (!rel || rel === ".") return "";
  if (rel.startsWith("..") || isAbsolute(rel)) return null;
  return rel;
}

function isInside(root, file) {
  const rel = relative(root, file);
  return rel === "" || (rel && !rel.startsWith("..") && !isAbsolute(rel));
}

// Resolve a request path into a filesystem target confined to the selected root.
function resolveTarget(urlPath, { galleryRoot, libRoot }) {
  const p = requestPath(urlPath);
  if (p === null) return null;
  const root = p.startsWith("/lib/") ? libRoot : galleryRoot;
  let rel;
  if (p === "/" || p === "") rel = "index.html";
  else if (p.startsWith("/lib/")) rel = safeRequestRel(p.slice(5));
  else rel = safeRequestRel(p);
  if (rel === null) return null;
  const file = resolve(root, rel);
  if (!isInside(root, file)) return null;
  return { root, rel, file };
}

function createGalleryServer({ galleryRoot, libRoot }) {
  return createServer((req, res) => {
    const target = resolveTarget(req.url || "/", { galleryRoot, libRoot });
    if (!target || !existsSync(target.file) || !statSync(target.file).isFile()) {
      res.writeHead(404); res.end("not found"); return;
    }
    res.writeHead(200, { "content-type": mimeType(target.file), "access-control-allow-origin": "*" });
    createReadStream(target.file).pipe(res);
  });
}

function main(argv = process.argv.slice(2)) {
  const a = parseArgs(argv);
  const galleryRoot = resolve(a.gallery);
  const libRoot = resolve(a.lib);
  const server = createGalleryServer({ galleryRoot, libRoot });
  server.listen(a.port, () => {
    console.log(`serving gallery ${galleryRoot}`);
    console.log(`        library ${libRoot} at /lib/`);
    console.log(`  http://localhost:${a.port}/`);
  });
  return server;
}

if (isMain(import.meta.url)) {
  try {
    main();
  } catch (e) {
    console.error(e.message);
    process.exit(1);
  }
}

export { createGalleryServer, main, parseArgs, resolveTarget };
