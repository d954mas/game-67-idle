// Game Page HTTP API adapter. Studio Shell mounts this on /api/game-page/.
// Marshals HTTP <-> ops.mjs only; no game logic lives here.
//   GET /api/game-page/games              -- all games, private included
//   GET /api/game-page/overview?game=<id> -- one game's overview header data
//   GET /api/game-page/builds?game=<id>   -- build configs, packs, release artifacts
//   GET /api/game-page/pack?game=<id>&path=<rel> -- parsed ntpack contents
import { getGameBuilds, getGameOverview, getPackDump, listGames } from "./ops.mjs";

function serveJson(res, status, value) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(value));
}

export function createGamePageApi(root) {
  return function handleGamePageApi(req, res, url) {
    if (!url.pathname.startsWith("/api/game-page/")) return false;
    if (req.method !== "GET") {
      serveJson(res, 405, { error: "method not allowed" });
      return true;
    }
    try {
      if (url.pathname === "/api/game-page/games") {
        serveJson(res, 200, listGames(root));
        return true;
      }
      if (url.pathname === "/api/game-page/builds") {
        const builds = getGameBuilds(root, url.searchParams.get("game") || "");
        if (!builds) serveJson(res, 404, { error: "unknown game" });
        else serveJson(res, 200, builds);
        return true;
      }
      if (url.pathname === "/api/game-page/pack") {
        const dump = getPackDump(root, url.searchParams.get("game") || "", url.searchParams.get("path") || "");
        if (!dump) serveJson(res, 404, { error: "unknown game or pack" });
        else serveJson(res, 200, dump);
        return true;
      }
      if (url.pathname === "/api/game-page/overview") {
        const overview = getGameOverview(root, url.searchParams.get("game") || "");
        if (!overview) serveJson(res, 404, { error: "unknown game" });
        else serveJson(res, 200, overview);
        return true;
      }
      serveJson(res, 404, { error: "unknown game-page endpoint" });
    } catch (error) {
      serveJson(res, 500, { error: error?.message || String(error) });
    }
    return true;
  };
}
