// Architecture Map HTTP API adapter.
//
// Studio Shell mounts this handler. Architecture Map owns the JSON payloads:
//   GET /api/architecture-tree        single authored workspace tree
//   GET /api/architecture-validation  live validation report (never committed)
//
// Serving the report live replaces the committed validation-report.json file.

import { loadArchitectureTree } from "./tree_loader.mjs";
import { createValidationReport } from "./validate_map.mjs";

const defaultMapPath = "ai_studio/tree.json";

function sendJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data));
}

export function createArchitectureMapApi(root) {
  return async function handleArchitectureMapApi(req, res, url) {
    if (req.method !== "GET") return false;
    try {
      if (url.pathname === "/api/architecture-tree") {
        sendJson(res, 200, loadArchitectureTree(root, defaultMapPath));
        return true;
      }
      if (url.pathname === "/api/architecture-validation") {
        sendJson(res, 200, createValidationReport({ repoRoot: root, mapPath: defaultMapPath }));
        return true;
      }
    } catch (error) {
      sendJson(res, 500, { error: error && error.message ? error.message : String(error) });
      return true;
    }
    return false;
  };
}
