import { existsSync } from "node:fs";
import { join } from "node:path";

import { listGameMounts } from "../workspace/games.mjs";
import {
  ACTIVE_TASK_STATUSES,
  agentContextPayload,
  agentTaskRow,
  boardPayload,
  countsByStatus,
  EPIC_STATUSES,
  findDoc,
  idNumber,
  listEpics,
  listProjects,
  listTasks,
  PROJECT_STATUSES,
  publicDoc,
  TASK_STATUSES,
  taskRank,
  validateStoreDetailed,
} from "./store.mjs";

export const STUDIO_STORE_ID = "studio";

export function studioTaskboardStore(root) {
  return {
    storeId: STUDIO_STORE_ID,
    visibility: "public",
    kind: "studio",
    label: "AI Studio",
    itemsRoot: join(root, "ai_studio", "taskboard", "items"),
  };
}

export function taskboardStoreSummary(store) {
  return {
    storeId: store.storeId,
    visibility: store.visibility,
    kind: store.kind,
    gameId: store.gameId || "",
    label: store.label || store.storeId,
  };
}

function gameTaskboardStore(root, mount) {
  return {
    storeId: mount.storeId,
    visibility: mount.visibility,
    kind: "game",
    gameId: mount.gameId,
    label: mount.publicAlias || mount.title || mount.gameId,
    itemsRoot: join(root, mount.root, ".ai_studio", "taskboard", "items"),
  };
}

function mountHasTaskboard(root, mount) {
  return Array.isArray(mount.enabledStores) && mount.enabledStores.includes("taskboard");
}

export function listTaskboardStores(root, options = {}) {
  const stores = [studioTaskboardStore(root)];
  const activeStoreId = options.activeStoreId && options.activeStoreId !== STUDIO_STORE_ID
    ? options.activeStoreId
    : "";
  const activeGameId = options.activeGameId || "";
  const includePrivate = options.includePrivate === true;
  const mounts = listGameMounts(root, {
    includePrivate,
    activeGameId,
    activeStoreId,
  });
  for (const mount of mounts) {
    if (mountHasTaskboard(root, mount)) {
      stores.push(gameTaskboardStore(root, mount));
    }
  }
  return stores;
}

export function selectTaskboardStore(root, options = {}) {
  const gameId = String(options.game || options.activeGameId || "").trim();
  const storeId = String(options.store || options.activeStoreId || "").trim();
  if (gameId && storeId && storeId !== `game:${gameId}`) {
    throw new Error(`--store ${storeId} does not match --game ${gameId}`);
  }
  if (!gameId && (!storeId || storeId === STUDIO_STORE_ID)) {
    return studioTaskboardStore(root);
  }
  const stores = listTaskboardStores(root, {
    activeGameId: gameId,
    activeStoreId: storeId || (gameId ? `game:${gameId}` : ""),
  });
  const selected = stores.find((store) =>
    (storeId && store.storeId === storeId) ||
    (gameId && store.gameId === gameId)
  );
  if (!selected) {
    throw new Error(`No Taskboard store found for ${storeId || `game:${gameId}`}`);
  }
  return selected;
}

export function taskboardStoresForQuery(root, options = {}) {
  if (options.store || options.game || options.activeStoreId || options.activeGameId) {
    return [selectTaskboardStore(root, options)];
  }
  if (options.includePrivate === true) {
    return listTaskboardStores(root, { includePrivate: true });
  }
  return [studioTaskboardStore(root)];
}

export function parseQualifiedItemId(rawId) {
  const text = String(rawId || "").trim();
  const colon = text.lastIndexOf(":");
  if (colon > 0 && /^[PET]\d+$/i.test(text.slice(colon + 1))) {
    return {
      storeId: text.slice(0, colon),
      id: text.slice(colon + 1),
      qualified: true,
    };
  }
  return { storeId: "", id: text, qualified: false };
}

export function storeOptions(store, options = {}) {
  return {
    ...options,
    store,
    itemsRoot: store.itemsRoot,
  };
}

export function mutationStore(root, options = {}) {
  if (options.includePrivate === true && !options.store && !options.game && !options.activeStoreId && !options.activeGameId) {
    throw new Error("Mutations in aggregate context require --store or --game");
  }
  return selectTaskboardStore(root, options);
}

export function findTaskboardDoc(root, rawId, options = {}) {
  const parsed = parseQualifiedItemId(rawId);
  if (!parsed.id) {
    return null;
  }
  if (parsed.qualified) {
    const store = selectTaskboardStore(root, { activeStoreId: parsed.storeId });
    const doc = findDoc(root, parsed.id, storeOptions(store));
    return doc ? { doc, store, id: parsed.id } : null;
  }
  if (options.store || options.game || options.activeStoreId || options.activeGameId) {
    const store = selectTaskboardStore(root, options);
    const doc = findDoc(root, parsed.id, storeOptions(store));
    return doc ? { doc, store, id: parsed.id } : null;
  }
  const stores = taskboardStoresForQuery(root, { includePrivate: options.includePrivate === true });
  const matches = [];
  for (const store of stores) {
    const doc = findDoc(root, parsed.id, storeOptions(store));
    if (doc) matches.push({ doc, store, id: parsed.id });
  }
  if (matches.length > 1) {
    throw new Error(`${parsed.id} is ambiguous across Taskboard stores; use a qualified id like game:<id>:${parsed.id}`);
  }
  return matches[0] || null;
}

export function boardPayloadForStores(root, stores) {
  if (stores.length === 1 && stores[0].storeId === STUDIO_STORE_ID) {
    return boardPayload(root);
  }
  return {
    ...boardPayload(root),
    stores: stores.map(taskboardStoreSummary),
    projects: stores.flatMap((store) =>
      listProjects(root, storeOptions(store)).map((doc) => publicDoc(doc, { store }))
    ),
    epics: stores.flatMap((store) =>
      listEpics(root, storeOptions(store)).map((doc) => publicDoc(doc, { store }))
    ),
    tasks: stores.flatMap((store) =>
      listTasks(root, storeOptions(store)).map((doc) => publicDoc(doc, { store }))
    ),
  };
}

export function agentContextPayloadForStores(root, stores, options = {}) {
  if (stores.length === 1 && stores[0].storeId === STUDIO_STORE_ID) {
    return agentContextPayload(root, options);
  }
  const limit = Number.isFinite(Number(options.limit)) ? Number(options.limit) : 5;
  const projects = stores.flatMap((store) => listProjects(root, storeOptions(store)).map((doc) => ({ doc, store })));
  const epics = stores.flatMap((store) => listEpics(root, storeOptions(store)).map((doc) => ({ doc, store })));
  const tasks = stores.flatMap((store) => listTasks(root, storeOptions(store)).map((doc) => ({ doc, store })));
  const epicsByStore = new Map(stores.map((store) => [
    store.storeId,
    new Map(epics.filter((entry) => entry.store.storeId === store.storeId).map(({ doc }) => [doc.fields.id, doc])),
  ]));
  const currentEntries = tasks
    .filter(({ doc }) => ACTIVE_TASK_STATUSES.includes(doc.fields.status))
    .sort((a, b) =>
      taskRank(a.doc) - taskRank(b.doc) ||
      idNumber(b.doc) - idNumber(a.doc) ||
      a.store.storeId.localeCompare(b.store.storeId) ||
      String(a.doc.fields.id).localeCompare(String(b.doc.fields.id))
    );
  const currentWork = currentEntries.slice(0, limit).map(({ doc, store }) =>
    agentTaskRow(root, doc, { store, epicsById: epicsByStore.get(store.storeId) })
  );
  return {
    schema: "ai_studio.taskboard.agent_context.v1",
    root,
    stores: stores.map(taskboardStoreSummary),
    counts: {
      projects: countsByStatus(projects.map(({ doc }) => doc), PROJECT_STATUSES),
      epics: countsByStatus(epics.map(({ doc }) => doc), EPIC_STATUSES),
      tasks: countsByStatus(tasks.map(({ doc }) => doc), TASK_STATUSES),
      currentWork: currentEntries.length,
      review: tasks.filter(({ doc }) => doc.fields.status === "review").length,
    },
    currentWork,
    agentNextStep: "Open only the task file(s) needed for the current decision; do not scan archives unless linked.",
  };
}

export function validateTaskboardStoresDetailed(root, stores) {
  const problems = stores.flatMap((store) =>
    validateStoreDetailed(root, storeOptions(store)).map((problem) => ({ ...problem, storeId: store.storeId }))
  );
  if (stores.length <= 1) {
    return problems;
  }

  const entries = [];
  for (const store of stores) {
    for (const doc of listProjects(root, storeOptions(store))) entries.push({ store, doc });
    for (const doc of listEpics(root, storeOptions(store))) entries.push({ store, doc });
    for (const doc of listTasks(root, storeOptions(store, { includeArchive: true }))) entries.push({ store, doc });
  }
  const byQualifiedId = new Map(entries.map((entry) => [`${entry.store.storeId}:${entry.doc.fields.id}`, entry]));
  const byBareId = new Map();
  for (const entry of entries) {
    const id = entry.doc.fields.id;
    if (!byBareId.has(id)) byBareId.set(id, []);
    byBareId.get(id).push(entry);
  }

  function add(owner, message) {
    problems.push({
      code: "taskboard_problem",
      message,
      taskId: owner.doc.fields.id,
      storeId: owner.store.storeId,
    });
  }

  function resolveReference(owner, field, expectedKind) {
    const value = String(owner.doc.fields[field] || "").trim();
    if (!value) return null;
    const parsed = parseQualifiedItemId(value);
    if (parsed.qualified) {
      const target = byQualifiedId.get(`${parsed.storeId}:${parsed.id}`);
      if (!target) {
        add(owner, `${owner.doc.fields.id}: ${field} references missing ${expectedKind} "${value}"`);
        return null;
      }
      if (target.doc.kind !== expectedKind) {
        add(owner, `${owner.doc.fields.id}: ${field} references ${target.doc.kind} "${value}", expected ${expectedKind}`);
        return null;
      }
      return target;
    }

    const local = byQualifiedId.get(`${owner.store.storeId}:${value}`);
    if (local && local.doc.kind === expectedKind) return local;
    const crossStoreMatches = (byBareId.get(value) || [])
      .filter((entry) => entry.store.storeId !== owner.store.storeId && entry.doc.kind === expectedKind);
    if (crossStoreMatches.length) {
      add(owner, `${owner.doc.fields.id}: bare cross-store reference "${field}: ${value}" is ambiguous; use ${crossStoreMatches[0].store.storeId}:${value}`);
    }
    return null;
  }

  for (const owner of entries) {
    if (owner.doc.kind === "epic") {
      resolveReference(owner, "project", "project");
    }
    if (owner.doc.kind !== "task") continue;
    const project = resolveReference(owner, "project", "project");
    const epic = resolveReference(owner, "epic", "epic");
    const epicProjectRef = epic ? String(epic.doc.fields.project || "").trim() : "";
    if (project && epicProjectRef) {
      const epicProject = resolveReference(epic, "project", "project");
      if (epicProject && `${project.store.storeId}:${project.doc.fields.id}` !== `${epicProject.store.storeId}:${epicProject.doc.fields.id}`) {
        add(owner, `${owner.doc.fields.id}: project ${owner.doc.fields.project} does not match epic ${owner.doc.fields.epic} project ${epicProject.doc.fields.id}`);
      }
    }
  }

  return problems;
}
