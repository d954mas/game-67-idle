import { existsSync, readFileSync, realpathSync } from "node:fs";
import { dirname, isAbsolute, join, resolve } from "node:path";

const FEATURE_WORKSPACE_SCHEMA = "ai_studio.feature_workspace.v1";

function comparable(path) {
  const text = realpathSync.native(path).replace(/\\/g, "/").replace(/\/+$/, "");
  return process.platform === "win32" ? text.toLowerCase() : text;
}

// The save root a feature workspace records beside its Studio worktree, or null
// for any other checkout. The record must name this very worktree, the same
// rule templates/template/cmake/GameWorkspaceStorage.cmake applies, so a stray
// workspace.json above an ordinary checkout never redirects its saves.
export function workspaceStorageRootFor(studioRoot) {
  try {
    const root = resolve(studioRoot);
    const recordPath = join(dirname(root), "workspace.json");
    if (!existsSync(recordPath)) return null;
    const record = JSON.parse(readFileSync(recordPath, "utf8"));
    if (record?.schema !== FEATURE_WORKSPACE_SCHEMA) return null;
    if (typeof record.storageRoot !== "string" || !isAbsolute(record.storageRoot)) return null;
    if (typeof record.studioWorktree !== "string" || !existsSync(record.studioWorktree)) return null;
    return comparable(record.studioWorktree) === comparable(root) ? record.storageRoot : null;
  } catch {
    return null;
  }
}
