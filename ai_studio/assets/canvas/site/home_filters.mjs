import { ALL_STORES_ID, STUDIO_STORE_ID, projectStoreId } from "./store_scope.js";

function ownerGame(project) {
  if (project?.ownership?.kind === "game") return String(project.ownership.gameId || "");
  const storeId = projectStoreId(project);
  return storeId.startsWith("game:") ? storeId.slice("game:".length) : "";
}

function inStore(project, storeId) {
  return storeId === ALL_STORES_ID || projectStoreId(project) === storeId;
}

export function ownerGameOptions(projects, storeId = ALL_STORES_ID) {
  return [...new Set((projects || [])
    .filter((project) => inStore(project, storeId))
    .map(ownerGame)
    .filter(Boolean))]
    .sort((left, right) => left.localeCompare(right));
}

export function filterHomeProjects(projects, { storeId = ALL_STORES_ID, ownerGame: selectedOwner = "" } = {}) {
  return (projects || []).filter((project) =>
    inStore(project, storeId) && (!selectedOwner || ownerGame(project) === selectedOwner)
  );
}

export function homeCreationStoreId(projects, { storeId = ALL_STORES_ID, ownerGame: selectedOwner = "" } = {}) {
  if (storeId !== ALL_STORES_ID) return storeId;
  if (!selectedOwner) return STUDIO_STORE_ID;
  const ownerStores = [...new Set(filterHomeProjects(projects, { ownerGame: selectedOwner }).map(projectStoreId))];
  return ownerStores.length === 1 ? ownerStores[0] : null;
}

export function projectLifecycleLabel(project) {
  return project?.archived === true ? "Archived" : "Active";
}
