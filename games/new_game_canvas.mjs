import { existsSync, mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

import {
  createProject, deleteProject, getProject, listProjects, patchProject,
} from "../ai_studio/assets/canvas/ops.mjs";
import {
  selectCanvasStore, withCanvasStore,
} from "../ai_studio/assets/canvas/stores.mjs";

const CANVAS_LINK_REL = join("design", "canvas.md");
const PRIVATE_CANVAS_PROJECTS_REL = join(".ai_studio", "canvas", "projects");

function canvasTitle(identity) {
  return identity.title === identity.id
    ? `${identity.id} Canvas`
    : `${identity.title} [${identity.id}] Canvas`;
}

function canvasRef(store, gameId, projectId) {
  return store.storeId === "studio"
    ? `canvas://${projectId}`
    : `canvas://game/${gameId}/${projectId}`;
}

function browserUrl(store, projectId) {
  const query = new URLSearchParams({ project: projectId, store: store.storeId });
  return `http://127.0.0.1:8765/canvas?${query}`;
}

function projectIdFromRef(ref, store, gameId) {
  const prefix = store.storeId === "studio" ? "canvas://" : `canvas://game/${gameId}/`;
  if (!ref.startsWith(prefix)) throw new Error(`game Canvas ref does not belong to ${store.storeId}`);
  const projectId = ref.slice(prefix.length);
  if (!/^[a-z0-9][a-z0-9-]*$/.test(projectId) || projectId.includes("/")) {
    throw new Error("game Canvas ref has an invalid project id");
  }
  return projectId;
}

function assertOwnedProject(project, gameId) {
  if (project.ownership?.kind !== "game" || project.ownership.gameId !== gameId) {
    throw new Error(`Canvas project '${project.id}' is not owned by game '${gameId}'`);
  }
  return project;
}

export function readGameCanvasLink(gameDir) {
  const path = join(gameDir, CANVAS_LINK_REL);
  if (!existsSync(path)) return null;
  const text = readFileSync(path, "utf8");
  const match = /^canvas_ref:\s*(.*?)\s*$/m.exec(text);
  if (!match) throw new Error(`${CANVAS_LINK_REL.replace(/\\/g, "/")} is missing canvas_ref`);
  const ref = match[1].split(/\s+—\s+/, 1)[0].trim();
  if (!ref) throw new Error(`${CANVAS_LINK_REL.replace(/\\/g, "/")} has an empty canvas_ref`);
  return { ref };
}

function writeGameCanvasLink(gameDir, identity, store, project) {
  const ref = canvasRef(store, identity.id, project.id);
  const url = browserUrl(store, project.id);
  const path = join(gameDir, CANVAS_LINK_REL);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, [
    "---",
    "type: Game Canvas",
    `game_id: ${identity.id}`,
    `canvas_ref: ${ref}`,
    `browser_url: ${url}`,
    "---",
    "",
    "# Canvas",
    "",
    `Dedicated Canvas project for **${identity.title}**.`,
    "",
    `- Reference: \`${ref}\``,
    `- Browser: ${url}`,
    "",
  ].join("\n"), "utf8");
  return { ref, browserUrl: url };
}

export function ensureGameCanvasProject(repoRoot, gameDir, identity, visibility, preferredLink = null) {
  const store = visibility === "private"
    ? selectCanvasStore(repoRoot, { game: identity.id })
    : selectCanvasStore(repoRoot);
  const title = canvasTitle(identity);
  return withCanvasStore(store, () => {
    let project = null;
    let mutation = null;
    try {
      if (preferredLink?.ref) {
        project = assertOwnedProject(getProject(repoRoot, projectIdFromRef(preferredLink.ref, store, identity.id)), identity.id);
      } else {
        const matches = listProjects(repoRoot, { includeArchived: true }).filter((candidate) =>
          candidate.title === title
          && candidate.ownership?.kind === "game"
          && candidate.ownership.gameId === identity.id
        );
        if (matches.length > 1) throw new Error(`multiple Canvas projects match game '${identity.id}' and title '${title}'`);
        project = matches[0] || null;
      }
      const created = !project;
      if (created) project = createProject(repoRoot, { title, gameId: identity.id });
      const renamedFromTitle = !created && project.title !== title ? project.title : "";
      mutation = { created, project, store, renamedFromTitle };
      if (renamedFromTitle) {
        // Arm compensation before patchProject: its project write precedes its
        // journal/snapshot writes, so a later I/O failure can still leave title
        // changed even though patchProject did not return.
        mutation.project = { ...project, title };
        project = patchProject(repoRoot, { projectId: project.id, title }).project;
        mutation.project = project;
      }
      const link = writeGameCanvasLink(gameDir, identity, store, project);
      return { ...mutation, ...link };
    } catch (error) {
      if (mutation) {
        try { rollbackGameCanvasMutation(repoRoot, mutation); }
        catch (cleanupError) {
          throw new Error(`${error.message}; Canvas compensation failed: ${cleanupError.message}`);
        }
      }
      throw error;
    }
  });
}

export function rollbackGameCanvasMutation(repoRoot, mutation) {
  if (!mutation) return;
  withCanvasStore(mutation.store, () => {
    if (mutation.created) {
      deleteProject(repoRoot, { projectId: mutation.project.id });
      return;
    }
    if (!mutation.renamedFromTitle) return;
    const current = getProject(repoRoot, mutation.project.id);
    if (current.title === mutation.renamedFromTitle) return;
    if (current.title !== mutation.project.title) {
      throw new Error(`Canvas project '${current.id}' changed after new-game rename; refusing rollback overwrite`);
    }
    patchProject(repoRoot, { projectId: current.id, title: mutation.renamedFromTitle });
  });
}

export function transferPrivateCanvasStore(backupGameDir, publishedGameDir) {
  const source = join(backupGameDir, PRIVATE_CANVAS_PROJECTS_REL);
  if (!existsSync(source)) return null;
  const target = join(publishedGameDir, PRIVATE_CANVAS_PROJECTS_REL);
  rmSync(target, { recursive: true, force: true });
  mkdirSync(dirname(target), { recursive: true });
  renameSync(source, target);
  return { source, target };
}

export function rollbackPrivateCanvasStoreTransfer(transfer) {
  if (!transfer || !existsSync(transfer.target)) return;
  if (existsSync(transfer.source)) {
    throw new Error(`private Canvas rollback target already exists: ${transfer.source}`);
  }
  mkdirSync(dirname(transfer.source), { recursive: true });
  renameSync(transfer.target, transfer.source);
}
