// Canvas history operation domain. Public API is ../ops.mjs.
import { randomUUID } from "node:crypto";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { performance } from "node:perf_hooks";
import { buildNodesSpec, descendantsOf, frontOrder } from "../tree.mjs";
import { appendJournal, getProject, projectExists, readErrors, readJournal, resolveProjectFile, updateProject } from "../store.mjs";
import { commitMutation, finite, groupsOf, isMutation, ms, snapshotForEntry } from "./core.mjs";
import { addImage, addImages, addText, alignNodes, distributeNodes, findNode, moveNodes, patchElement, patchElements, patchProject, removeElement, removeElements, reorderElement, reorderNode, reorderNodes, setExportSettings, setRegions } from "./elements.mjs";
import { applyStyleAutoRef, assignToGroup, createGroup, deleteGroup, findGroup, fitGroup, patchGroup, patchGroups, reparentGroup, scaleGroup, ungroupGroup } from "./groups.mjs";
import { animateElementFromText, createAnimCard, createRecipeCard, createStyleCard, expandRecipePrompt, extractFromElement, generateAnimFromCard, generateFromRecipe, patchAnim, patchRecipe, patchStyle, promoteExtractedRecipe, promoteExtractedStyle } from "./generation.mjs";

// Clipboard paste/duplicate/delete and history navigation are journal-domain
// operations: specs validate atomically, fresh ids are minted server-side, and
// each visible gesture produces one reversible entry.

export function pasteNodes(root, { projectId, spec, dx, dy, scopeId } = {}) {
  if (!projectId) throw new Error("pasteNodes requires projectId");
  if (!spec || typeof spec !== "object" || Array.isArray(spec)) throw new Error("pasteNodes requires a spec object");
  const nodes = spec.nodes;
  if (!Array.isArray(nodes) || !nodes.length) throw new Error("pasteNodes spec has no nodes to paste");
  const offX = dx === undefined || dx === null || dx === "" ? 0 : Number(dx);
  const offY = dy === undefined || dy === null || dy === "" ? 0 : Number(dy);
  if (!Number.isFinite(offX) || !Number.isFinite(offY)) throw new Error("pasteNodes dx/dy must be finite numbers");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const scope = scopeId == null || scopeId === "" ? null : String(scopeId);
  if (scope != null) findGroup(before, scope); // loud on an unknown destination scope

  // Validate the WHOLE spec loudly before any mint/write (atomic): structure + every
  // image file ref must resolve in this project's immutable files/.
  const validateNode = (node, path) => {
    if (!node || typeof node !== "object") throw new Error(`paste node ${path} is not an object`);
    if (node.kind === "element") {
      const def = node.element;
      if (!def || typeof def !== "object") throw new Error(`paste node ${path} has no element def`);
      if (!Number.isFinite(Number(def.x)) || !Number.isFinite(Number(def.y))) {
        throw new Error(`paste node ${path} element has non-finite x/y`);
      }
      if ((def.type || "image") === "image") {
        if (!def.src || typeof def.src !== "string") throw new Error(`paste node ${path} image element has no src`);
        if (!existsSync(resolveProjectFile(root, projectId, def.src))) {
          throw new Error(`paste node ${path} references an unknown file: ${def.src}`);
        }
      }
    } else if (node.kind === "group") {
      if (!node.group || typeof node.group !== "object") throw new Error(`paste node ${path} has no group def`);
      const children = node.children || [];
      if (!Array.isArray(children)) throw new Error(`paste node ${path} group children must be an array`);
      children.forEach((child, index) => validateNode(child, `${path}.${index}`));
    } else {
      throw new Error(`paste node ${path} has unknown kind: ${JSON.stringify(node.kind)}`);
    }
  };
  nodes.forEach((node, index) => validateNode(node, String(index)));

  const newElements = [];
  const newGroups = [];
  // Pointer-remap maps (T0239-3 fix): original spec id -> freshly-minted paste id, for
  // every node this paste actually instantiates. buildNodesSpec (tree.mjs) now KEEPS each
  // node's original id in the spec (`def.id`/`gdef.id` below) purely so it can be captured
  // HERE before instantiate overwrites it with a fresh one (`{...def, id: fresh}` always
  // wins) — every minted node still gets a brand-new id exactly as before. A hand-authored
  // spec (CLI/API JSON with no `id` fields) simply never populates these maps, so every
  // pointer normalized below falls through to the "not found" branch — never a throw.
  const elementIdMap = new Map(); // old element id -> new element id
  const groupIdMap = new Map(); // old group id -> new group id
  const instantiate = (node, parentScope, orderVal) => {
    if (node.kind === "element") {
      const def = JSON.parse(JSON.stringify(node.element)); // isolate from the (page-held) spec
      const oldId = typeof def.id === "string" ? def.id : null;
      const rec = { ...def, id: `el_${randomUUID().slice(0, 8)}`, x: Number(def.x) + offX, y: Number(def.y) + offY };
      delete rec.order;
      if (parentScope != null) rec.groupId = parentScope;
      else delete rec.groupId;
      if (orderVal != null) rec.order = orderVal;
      newElements.push(rec);
      if (oldId) elementIdMap.set(oldId, rec.id);
      return { kind: "element", id: rec.id };
    }
    const gdef = JSON.parse(JSON.stringify(node.group));
    const oldGroupId = typeof gdef.id === "string" ? gdef.id : null;
    const grec = { ...gdef, id: `grp_${randomUUID().slice(0, 8)}`, x: Number(gdef.x || 0) + offX, y: Number(gdef.y || 0) + offY };
    delete grec.parentId;
    delete grec.order;
    if (parentScope != null) grec.parentId = parentScope;
    if (orderVal != null) grec.order = orderVal;
    newGroups.push(grec);
    if (oldGroupId) groupIdMap.set(oldGroupId, grec.id);
    // A fresh pasted scope: assign contiguous 0..N-1 order so internal z-order is exact.
    (node.children || []).forEach((child, index) => instantiate(child, grec.id, index));
    return { kind: "group", id: grec.id };
  };

  let fo = frontOrder(before, scope); // null on a never-reordered (implicit) destination
  const roots = nodes.map((node) => instantiate(node, scope, fo === null ? null : fo++));

  // Pointer normalization (T0239-3 fix): a pasted style/recipe card's blob may carry a bare
  // element/group id POINTER captured verbatim from the spec (style.ref -> an element id;
  // recipe.style_ref -> a style-card group id) — those name the ORIGINAL nodes, not this
  // paste's fresh copies, so every pointer on a NEWLY PASTED group is normalized here, BEFORE
  // applyStyleAutoRef runs (so a pointer cleared to null can still be auto-claimed by a
  // pasted member image in the SAME gesture). Existing (pre-paste) groups are untouched —
  // nothing about a paste invalidates a pointer that was already valid before it ran.
  //  - style.ref: remapped via elementIdMap when the ref element was ALSO pasted this
  //    gesture (a whole-card copy) — the copy's ref can then only ever name one of ITS OWN
  //    copied members, never the original card's. Otherwise NULLED (it can never resolve to
  //    a member of the new card — dangling on a member that never came along).
  //  - recipe.style_ref: remapped via groupIdMap when the linked style card was ALSO pasted
  //    this gesture (recipe + its style card copied together). Otherwise KEPT AS-IS when it
  //    still resolves to an existing style-card group IN THIS PROJECT — a same-project alias
  //    to a SHARED style card is legitimate and desirable (R1's "one style card, many
  //    recipes" reuse). Otherwise NULLED (a cross-project dangling pointer is never silently
  //    kept — R1's cross-canvas reuse is copy-the-whole-card, not a stale cross-project id).
  const newGroupIds = new Set(newGroups.map((group) => group.id));
  const liveStyleCardIds = new Set(
    groupsOf(before)
      .filter((group) => group.style && typeof group.style === "object")
      .map((group) => group.id),
  );
  const normalizedGroups = [...groupsOf(before), ...newGroups].map((group) => {
    if (!newGroupIds.has(group.id)) return group;
    let next = group;
    if (next.style && typeof next.style === "object" && next.style.ref) {
      const remapped = elementIdMap.get(next.style.ref) || null;
      if (remapped !== next.style.ref) next = { ...next, style: { ...next.style, ref: remapped } };
    }
    if (next.recipe && typeof next.recipe === "object" && next.recipe.style_ref) {
      const remapped = groupIdMap.get(next.recipe.style_ref);
      const kept = remapped || (liveStyleCardIds.has(next.recipe.style_ref) ? next.recipe.style_ref : null);
      if (kept !== next.recipe.style_ref) next = { ...next, recipe: { ...next.recipe, style_ref: kept } };
    }
    return next;
  });

  // Auto-ref (R1 increment 3, applyStyleAutoRef — style-card section): any IMAGE minted by
  // this paste that lands as a member of a STYLE CARD whose ref is still null (including one
  // just nulled by the pointer normalization above) claims that ref — covers BOTH pasting
  // straight into an EXISTING style card (scope targets it) and a freshly-pasted style card
  // whose own copied members include one. Folded into this commit.
  const membershipChanges = new Map(
    newElements.map((element) => [element.id, { groupId: element.groupId ?? null, type: element.type }]),
  );
  const nextGroups = applyStyleAutoRef(normalizedGroups, membershipChanges);

  const after = updateProject(root, projectId, {
    elements: [...(before.elements || []), ...newElements],
    groups: nextGroups,
  });
  const project = commitMutation(root, projectId, {
    op: "pasteNodes",
    args_summary: {
      count: roots.length,
      scope,
      nodeIds: roots.map((node) => node.id),
      elements: newElements.length,
      groups: newGroups.length,
    },
    before,
    after,
    startedAt,
  });
  const elementIds = roots.filter((node) => node.kind === "element").map((node) => node.id);
  const groupIds = roots.filter((node) => node.kind === "group").map((node) => node.id);
  return { project, nodeIds: roots.map((node) => node.id), elementIds, groupIds, count: roots.length };
}

// Duplicate LIVE nodes in place (+offset) in ONE journaled gesture — the page's Ctrl+D and
// the CLI nodes-duplicate convenience. Builds the spec from the current project (pure
// tree.buildNodesSpec, throws on an unknown id) and delegates to pasteNodes (so it stays
// ONE op = one undo). Default offset +16,+16; default destination = the originals' COMMON
// scope (a duplicate lands beside its source), overridable by an explicit scopeId
// (including null for root).
export function duplicateNodes(root, { projectId, nodeIds, dx, dy, scopeId } = {}) {
  if (!projectId) throw new Error("duplicateNodes requires projectId");
  if (!Array.isArray(nodeIds) || !nodeIds.length) throw new Error("duplicateNodes requires a non-empty nodeIds array");
  const before = getProject(root, projectId);
  const spec = buildNodesSpec(before, nodeIds); // throws on an unknown id
  const offX = dx === undefined || dx === null || dx === "" ? 16 : Number(dx);
  const offY = dy === undefined || dy === null || dy === "" ? 16 : Number(dy);
  let scope = scopeId;
  if (scope === undefined) {
    const scopes = new Set(nodeIds.map((id) => {
      const node = findNode(before, id);
      return node.scopeId == null ? null : node.scopeId;
    }));
    scope = scopes.size === 1 ? [...scopes][0] : null;
  }
  return pasteNodes(root, { projectId, spec, dx: offX, dy: offY, scopeId: scope });
}

// Delete a MIXED set of nodes (loose elements AND whole group subtrees) in ONE journaled
// gesture — the page's Delete key on a multi-group or mixed selection, and the CLI
// nodes-delete. A group id deletes its FULL closure (nested subgroups AND every element in
// the subtree), de-duplicated against any directly-selected member. Loud + atomic: an
// unknown id throws before any write; an empty list is a loud error (the page only calls
// this for 2+ selections). One commitMutation, so a single undo deep-restores every group
// and element at its exact z-slot (the before/after snapshot). Backing image files stay in
// files/ (non-destructive storage).
export function deleteNodes(root, { projectId, nodeIds } = {}) {
  if (!projectId) throw new Error("deleteNodes requires projectId");
  if (!Array.isArray(nodeIds) || !nodeIds.length) throw new Error("deleteNodes requires a non-empty nodeIds array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const ids = [...new Set(nodeIds.map((value) => String(value)))];
  const removedGroupIds = new Set();
  const removedElementIds = new Set();
  for (const id of ids) {
    const node = findNode(before, id); // throws on an unknown id (atomic)
    if (node.kind === "element") {
      removedElementIds.add(id);
    } else {
      removedGroupIds.add(id);
      const descendants = descendantsOf(before, id);
      for (const group of descendants.groups) removedGroupIds.add(group.id);
      for (const element of descendants.elements) removedElementIds.add(element.id);
    }
  }
  const nextGroups = groupsOf(before).filter((group) => !removedGroupIds.has(group.id));
  const nextElements = (before.elements || []).filter((element) => !removedElementIds.has(element.id));
  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "deleteNodes",
    args_summary: { nodeIds: ids, deletedGroups: [...removedGroupIds], deletedElements: [...removedElementIds] },
    before,
    after,
    startedAt,
  });
  return { project, removedGroups: [...removedGroupIds], removedElements: [...removedElementIds], nodeIds: ids };
}

// ---- undo / redo / history ---------------------------------------------------

// Agent-side concurrency guard (T0234, after the 2026-07-03 history-jump race: an
// agent read a project at head 823, the lead kept working live to head 876, and the
// agent's jump forked the spine and orphaned the lead's newest entries). undoOp/
// redoOp/jumpHistory accept an optional `expectHead` the caller reads BEFORE calling
// (from listHistory/history-list); when given, it must still match the ACTUAL current
// head or the call refuses LOUDLY before any write. `expectHead` may arrive as a
// string from the CLI, so it is coerced with Number() and validated as a finite
// integer here (a non-numeric value is its own loud error). Absent/null/"" means no
// guard was requested — the page path today, byte-identical to pre-T0234 behavior.
export function checkExpectHead(expectHead, head) {
  if (expectHead === undefined || expectHead === null || expectHead === "") return;
  const expected = Number(expectHead);
  if (!Number.isFinite(expected) || !Number.isInteger(expected)) {
    throw new Error(`expectHead must be a finite integer, got ${JSON.stringify(expectHead)}`);
  }
  if (expected !== head) {
    const error = new Error(
      `history advanced: head is now ${head}, you read ${expected} — the project is live; re-read history (history-list) and retry`,
    );
    // T0254 Tier 1 #2: a stable marker (not prose-matching) so api.mjs's
    // statusForError maps this to 409, same family as commitMutation's own
    // stale-before refusal above.
    error.code = "HEAD_CONFLICT";
    throw error;
  }
}

export function undoOp(root, { projectId, expectHead } = {}) {
  if (!projectId) throw new Error("undoOp requires projectId");
  const startedAt = performance.now();
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
  checkExpectHead(expectHead, head);
  if (!head) throw new Error("nothing to undo");
  const entry = readJournal(root, projectId).find((item) => Number(item.seq) === head && isMutation(item));
  // The head entry can be absent once history has been compacted past this point
  // (its parent was rebased to 0), so undo bottoms out cleanly at the horizon.
  if (!entry) throw new Error("nothing to undo");
  const undoPatch = snapshotForEntry(root, projectId, entry).undo_patch || {};
  const restore = {
    elements: undoPatch.elements || [],
    groups: undoPatch.groups || [],
    tool_runs: undoPatch.tool_runs || [],
    history_seq: Number(entry.parent) || 0,
  };
  // Older snapshots predate title-in-snapshot; only restore title when present so
  // updateProject never clobbers the live title with undefined.
  if (undoPatch.title !== undefined) restore.title = undoPatch.title;
  if (Object.hasOwn(undoPatch, "ownership")) {
    restore.ownership = undoPatch.ownership === null ? undefined : undoPatch.ownership;
  }
  if (Object.hasOwn(undoPatch, "archived")) restore.archived = undoPatch.archived === true;
  const saved = updateProject(root, projectId, restore);
  appendJournal(root, projectId, { op: "undo", target_seq: head, duration_ms: ms(performance.now() - startedAt) });
  return { project: saved, undone_seq: head, history_seq: saved.history_seq };
}

export function redoOp(root, { projectId, expectHead } = {}) {
  if (!projectId) throw new Error("redoOp requires projectId");
  const startedAt = performance.now();
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
  checkExpectHead(expectHead, head);
  const candidates = readJournal(root, projectId).filter((item) => isMutation(item) && (Number(item.parent) || 0) === head);
  if (!candidates.length) throw new Error("nothing to redo");
  const entry = candidates.reduce((best, item) => (Number(item.seq) > Number(best.seq) ? item : best));
  const state = snapshotForEntry(root, projectId, entry).state || {};
  const restore = {
    elements: state.elements || [],
    groups: state.groups || [],
    tool_runs: state.tool_runs || [],
    history_seq: Number(entry.seq),
  };
  if (state.title !== undefined) restore.title = state.title;
  if (Object.hasOwn(state, "ownership")) {
    restore.ownership = state.ownership === null ? undefined : state.ownership;
  }
  if (Object.hasOwn(state, "archived")) restore.archived = state.archived === true;
  const saved = updateProject(root, projectId, restore);
  appendJournal(root, projectId, { op: "redo", target_seq: entry.seq, duration_ms: ms(performance.now() - startedAt) });
  return { project: saved, redone_seq: entry.seq, history_seq: saved.history_seq };
}

// Compact journal view for `history <id>` / GET .../history: seq, timestamp, op,
// the small args_summary (or marker target), and duration_ms — never the big
// snapshots (thin lines carry no snapshot, so this is a cheap metadata scan). Back-
// compatible: canUndo/canRedo are computed from metadata only (no snapshot loads)
// and existing fields are unchanged; duration_ms is added, never removed/renamed.
// Undo/redo availability for a project from its history head + journal: canUndo iff
// the head entry (seq === history_seq) is a live mutation; canRedo iff any mutation's
// parent is the current head. Shared by readHistory (full view) and historyFlags (the
// lightweight summary folded into mutation responses).
export function historyAvailability(project, journal) {
  const head = Number(project.history_seq) || 0;
  const canUndo = head > 0 && journal.some((item) => Number(item.seq) === head && isMutation(item));
  const canRedo = journal.some((item) => isMutation(item) && (Number(item.parent) || 0) === head);
  return { seq: head, canUndo, canRedo };
}

// The undo/redo summary { seq, canUndo, canRedo } for a project — the same flags
// readHistory computes, minus the (capped) entries list. Cheap on the thin journal,
// and folded into every mutating API response (api.mjs sendMutation) so the page
// updates state.history straight from the op result with ZERO extra /history GET.
export function historyFlags(root, { projectId } = {}) {
  if (!projectId) throw new Error("historyFlags requires projectId");
  return historyAvailability(getProject(root, projectId), readJournal(root, projectId));
}

export function readHistory(root, { projectId } = {}) {
  if (!projectId) throw new Error("readHistory requires projectId");
  const project = getProject(root, projectId);
  const journal = readJournal(root, projectId);
  const { seq: head, canUndo, canRedo } = historyAvailability(project, journal);
  const entries = journal.map((item) => ({
    seq: item.seq,
    at: item.at,
    op: item.op,
    ...(item.args_summary ? { args_summary: item.args_summary } : {}),
    ...(item.target_seq !== undefined ? { target_seq: item.target_seq } : {}),
    ...(item.duration_ms !== undefined ? { duration_ms: item.duration_ms } : {}),
  }));
  return { history_seq: head, canUndo, canRedo, entries };
}

// Per-op timing rollup for `ops-stats <id>` / GET .../ops-stats: from the thin
// journal, group by op and report count + median + p95 duration_ms; from
// errors.jsonl, the failure count (and a small tail of recent errors). A read-only
// observability op — no journal entry, no mutation.
export function opsStats(root, { projectId } = {}) {
  if (!projectId) throw new Error("opsStats requires projectId");
  if (!projectExists(root, projectId)) throw new Error(`canvas project not found: ${projectId}`);
  const journal = readJournal(root, projectId);
  const byOp = new Map();
  for (const line of journal) {
    const op = String(line.op || "");
    if (!byOp.has(op)) byOp.set(op, { op, count: 0, durations: [] });
    const bucket = byOp.get(op);
    bucket.count += 1;
    if (Number.isFinite(Number(line.duration_ms))) bucket.durations.push(Number(line.duration_ms));
  }
  const percentile = (sorted, q) => {
    if (!sorted.length) return null;
    const idx = Math.min(sorted.length - 1, Math.max(0, Math.ceil(q * sorted.length) - 1));
    return sorted[idx];
  };
  const ops = [...byOp.values()].map(({ op, count, durations }) => {
    const sorted = [...durations].sort((a, b) => a - b);
    const mid = Math.floor(sorted.length / 2);
    const median = sorted.length ? (sorted.length % 2 ? sorted[mid] : ms((sorted[mid - 1] + sorted[mid]) / 2)) : null;
    return { op, count, timed: sorted.length, median_ms: median, p95_ms: percentile(sorted, 0.95) };
  }).sort((a, b) => a.op.localeCompare(b.op));
  const errors = readErrors(root, projectId);
  return {
    projectId,
    total_entries: journal.length,
    ops,
    errors: { count: errors.length, recent: errors.slice(-5) },
  };
}

// ---- history panel view + jump navigation (T0204) ----------------------------
//
// The Photoshop-style history panel (page) and `history-list` (CLI) are a THIN view
// over the journal: the op layer computes the human LABEL/summary AND the current
// linear spine, so neither client parses the journal. jumpHistory is history
// NAVIGATION — it moves the applied head to any seq on that spine by restoring the
// seq's EXISTING sidecar snapshot. It composes with undo/redo: jumping N steps
// back/forward lands on exactly the state N undos/redos would (the same snapshots), in
// ONE call, appending only a nav marker (like the undo/redo markers, never a mutation).
// No parent pointer changes and no snapshot is written, so undo/redo/jump from the new
// head behave identically to N manual steps — the undo/redo chain stays coherent and a
// jump is itself reversible (redo/jump-forward after a back jump, and vice-versa).

// The label for a patchElement entry, derived from which fields its patch touched (the
// op records the clean patch in args_summary). Move/Resize/Transform/Rename/Show/Hide/
// Edit text — the frequent single-field edits get a precise verb.
export function patchElementLabel(patch = {}) {
  const p = patch && typeof patch === "object" ? patch : {};
  const has = (key) => p[key] !== undefined;
  if (has("content") || has("style")) return "Edit text";
  if (has("visible")) return p.visible === false ? "Hide" : "Show";
  const geo = ["x", "y", "w", "h"].filter(has);
  if (geo.length) {
    const sized = has("w") || has("h");
    const moved = has("x") || has("y");
    if (sized && !moved) return "Resize";
    if (moved && !sized) return "Move";
    return "Transform";
  }
  if (has("name")) return "Rename";
  return "Edit element";
}

// Human { label, summary } for a journaled mutation, from its op name + the small
// args_summary the op recorded. PURE (no disk) and exported so BOTH clients render
// identical text off listHistory's output — the page never maps op names itself. An
// unknown op falls back to the raw op name (no silent blank row).
export function historyEntryLabel(op, args = {}) {
  const a = args && typeof args === "object" ? args : {};
  const count = (value) => (Number.isFinite(Number(value)) ? Number(value) : 0);
  const items = (value) => (Array.isArray(value) ? value.length : 0);
  const plural = (n, noun) => `${n} ${noun}${n === 1 ? "" : "s"}`;
  switch (op) {
    case "base": return { label: "Base", summary: "" };
    case "addImage": return { label: "Add image", summary: String(a.name || "") };
    case "addImages": return { label: "Add images", summary: plural(count(a.count), "image") };
    case "addText": return { label: "Add text", summary: String(a.content || "") };
    case "patchElement": return { label: patchElementLabel(a.patch), summary: "" };
    case "setAssetStatus": return { label: "Set asset status", summary: String(a.status || "") };
    case "runAssetTechnicalGate": return { label: "Check asset quality", summary: String(a.verdict || "") };
    case "patchElements": return { label: "Move elements", summary: plural(count(a.count), "element") };
    case "removeElement": return { label: "Delete element", summary: "" };
    case "removeElements": return { label: "Delete elements", summary: plural(count(a.count), "element") };
    case "moveNodes": return { label: "Move", summary: plural(count(a.count), "item") };
    case "alignNodes": return { label: "Align", summary: plural(items(a.nodeIds), "item") };
    case "distributeNodes": return { label: "Distribute", summary: plural(items(a.nodeIds), "item") };
    case "reorderNode": return { label: "Reorder", summary: String(a.kind || "") };
    case "reorderElement": return { label: "Reorder", summary: "" };
    case "reorderNodes": return { label: "Reorder", summary: plural(items(a.nodeIds), "item") };
    case "setRegions": return { label: "Edit regions", summary: a.region_count != null ? plural(count(a.region_count), "region") : "" };
    case "detectRegions": return { label: "Detect regions", summary: "" };
    case "sliceRegions": return { label: "Slice", summary: "" };
    case "alphaCutout": {
      if (a.count) return { label: "Alpha cutout", summary: plural(count(a.count), "image") };
      return { label: "Alpha cutout", summary: a.region_count ? plural(count(a.region_count), "region") : String(a.method || "") };
    }
    case "alphaDualPlate": return { label: "Dual-plate alpha", summary: "" };
    case "alphaDualPlateGenerate": return { label: "Dual-plate alpha (generated)", summary: "" };
    case "cleanupApply": return { label: a.tool === "denoise" ? "Denoise" : "Quantize", summary: "" };
    case "bakeFilters": return { label: "Apply filters", summary: a.count ? plural(count(a.count), "image") : "" };
    case "setExportSettings": return { label: "Export settings", summary: "" };
    case "patchProject": return { label: "Rename project", summary: String(a.title || "") };
    case "createGroup": return { label: "Group", summary: String(a.name || "") };
    case "patchGroup": return { label: "Edit group", summary: "" };
    case "patchGroups": return { label: "Edit groups", summary: plural(items(a.groupIds), "group") };
    case "migrateScreenFlags": return { label: "Migrate screen flags", summary: a.count ? plural(count(a.count), "group") : "" };
    case "deleteGroup": return { label: "Delete group", summary: "" };
    case "assignToGroup": return { label: "Move to group", summary: "" };
    case "fitGroup": return { label: "Fit group", summary: "" };
    case "scaleGroup": return { label: "Scale group", summary: "" };
    case "reparentGroup": return { label: "Nest group", summary: "" };
    case "ungroupGroup": return { label: "Ungroup", summary: "" };
    case "createRecipeCard": return { label: "Recipe card", summary: String(a.name || "") };
    case "patchRecipe": return { label: "Edit recipe", summary: "" };
    case "generateFromRecipe": return { label: "Generate from recipe", summary: String(a.engine || "") };
    case "generateRecipePackSheet": return { label: "Generate pack sheet", summary: String(a.sheet || "") };
    case "createStyleCard": return { label: "Style card", summary: String(a.name || "") };
    case "patchStyle": return { label: "Edit style", summary: "" };
    case "createAnimCard": return { label: a.memberId ? "Anim card from image" : "Anim card", summary: String(a.name || "") };
    case "patchAnim": return { label: "Edit anim card", summary: "" };
    case "generateAnimFromCard": return { label: "Generate animation", summary: "" };
    case "expandRecipePrompt": return { label: "Expand prompt", summary: "" };
    case "extractFromElement": return { label: "Extract", summary: "" };
    case "promoteExtractedRecipe": return { label: "Recipe from extract", summary: "" };
    case "promoteExtractedStyle": return { label: "Style from extract", summary: "" };
    case "animateElementFromText": return { label: "Animate", summary: String(a.text || "") };
    case "pasteNodes": return { label: "Paste", summary: plural(count(a.count), "item") };
    case "duplicateNodes": return { label: "Duplicate", summary: plural(count(a.count), "item") };
    case "deleteNodes": return { label: "Delete", summary: plural(items(a.deletedElements) + items(a.deletedGroups), "item") };
    default: return { label: String(op || "Edit"), summary: "" };
  }
}

// The current LINEAR history spine (Photoshop-style): the undo chain from the applied
// head back to base, plus the redo chain forward from the head to the tip. Both mirror
// the exact walks undoOp/redoOp take — undoChain follows `parent` to 0; redoChain picks
// the greatest-seq child at each step (the branch redo would re-enter) — so a stale
// (invalidated) branch is never on the spine. `seen` guards a malformed parent cycle.
export function historySpine(project, journal) {
  const head = Number(project.history_seq) || 0;
  const mutationsBySeq = new Map();
  for (const line of journal) if (isMutation(line)) mutationsBySeq.set(Number(line.seq), line);

  const undoChain = [];
  const seen = new Set();
  let cursor = head;
  while (cursor && mutationsBySeq.has(cursor) && !seen.has(cursor)) {
    seen.add(cursor);
    const entry = mutationsBySeq.get(cursor);
    undoChain.push(entry);
    cursor = Number(entry.parent) || 0;
  }
  undoChain.reverse(); // oldest → head (head is last)

  const redoChain = [];
  cursor = head;
  for (;;) {
    let best = null;
    for (const line of journal) {
      if (!isMutation(line)) continue;
      const seq = Number(line.seq);
      if (seen.has(seq)) continue;
      if ((Number(line.parent) || 0) !== cursor) continue;
      if (!best || seq > Number(best.seq)) best = line;
    }
    if (!best) break;
    seen.add(Number(best.seq));
    redoChain.push(best);
    cursor = Number(best.seq);
  }
  return { head, undoChain, redoChain };
}

// Labeled linear history for the panel / `history-list`: a synthetic `Base` (seq 0) row
// then the undo chain (applied) then the redo chain (undone — dimmed, still clickable).
// Each entry is { seq, op, label, summary, at, current, undone }. current marks the
// applied head; undone marks the redo-tail (future) states. canUndo/canRedo match
// historyFlags. A cheap metadata scan (no snapshot loads).
export function listHistory(root, { projectId } = {}) {
  if (!projectId) throw new Error("listHistory requires projectId");
  const project = getProject(root, projectId);
  const journal = readJournal(root, projectId);
  const { head, undoChain, redoChain } = historySpine(project, journal);
  const rowOf = (line, undone) => {
    const { label, summary } = historyEntryLabel(line.op, line.args_summary);
    // Agent-made entries carry the robot marker IN the label (lead 2026-07-03), so the
    // page palette and CLI history-list render identical text — parity by construction.
    const actor = line.actor === "agent" ? "agent" : "user";
    return {
      seq: Number(line.seq),
      op: line.op,
      actor,
      label: actor === "agent" ? `🤖 ${label}` : label,
      summary,
      at: line.at ?? null,
      current: Number(line.seq) === head,
      undone,
    };
  };
  const entries = [
    { seq: 0, op: "base", actor: "user", label: "Base", summary: "", at: null, current: head === 0, undone: false },
    ...undoChain.map((line) => rowOf(line, false)),
    ...redoChain.map((line) => rowOf(line, true)),
  ];
  // `head` duplicates `history_seq` under the same vocabulary the concurrency-guard
  // error message uses ("head is now N") — additive (T0234); history_seq is unchanged
  // and still the field both existing clients (page, API) read.
  return { history_seq: head, head, canUndo: head > 0 && undoChain.length > 0, canRedo: redoChain.length > 0, entries };
}

// Jump the applied head to `seq` — any seq on the current spine (0 = base, an undo-chain
// seq = jump back, a redo-chain seq = jump forward). Restores that seq's EXISTING sidecar
// snapshot (state for a real entry; the oldest retained entry's undo_patch for base) and
// repoints history_seq, so the result equals N undos/redos with ZERO recomputation. One
// call; appends only a `jump` nav marker (no snapshot, not a mutation) so undo/redo stay
// coherent and the jump is reversible. LOUD on a non-integer/negative seq or a seq that is
// not on the current spine (an unknown or stale-branch seq). No-op (no marker) when already
// there. Like undo/redo it never grows depth, so no compaction runs. Optional `expectHead`
// (T0234) is the caller's concurrency guard — see checkExpectHead above.
export function jumpHistory(root, { projectId, seq, expectHead } = {}) {
  if (!projectId) throw new Error("jumpHistory requires projectId");
  if (seq === undefined || seq === null || seq === "") throw new Error("jumpHistory requires a target seq");
  const target = Number(seq);
  if (!Number.isInteger(target) || target < 0) {
    throw new Error(`jumpHistory seq must be a non-negative integer, got ${JSON.stringify(seq)}`);
  }
  const startedAt = performance.now();
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
  checkExpectHead(expectHead, head);
  if (target === head) return { project, history_seq: head, jumped_from: head, jumped_to: head }; // already here

  const journal = readJournal(root, projectId);
  const { undoChain, redoChain } = historySpine(project, journal);
  const back = undoChain.find((entry) => Number(entry.seq) === target);
  const forward = redoChain.find((entry) => Number(entry.seq) === target);
  if (target !== 0 && !back && !forward) {
    const reachable = [0, ...undoChain.map((entry) => Number(entry.seq)), ...redoChain.map((entry) => Number(entry.seq))];
    throw new Error(`jumpHistory seq ${target} is not on the current history (reachable seqs: ${reachable.join(", ")})`);
  }

  // Resolve the target project state from EXISTING snapshots only. A jump to base (0)
  // restores the oldest retained entry's BEFORE snapshot (undo_patch) — exactly where undo
  // bottoms out; any other seq restores its AFTER snapshot (state) — exactly where redo/undo
  // to that seq would land.
  let snap;
  if (target === 0) {
    snap = snapshotForEntry(root, projectId, undoChain[0]).undo_patch || {}; // head>0 here → non-empty chain
  } else {
    snap = snapshotForEntry(root, projectId, back || forward).state || {};
  }
  const restore = {
    elements: snap.elements || [],
    groups: snap.groups || [],
    tool_runs: snap.tool_runs || [],
    history_seq: target,
  };
  if (snap.title !== undefined) restore.title = snap.title;
  if (Object.hasOwn(snap, "ownership")) {
    restore.ownership = snap.ownership === null ? undefined : snap.ownership;
  }
  if (Object.hasOwn(snap, "archived")) restore.archived = snap.archived === true;
  const saved = updateProject(root, projectId, restore);
  appendJournal(root, projectId, { op: "jump", target_seq: target, from_seq: head, duration_ms: ms(performance.now() - startedAt) });
  return { project: saved, history_seq: saved.history_seq, jumped_from: head, jumped_to: target };
}
