// Canvas elements operation domain. Public API is ../ops.mjs.
import { existsSync, readFileSync } from "node:fs";
import { performance } from "node:perf_hooks";
import { alignMoves, ancestorsOf, blockReorder, distributeMoves, frontOrder, nodeScope, orderedChildren } from "../tree.mjs";
import { DEFAULT_NOTE_BACKGROUND, DEFAULT_NOTE_SIZE, defaultNoteStyle, defaultTextStyle, firstTextLine, mergeNoteStyle, mergeTextStyle, nominalTextBox } from "../fonts.mjs";
import { validateSlice9 } from "../slice9.mjs";
import { validateAnimation } from "../animation.mjs";
import { addImage as storeAddImage, addNote as storeAddNote, addText as storeAddText, getProject, imageSize, patchElement as storePatchElement, patchElements as storePatchElements, removeElement as storeRemoveElement, removeElements as storeRemoveElements, resolveProjectFile, updateProject } from "../store.mjs";
import { parseScaleSpec } from "./export_scale.mjs";
import { normalizeProjectOwnership } from "./project_lifecycle.mjs";
import { clampRound, commitMutation, groupsOf, normalizeNoteBackground, normalizeOpacityPatch, polygonBBox, readFontsManifest, sanitizeFiltersPatch, sanitizeTextPatch, sanitizeTransformPatch } from "./core.mjs";

export function addImage(root, projectId, args = {}) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const result = storeAddImage(root, projectId, args);
  // Keep an explicitly-ordered destination scope explicit (scopes never go
  // half-explicit): a fresh image lands at the FRONT of its scope. Computed from
  // `before` (the scope's siblings prior to the add); a no-op on any scope that
  // has never been reordered (frontOrder === null).
  let after = result.project;
  const fo = frontOrder(before, result.element.groupId == null ? null : result.element.groupId);
  if (fo !== null) {
    after = updateProject(root, projectId, {
      elements: (result.project.elements || []).map((element) =>
        element.id === result.element.id ? { ...element, order: fo } : element,
      ),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "addImage",
    args_summary: { name: result.element.name, elementId: result.element.id, w: result.element.w, h: result.element.h },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === result.element.id) || result.element;
  return { project, element };
}

// Add SEVERAL images in ONE journaled gesture — the page's multi-file drop/paste. Each
// image is {name, bytes, x?, y?}; they append in array order and, when the destination
// (root) scope is already explicitly ordered, land stacked at the FRONT in add order (so
// a reordered scope stays explicit — the same front hook addImage uses, extended to a
// batch). Loud + atomic: EVERY image's bytes are validated up front (non-empty + a
// parseable header), so a bad image in the batch throws before any element is appended;
// an empty list is a loud error (the page only calls this with real files). The
// content-addressed file writes are harmless even on a later throw (immutable storage
// keeps orphan files; only project.json state matters, and it stays at `before`). One
// commitMutation, so a single undo removes every added image. Single adds stay on addImage.
export function addImages(root, projectId, { images } = {}) {
  if (!projectId) throw new Error("addImages requires projectId");
  if (!Array.isArray(images) || !images.length) throw new Error("addImages requires a non-empty images array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  // Validate every image FIRST (atomic like patchElements): coerce bytes to a non-empty
  // Buffer and parse the header (imageSize throws on a corrupt/unsupported image).
  const prepared = images.map((image, index) => {
    if (!image || typeof image !== "object") throw new Error(`image ${index} is not an object`);
    const bytes = Buffer.isBuffer(image.bytes) ? image.bytes : Buffer.from(image.bytes || []);
    if (!bytes.length) throw new Error(`image ${index} (${image.name || "unnamed"}) has empty bytes`);
    imageSize(bytes); // throws on an unsupported/corrupt header
    return { name: image.name, bytes, x: image.x, y: image.y };
  });
  const added = [];
  for (const image of prepared) added.push(storeAddImage(root, projectId, image).element.id);
  // Front-order hook (addImage's, extended to a batch): stack the fresh images at the
  // front of the root scope in add order when that scope is already explicit; a no-op on
  // a never-reordered scope.
  let after = getProject(root, projectId);
  let fo = frontOrder(before, null);
  if (fo !== null) {
    const orderById = new Map(added.map((id) => [id, fo++]));
    after = updateProject(root, projectId, {
      elements: (after.elements || []).map((element) =>
        orderById.has(element.id) ? { ...element, order: orderById.get(element.id) } : element,
      ),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "addImages",
    args_summary: { count: added.length, elementIds: added },
    before,
    after,
    startedAt,
  });
  const idSet = new Set(added);
  return { project, elements: (project.elements || []).filter((item) => idSet.has(item.id)), count: added.length };
}

// Mint a normal journaled image element from an EXISTING project file `src` — no browser
// re-upload, no duplicate bytes (content-addressing already dedupes identical bytes to
// the same files/ entry, so this is a plain disk read + the SAME addImage op every other
// add goes through: front-order hook, journaling, meta all included for free). Backs the
// inspector's per-plate "Add to canvas" button (T0238): promote a dual-plate-generate
// plate (light or dark) straight onto the canvas as its own element. Loud when `src` is
// missing, unsafe (resolveProjectFile's own path confinement), or not found on disk.
export function addImageFromFile(root, projectId, { src, name, x, y } = {}) {
  if (!projectId) throw new Error("addImageFromFile requires projectId");
  if (!src) throw new Error("addImageFromFile requires src");
  const absPath = resolveProjectFile(root, projectId, src);
  if (!existsSync(absPath)) throw new Error(`addImageFromFile: file not found: ${src}`);
  const bytes = readFileSync(absPath);
  const fallbackName = String(src).split("/").pop();
  return addImage(root, projectId, { name: name || fallbackName, bytes, x, y });
}

// Add a TEXT element (Figma text node). Mirrors addImage: builds the element via the
// store, gives it a FRONT order when its destination scope is already explicit (so a
// reordered scope stays explicit), and journals ONE undoable entry. `style` is merged
// over the defaults and validated LOUDLY against the fonts manifest (an unknown
// family/weight, bad align/color, or non-finite size throws before any write). Optional
// `groupId` drops the text straight into a group (validated). The stored w/h is a
// NOMINAL box (the page re-measures on open, the renderer re-measures at export).
export function addText(root, projectId, args = {}) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const manifest = readFontsManifest(root);
  const style = mergeTextStyle(defaultTextStyle(), args.style || {}, manifest);
  const content = args.content == null ? "Text" : String(args.content);
  const groupId = args.groupId == null || args.groupId === "" ? undefined : String(args.groupId);
  if (groupId && !groupsOf(before).some((group) => group.id === groupId)) {
    throw new Error(`group not found: ${groupId}`);
  }
  const box = nominalTextBox(content, style);
  const name = firstTextLine(content) || "Text";
  const result = storeAddText(root, projectId, {
    x: args.x,
    y: args.y,
    w: box.w,
    h: box.h,
    content,
    style,
    name,
    groupId,
  });
  // Front-order hook (identical to addImage): a fresh text lands at the FRONT of its
  // scope when that scope is already explicitly ordered; a no-op otherwise.
  let after = result.project;
  const fo = frontOrder(before, result.element.groupId == null ? null : result.element.groupId);
  if (fo !== null) {
    after = updateProject(root, projectId, {
      elements: (result.project.elements || []).map((element) =>
        element.id === result.element.id ? { ...element, order: fo } : element,
      ),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "addText",
    args_summary: {
      elementId: result.element.id,
      content: content.slice(0, 40),
      fontFamily: style.fontFamily,
      fontSize: style.fontSize,
    },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === result.element.id) || result.element;
  return { project, element };
}

// Add a NOTE element (T0268 — a Miro/FigJam sticky card). Mirrors addText's shape (store
// build + front-order hook + ONE journaled entry), but the box is FULLY user-fixed (both
// w and h; defaults to DEFAULT_NOTE_SIZE, both resizable afterward), the `style` is the
// note font SUBSET merged over the note defaults + validated against the fonts manifest,
// and it carries a `background` fill (a sticky preset or arbitrary #rrggbb; defaults to the
// first preset, `null` = no fill). Notes are annotations, never render content: renderGroup/
// exportProject skip them and exportElements refuses them (see below). Optional `groupId`
// drops the note into a group (validated).
export function addNote(root, projectId, args = {}) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const manifest = readFontsManifest(root);
  const style = mergeNoteStyle(defaultNoteStyle(), args.style || {}, manifest);
  const background = args.background === undefined ? { ...DEFAULT_NOTE_BACKGROUND } : normalizeNoteBackground(args.background);
  const content = args.content == null ? "" : String(args.content);
  const groupId = args.groupId == null || args.groupId === "" ? undefined : String(args.groupId);
  if (groupId && !groupsOf(before).some((group) => group.id === groupId)) {
    throw new Error(`group not found: ${groupId}`);
  }
  // User-fixed box: honor an explicit finite positive w/h, else the default sticky size.
  const w = Number.isFinite(Number(args.w)) && Number(args.w) > 0 ? Number(args.w) : DEFAULT_NOTE_SIZE.w;
  const h = Number.isFinite(Number(args.h)) && Number(args.h) > 0 ? Number(args.h) : DEFAULT_NOTE_SIZE.h;
  const name = firstTextLine(content) || "Note";
  const result = storeAddNote(root, projectId, { x: args.x, y: args.y, w, h, content, style, background, name, groupId });
  // Front-order hook (identical to addText/addImage): a fresh note lands at the FRONT of its
  // scope when that scope is already explicitly ordered; a no-op otherwise.
  let after = result.project;
  const fo = frontOrder(before, result.element.groupId == null ? null : result.element.groupId);
  if (fo !== null) {
    after = updateProject(root, projectId, {
      elements: (result.project.elements || []).map((element) =>
        element.id === result.element.id ? { ...element, order: fo } : element,
      ),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "addNote",
    args_summary: {
      elementId: result.element.id,
      content: content.slice(0, 40),
      background: background ? background.color : null,
      w,
      h,
    },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === result.element.id) || result.element;
  return { project, element };
}

export function patchElement(root, projectId, elementId, patch = {}) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const clean = sanitizeFiltersPatch(
    before,
    elementId,
    sanitizeTransformPatch(before, elementId, sanitizeTextPatch(root, before, elementId, patch)),
  );
  // Validate opacity BEFORE any write so a bad value throws atomically (nothing applied).
  const opacity = normalizeOpacityPatch(clean);
  const result = storePatchElement(root, projectId, elementId, clean);
  // opacity is not a store field: apply the set/delete here (stored only when != 1) on top
  // of the store's geometry write, so the single commit below captures the full change.
  let after = result.project;
  if (opacity !== undefined) {
    after = updateProject(root, projectId, {
      elements: (after.elements || []).map((item) => {
        if (item.id !== elementId) return item;
        const next = { ...item };
        if (opacity === 1) delete next.opacity;
        else next.opacity = opacity;
        return next;
      }),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "patchElement",
    args_summary: { elementId, patch: clean },
    before,
    after,
    startedAt,
  });
  return { project, element: (project.elements || []).find((item) => item.id === elementId) || result.element };
}

export function removeElement(root, projectId, elementId) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const result = storeRemoveElement(root, projectId, elementId);
  const project = commitMutation(root, projectId, {
    op: "removeElement",
    args_summary: { elementId },
    before,
    after: result.project,
    startedAt,
  });
  return { project, removed: result.removed };
}

// Patch several elements in ONE journaled gesture — the marquee/multi-select
// move commit and any agent multi-move. Each patch is {elementId, x?, y?, w?, h?,
// name?, visible?} with the SAME per-field rules as patchElement (incl. rotation/flip,
// filters, and opacity). A bad/missing elementId throws before any write (atomic — no
// partial batch), and the whole batch is ONE commitMutation, so a single undo restores
// the entire gesture (not N steps). An empty batch is a no-op (no journal entry), like
// the other no-op-guarded ops.
export function patchElements(root, { projectId, patches } = {}) {
  if (!projectId) throw new Error("patchElements requires projectId");
  if (!Array.isArray(patches)) throw new Error("patchElements requires a patches array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  // opacity is not a store field (same as patchElement) — validated per-element here,
  // applied in a second pass below once the store has written everything else.
  const opacityByElementId = new Map();
  const clean = patches.map((patch, index) => {
    if (!patch || typeof patch !== "object") throw new Error(`patch ${index} is not an object`);
    const elementId = String(patch.elementId == null ? "" : patch.elementId).trim();
    if (!elementId) throw new Error(`patch ${index} is missing an elementId`);
    // Text content/style, rotation/flip, AND filters patches are validated + normalized
    // here (same as patchElement); plain geometry patches skip all three fast paths.
    const textClean = sanitizeTextPatch(root, before, elementId, patch);
    const transformClean = sanitizeTransformPatch(before, elementId, textClean);
    const filtersClean = sanitizeFiltersPatch(before, elementId, transformClean);
    const opacity = normalizeOpacityPatch(filtersClean);
    if (opacity !== undefined) opacityByElementId.set(elementId, opacity);
    return { ...filtersClean, elementId };
  });
  const result = storePatchElements(root, projectId, clean);
  let after = result.project;
  if (opacityByElementId.size) {
    after = updateProject(root, projectId, {
      elements: (after.elements || []).map((item) => {
        if (!opacityByElementId.has(item.id)) return item;
        const opacity = opacityByElementId.get(item.id);
        const next = { ...item };
        if (opacity === 1) delete next.opacity;
        else next.opacity = opacity;
        return next;
      }),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "patchElements",
    args_summary: { count: clean.length, elementIds: clean.map((patch) => patch.elementId) },
    before,
    after,
    startedAt,
  });
  const ids = new Set(clean.map((patch) => patch.elementId));
  return { project, elements: (project.elements || []).filter((item) => ids.has(item.id)), count: clean.length };
}

// Remove several elements in ONE journaled gesture — the multi-select delete. All
// ids must exist (throws before any write — atomic; no partial delete), duplicates
// are de-duplicated, and the whole batch is ONE commitMutation, so a single undo
// restores every removed element. Backing files stay on disk (immutable storage).
export function removeElements(root, { projectId, elementIds } = {}) {
  if (!projectId) throw new Error("removeElements requires projectId");
  if (!Array.isArray(elementIds)) throw new Error("removeElements requires an elementIds array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const result = storeRemoveElements(root, projectId, elementIds.map((value) => String(value)));
  const project = commitMutation(root, projectId, {
    op: "removeElements",
    args_summary: { elementIds: result.removed, count: result.removed.length },
    before,
    after: result.project,
    startedAt,
  });
  return { project, removed: result.removed };
}

// Locate a node (element OR group) by id and resolve its parent scope (the group it
// lives in, or null for root). Throws loudly on an unknown id. Element and group ids
// are disjoint namespaces, so one id resolves to at most one node.
export function findNode(project, nodeId) {
  const element = (project.elements || []).find((item) => item.id === nodeId);
  if (element) return { kind: "element", ref: element, scopeId: nodeScope(project, element) };
  const group = groupsOf(project).find((item) => item.id === nodeId);
  if (group) return { kind: "group", ref: group, scopeId: nodeScope(project, group) };
  throw new Error(`node not found: ${nodeId}`);
}

// Move a node (ELEMENT or GROUP) to a target `index` among its MERGED same-scope
// siblings — the elements AND groups sharing its parent scope, in the computed
// back → front order tree.mjs.orderedChildren yields (0 = back / painted first,
// N-1 = front / painted last). The move assigns explicit contiguous `order` values
// (0..N-1) to EVERY sibling of that scope reflecting the new arrangement — the design's
// lazy per-scope normalization: the first reorder on a scope makes it explicit, and it
// never goes half-explicit afterwards (see the frontOrder hook on add/assign). Only this
// scope's siblings are touched; every other scope is left exactly as it was. One journal
// entry; undo restores the whole scope's previous `order` fields for free (snapshots).
// An unknown node or an out-of-range index is a loud error (no silent clamp — the thin
// reorderElement delegate keeps the historical clamping contract for element callers).
export function reorderNode(root, { projectId, nodeId, index } = {}) {
  if (!projectId) throw new Error("reorderNode requires projectId");
  if (!nodeId) throw new Error("reorderNode requires nodeId");
  if (!Number.isFinite(Number(index))) throw new Error("reorderNode requires a numeric index");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const node = findNode(before, nodeId);
  const target = Math.round(Number(index));

  // Merged siblings of the scope in current visual (back → front) order.
  const siblings = orderedChildren(before, node.scopeId);
  const count = siblings.length;
  if (target < 0 || target > count - 1) {
    throw new Error(`reorderNode index ${target} is out of range 0..${count - 1} for scope ${node.scopeId == null ? "(root)" : node.scopeId}`);
  }
  const from = siblings.findIndex((sibling) => sibling.id === nodeId);
  if (from === target) return { project: before, node: node.ref, kind: node.kind, index: target }; // no-op

  // New arrangement; assign contiguous order 0..N-1 to every sibling.
  const arranged = siblings.slice();
  const [moved] = arranged.splice(from, 1);
  arranged.splice(target, 0, moved);
  const orderByNodeId = new Map(arranged.map((sibling, order) => [sibling.id, order]));

  const nextElements = (before.elements || []).map((element) =>
    orderByNodeId.has(element.id) ? { ...element, order: orderByNodeId.get(element.id) } : element,
  );
  const nextGroups = groupsOf(before).map((group) =>
    orderByNodeId.has(group.id) ? { ...group, order: orderByNodeId.get(group.id) } : group,
  );

  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "reorderNode",
    args_summary: { nodeId, kind: node.kind, index: target, scope: node.scopeId },
    before,
    after,
    startedAt,
  });
  return { project, node: findNode(project, nodeId).ref, kind: node.kind, index: target };
}

// Move an ELEMENT to a target sibling index — a thin delegate to reorderNode (element
// ids ARE node ids). The index is over the MERGED same-scope siblings (elements + groups
// of the element's scope), matching the computed paint order. Kept as its own op + route
// + CLI for back-compat; it preserves the historical FORGIVING contract that an
// out-of-range index snaps to the nearest edge (reorderNode itself is strict), so page
// z-order nudges and older callers never throw on a clamp. One journal entry.
export function reorderElement(root, { projectId, elementId, index } = {}) {
  if (!projectId) throw new Error("reorderElement requires projectId");
  if (!elementId) throw new Error("reorderElement requires elementId");
  if (!Number.isFinite(Number(index))) throw new Error("reorderElement requires a numeric index");
  const project = getProject(root, projectId);
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const count = orderedChildren(project, nodeScope(project, element)).length;
  const clamped = Math.max(0, Math.min(count - 1, Math.round(Number(index))));
  const result = reorderNode(root, { projectId, nodeId: elementId, index: clamped });
  return { project: result.project, element: (result.project.elements || []).find((item) => item.id === elementId), index: clamped };
}

// Move several NODES (elements AND/OR groups) to absolute positions in ONE journaled
// gesture — the page's mixed marquee/multi-select move commit (loose elements + one or
// more group frames). Each move is {nodeId, x, y} (the node's new top-left). A group move
// cascades its FULL descendant closure (nested subgroup frames AND every element in the
// subtree) by the same delta, exactly like the single patchGroup move path; an element
// move just repositions that element. Overlap-safe: when the selection holds a group AND a
// node inside its subtree, the node shifts by the TOPMOST moved ancestor's delta only (once,
// with the parent), never twice. Loud + atomic: an unknown nodeId, a non-object move, or a
// non-finite x/y throws before any write; an empty moves list is a loud error (the page only
// calls this with real moves). One commitMutation, so a single undo restores every position.
// Shared overlap-safe move cascade (T0232 extraction — the core moveNodes always had,
// reused by alignNodes/distributeNodes too): resolve each move's DIRECT delta against
// `before`, then apply the EFFECTIVE delta (the delta of the TOPMOST moved node in a
// node's ancestor-or-self chain — ancestorsOf is nearest-first, so overwriting ends on
// the topmost) to every element AND group, so a group carries its delta across its whole
// subtree and a selection holding both a group and one of its own descendants shifts that
// descendant ONCE, with the parent, never twice. Pure w.r.t. `before` (no disk I/O);
// returns the {nextElements, nextGroups, nodeIds} the caller feeds to updateProject. Loud:
// an unknown nodeId or a non-finite x/y throws before anything is computed. `moves` may be
// empty (align/distribute only pass the nodes that actually need to move — the caller's
// own before===after journal guard then turns that into a no-op with no entry written).
export function applyNodeMoves(before, moves) {
  const directDelta = new Map();
  const nodeIds = [];
  (moves || []).forEach((move, index) => {
    if (!move || typeof move !== "object") throw new Error(`move ${index} is not an object`);
    const nodeId = String(move.nodeId == null ? "" : move.nodeId).trim();
    if (!nodeId) throw new Error(`move ${index} is missing a nodeId`);
    if (!Number.isFinite(Number(move.x)) || !Number.isFinite(Number(move.y))) {
      throw new Error(`move ${index} (${nodeId}) requires finite x and y`);
    }
    const node = findNode(before, nodeId); // throws on an unknown id
    directDelta.set(nodeId, { dx: Number(move.x) - (Number(node.ref.x) || 0), dy: Number(move.y) - (Number(node.ref.y) || 0) });
    nodeIds.push(nodeId);
  });

  const effectiveDelta = (node) => {
    let best = directDelta.has(node.id) ? directDelta.get(node.id) : null;
    for (const ancestor of ancestorsOf(before, node)) {
      if (directDelta.has(ancestor.id)) best = directDelta.get(ancestor.id);
    }
    return best;
  };

  const nextElements = (before.elements || []).map((element) => {
    const delta = effectiveDelta(element);
    return delta ? { ...element, x: (Number(element.x) || 0) + delta.dx, y: (Number(element.y) || 0) + delta.dy } : element;
  });
  const nextGroups = groupsOf(before).map((group) => {
    const delta = effectiveDelta(group);
    return delta ? { ...group, x: (Number(group.x) || 0) + delta.dx, y: (Number(group.y) || 0) + delta.dy } : group;
  });

  return { nextElements, nextGroups, nodeIds };
}

export function moveNodes(root, { projectId, moves } = {}) {
  if (!projectId) throw new Error("moveNodes requires projectId");
  if (!Array.isArray(moves) || !moves.length) throw new Error("moveNodes requires a non-empty moves array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const { nextElements, nextGroups, nodeIds } = applyNodeMoves(before, moves);
  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "moveNodes",
    args_summary: { count: nodeIds.length, nodeIds },
    before,
    after,
    startedAt,
  });
  return { project, count: nodeIds.length, nodeIds };
}

// Align 2+ nodes (elements AND/OR groups, mixed OK) — or exactly 1 node that lives inside a
// parent group — to a shared reference frame in ONE journaled gesture (T0232 increment 1):
// the inspector's Align row / the agent's nodes-align. Target math is PURE (tree.alignMoves
// — Figma-auto reference: the union bbox of 2+ selected nodes, or the PARENT GROUP frame for
// exactly 1 node inside a group — see tree.mjs for the exact reference rules); this op only
// resolves the targets and applies them through the SAME overlap-safe cascade moveNodes uses
// (a moved group carries its whole subtree). Loud + atomic: an unknown nodeId, an unknown
// align key, or an unresolvable reference throws before any write. Already-aligned nodes
// write NO journal entry (commitMutation's own before===after no-op guard — an empty `moves`
// list from alignMoves leaves `after` identical to `before`).
export function alignNodes(root, { projectId, nodeIds, align, reference } = {}) {
  if (!projectId) throw new Error("alignNodes requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const ids = (nodeIds || []).map((value) => String(value));
  const moves = alignMoves(before, ids, align, reference); // throws on bad input
  const { nextElements, nextGroups } = applyNodeMoves(before, moves);
  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "alignNodes",
    args_summary: { count: ids.length, nodeIds: ids, align, reference: reference || "auto" },
    before,
    after,
    startedAt,
  });
  return { project, nodeIds: ids, moved: moves.map((move) => move.nodeId) };
}

// Distribute 3+ nodes (elements AND/OR groups, mixed OK) with equal gaps along an axis in
// ONE journaled gesture (T0232 increment 1) — the inspector's Distribute buttons / the
// agent's nodes-distribute. Target math is PURE (tree.distributeMoves — sorted by position
// along the axis, endpoints fixed); applied through the SAME shared cascade + no-op guard as
// alignNodes. Loud + atomic: an unknown nodeId, an unknown axis, or <3 nodes throws before
// any write.
export function distributeNodes(root, { projectId, nodeIds, axis } = {}) {
  if (!projectId) throw new Error("distributeNodes requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const ids = (nodeIds || []).map((value) => String(value));
  const moves = distributeMoves(before, ids, axis); // throws on bad input
  const { nextElements, nextGroups } = applyNodeMoves(before, moves);
  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "distributeNodes",
    args_summary: { count: ids.length, nodeIds: ids, axis },
    before,
    after,
    startedAt,
  });
  return { project, nodeIds: ids, moved: moves.map((move) => move.nodeId) };
}

// Move a SET of nodes (elements AND/OR groups) as ONE journaled z-order gesture — the
// page's multi-selection Ctrl+[/] and the Order menu on a multi-selection. The selected
// same-scope siblings move together as a BLOCK preserving their relative order (Figma
// semantics: front/back jump to the edge, forward/backward nudge one step past the nearest
// unselected neighbor). A CROSS-scope selection applies the same block move independently
// per scope but stays ONE commitMutation (one undo). `direction` is front|back|forward|
// backward; `index` is an absolute slot among the unselected siblings (single-scope only).
// Exactly one of direction|index is required. Assigning contiguous order to each touched
// scope makes it explicit (the reorderNode normalization; scopes never go half-explicit).
// Loud + atomic: an unknown nodeId, an empty set, both/neither of direction|index, an
// unknown direction, or an index on a cross-scope selection throws before any write.
export function reorderNodes(root, { projectId, nodeIds, direction, index } = {}) {
  if (!projectId) throw new Error("reorderNodes requires projectId");
  if (!Array.isArray(nodeIds) || !nodeIds.length) throw new Error("reorderNodes requires a non-empty nodeIds array");
  const hasDirection = direction !== undefined && direction !== null && direction !== "";
  const hasIndex = index !== undefined && index !== null && index !== "" && Number.isFinite(Number(index));
  if (hasDirection === hasIndex) {
    throw new Error("reorderNodes requires exactly one of direction (front|back|forward|backward) or index");
  }
  const startedAt = performance.now();
  const before = getProject(root, projectId);

  // Resolve every node (throws on an unknown id) and record its scope. Ids in one scope
  // move as a block; a selection spanning several scopes applies per scope.
  const ids = nodeIds.map((value) => String(value));
  const idSet = new Set(ids);
  const scopeById = new Map();
  for (const id of ids) {
    const node = findNode(before, id);
    scopeById.set(id, node.scopeId == null ? null : node.scopeId);
  }
  const scopes = [...new Set(scopeById.values())];
  if (hasIndex && scopes.length > 1) {
    throw new Error("reorderNodes index is only valid for a single-scope selection; use direction across scopes");
  }
  const spec = hasIndex ? { index: Math.round(Number(index)) } : { direction: String(direction) };

  // Per scope: reorder its merged siblings with the selected ones moved as a block, then
  // assign contiguous 0..N-1 order across that scope (makes it explicit). A gesture that
  // doesn't change any scope's sequence (e.g. "bring forward" already at the front) is a
  // no-op that writes nothing — matching the single-node reorderNode guard.
  const orderByNodeId = new Map();
  let changed = false;
  for (const scope of scopes) {
    const siblings = orderedChildren(before, scope);
    const arranged = blockReorder(siblings, idSet, spec); // throws on a bad spec
    if (arranged.some((node, position) => node.id !== siblings[position].id)) changed = true;
    arranged.forEach((node, order) => orderByNodeId.set(node.id, order));
  }
  if (!changed) return { project: before, nodeIds: ids, count: ids.length };

  const nextElements = (before.elements || []).map((element) =>
    orderByNodeId.has(element.id) ? { ...element, order: orderByNodeId.get(element.id) } : element,
  );
  const nextGroups = groupsOf(before).map((group) =>
    orderByNodeId.has(group.id) ? { ...group, order: orderByNodeId.get(group.id) } : group,
  );

  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "reorderNodes",
    args_summary: {
      nodeIds: ids,
      direction: hasDirection ? String(direction) : undefined,
      index: hasIndex ? Math.round(Number(index)) : undefined,
      scopes: scopes.map((scope) => (scope == null ? null : scope)),
    },
    before,
    after,
    startedAt,
  });
  return { project, nodeIds: ids, count: ids.length };
}

// Replace an element's regions array (the ADJUST/SELECT step before slicing).
// Validates each region carries an id and an in-source-bounds integer rect while
// preserving any extra fields the detector/slicer attach (content_bbox, area_px,
// merged_from, ...). Journaled like any metadata mutation, so the before/after
// snapshot restores the previous regions on undo/redo. This is the one op behind
// both the page's region editing (drag/resize/rubber-band) and the CLI regions-set.
export function setRegions(root, { projectId, elementId, regions } = {}) {
  if (!projectId) throw new Error("setRegions requires projectId");
  if (!elementId) throw new Error("setRegions requires elementId");
  if (!Array.isArray(regions)) throw new Error("setRegions requires a regions array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const boundsW = Number(element.source_w) || Number(element.w) || 0;
  const boundsH = Number(element.source_h) || Number(element.h) || 0;

  const seen = new Set();
  const clean = regions.map((region, index) => {
    if (!region || typeof region !== "object") throw new Error(`region ${index} is not an object`);
    const id = String(region.id == null ? "" : region.id).trim();
    if (!id) throw new Error(`region ${index} is missing an id`);
    if (seen.has(id)) throw new Error(`duplicate region id: ${id}`);
    seen.add(id);

    // Optional polygon shape: >=3 finite [x, y] pairs, each rounded to int and clamped
    // to source bounds (far edge inclusive). A clean polygon makes this a polygonal
    // region — the discriminator is polygon.length >= 3 (no `shape` field), matching the
    // Python slicer. <3 clean points drops the polygon back to a plain rect region.
    let polygon = null;
    if (region.polygon !== undefined && region.polygon !== null) {
      if (!Array.isArray(region.polygon)) throw new Error(`region ${id} polygon must be an array of [x, y] points`);
      const points = [];
      for (const point of region.polygon) {
        if (!Array.isArray(point) || point.length !== 2) continue;
        const px = Number(point[0]);
        const py = Number(point[1]);
        if (!Number.isFinite(px) || !Number.isFinite(py)) continue;
        points.push([clampRound(px, boundsW), clampRound(py, boundsH)]);
      }
      if (points.length >= 3) polygon = points;
    }

    // rect: a polygon's stored rect IS its bounding box (rectFromPolygon), so shape and
    // bbox never diverge; otherwise validate the supplied rect as an in-bounds integer box.
    let box;
    if (polygon) {
      box = polygonBBox(polygon, boundsW, boundsH);
    } else {
      const rect = region.rect;
      if (!Array.isArray(rect) || rect.length !== 4 || !rect.every((value) => Number.isFinite(Number(value)))) {
        throw new Error(`region ${id} rect must be [x, y, w, h] numbers`);
      }
      const x = Math.round(Number(rect[0]));
      const y = Math.round(Number(rect[1]));
      const w = Math.round(Number(rect[2]));
      const h = Math.round(Number(rect[3]));
      if (w <= 0 || h <= 0) throw new Error(`region ${id} rect must have positive width and height`);
      if (x < 0 || y < 0 || x + w > boundsW || y + h > boundsH) {
        throw new Error(`region ${id} rect [${x}, ${y}, ${w}, ${h}] is out of source bounds ${boundsW}x${boundsH}`);
      }
      box = [x, y, w, h];
    }

    // Preserve extra detector/slicer fields (content_bbox, area_px, ...); normalize id +
    // rect, the optional polygon, and the optional first-class `name` (trimmed string).
    const out = { ...region, id, rect: box };
    if (polygon) out.polygon = polygon;
    else delete out.polygon;
    if (out.name !== undefined && out.name !== null) {
      const name = String(out.name).trim();
      if (name) out.name = name;
      else delete out.name;
    }
    return out;
  });

  const nextElements = (before.elements || []).map((item) =>
    item.id === elementId ? { ...item, regions: clean } : item,
  );
  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "setRegions",
    args_summary: { elementId, region_count: clean.length },
    before,
    after,
    startedAt,
  });
  const updated = (project.elements || []).find((item) => item.id === elementId);
  return { project, element: updated, regions: (updated && updated.regions) || [] };
}

// ---- 9-slice element display (T0233) ------------------------------------------
//
// An IMAGE element carries an optional `element.slice9 = {left, top, right,
// bottom, scale?}` — insets in SOURCE PIXELS; corners paint at a FIXED size while
// the box resizes (edges stretch one axis, the center stretches both). Absent =
// today's single-drawImage/resize behavior everywhere (additive, zero migration).
// Top-level, not `meta` — like `regions`/`export`, slice9 is a live render
// property, not provenance. A DEDICATED op (not a `patchElement` field, T0233
// design section 0/8 decision 0): matches the setRegions/setExportSettings
// structural precedent (top-level, separately-validated render property with its
// own inspector panel, Packet 2), localizes the source-dim loud validation, and
// gives a clean null-to-clear. `scale` (T0233 scope addition, lead: «важно чтобы я
// мог скейлить края, иногда мне нужно больше или меньше») multiplies the
// DESTINATION corner/edge band only — see slice9.mjs for the exact math.

// Replace (or clear) an element's slice9 insets — the page's inspector Slice-9
// section (Packet 2) and the CLI's slice9-set both commit through here (tool
// parity). `insets` an object -> validateSlice9 (loud on a non-int/negative inset,
// a corner pair that would consume the source axis, or an out-of-range `scale`)
// then stores `element.slice9`; `insets === null` clears the field. Undo/redo:
// free (slice9 rides in the elements snapshot like any element field —
// commitMutation's snapshotOf already deep-clones the whole array). Image-only: a
// text element throws (mirrors the flipH/flipV image-only guard in
// sanitizeTransformPatch above).
export function setSlice9(root, { projectId, elementId, insets } = {}) {
  if (!projectId) throw new Error("setSlice9 requires projectId");
  if (!elementId) throw new Error("setSlice9 requires elementId");
  if (insets === undefined) throw new Error("setSlice9 requires insets (an object) or null to clear");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image") {
    throw new Error(`element ${elementId} is a ${element.type} element — slice9 is image-only (does not apply to type:"${element.type}")`);
  }
  let slice9 = null;
  if (insets !== null) {
    // source_w/source_h are persisted by addImage; fall back to w/h for a legacy
    // element that predates that field (the same fallback setRegions uses above).
    const sourceW = Number(element.source_w) || Number(element.w) || 0;
    const sourceH = Number(element.source_h) || Number(element.h) || 0;
    slice9 = validateSlice9(insets, sourceW, sourceH); // throws loudly
  }
  const nextElements = (before.elements || []).map((item) => {
    if (item.id !== elementId) return item;
    const clone = { ...item };
    if (slice9) clone.slice9 = slice9;
    else delete clone.slice9;
    return clone;
  });
  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "setSlice9",
    args_summary: { elementId, insets: slice9 },
    before,
    after,
    startedAt,
  });
  const updated = (project.elements || []).find((item) => item.id === elementId);
  return { project, element: updated };
}

// ---- procedural animation (T0260 Track A) -------------------------------------
//
// An element carries an optional `element.animation = { v:1, channels:[...] }` — the
// shared `ai_studio.canvas.animation.v1` spec (see animation.mjs for the model, units,
// and osc/keyframes semantics). Top-level, not `meta` — like regions/slice9/export it is
// a live property, not provenance. A DEDICATED op (mirrors setSlice9/setExportSettings):
// element-scoped, its own loud validation at set time, one journal entry, clean
// null-to-clear. Image AND text elements both allowed (animation is geometry/opacity-
// level, never pixel-level). No renderer integration in this increment — the preview
// (increment 2) and bake (increment 3) consume it via animation.mjs's sampleAnimation.

// Replace (or clear) an element's animation spec. `animation` an object -> validateAnimation
// (loud on a bad kind/prop, a duplicate-property channel, non-increasing keyframes, period
// 0, ...) then stores `element.animation`; `animation` null OR undefined clears the field.
// Undo/redo: free (animation rides in the elements snapshot like any element field —
// commitMutation's snapshotOf deep-clones the whole array).
export function setElementAnimation(root, { projectId, elementId, animation } = {}) {
  if (!projectId) throw new Error("setElementAnimation requires projectId");
  if (!elementId) throw new Error("setElementAnimation requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const normalized = animation === null || animation === undefined ? null : validateAnimation(animation); // throws loudly
  const nextElements = (before.elements || []).map((item) => {
    if (item.id !== elementId) return item;
    const clone = { ...item };
    if (normalized) clone.animation = normalized;
    else delete clone.animation;
    return clone;
  });
  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "setElementAnimation",
    args_summary: { elementId, channels: normalized ? normalized.channels.length : 0 },
    before,
    after,
    startedAt,
  });
  const updated = (project.elements || []).find((item) => item.id === elementId);
  return { project, element: updated };
}

// ---- per-element export settings (Figma-style rows) --------------------------
//
// An element carries an optional `export` array of rows
// [{scale, format, quality?, resample, base?}] — the Figma Export section persisted on
// the layer (T0229 removed the per-row suffix; file names are automatic at export time;
// T0235 added the additive `base` field: "source" (default, absent) or "canvas").
// setExportSettings validates + normalizes the rows and journals them
// like any metadata mutation (undo/redo restore the previous rows). The scale math
// (resolveExportScale) also runs at export time; validated here so an unknown
// scale/format/resample/base is a clear error the moment it is set, from either client.

export const EXPORT_FORMATS = new Set(["png", "jpg", "webp"]);
export const EXPORT_RESAMPLE = new Set(["lanczos", "nearest"]);
// T0235: export base — which dims a row's scale token resolves against. "canvas" (the
// default since the lead's same-day flip, stored as an ABSENT field) resolves against
// the element's CURRENT on-canvas w/h at export time (tracks later resizes — "2x" =
// twice what you see); "source" resolves against the original source pixels
// (source_w/h) for full-resolution export of a downscaled sprite.
export const EXPORT_BASES = new Set(["source", "canvas"]);
// T0229: the per-row filename suffix was removed — file naming is now automatic
// (element/screen name + a Figma-style scale marker only when several rows would
// collide). The default row therefore carries no suffix.
export const DEFAULT_EXPORT_ROW = { scale: "1x", format: "png", resample: "lanczos" };

// Parse a Figma-style scale token into a spec: a multiplier ("0.5x", "1x", "2x",
// "3x", "4x", or a bare "2") or a fixed target dimension ("512w" = 512px wide,
// "512h" = 512px tall; the other axis keeps aspect). Throws on anything else so an
// unknown scale is a clear validation error, not a silent fallback.

// Validate + normalize export rows to {scale, format, resample, quality?, base?}.
// quality (1-100) is kept only for the lossy formats (jpg/webp), defaulting to 90;
// a png row never carries a quality. base (T0235) is kept ONLY when it resolves to
// "canvas" — the default "source" (absent or explicit) is dropped so stored JSON stays
// minimal and old rows are untouched. Throws on any invalid field.
//
// Suffix (T0229): the field is GONE. `rejectSuffix` splits the two callers by the
// additive-schema stance: setExportSettings (a NEW WRITE) rejects any row carrying a
// `suffix` LOUDLY (a stale client) so bad rows never persist; the export READERS
// (rowsForElement / override rows) leave it false and simply ignore a legacy stored
// `suffix` — no silent write-back, filenames come from the automatic namer instead.
export function cleanExportRows(rows, { rejectSuffix = false } = {}) {
  if (!Array.isArray(rows)) throw new Error("export rows must be an array");
  return rows.map((row, index) => {
    if (!row || typeof row !== "object") throw new Error(`export row ${index} is not an object`);
    if (rejectSuffix && row.suffix !== undefined) {
      throw new Error(
        `export row ${index} carries a removed "suffix" field — export file names are automatic now (T0229); drop suffix`,
      );
    }
    const scale = String(row.scale == null ? "" : row.scale).trim() || "1x";
    parseScaleSpec(scale); // validate syntax now (throws on invalid)
    const format = String(row.format == null ? "png" : row.format).trim().toLowerCase();
    if (!EXPORT_FORMATS.has(format)) {
      throw new Error(`export row ${index} format must be png/jpg/webp, got ${JSON.stringify(row.format)}`);
    }
    const resample = String(row.resample == null ? "lanczos" : row.resample).trim().toLowerCase();
    if (!EXPORT_RESAMPLE.has(resample)) {
      throw new Error(`export row ${index} resample must be lanczos/nearest, got ${JSON.stringify(row.resample)}`);
    }
    const clean = { scale, format, resample };
    if (format === "jpg" || format === "webp") {
      const raw = row.quality === undefined || row.quality === null || row.quality === "" ? 90 : Number(row.quality);
      if (!Number.isFinite(raw)) throw new Error(`export row ${index} quality must be a number 1-100`);
      clean.quality = Math.max(1, Math.min(100, Math.round(raw)));
    }
    if (row.base !== undefined && row.base !== null && row.base !== "") {
      const base = String(row.base).trim().toLowerCase();
      if (!EXPORT_BASES.has(base)) {
        throw new Error(`export row ${index} base must be source/canvas, got ${JSON.stringify(row.base)}`);
      }
      if (base === "source") clean.base = base; // "canvas" is the default: omit, keep JSON minimal
    }
    return clean;
  });
}

// Replace an element's export rows (the Figma Export section persisted on the layer).
// Journaled like setRegions: the before/after snapshot restores the previous rows on
// undo/redo. Both the page's Export section and the CLI export-set drive this one op.
export function setExportSettings(root, { projectId, elementId, rows } = {}) {
  if (!projectId) throw new Error("setExportSettings requires projectId");
  if (!elementId) throw new Error("setExportSettings requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const clean = cleanExportRows(rows, { rejectSuffix: true });
  const nextElements = (before.elements || []).map((item) =>
    item.id === elementId ? { ...item, export: clean } : item,
  );
  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "setExportSettings",
    args_summary: { elementId, row_count: clean.length },
    before,
    after,
    startedAt,
  });
  const updated = (project.elements || []).find((item) => item.id === elementId);
  return { project, element: updated, rows: (updated && updated.export) || [] };
}

// ---- project-level ops -------------------------------------------------------

// Instant-create default titles (Figma-style: no name prompt). Two small
// game-flavored word lists; the generator lives here in the op layer, not the
// page, so the CLI and the "+ New project" button get identical defaults.
// Create a project. `title` is optional: a missing/empty title gets a random
// "Adjective Noun" default (Title Case) instead of a name prompt. The id still
// derives from the (possibly generated) title via the store's existing slug
// scheme, so a random title yields ids like amber-fox-a1b2c3.
// Patch project-level metadata. Journaled like any metadata mutation: title,
// ownership, and archived live in the snapshot, so undo/redo restore them too.
export function patchProject(root, { projectId, title, ownership, gameId, archived } = {}) {
  if (!projectId) throw new Error("patchProject requires projectId");
  const hasTitle = title !== undefined;
  const args = arguments[1] || {};
  const hasOwnership = Object.hasOwn(args, "ownership") || Object.hasOwn(args, "gameId");
  const hasArchived = Object.hasOwn(args, "archived");
  if (!hasTitle && !hasOwnership && !hasArchived) throw new Error("patchProject requires a title, ownership, and/or archived");
  if (hasArchived && typeof archived !== "boolean") throw new Error("Canvas project archived must be boolean");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const patch = {};
  if (hasTitle) patch.title = String(title).trim() || before.title;
  if (hasOwnership) patch.ownership = normalizeProjectOwnership(ownership ?? gameId, { allowClear: true });
  if (hasArchived) patch.archived = archived;
  const after = updateProject(root, projectId, patch);
  const project = commitMutation(root, projectId, {
    op: "patchProject",
    args_summary: {
      ...(hasTitle ? { title: patch.title } : {}),
      ...(hasOwnership ? { ownership: patch.ownership || null } : {}),
      ...(hasArchived ? { archived } : {}),
    },
    before,
    after,
    startedAt,
  });
  return { project };
}
