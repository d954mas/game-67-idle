import { createProject as storeCreateProject, deleteProject as storeDeleteProject } from "../store.mjs";

const TITLE_ADJECTIVES = ["Amber", "Blazing", "Crimson", "Drifting", "Emerald", "Frosty", "Golden", "Hidden", "Indigo", "Jade", "Lunar", "Mystic", "Neon", "Obsidian", "Radiant", "Velvet"];
const TITLE_NOUNS = ["Fox", "Griffin", "Nebula", "Phoenix", "Quest", "Raven", "Relic", "Rogue", "Sentinel", "Talisman", "Voyager", "Wraith", "Wyvern", "Citadel", "Compass", "Portal"];

function randomProjectTitle() {
  const adjective = TITLE_ADJECTIVES[Math.floor(Math.random() * TITLE_ADJECTIVES.length)];
  const noun = TITLE_NOUNS[Math.floor(Math.random() * TITLE_NOUNS.length)];
  return `${adjective} ${noun}`;
}

function normalizeProjectGameId(value) {
  if (value === undefined || value === null) throw new Error("Canvas project ownership.gameId must be lowercase kebab-case");
  const text = String(value).trim();
  if (!/^[a-z][a-z0-9-]*$/.test(text)) throw new Error("Canvas project ownership.gameId must be lowercase kebab-case");
  return text;
}

export function normalizeProjectOwnership(value, { allowClear = false } = {}) {
  if (value === undefined) return undefined;
  if (value === null) return allowClear ? undefined : value;
  if (typeof value === "string") {
    const text = value.trim();
    if (allowClear && (!text || text === "none" || text === "null")) return undefined;
    return { kind: "game", gameId: normalizeProjectGameId(text) };
  }
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Canvas project ownership must be an object");
  const kind = String(value.kind || "").trim();
  if (kind !== "game") throw new Error("Canvas project ownership.kind must be game");
  return { kind, gameId: normalizeProjectGameId(value.gameId) };
}

export function createProject(root, { title, ownership, gameId } = {}) {
  const cleanOwnership = normalizeProjectOwnership(ownership ?? (gameId ? { kind: "game", gameId } : undefined));
  const cleanTitle = String(title || "").trim() || randomProjectTitle();
  return storeCreateProject(root, { title: cleanTitle, ownership: cleanOwnership });
}

export function deleteProject(root, { projectId } = {}) {
  // Recoverable folder trash, intentionally outside the per-project journal.
  if (!projectId) throw new Error("deleteProject requires projectId");
  return storeDeleteProject(root, projectId);
}
