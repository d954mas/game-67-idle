import { existsSync } from "node:fs";
import { join, resolve } from "node:path";

import { validateStyleLockFile } from "./validate.mjs";

export const GENERATION_ORIGIN_SCHEMA = "ai_studio.asset.generation_origin.v1";
const GAME_ID = /^[a-z][a-z0-9-]*$/;

function origin({ mode, gameId = null, styleLockId = null, tainted = false, taintReason = null }) {
  return {
    schema: GENERATION_ORIGIN_SCHEMA,
    source: "ai",
    mode,
    game_id: gameId,
    style_lock_id: styleLockId,
    tainted,
    taint_reason: taintReason,
  };
}

function gameStyleLockPath(root, gameId) {
  const candidates = [
    join(resolve(root), "games", gameId, "design", "style_lock.json"),
    join(resolve(root), "games", "private", gameId, "design", "style_lock.json"),
  ].filter((candidate) => existsSync(candidate));
  if (candidates.length > 1) {
    throw new Error(`production generation found ambiguous public/private style locks for game ${gameId}`);
  }
  return candidates[0] || null;
}

// Resolve one frozen provenance record before any paid/slow generation begins.
// Unowned canvases are deliberate exploration workspaces. Game-owned canvases default to
// production: they need one accepted game lock, unless the caller explicitly chooses the
// review-visible --no-lock escape hatch.
export function resolveGenerationOrigin(root, project, { noLock = false } = {}) {
  if (typeof noLock !== "boolean") throw new Error("generation noLock must be a boolean");
  const ownership = project?.ownership;
  let gameId = null;
  if (ownership !== undefined) {
    if (!ownership || typeof ownership !== "object" || Array.isArray(ownership) || ownership.kind !== "game") {
      throw new Error("generation project ownership must be {kind:game, gameId:<lowercase-slug>}");
    }
    if (!GAME_ID.test(ownership.gameId || "")) {
      throw new Error("generation project ownership.gameId must be a lowercase slug");
    }
    gameId = ownership.gameId;
  }
  if (!gameId) {
    return origin({ mode: "explore", tainted: noLock, taintReason: noLock ? "no-lock" : null });
  }
  if (noLock) {
    return origin({ mode: "explore", gameId, tainted: true, taintReason: "no-lock" });
  }

  const lockPath = gameStyleLockPath(root, gameId);
  if (!lockPath) {
    throw new Error(
      `production generation requires an accepted style_lock.json for game ${gameId}; create/accept it or rerun with --no-lock for tainted explore output`,
    );
  }
  const lock = validateStyleLockFile(lockPath, { workspaceRoot: root });
  if (lock.game_id !== gameId) throw new Error("generation style lock game_id must match Canvas project ownership.gameId");
  if (lock.status !== "accepted") {
    throw new Error(`production generation requires an accepted style lock for game ${gameId}; current status is ${lock.status}`);
  }
  return origin({ mode: "production", gameId, styleLockId: lock.id });
}
