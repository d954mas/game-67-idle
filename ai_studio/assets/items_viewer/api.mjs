// Items Workbench HTTP API adapter — focused read-only slice.
//
// Studio Shell mounts this on /api/items-viewer/. It only marshals HTTP <-> ops.mjs; no
// items logic lives here (mirrors assets/canvas/api.mjs:239's shape — an async
// (req,res,url) => bool handler that never rejects). Focused GET endpoints; no
// POST/PUT/PATCH — there is no write op-layer yet (spec §1/§3):
//   GET /api/items-viewer/catalogs         -- the dropdown list
//   GET /api/items-viewer/catalog?id=<id>  -- the whole view for one catalog
//   GET /api/items-viewer/item             -- one selected Snapshot detail
//   GET /api/items-viewer/chart            -- one selected generated series
import { getCatalogView, getItemChart, getItemDetail, listCatalogs } from "./ops.mjs";

function boolQueryParam(value) {
  return value === "true" || value === "1" || value === "yes";
}

function sendJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data));
}

export function createItemsViewerApi(root) {
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

      sendJson(res, 404, { error: "not found" });
      return true;
    } catch (error) {
      // TOOL-FAILURE only: Studio Python could not be spawned, or items_cli.py emitted
      // unparseable JSON on an otherwise-successful exit — a viewer/env bug, never the
      // game's own data (that is content_error, 200, handled above).
      sendJson(res, 500, { error: error && error.message ? error.message : String(error) });
      return true;
    }
  };
}
