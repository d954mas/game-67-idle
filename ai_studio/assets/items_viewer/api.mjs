// Items Viewer HTTP API adapter (T0316 phase 1) — read-only.
//
// Studio Shell mounts this on /api/items-viewer/. It only marshals HTTP <-> ops.mjs; no
// items logic lives here (mirrors assets/canvas/api.mjs:239's shape — an async
// (req,res,url) => bool handler that never rejects). Two GET endpoints; no
// POST/PUT/PATCH — there is no write op-layer yet (spec §1/§3):
//   GET /api/items-viewer/catalogs         -- the dropdown list
//   GET /api/items-viewer/catalog?id=<id>  -- the whole view for one catalog
import { getCatalogView, listCatalogs } from "./ops.mjs";

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
        sendJson(res, 200, listCatalogs(root));
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
        const view = await getCatalogView(root, id);
        if (!view) {
          sendJson(res, 404, { error: "catalog not found" });
          return true;
        }
        sendJson(res, 200, view);
        return true;
      }

      sendJson(res, 404, { error: "not found" });
      return true;
    } catch (error) {
      // TOOL-FAILURE only (spec §3): py could not be spawned, or items_ops.py emitted
      // unparseable JSON on an otherwise-successful exit — a viewer/env bug, never the
      // game's own data (that is content_error, 200, handled above).
      sendJson(res, 500, { error: error && error.message ? error.message : String(error) });
      return true;
    }
  };
}
