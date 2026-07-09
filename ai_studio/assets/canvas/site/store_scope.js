export const STUDIO_STORE_ID = "studio";
export const LAST_PROJECT_SCHEMA = "ai_studio.canvas.last_project.v2";

export function normalizeStoreId(storeId) {
  const value = String(storeId || "").trim();
  return value || STUDIO_STORE_ID;
}

export function projectStoreId(projectOrStoreId) {
  if (projectOrStoreId && typeof projectOrStoreId === "object") {
    return normalizeStoreId(projectOrStoreId.storeId);
  }
  return normalizeStoreId(projectOrStoreId);
}

export function gameIdFromStoreId(storeId) {
  const value = normalizeStoreId(storeId);
  return value.startsWith("game:") ? value.slice("game:".length) : "";
}

export function isStudioStore(storeId) {
  return normalizeStoreId(storeId) === STUDIO_STORE_ID;
}

export function storeIdFromParams(params) {
  const store = params.get("store");
  if (store) return normalizeStoreId(store);
  const game = params.get("game");
  return game ? `game:${game}` : STUDIO_STORE_ID;
}

export function setStoreParams(params, storeId) {
  if (isStudioStore(storeId)) {
    params.delete("store");
    params.delete("game");
    return params;
  }
  params.set("store", normalizeStoreId(storeId));
  params.delete("game");
  return params;
}

export function appendStoreQuery(path, storeId) {
  const store = normalizeStoreId(storeId);
  if (isStudioStore(store)) return path;
  const separator = path.includes("?") ? "&" : "?";
  return `${path}${separator}store=${encodeURIComponent(store)}`;
}

export function canvasApiUrl(path, storeId) {
  return `/api/canvas${appendStoreQuery(path, storeId)}`;
}

export function projectFileUrl(project, src) {
  return canvasApiUrl(`/projects/${project.id}/${src}`, projectStoreId(project));
}

export function projectCacheKey(project, src) {
  return `${projectStoreId(project)}:${project.id}:${src}`;
}

export function projectKey(projectOrId, storeId) {
  const projectId = typeof projectOrId === "object" ? projectOrId.id : projectOrId;
  const store = typeof projectOrId === "object" ? projectStoreId(projectOrId) : projectStoreId(storeId);
  return `${store}:${projectId || ""}`;
}

export function encodeLastProject(projectOrId, storeId) {
  if (!projectOrId) return "";
  const projectId = typeof projectOrId === "object" ? projectOrId.id : projectOrId;
  if (!projectId) return "";
  const store = typeof projectOrId === "object" ? projectStoreId(projectOrId) : projectStoreId(storeId);
  return JSON.stringify({ schema: LAST_PROJECT_SCHEMA, storeId: store, projectId });
}

export function decodeLastProject(value) {
  if (!value) return null;
  try {
    const parsed = JSON.parse(value);
    if (parsed && parsed.schema === LAST_PROJECT_SCHEMA && parsed.projectId) {
      return { storeId: projectStoreId(parsed.storeId), projectId: String(parsed.projectId) };
    }
  } catch {
    // Legacy v1 stored only the public project id as plain text.
  }
  return { storeId: STUDIO_STORE_ID, projectId: String(value) };
}

export function canvasRefBase(project) {
  const gameId = gameIdFromStoreId(projectStoreId(project));
  if (gameId) return { uri: `canvas://game/${gameId}/${project.id}`, private: true, title: project.title || project.id };
  return { uri: `canvas://${project.id}`, private: false, title: project.title || project.id };
}
