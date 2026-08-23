// Game Page HTTP API adapter. Studio Shell mounts this on /api/game-page/.
// Marshals HTTP <-> ops.mjs only; no game logic lives here. The route list
// below is the complete public contract of this module.
//   GET /api/game-page/games              -- all games, private included
//   GET /api/game-page/overview?game=<id> -- one game's overview header data
//   GET /api/game-page/builds?game=<id>   -- build configs, packs, release artifacts
//   GET /api/game-page/state?game=<id>    -- state schema files
//   GET /api/game-page/saves?game=<id>    -- native save slots with envelope meta
//   GET /api/game-page/save?game&slot     -- one save slot's parsed content
//   GET /api/game-page/captures?game=<id> -- capture takes, newest first
//   GET /api/game-page/pack?game&path     -- parsed ntpack contents
//   GET /api/game-page/pack-entry?game&path&index -- one entry's typed detail
//   GET /api/game-page/pack-entry-data?game&path&index -- one entry's raw bytes
import {
  getGameBuilds,
  getGameCaptures,
  getGameOverview,
  getGameSaveContent,
  getGameSaves,
  getGameStateSchemas,
  getPackDump,
  getPackEntryData,
  getPackEntryDetail,
  listGames,
} from "./ops.mjs";

function serveJson(res, status, value) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(value));
}

// game-keyed JSON endpoints; a null from ops means the game (or pack/entry)
// does not exist.
const GAME_ROUTES = {
  overview: (root, url) => getGameOverview(root, url.searchParams.get("game") || ""),
  builds: (root, url) => getGameBuilds(root, url.searchParams.get("game") || ""),
  state: (root, url) => getGameStateSchemas(root, url.searchParams.get("game") || ""),
  saves: (root, url) => getGameSaves(root, url.searchParams.get("game") || ""),
  save: (root, url) => getGameSaveContent(root, url.searchParams.get("game") || "", url.searchParams.get("slot") || ""),
  captures: (root, url) => getGameCaptures(root, url.searchParams.get("game") || ""),
  pack: (root, url) => getPackDump(root, url.searchParams.get("game") || "", url.searchParams.get("path") || ""),
  "pack-entry": (root, url) => getPackEntryDetail(
    root,
    url.searchParams.get("game") || "",
    url.searchParams.get("path") || "",
    url.searchParams.get("index") || "-1",
  ),
};

export function createGamePageApi(root) {
  return function handleGamePageApi(req, res, url) {
    if (!url.pathname.startsWith("/api/game-page/")) return false;
    if (req.method !== "GET") {
      serveJson(res, 405, { error: "method not allowed" });
      return true;
    }
    const route = url.pathname.slice("/api/game-page/".length);
    try {
      if (route === "games") {
        serveJson(res, 200, listGames(root));
        return true;
      }
      if (route === "pack-entry-data") {
        const data = getPackEntryData(
          root,
          url.searchParams.get("game") || "",
          url.searchParams.get("path") || "",
          url.searchParams.get("index") || "-1",
        );
        if (!data) {
          serveJson(res, 404, { error: "unknown game, pack, or entry" });
        } else {
          res.writeHead(200, {
            "content-type": "application/octet-stream",
            "content-length": String(data.bytes.length),
          });
          res.end(Buffer.from(data.bytes));
        }
        return true;
      }
      const handler = GAME_ROUTES[route];
      if (!handler) {
        serveJson(res, 404, { error: "unknown game-page endpoint" });
        return true;
      }
      const value = handler(root, url);
      if (!value) serveJson(res, 404, { error: "unknown game, pack, or entry" });
      else serveJson(res, 200, value);
    } catch (error) {
      serveJson(res, 500, { error: error?.message || String(error) });
    }
    return true;
  };
}
