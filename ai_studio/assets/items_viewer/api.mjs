// Items Workbench HTTP API adapter.
//
// Studio Shell mounts this on /api/items-viewer/. It only marshals HTTP <-> ops.mjs; no
// items logic lives here (mirrors assets/canvas/api.mjs:239's shape — an async
// (req,res,url) => bool handler that never rejects). It owns no Items writer:
// POST preview/apply delegates directly to the shared T0366 semantic CLI.
//   GET /api/items-viewer/catalogs         -- the dropdown list
//   GET /api/items-viewer/catalog?id=<id>  -- the whole view for one catalog
//   GET /api/items-viewer/item             -- one selected Snapshot detail
//   GET /api/items-viewer/chart            -- one selected generated series
//   POST /api/items-viewer/edit             -- preview/apply one semantic patch
import {
  editCatalogItem,
  getCatalogView,
  getItemChart,
  getItemDetail,
  ItemEditInputError,
  listCatalogs,
} from "./ops.mjs";

const MAX_JSON_BODY = 64 * 1024;

class RequestInputError extends Error {}

async function readJsonBody(req) {
  const contentType = String(req.headers?.["content-type"] || "");
  if (!contentType.startsWith("application/json")) throw new RequestInputError("content-type must be application/json");
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    const bytes = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
    size += bytes.length;
    if (size > MAX_JSON_BODY) throw new RequestInputError("request body exceeds 64 KiB");
    chunks.push(bytes);
  }
  try {
    return JSON.parse(Buffer.concat(chunks).toString("utf8"));
  } catch {
    throw new RequestInputError("request body must be valid JSON");
  }
}

function boolQueryParam(value) {
  return value === "true" || value === "1" || value === "yes";
}

function sendJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data));
}

function headerValue(req, name) {
  const value = req.headers?.[name.toLowerCase()];
  return Array.isArray(value) ? value[0] : (value || "");
}

function sameOriginRequest(req, allowedHosts) {
  const host = headerValue(req, "host");
  const origin = headerValue(req, "origin");
  if (!allowedHosts.has(host) || !origin) return false;
  try {
    const parsed = new URL(origin);
    return parsed.protocol === "http:" && parsed.host === host && parsed.origin === origin;
  } catch {
    return false;
  }
}

export function createItemsViewerApi(root, options = {}) {
  const allowedHosts = new Set(options.allowedHosts || []);
  return async function handleItemsViewerApi(req, res, url) {
    try {
      if (url.pathname === "/api/items-viewer/catalogs") {
        if (req.method !== "GET") {
          sendJson(res, 405, { error: "method not allowed" });
          return true;
        }
        sendJson(res, 200, listCatalogs(root, {
          includePrivate: boolQueryParam(url.searchParams.get("include-private")) || boolQueryParam(url.searchParams.get("includePrivate")),
        }));
        return true;
      }

      // The page always calls this for the selected id (no dropdown-flag
      // short-circuit — spec §3/§4). 200 with hasItems:false is a valid empty state,
      // 404 only when `id` matches neither registry, content_error is 200 (the game's
      // own data is broken, not a viewer failure).
      if (url.pathname === "/api/items-viewer/catalog") {
        if (req.method !== "GET") {
          sendJson(res, 405, { error: "method not allowed" });
          return true;
        }
        const id = url.searchParams.get("id");
        const view = await getCatalogView(root, id, {
          includePrivate: boolQueryParam(url.searchParams.get("include-private")) || boolQueryParam(url.searchParams.get("includePrivate")),
        });
        if (!view) {
          sendJson(res, 404, { error: "catalog not found" });
          return true;
        }
        sendJson(res, 200, view);
        return true;
      }

      if (url.pathname === "/api/items-viewer/item" || url.pathname === "/api/items-viewer/chart") {
        if (req.method !== "GET") {
          sendJson(res, 405, { error: "method not allowed" });
          return true;
        }
        const catalogId = url.searchParams.get("catalog");
        const itemId = url.searchParams.get("item");
        const field = url.searchParams.get("field");
        if (!catalogId || !itemId) {
          sendJson(res, 400, { error: "catalog and item are required" });
          return true;
        }
        if (url.pathname.endsWith("/chart") && !field) {
          sendJson(res, 400, { error: "selected chart field is required" });
          return true;
        }
        const options = {
          includePrivate: boolQueryParam(url.searchParams.get("include-private")) || boolQueryParam(url.searchParams.get("includePrivate")),
        };
        const result = url.pathname.endsWith("/chart")
          ? await getItemChart(root, catalogId, itemId, field, options)
          : await getItemDetail(root, catalogId, itemId, options);
        if (!result) {
          sendJson(res, 404, { error: "catalog not found" });
          return true;
        }
        sendJson(res, 200, result);
        return true;
      }

      if (url.pathname === "/api/items-viewer/edit") {
        if (req.method !== "POST") {
          sendJson(res, 405, { error: "method not allowed" });
          return true;
        }
        if (!sameOriginRequest(req, allowedHosts)) {
          sendJson(res, 403, { error: "items request rejected" });
          return true;
        }
        const payload = await readJsonBody(req);
        if (!payload || typeof payload !== "object" || Array.isArray(payload)
            || Object.keys(payload).sort().join(",") !== "apply,catalog,edit"
            || typeof payload.catalog !== "string" || typeof payload.apply !== "boolean") {
          throw new RequestInputError("body requires exactly catalog, edit, and boolean apply");
        }
        const options = {
          includePrivate: boolQueryParam(url.searchParams.get("include-private")) || boolQueryParam(url.searchParams.get("includePrivate")),
        };
        const result = await editCatalogItem(root, payload.catalog, payload.edit, { apply: payload.apply }, options);
        if (!result) {
          sendJson(res, 404, { error: "catalog not found" });
          return true;
        }
        const status = result.ok ? 200 : result.error?.code === "edit.conflict" ? 409 : 422;
        sendJson(res, status, result);
        return true;
      }

      sendJson(res, 404, { error: "not found" });
      return true;
    } catch (error) {
      if (error instanceof RequestInputError || error instanceof ItemEditInputError) {
        sendJson(res, 400, { error: error.message });
        return true;
      }
      // TOOL-FAILURE only: Studio Python could not be spawned, or items_cli.py emitted
      // unparseable JSON on an otherwise-successful exit — a viewer/env bug, never the
      // game's own data (that is content_error, 200, handled above).
      sendJson(res, 500, { error: error && error.message ? error.message : String(error) });
      return true;
    }
  };
}
