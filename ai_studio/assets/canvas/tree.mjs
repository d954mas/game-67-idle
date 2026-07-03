// Canvas scene-tree math: per-scope children, computed z-order, subtree/ancestor
// walks, the visibility cascade, and the reparent cycle guard. PURE and
// dependency-free (like viewport.mjs) so ops.mjs imports it in node AND the site
// loads it statically over the /ai_studio/ route — paint order itself obeys tool
// parity: ONE implementation, two clients.
//
// Model (lead's call — flat Defold-style, NO stored tree): project.elements[] and
// project.groups[] are the source of truth; nesting is expressed by additive
// optional fields — element.groupId (its parent group; absent/null = root) and
// group.parentId (its parent group; absent/null = root). An optional numeric
// `order` field (on elements AND groups) is the explicit z-key among same-scope
// siblings. Z-order is COMPUTED here per scope, never persisted as a tree.
//
// Legacy (v1) projects carry no `order`/`parentId`: they fall back to elements[]
// array order (element key = its array index; a group anchors at its backmost
// subtree member), so a v1 project paints exactly as before except that a group's
// members now form one contiguous band anchored at the backmost member.
//
// All downward/upward walks are cycle-safe: a corrupt parent cycle is capped and a
// dangling/deleted parent resolves to root, so a bad project never hangs or hides
// content.

function groupsArr(project) {
  return Array.isArray(project && project.groups) ? project.groups : [];
}

function elementsArr(project) {
  return Array.isArray(project && project.elements) ? project.elements : [];
}

function groupMap(project) {
  const map = new Map();
  for (const group of groupsArr(project)) map.set(group.id, group);
  return map;
}

// The scope an element actually lives in: its groupId when that group exists, else
// root (null). A dangling groupId (its group was deleted out from under it) resolves
// to root so the element still paints instead of vanishing.
function elementScope(element, gmap) {
  const gid = element.groupId == null ? null : element.groupId;
  return gid != null && gmap.has(gid) ? gid : null;
}

// The scope a group lives in (its parentId when that group exists, else root).
function groupParent(group, gmap) {
  const pid = group.parentId == null ? null : group.parentId;
  return pid != null && gmap.has(pid) ? pid : null;
}

function nodeIsGroup(project, node) {
  return node != null && groupsArr(project).some((group) => group.id === node.id);
}

// The set of group ids in a group's subtree INCLUDING the group itself. Cycle-safe
// (a visited set stops a corrupt parentId ring), so it terminates on any input.
function subtreeGroupIds(project, groupId, gmap = groupMap(project)) {
  const ids = new Set();
  const stack = [groupId];
  while (stack.length) {
    const current = stack.pop();
    if (current == null || ids.has(current)) continue;
    ids.add(current);
    for (const group of groupsArr(project)) {
      if (groupParent(group, gmap) === current && !ids.has(group.id)) stack.push(group.id);
    }
  }
  return ids;
}

// Merged direct children of a scope (null/undefined = root): the elements and groups
// whose parent resolves to that scope, each in its native array order. Backs the
// site's memberElements/ungroupedElements and the paint/layers tree.
export function childrenOf(project, scopeId) {
  const scope = scopeId == null ? null : scopeId;
  const gmap = groupMap(project);
  const elements = elementsArr(project).filter((element) => elementScope(element, gmap) === scope);
  const groups = groupsArr(project).filter((group) => groupParent(group, gmap) === scope);
  return { elements, groups };
}

function hasNumericOrder(node) {
  return node != null && typeof node.order === "number" && Number.isFinite(node.order);
}

// The scope's children as tagged nodes ({ kind: "element"|"group", id, ref }) sorted
// BACK -> FRONT (index 0 = painted first/behind). When EVERY sibling carries a finite
// numeric `order`, that is the key; otherwise the v1 fallback keys an element on its
// elements[] index and a group on the MIN elements[]-index across its subtree members
// (anchored at the backmost member; an empty group sorts to the front / on top). Ties
// break by the merged array position (stable), so legacy projects reproduce v1 paint
// order exactly.
export function orderedChildren(project, scopeId) {
  const { elements, groups } = childrenOf(project, scopeId);
  const merged = [
    ...elements.map((ref) => ({ kind: "element", id: ref.id, ref })),
    ...groups.map((ref) => ({ kind: "group", id: ref.id, ref })),
  ];
  if (merged.length < 2) return merged;

  let keyOf;
  if (merged.every((node) => hasNumericOrder(node.ref))) {
    keyOf = (node) => node.ref.order;
  } else {
    const gmap = groupMap(project);
    const indexById = new Map(elementsArr(project).map((element, index) => [element.id, index]));
    keyOf = (node) => {
      if (node.kind === "element") return indexById.has(node.id) ? indexById.get(node.id) : 0;
      // Group key = backmost (min-index) subtree member; empty subtree => +Infinity
      // so it sorts to the front (on top).
      const ids = subtreeGroupIds(project, node.id, gmap);
      let min = Infinity;
      for (const element of elementsArr(project)) {
        if (ids.has(elementScope(element, gmap))) {
          const index = indexById.get(element.id);
          if (index < min) min = index;
        }
      }
      return min;
    };
  }

  return merged
    .map((node, index) => ({ node, index, key: keyOf(node) }))
    // Comparator avoids Infinity arithmetic (Infinity - Infinity = NaN would corrupt
    // the sort): compare keys directly, then fall back to the stable merged index.
    .sort((a, b) => (a.key === b.key ? a.index - b.index : a.key < b.key ? -1 : 1))
    .map((decorated) => decorated.node);
}

// Everything nested under a group: the descendant groups (excluding the group itself)
// and every element in the subtree. Used by later increments' move-cascade + render.
export function descendantsOf(project, groupId) {
  const gmap = groupMap(project);
  const ids = subtreeGroupIds(project, groupId, gmap);
  const groups = groupsArr(project).filter((group) => group.id !== groupId && ids.has(group.id));
  const elements = elementsArr(project).filter((element) => ids.has(elementScope(element, gmap)));
  return { groups, elements };
}

// The ancestor GROUP chain of a node (element or group), nearest first. Capped at
// groups.length steps with a visited set, so a corrupt parent cycle warns once and
// bottoms out at root instead of looping forever; a dangling parent is treated as root.
export function ancestorsOf(project, node) {
  const chain = [];
  if (!node) return chain;
  const gmap = groupMap(project);
  const startId = nodeIsGroup(project, node)
    ? (node.parentId == null ? null : node.parentId)
    : (node.groupId == null ? null : node.groupId);
  let current = startId;
  const seen = new Set();
  const cap = groupsArr(project).length;
  while (current != null) {
    if (seen.has(current) || chain.length >= cap) {
      console.warn(`canvas tree: corrupt parent cycle at group ${current}; treating as root`);
      break;
    }
    seen.add(current);
    const group = gmap.get(current);
    if (!group) break; // dangling parent -> root
    chain.push(group);
    current = group.parentId == null ? null : group.parentId;
  }
  return chain;
}

// A node is hidden when its own `visible === false` OR any ancestor group is hidden.
// Defensive: the ancestor walk is cycle-capped (see ancestorsOf), so a corrupt cycle
// can never infinite-loop here.
export function isNodeHidden(project, node) {
  if (!node) return false;
  if (node.visible === false) return true;
  for (const ancestor of ancestorsOf(project, node)) {
    if (ancestor.visible === false) return true;
  }
  return false;
}

// The resolved parent scope of a node (element OR group): the group id it lives in, or
// null for root. Mirrors orderedChildren's scope resolution (a dangling parent resolves
// to root), so the reorder op and the site compute the SAME sibling set the paint order
// uses. Backs reorderNode and the site's group/element z-order helpers.
export function nodeScope(project, node) {
  if (!node) return null;
  const gmap = groupMap(project);
  return nodeIsGroup(project, node) ? groupParent(node, gmap) : elementScope(node, gmap);
}

// The `order` value a NEW or MOVED node needs to sit at the FRONT of a scope while
// keeping that scope EXPLICIT — max existing sibling order + 1 — or null when the scope
// is not (yet) explicit (empty, or some sibling lacks a finite order), in which case the
// node stays order-less and the scope keeps painting by the v1 fallback. This is the
// "scopes never go half-explicit" hook: once a scope has been reordered (every sibling
// carries `order`), adding/moving a node into it assigns a front order so orderedChildren
// keeps honoring the explicit arrangement instead of silently reverting to array order.
export function frontOrder(project, scopeId) {
  const { elements, groups } = childrenOf(project, scopeId);
  const all = [...elements, ...groups];
  if (!all.length || !all.every(hasNumericOrder)) return null;
  return Math.max(...all.map((node) => node.order)) + 1;
}

// Arrange a scope's MERGED siblings (the tagged {kind, id, ref} nodes orderedChildren
// yields, back -> front) after moving the SELECTED ones as ONE block that keeps their
// relative order — the multi-node z-order math behind reorderNodes. `spec` is either
// { index } (insert the block at an absolute slot among the UNSELECTED siblings) or
// { direction } where direction is:
//   - "front": the block jumps to the very front (painted last / on top);
//   - "back":  the block jumps to the very back (painted first / behind);
//   - "forward"/"backward": the block nudges ONE step past its nearest unselected
//     neighbor, never passing selected items through each other (Figma bring-forward /
//     send-backward for a multi-selection).
// Pure: consumes and returns tagged-node arrays, never touches the project, so a block
// reorder is computed identically wherever it runs. Throws on a spec with neither a
// numeric index nor a known direction (no silent fallback).
export function blockReorder(siblings, selectedIds, spec = {}) {
  const selectedSet = selectedIds instanceof Set ? selectedIds : new Set(selectedIds);
  if (spec.index !== undefined && spec.index !== null && Number.isFinite(Number(spec.index))) {
    const selected = siblings.filter((node) => selectedSet.has(node.id));
    const others = siblings.filter((node) => !selectedSet.has(node.id));
    const at = Math.max(0, Math.min(others.length, Math.round(Number(spec.index))));
    return [...others.slice(0, at), ...selected, ...others.slice(at)];
  }
  const direction = String(spec.direction || "");
  if (direction === "front") {
    return [...siblings.filter((node) => !selectedSet.has(node.id)), ...siblings.filter((node) => selectedSet.has(node.id))];
  }
  if (direction === "back") {
    return [...siblings.filter((node) => selectedSet.has(node.id)), ...siblings.filter((node) => !selectedSet.has(node.id))];
  }
  const arranged = siblings.slice();
  if (direction === "forward") {
    // Walk front -> back so the whole block shifts up by one as a unit.
    for (let i = arranged.length - 2; i >= 0; i -= 1) {
      if (selectedSet.has(arranged[i].id) && !selectedSet.has(arranged[i + 1].id)) {
        const swap = arranged[i];
        arranged[i] = arranged[i + 1];
        arranged[i + 1] = swap;
      }
    }
    return arranged;
  }
  if (direction === "backward") {
    // Walk back -> front so the whole block shifts down by one as a unit.
    for (let i = 1; i < arranged.length; i += 1) {
      if (selectedSet.has(arranged[i].id) && !selectedSet.has(arranged[i - 1].id)) {
        const swap = arranged[i];
        arranged[i] = arranged[i - 1];
        arranged[i - 1] = swap;
      }
    }
    return arranged;
  }
  throw new Error("blockReorder requires a numeric index or a direction (front|back|forward|backward)");
}

// Would reparenting `groupId` under `newParentId` create a cycle? True when the new
// parent is the group itself or any group in its subtree; false for root (null) or an
// unrelated/ancestor target. Used by the (later) reparent op's guard.
export function wouldCycle(project, groupId, newParentId) {
  if (newParentId == null) return false;
  if (newParentId === groupId) return true;
  return subtreeGroupIds(project, groupId).has(newParentId);
}

// ---- rotation-aware geometry (T0232 increment 3a) ------------------------------
//
// `element.rotation` is DEGREES CLOCKWISE ON SCREEN about the node's own box center
// (x+w/2, y+h/2) — the one load-bearing convention the canvas (`ctx.rotate(+theta)`) and
// the PIL exporter (`Image.rotate(-theta, expand=True)`, negated because PIL's own angle
// is CCW-positive) must agree on; see README "Rotation & flip". `flipH`/`flipV` mirror
// pixels WITHIN the box and never change it, so only rotation affects extent below.

// The node's 4 world-space corners (top-left, top-right, bottom-right, bottom-left, in
// that order), rotated about the box center when `rotation` is set. Absent/zero rotation
// returns the plain box corners (cheap identity path — most nodes never rotate).
export function rotatedCorners(node) {
  const x = Number(node.x) || 0;
  const y = Number(node.y) || 0;
  const w = Number(node.w) || 0;
  const h = Number(node.h) || 0;
  const corners = [
    { x, y },
    { x: x + w, y },
    { x: x + w, y: y + h },
    { x, y: y + h },
  ];
  const rotation = Number(node.rotation) || 0;
  if (!rotation) return corners;
  const cx = x + w / 2;
  const cy = y + h / 2;
  const rad = (rotation * Math.PI) / 180;
  const cos = Math.cos(rad);
  const sin = Math.sin(rad);
  // World point = center + R(theta)*(local point - center), R(theta) matching
  // ctx.rotate(+theta) on a Y-down canvas (CW-positive) — see the file header.
  return corners.map((corner) => {
    const dx = corner.x - cx;
    const dy = corner.y - cy;
    return { x: cx + dx * cos - dy * sin, y: cy + dx * sin + dy * cos };
  });
}

// The axis-aligned bounding box of a node's (possibly rotated) corners — what
// createGroup(fromElements)/fitGroup pad around (ops.elementsBBox), so a rotated child's
// footprint is never clipped by a freshly-sized group frame. Identical to the node's own
// x/y/w/h box when unrotated.
export function nodeAABB(node) {
  const corners = rotatedCorners(node);
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const corner of corners) {
    minX = Math.min(minX, corner.x);
    minY = Math.min(minY, corner.y);
    maxX = Math.max(maxX, corner.x);
    maxY = Math.max(maxY, corner.y);
  }
  return { minX, minY, maxX, maxY, x: minX, y: minY, w: maxX - minX, h: maxY - minY };
}

// True once a node carries a nonzero rotation or either flip flag — the R7 refusal
// threshold: source-space ops (detectRegions/sliceRegions/alphaCutout, and the page's
// region-edit entry points) read UNTRANSFORMED source pixels, so they refuse loudly while
// this is true (reset rotation/flip first). Pure predicate shared by ops.mjs and the site
// so both refuse/gray out on the exact same condition.
export function isNodeTransformed(node) {
  if (!node) return false;
  return (Number(node.rotation) || 0) !== 0 || node.flipH === true || node.flipV === true;
}

// ---- align / distribute (T0232 increment 1) -----------------------------------
//
// Pure target-position math for the inspector's Align row + the CLI/API nodes-align /
// nodes-distribute verbs. Both compute [{nodeId, x, y}] ABSOLUTE top-lefts for only the
// nodes that actually need to move (an already-aligned/spaced node is omitted, so the op
// layer's commitMutation no-op guard sees "nothing changed" and writes no journal entry).
// Applying the moves — and cascading a moved GROUP over its subtree, overlap-safe — is the
// op layer's job: ops.alignNodes/distributeNodes feed these results through the SAME
// shared cascade moveNodes uses, so a group here behaves exactly like a group drag.

const ALIGN_KEYS = new Set(["left", "hcenter", "right", "top", "vcenter", "bottom", "center"]);
const DISTRIBUTE_AXES = new Set(["h", "v"]);

// A node's frame box — elements AND groups both carry x/y/w/h natively, so alignment
// treats them uniformly.
function nodeBox(node) {
  return { x: Number(node.x) || 0, y: Number(node.y) || 0, w: Number(node.w) || 0, h: Number(node.h) || 0 };
}

// Union bounding box of one or more box-like {x,y,w,h} records (elements, groups, or plain
// boxes) — also the shape the align reference frame is built from. Pure; throws on an
// empty list (no box to speak of; every caller here already guards a non-empty set).
export function unionBBox(boxes) {
  if (!Array.isArray(boxes) || !boxes.length) throw new Error("unionBBox requires a non-empty array of boxes");
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const box of boxes) {
    const b = nodeBox(box);
    minX = Math.min(minX, b.x);
    minY = Math.min(minY, b.y);
    maxX = Math.max(maxX, b.x + b.w);
    maxY = Math.max(maxY, b.y + b.h);
  }
  return { minX, minY, maxX, maxY, x: minX, y: minY, w: maxX - minX, h: maxY - minY };
}

// Resolve a node id to its tagged {kind, ref}; throws on an unknown id (align/distribute
// are always loud about a bad selection, never a silent skip).
function resolveNode(project, nodeId) {
  const node = taggedNode(project, nodeId);
  if (!node) throw new Error(`node not found: ${nodeId}`);
  return node;
}

// The ALIGN REFERENCE FRAME for a resolved node set (Figma-auto semantics — the lead's
// call, T0232): 2+ nodes align to the UNION bounding box of the selection; EXACTLY 1 node
// aligns to its PARENT GROUP's frame (the "center this widget inside the screen" case that
// directly serves screen assembly); 1 node with no parent is a loud error (nothing to align
// to). `reference` can force a mode: "selection" always wants the union bbox (2+ nodes — a
// single node's own bbox is a no-op reference, not useful); "parent" always wants the
// parent frame (a single node only — a multi-node "each to its own parent" is a different
// feature, not v1).
function resolveAlignReference(project, nodes, reference) {
  const mode = reference == null || reference === "" ? "auto" : String(reference);
  if (mode !== "auto" && mode !== "selection" && mode !== "parent") {
    throw new Error(`alignNodes reference must be auto/selection/parent, got ${JSON.stringify(reference)}`);
  }
  if (mode === "selection" || (mode === "auto" && nodes.length >= 2)) {
    if (nodes.length < 2) throw new Error('alignNodes reference "selection" requires 2+ nodes');
    return unionBBox(nodes.map((node) => node.ref));
  }
  if (nodes.length !== 1) {
    throw new Error('alignNodes reference "parent" requires exactly 1 node');
  }
  const [node] = nodes;
  const scopeId = nodeScope(project, node.ref);
  if (scopeId == null) {
    throw new Error("alignNodes: select 2+ objects, or one object inside a screen (group)");
  }
  const group = groupsArr(project).find((item) => item.id === scopeId);
  // nodeScope already resolves a dangling parent to root, so this should always hit;
  // defensive throw keeps the error loud rather than silently returning an undefined box.
  if (!group) throw new Error(`alignNodes: parent group not found: ${scopeId}`);
  return unionBBox([group]);
}

// alignMoves(project, nodeIds, align, reference?) -> [{nodeId, x, y}]
// Each node aligns by ITS OWN frame box against the resolved reference box: left ->
// x=ref.minX; hcenter -> x=ref.x+ref.w/2-w/2; right -> x=ref.maxX-w; top/vcenter/bottom
// analogous on y. Loud + pure: an empty nodeIds list, an unknown align key, an unknown
// node id, or an unresolvable reference (see resolveAlignReference) throws before
// returning anything.
export function alignMoves(project, nodeIds, align, reference) {
  const ids = (nodeIds || []).map((value) => String(value));
  if (!ids.length) throw new Error("alignNodes requires a non-empty nodeIds array");
  if (!ALIGN_KEYS.has(align)) {
    throw new Error(`alignNodes align must be one of ${[...ALIGN_KEYS].join("/")}, got ${JSON.stringify(align)}`);
  }
  const nodes = ids.map((id) => ({ id, ref: resolveNode(project, id).ref }));
  const ref = resolveAlignReference(project, nodes, reference);
  const moves = [];
  for (const node of nodes) {
    const box = nodeBox(node.ref);
    let x = box.x;
    let y = box.y;
    switch (align) {
      case "left": x = ref.minX; break;
      case "hcenter": x = ref.x + ref.w / 2 - box.w / 2; break;
      case "right": x = ref.maxX - box.w; break;
      case "top": y = ref.minY; break;
      case "vcenter": y = ref.y + ref.h / 2 - box.h / 2; break;
      case "bottom": y = ref.maxY - box.h; break;
      // Both axes at once (one journal entry): a member dropped far from its card/group
      // snaps to the frame's center in ONE gesture instead of hcenter+vcenter (= 2 undos).
      case "center":
        x = ref.x + ref.w / 2 - box.w / 2;
        y = ref.y + ref.h / 2 - box.h / 2;
        break;
      default: break; // unreachable — align is validated above
    }
    if (x !== box.x || y !== box.y) moves.push({ nodeId: node.id, x, y });
  }
  return moves;
}

// distributeMoves(project, nodeIds, axis) -> [{nodeId, x, y}]
// Sorts the nodes' boxes by their MIN edge along `axis` (x for "h", y for "v"), then
// respaces them so every gap between adjacent boxes is equal — the two extreme boxes (by
// sorted position) stay exactly where they are (Figma "distribute spacing"); only interior
// boxes move. Needs 3+ nodes (2 boxes already have exactly one gap — nothing to equalize).
// Loud + pure: <3 nodes, an unknown axis, or an unknown node id throws before returning
// anything.
export function distributeMoves(project, nodeIds, axis) {
  const ids = (nodeIds || []).map((value) => String(value));
  if (!DISTRIBUTE_AXES.has(axis)) {
    throw new Error(`distributeNodes axis must be "h" or "v", got ${JSON.stringify(axis)}`);
  }
  if (ids.length < 3) throw new Error("distributeNodes requires 3+ nodes");
  const posKey = axis === "h" ? "x" : "y";
  const sizeKey = axis === "h" ? "w" : "h";
  const boxed = ids.map((id) => ({ id, box: nodeBox(resolveNode(project, id).ref) })); // throws on an unknown id
  const sorted = boxed.slice().sort((a, b) => a.box[posKey] - b.box[posKey]);
  const first = sorted[0];
  const last = sorted[sorted.length - 1];
  const span = last.box[posKey] + last.box[sizeKey] - first.box[posKey];
  const totalSize = sorted.reduce((sum, item) => sum + item.box[sizeKey], 0);
  const gap = (span - totalSize) / (sorted.length - 1);
  const moves = [];
  let cursor = first.box[posKey];
  for (const item of sorted) {
    const target = cursor;
    if (target !== item.box[posKey]) {
      moves.push({ nodeId: item.id, x: axis === "h" ? target : item.box.x, y: axis === "h" ? item.box.y : target });
    }
    cursor = target + item.box[sizeKey] + gap;
  }
  return moves;
}

// ---- copy/paste node spec (T0227) --------------------------------------------
//
// The serialized shape a Ctrl+C snapshot / CLI nodes-duplicate builds and the paste
// op (ops.pasteNodes) instantiates. PURE spec math, so the page and the agent capture
// a subtree identically — tool parity for the copy buffer too.
export const NODES_SPEC_SCHEMA = "ai_studio.canvas.nodes_spec.v1";

// Resolve a node id to a tagged {kind, ref} using the op layer's disjoint-namespace rule
// (element ids and group ids never collide). null when the id is unknown.
function taggedNode(project, nodeId) {
  const element = elementsArr(project).find((item) => item.id === nodeId);
  if (element) return { kind: "element", ref: element };
  const group = groupsArr(project).find((item) => item.id === nodeId);
  if (group) return { kind: "group", ref: group };
  return null;
}

// Serialize ONE node into a paste-spec entry: a deep clone of the stored record MINUS the
// PLACEMENT keys the paste re-mints (groupId/parentId, order — a pasted node always lands
// fresh into whatever scope/z-slot the paste call chooses). `id` is KEPT (T0239-3 fix, was
// deleted before): pasteNodes always mints a FRESH id for every instantiated node regardless
// (`{...def, id: fresh}` — the explicit id always wins the spread), so keeping the ORIGINAL
// id here is purely additive — nothing downstream that reads a serialized node needs it. What
// it buys: an id-carrying blob field (a style card's `style.ref`, a recipe card's
// `recipe.style_ref` — both bare element/group id POINTERS, not placement) can be remapped
// at paste time from "the original node's id" to "this paste's own fresh copy of that same
// node" instead of dangling — see pasteNodes' pointer-remap step in ops.mjs. A group carries
// its children (orderedChildren, back -> front) so the whole subtree round-trips with its
// internal z-order preserved. Image element clones keep `src` — files/ is immutable and
// content-addressed, so the spec stays valid even after the source is deleted (paste
// references the same immutable file).
function serializeNode(project, node) {
  if (node.kind === "element") {
    const element = JSON.parse(JSON.stringify(node.ref));
    delete element.groupId;
    delete element.order;
    return { kind: "element", element };
  }
  const group = JSON.parse(JSON.stringify(node.ref));
  delete group.parentId;
  delete group.order;
  const children = orderedChildren(project, node.ref.id).map((child) =>
    serializeNode(project, { kind: child.kind, ref: child.ref }),
  );
  return { kind: "group", group, children };
}

// Build a serializable spec of the deep subtree(s) under the given node ids (elements
// AND groups, mixed OK). PURE — reads the project only, never mutates. Roots are emitted
// back -> front within each scope (grouped by first-seen scope), so paste preserves the
// selection's relative z-order. Throws on an unknown id. Backs the page's Ctrl+C copy
// buffer and the op layer's duplicateNodes. Every serialized node CARRIES its original id
// (serializeNode) — pasteNodes still always mints a fresh one on instantiation, so this
// is not identity leakage; it is what lets pasteNodes remap an id-pointer field (style.ref,
// recipe.style_ref) from the original node to its own freshly-pasted copy instead of
// leaving it dangling.
export function buildNodesSpec(project, nodeIds) {
  const wanted = [...new Set((nodeIds || []).map((value) => String(value)))];
  const roots = wanted.map((id) => {
    const node = taggedNode(project, id);
    if (!node) throw new Error(`node not found: ${id}`);
    return node;
  });
  const scopeKey = (node) => {
    const scope = nodeScope(project, node.ref);
    return scope == null ? "\u0000root" : scope;
  };
  const orderCache = new Map();
  const zIndex = (node) => {
    const key = scopeKey(node);
    if (!orderCache.has(key)) {
      const scope = key === "\u0000root" ? null : key;
      orderCache.set(key, orderedChildren(project, scope).map((child) => child.id));
    }
    const idx = orderCache.get(key).indexOf(node.ref.id);
    return idx === -1 ? Number.MAX_SAFE_INTEGER : idx;
  };
  const scopeOrder = [];
  const scopeRank = (node) => {
    const key = scopeKey(node);
    let rank = scopeOrder.indexOf(key);
    if (rank === -1) {
      rank = scopeOrder.length;
      scopeOrder.push(key);
    }
    return rank;
  };
  const decorated = roots.map((node, index) => ({ node, index, rank: scopeRank(node), z: zIndex(node) }));
  decorated.sort((a, b) => (a.rank !== b.rank ? a.rank - b.rank : a.z !== b.z ? a.z - b.z : a.index - b.index));
  return { schema: NODES_SPEC_SCHEMA, nodes: decorated.map((entry) => serializeNode(project, entry.node)) };
}
