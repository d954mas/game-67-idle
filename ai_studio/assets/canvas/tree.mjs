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

// Serialize ONE node into a paste-spec entry: a deep clone of the stored record MINUS
// the identity/placement keys the paste re-mints (id, groupId/parentId, order). A group
// carries its children (orderedChildren, back -> front) so the whole subtree round-trips
// with its internal z-order preserved. Image element clones keep `src` — files/ is
// immutable and content-addressed, so the spec stays valid even after the source is
// deleted (paste references the same immutable file).
function serializeNode(project, node) {
  if (node.kind === "element") {
    const element = JSON.parse(JSON.stringify(node.ref));
    delete element.id;
    delete element.groupId;
    delete element.order;
    return { kind: "element", element };
  }
  const group = JSON.parse(JSON.stringify(node.ref));
  delete group.id;
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
// buffer and the op layer's duplicateNodes.
export function buildNodesSpec(project, nodeIds) {
  const wanted = [...new Set((nodeIds || []).map((value) => String(value)))];
  const roots = wanted.map((id) => {
    const node = taggedNode(project, id);
    if (!node) throw new Error(`node not found: ${id}`);
    return node;
  });
  const scopeKey = (node) => {
    const scope = nodeScope(project, node.ref);
    return scope == null ? " root" : scope;
  };
  const orderCache = new Map();
  const zIndex = (node) => {
    const key = scopeKey(node);
    if (!orderCache.has(key)) {
      const scope = key === " root" ? null : key;
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
