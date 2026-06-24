// Single source for the static-file MIME types used by the local dev servers
// (serve_tunnel.mjs, asset_review/serve_gallery.mjs). Just the extension->type
// map + a lookup — each server keeps its own routing/security/tunnel logic.
// A shared superset is safe: every overlapping extension maps identically, and
// both servers fall back to application/octet-stream for unknown types.
// NOT shared by taskboard/server.mjs on purpose — that admin UI uses a different
// contract (charset-suffixed text/* types for a handful of web assets).
import { extname } from "node:path";

export const MIME_TYPES = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript",
  ".mjs": "text/javascript",
  ".css": "text/css",
  ".json": "application/json",
  ".wasm": "application/wasm",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".gif": "image/gif",
  ".webp": "image/webp",
  ".svg": "image/svg+xml",
  ".ico": "image/x-icon",
  ".ttf": "font/ttf",
  ".otf": "font/otf",
  ".woff": "font/woff",
  ".woff2": "font/woff2",
  ".wav": "audio/wav",
  ".mp3": "audio/mpeg",
  ".ogg": "audio/ogg",
  ".glb": "model/gltf-binary",
  ".gltf": "model/gltf+json",
  ".obj": "text/plain",
  ".hdr": "application/octet-stream",
  ".data": "application/octet-stream",
  ".mem": "application/octet-stream",
};

// MIME type for a file path by extension; binary fallback for unknown types.
export function mimeType(path, fallback = "application/octet-stream") {
  return MIME_TYPES[extname(path).toLowerCase()] || fallback;
}
