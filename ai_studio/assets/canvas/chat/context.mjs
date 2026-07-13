// Chat context digest + per-project chat history store (T0242 increment 1).
//
// Two independent read/write surfaces, BOTH scoped under one project's own store dir
// (never a shell-global chat dir) — the chat history belongs WITH the project it talks
// about, same lifecycle as the rest of <project>/* (move/backup/inspect the project
// folder and its chat comes along):
//
//   1. buildChatContext() — a PURE, READ-ONLY digest over ops.getProject + ops.listHistory:
//      the bounded "what is selected right now" snapshot that rides on the FIRST turn of a
//      codex conversation (chat/agent.mjs embeds it in the prompt). Never the full project
//      JSON — see tmp/design_T0242_chat_panel_2026-07-03.md section 2 for why (selection
//      summary + counts + head in the prompt; full project one CLI `show` call away).
//   2. The chat/ transcript store: <project>/chat/transcript.jsonl (append-only, one line
//      per turn — PANEL DISPLAY ONLY per design R3, the model's actual memory is the codex
//      session, not this file) + <project>/chat/state.json (the one field that matters:
//      the current codex session_id, null before the first turn and after Clear).
//
// Chat NEVER mutates project.json — only its own chat/ subdir — so this module never
// touches the ops.mjs MUTATION surface; every canvas mutation still goes exclusively
// through the CLI (see agent.mjs), keeping tool parity intact. Path confinement reuses
// store.mjs's own resolveProjectPath (re-exported by ops.mjs) — the same segment-
// confinement rule the export/render ops rely on — so a "chat" + filename pair can never
// escape the project's own directory (R2: "any file access outside the project store" is
// exactly what this module itself must never do either).
//
// `root` is threaded as the LEADING argument on every export here, matching the
// ops.mjs/store.mjs convention used throughout this codebase (`ops.getProject(root, id)`,
// `createCanvasApi(root)`, …) — the design doc's abbreviated signatures elide it the same
// way it elides it from `ops.getProject`'s own prose description; CANVAS_PROJECTS_ROOT env
// override reaches this module exactly the way it reaches every other canvas op/test.
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { appendFileSync } from "node:fs";
import { getProject, listHistory, resolveProjectPath } from "../ops.mjs";

const CHAT_DIGEST_SCHEMA = "ai_studio.studio_shell.chat_context.v1";
const STUDIO_STORE_ID = "studio";
// Built from a code point (not a literal BOM character in source) so the file itself stays
// plain ASCII/UTF-8 with no invisible leading byte of its own.
const BOM_RE = new RegExp(`^${String.fromCharCode(0xfeff)}`);

function decorateProjectForChat(project, store) {
  const storeId = store && store.storeId ? store.storeId : STUDIO_STORE_ID;
  return {
    ...project,
    storeId,
    visibility: store && store.visibility ? store.visibility : "public",
    qualifiedId: `${storeId}:${project.id}`,
    gameId: store && store.gameId ? store.gameId : "",
  };
}

function projectRefBase(project) {
  if (project.storeId && project.storeId !== STUDIO_STORE_ID && project.gameId) {
    return { uri: `canvas://game/${project.gameId}/${project.id}`, title: project.title || project.id, private: true };
  }
  return { uri: `canvas://${project.id}`, title: project.title || project.id, private: false };
}

// Mirrors site/context_menu.js's Copy ID formatter (elementRef/copyIdItemFor,
// context_menu.js:97-153) byte-for-byte, so a ref this digest prints and a ref the page's
// own Copy ID pastes are the EXACT same string for the exact same node — one format, two
// producers. `target` is one selection entry in the shape the page's own selection state
// carries: {kind:"element", id} | {kind:"group", id} | {kind:"region", elementId, regionId}.
// Throws loudly on an id that no longer resolves — a stale selection entry is never
// silently dropped, the caller explicitly asked about it.
export function formatSelectionRef(project, target) {
  if (!target || !target.kind) throw new Error(`buildChatContext: selection entry missing kind: ${JSON.stringify(target)}`);
  const base = projectRefBase(project);
  if (target.kind === "element") {
    const element = (project.elements || []).find((item) => item.id === target.id);
    if (!element) throw new Error(`buildChatContext: selection element not found: ${target.id}`);
    const ref = `${base.uri}/element/${element.id}`;
    return {
      ref: base.private ? ref : `${ref} — project "${base.title}", element "${element.name || element.id}"`,
      id: element.id,
      kind: "element",
      type: element.type,
      name: element.name || element.id,
      w: element.w,
      h: element.h,
      groupId: element.groupId == null ? null : element.groupId,
    };
  }
  if (target.kind === "group") {
    const group = (project.groups || []).find((item) => item.id === target.id);
    if (!group) throw new Error(`buildChatContext: selection group not found: ${target.id}`);
    const ref = `${base.uri}/group/${group.id}`;
    return {
      ref: base.private ? ref : `${ref} — project "${base.title}", group "${group.name || "Group"}"`,
      id: group.id,
      kind: "group",
      type: null,
      name: group.name || "Group",
      w: group.w,
      h: group.h,
      groupId: group.parentId == null ? null : group.parentId,
    };
  }
  if (target.kind === "region") {
    const element = (project.elements || []).find((item) => item.id === target.elementId);
    const region = element && (element.regions || []).find((item) => item.id === target.regionId);
    if (!element || !region) {
      throw new Error(`buildChatContext: selection region not found: ${target.elementId}/${target.regionId}`);
    }
    const ref = `${base.uri}/element/${element.id}/region/${region.id}`;
    return {
      ref: base.private
        ? ref
        : `${ref} — project "${base.title}", element "${element.name || element.id}", region "${region.name || region.id}"`,
      id: region.id,
      kind: "region",
      type: null,
      name: region.name || region.id,
      w: null,
      h: null,
      groupId: element.groupId == null ? null : element.groupId,
      elementId: element.id,
    };
  }
  throw new Error(`buildChatContext: unknown selection kind: ${JSON.stringify(target.kind)}`);
}

// The bounded "what is selected right now" snapshot the chat prompt travels with —
// bounded by SELECTION size, never project size (design doc section 2). `selection` is an
// array of targets in the shape formatSelectionRef expects (above); an empty/omitted
// selection is valid ("act on the project as a whole"). `head` is the current journal head
// (listHistory's own value — the T0234 `--expect-head` seed the driving contract points the
// agent at before any undo/redo/history-jump).
export function buildChatContext(root, { projectId, selection, store } = {}) {
  if (!projectId) throw new Error("buildChatContext requires projectId");
  const project = decorateProjectForChat(getProject(root, projectId), store);
  const resolvedSelection = (selection || []).map((target) => formatSelectionRef(project, target));
  const history = listHistory(root, { projectId });
  return {
    schema: CHAT_DIGEST_SCHEMA,
    projectId: project.id,
    title: project.title || project.id,
    storeId: project.storeId,
    visibility: project.visibility,
    qualifiedId: project.qualifiedId,
    gameId: project.gameId,
    selection: resolvedSelection,
    counts: { elements: (project.elements || []).length, groups: (project.groups || []).length },
    head: history.head,
  };
}

// ---- chat/ per-project store: transcript.jsonl (display log) + state.json (session id) --

function chatDir(root, projectId) {
  return resolveProjectPath(root, projectId, "chat");
}

function transcriptPath(root, projectId) {
  return resolveProjectPath(root, projectId, "chat", "transcript.jsonl");
}

function statePath(root, projectId) {
  return resolveProjectPath(root, projectId, "chat", "state.json");
}

// getProject's own "canvas project not found" throw is the existence check here — it runs
// BEFORE any chat/ dir is created, so a chat write against an unknown/mistyped project id
// fails loudly instead of silently minting an orphan folder.
function ensureChatDir(root, projectId) {
  getProject(root, projectId);
  const dir = chatDir(root, projectId);
  mkdirSync(dir, { recursive: true });
  return dir;
}

// Append ONE line to the per-project transcript (panel display log only — see the module
// doc; the model's actual conversation memory is the codex session, not this file). `role`
// is "user" | "assistant"; `seqRange` is the [head_before, head_after] pair an ASSISTANT
// turn produced (omitted on a user turn and on an assistant turn that committed no ops), so
// a later panel reload can render "this reply created ops #12-14" without recomputing
// anything from the journal.
export function appendTurn(root, { projectId, role, text, seqRange, at } = {}) {
  if (role !== "user" && role !== "assistant") {
    throw new Error(`appendTurn requires role "user" or "assistant", got ${JSON.stringify(role)}`);
  }
  if (typeof text !== "string") throw new Error("appendTurn requires text");
  ensureChatDir(root, projectId);
  const line = { at: at || new Date().toISOString(), role, text };
  if (seqRange) line.seqRange = seqRange;
  appendFileSync(transcriptPath(root, projectId), `${JSON.stringify(line)}\n`);
  return line;
}

// Parsed transcript, oldest first. No project-existence check (mirrors store.mjs's own
// readErrors/readJournal — a not-yet-existing sidecar file is just "nothing happened yet",
// not an error) — a torn/partial trailing line (a crash mid-append) is tolerated and
// skipped, same tolerance as those two.
export function readTranscript(root, { projectId } = {}) {
  if (!projectId) throw new Error("readTranscript requires projectId");
  const path = transcriptPath(root, projectId);
  if (!existsSync(path)) return [];
  const out = [];
  for (const line of readFileSync(path, "utf8").replace(BOM_RE, "").split("\n")) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    try {
      out.push(JSON.parse(trimmed));
    } catch {
      // tolerate a torn line — store.mjs's readErrors/readJournal precedent.
    }
  }
  return out;
}

// CLEAR CONTEXT (R1, revised by R3): archive the transcript.jsonl (RENAME, never delete —
// "archives kept, loud never-delete") and reset state.json's session_id to null so the
// conversation's next turn mints a brand new codex session (see agent.mjs). Idempotent when
// there is no transcript yet (nothing to rotate, session_id is reset regardless).
export function clearConversation(root, { projectId } = {}) {
  ensureChatDir(root, projectId);
  const current = transcriptPath(root, projectId);
  let archivedAs = null;
  if (existsSync(current)) {
    const stamp = new Date().toISOString().replace(/[:.]/g, "-");
    archivedAs = resolveProjectPath(root, projectId, "chat", `transcript-${stamp}.jsonl`);
    renameSync(current, archivedAs);
  }
  writeChatState(root, { projectId, sessionId: null });
  return { archivedAs, sessionId: null };
}

// The one field that matters: the current codex session_id (R3 — one chat conversation =
// one codex session). Missing state.json reads as "no conversation yet" (sessionId: null),
// same tolerant-read stance as readTranscript.
export function readChatState(root, { projectId } = {}) {
  if (!projectId) throw new Error("readChatState requires projectId");
  const path = statePath(root, projectId);
  if (!existsSync(path)) return { sessionId: null };
  const parsed = JSON.parse(readFileSync(path, "utf8").replace(BOM_RE, ""));
  return { sessionId: parsed.sessionId == null ? null : parsed.sessionId };
}

export function writeChatState(root, { projectId, sessionId } = {}) {
  ensureChatDir(root, projectId);
  writeFileSync(statePath(root, projectId), `${JSON.stringify({ sessionId: sessionId == null ? null : sessionId }, null, 2)}\n`);
  return { sessionId: sessionId == null ? null : sessionId };
}
