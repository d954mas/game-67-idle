import { existsSync } from "node:fs";
import { isAbsolute, join, resolve, sep } from "node:path";

import { canvasProjectsRoot } from "./config.mjs";
import { listGameMounts } from "../../workspace/games.mjs";
import { resolveProjectPath, withCanvasProjectsRoot } from "./store.mjs";

export const STUDIO_CANVAS_STORE_ID = "studio";

export function studioCanvasStore(root) {
  return {
    storeId: STUDIO_CANVAS_STORE_ID,
    visibility: "public",
    kind: "studio",
    label: "AI Studio",
    projectsRoot: canvasProjectsRoot(root),
  };
}

export function canvasStoreSummary(store) {
  return {
    storeId: store.storeId,
    visibility: store.visibility,
    kind: store.kind,
    gameId: store.gameId || "",
    label: store.label || store.storeId,
  };
}

function gameCanvasStore(root, mount) {
  return {
    storeId: mount.storeId,
    visibility: mount.visibility,
    kind: "game",
    gameId: mount.gameId,
    label: mount.publicAlias || mount.title || mount.gameId,
    projectsRoot: join(root, mount.root, ".ai_studio", "canvas", "projects"),
    gameRoot: join(root, mount.root),
  };
}

function mountHasCanvas(mount) {
  return Array.isArray(mount.enabledStores) && mount.enabledStores.includes("canvas");
}

export function listCanvasStores(root, options = {}) {
  const stores = [studioCanvasStore(root)];
  const activeStoreId = options.activeStoreId && options.activeStoreId !== STUDIO_CANVAS_STORE_ID
    ? options.activeStoreId
    : "";
  const activeGameId = options.activeGameId || "";
  const mounts = listGameMounts(root, {
    includePrivate: options.includePrivate === true,
    activeGameId,
    activeStoreId,
  });
  for (const mount of mounts) {
    if (mountHasCanvas(mount)) {
      stores.push(gameCanvasStore(root, mount));
    }
  }
  return stores;
}

export function selectCanvasStore(root, options = {}) {
  const gameId = String(options.game || options.activeGameId || "").trim();
  const storeId = String(options.store || options.activeStoreId || "").trim();
  if (gameId && storeId) {
    const mount = listGameMounts(root, { activeGameId: gameId, activeStoreId: storeId })
      .find((entry) => entry.gameId === gameId);
    if (mount && storeId !== mount.storeId) {
      throw new Error(`--store ${storeId} does not match --game ${gameId}`);
    }
  }
  if (!gameId && (!storeId || storeId === STUDIO_CANVAS_STORE_ID)) {
    return studioCanvasStore(root);
  }
  const stores = listCanvasStores(root, {
    activeGameId: gameId,
    activeStoreId: storeId || (gameId ? `game:${gameId}` : ""),
  });
  const selected = stores.find((store) =>
    (storeId && store.storeId === storeId) ||
    (gameId && store.gameId === gameId)
  );
  if (!selected) {
    throw new Error(`No Canvas store found for ${storeId || `game:${gameId}`}`);
  }
  return selected;
}

export function canvasStoresForQuery(root, options = {}) {
  if (options.store || options.game || options.activeStoreId || options.activeGameId) {
    return [selectCanvasStore(root, options)];
  }
  if (options.includePrivate === true) {
    return listCanvasStores(root, { includePrivate: true });
  }
  return [studioCanvasStore(root)];
}

export function canvasStoreArgs(flags = {}) {
  return {
    store: typeof flags.store === "string" ? flags.store : "",
    game: typeof flags.game === "string" ? flags.game : "",
    includePrivate: flags["include-private"] === "true" || flags["include-private"] === true,
  };
}

export function withCanvasStore(store, fn) {
  return withCanvasProjectsRoot(store.projectsRoot, fn);
}

export function decorateCanvasProject(project, store) {
  return {
    ...project,
    storeId: store.storeId,
    visibility: store.visibility,
    qualifiedId: `${store.storeId}:${project.id}`,
  };
}

export function assertBareCanvasProjectIdIsUnambiguous(root, projectId) {
  if (!projectId) return;
  const matches = [];
  for (const store of listCanvasStores(root, { includePrivate: true })) {
    const exists = withCanvasStore(store, () => existsSync(resolveProjectPath(root, projectId, "project.json")));
    if (exists) matches.push(store);
  }
  if (matches.length === 0) return;
  if (matches.length === 1 && matches[0].storeId === STUDIO_CANVAS_STORE_ID) return;
  if (matches.length === 1) {
    throw new Error("Canvas project id belongs to a non-studio store; pass --store or --game");
  }
  throw new Error("Canvas project id is ambiguous across mounted stores; pass --store or --game");
}

function pathIsInside(parent, child) {
  const normalize = (value) => {
    const resolved = resolve(value);
    return process.platform === "win32" ? resolved.toLowerCase() : resolved;
  };
  const p = normalize(parent);
  const c = normalize(child);
  return c === p || c.startsWith(p + sep);
}

export function assertCanvasExportDestination(root, store, destination) {
  if (!destination || store.visibility === "public") return;
  const absolute = isAbsolute(destination) ? resolve(destination) : resolve(process.cwd(), destination);
  if (!pathIsInside(root, absolute)) return;
  if (store.gameRoot && pathIsInside(store.gameRoot, absolute)) return;
  throw new Error(
    `private Canvas export for ${store.storeId} cannot be copied to a parent Studio path; export inside the owning game store or outside the parent repository`,
  );
}
