// Canvas groups operation domain. Public API is ../ops.mjs.
import { randomUUID } from "node:crypto";
import { performance } from "node:perf_hooks";
import { descendantsOf, frontOrder, nodeAABB, nodeScope, orderedChildren, scaleGroupMoves, wouldCycle } from "../tree.mjs";
import { getProject, updateProject } from "../store.mjs";
import { commitMutation, finite, groupsOf, hexColor } from "./core.mjs";

// Groups are Figma-frame-like screen regions. Group, membership, visibility,
// and fit mutations share the same one-entry journal contract as element ops.

export function findGroup(project, groupId) {
  const group = groupsOf(project).find((item) => item.id === groupId);
  if (!group) throw new Error(`group not found: ${groupId}`);
  return group;
}

export function applyStyleAutoRef(groups, membershipChanges) {
  return groups.map((group) => {
    if (!group.style || typeof group.style !== "object" || group.style.ref) return group;
    for (const [elementId, change] of membershipChanges) {
      if (change && change.groupId === group.id && change.type === "image") {
        return { ...group, style: { ...group.style, ref: elementId } };
      }
    }
    return group;
  });
}

// Validate + normalize an optional group background (additive field). Accepts null
// (clear) or {type:"color", color:"#rrggbb"}; anything else throws a loud error (no
// silent fallback). Returns null or the normalized {type:"color", color} object.
export function normalizeGroupBackground(background) {
  if (background === null) return null;
  if (typeof background !== "object" || Array.isArray(background)) {
    throw new Error(`group background must be null or {type:"color", color:"#rrggbb"}, got ${JSON.stringify(background)}`);
  }
  if (background.type !== "color") {
    throw new Error(`group background type must be "color", got ${JSON.stringify(background.type)}`);
  }
  const color = hexColor(background.color);
  if (!color) throw new Error(`group background color must be #rrggbb, got ${JSON.stringify(background.color)}`);
  return { type: "color", color };
}

// Validate an optional group clip flag (additive field). Accepts only a real boolean;
// anything else is a loud error (no silent coercion — the CLI converts its string flag
// before calling). Returns the boolean; `false` is the "unclipped" default that patchGroup
// stores as an ABSENT field (mirrors background:null), so an untouched group stays clean.
export function normalizeGroupClip(clip) {
  if (typeof clip !== "boolean") {
    throw new Error(`group clip must be a boolean (true|false), got ${JSON.stringify(clip)}`);
  }
  return clip;
}

// Validate an optional group `screen` flag (additive field, T0332 build-spec "ЭКСПОРТ —
// ИНВЕРСИЯ НА OPT-IN", lead 2026-07-07: "чтобы группа считалась экраном и экспортировалась,
// я явно ставлю галочку"). This is now the ONLY thing that makes a top-level group an
// exportable screen: exportProject/visibleScreenCount both filter on `screen === true` —
// the old implicit "every top-level visible group except a recipe/style card" rule is gone
// (see exportProject's own doc). Same shape as clip: a real boolean only (no silent coercion
// — the CLI's own parseBool converts its string flag first), and `false` is stored as an
// ABSENT field (mirrors clip/background), so an untouched or opted-out group stays clean and
// re-sending `false` on an already-unflagged group is a no-op.
export function normalizeGroupScreen(screen) {
  if (typeof screen !== "boolean") {
    throw new Error(`group screen must be a boolean (true|false), got ${JSON.stringify(screen)}`);
  }
  return screen;
}

// Union bbox of a set of elements/groups, per-node via tree.nodeAABB (T0232 increment
// 3a) so a ROTATED element's footprint — not its stored x/y/w/h — is what createGroup
// (fromElements) pads around and fitGroup fits to; unrotated nodes are unaffected
// (nodeAABB is an identity pass-through when rotation is absent/0).
export function elementsBBox(elements) {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const element of elements) {
    const box = nodeAABB(element);
    minX = Math.min(minX, box.minX);
    minY = Math.min(minY, box.minY);
    maxX = Math.max(maxX, box.maxX);
    maxY = Math.max(maxY, box.maxY);
  }
  return { minX, minY, maxX, maxY };
}

// Create a screen group. Either explicit bounds (x/y/w/h) OR fromElements: an
// array of element ids whose bounding box (+24px padding) becomes the frame and
// which are assigned this group. Optional `parentId` NESTS the new group inside an
// existing group (validated; null/absent = a top-level screen); for fromElements a
// missing parentId defaults to the members' COMMON groupId (nest a widget group
// inside the screen it was built from), root when they differ. One journal entry.
export function createGroup(root, { projectId, name, x, y, w, h, fromElements, parentId } = {}) {
  if (!projectId) throw new Error("createGroup requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const cleanName = String(name || "").trim() || "Group";

  let bounds;
  let memberIds = [];
  let members = [];
  if (Array.isArray(fromElements) && fromElements.length) {
    memberIds = fromElements.map(String);
    members = memberIds.map((id) => {
      const element = (before.elements || []).find((item) => item.id === id);
      if (!element) throw new Error(`element not found: ${id}`);
      return element;
    });
    const pad = 24;
    const { minX, minY, maxX, maxY } = elementsBBox(members);
    bounds = { x: minX - pad, y: minY - pad, w: maxX - minX + pad * 2, h: maxY - minY + pad * 2 };
  } else {
    if (!finite(w) || !finite(h) || Number(w) <= 0 || Number(h) <= 0) {
      throw new Error("createGroup requires fromElements or positive w/h bounds");
    }
    bounds = { x: finite(x) ? Number(x) : 0, y: finite(y) ? Number(y) : 0, w: Number(w), h: Number(h) };
  }

  // Resolve the parent scope. An explicit parentId (validated below) wins; else, for
  // fromElements, the members' common groupId; else root.
  let parentScope;
  if (parentId !== undefined) {
    parentScope = parentId == null || parentId === "" ? null : String(parentId);
  } else if (members.length) {
    const scopes = new Set(members.map((m) => (m.groupId == null || m.groupId === "" ? null : String(m.groupId))));
    parentScope = scopes.size === 1 ? [...scopes][0] : null;
  } else {
    parentScope = null;
  }
  if (parentScope != null) findGroup(before, parentScope); // loud error on an unknown parent

  const group = { id: groupId, name: cleanName, x: bounds.x, y: bounds.y, w: bounds.w, h: bounds.h, visible: true };
  if (parentScope != null) group.parentId = parentScope;
  // Keep an explicitly-ordered destination scope explicit by giving the new group a
  // front order (no-op on a never-reordered scope).
  const groupFront = frontOrder(before, parentScope);
  if (groupFront !== null) group.order = groupFront;
  const memberSet = new Set(memberIds);
  // Members entering the fresh group scope drop any stale `order` from their old scope,
  // so the new group starts implicit (v1 array-order fallback) rather than half-explicit.
  const nextElements = (before.elements || []).map((element) => {
    if (!memberSet.has(element.id)) return element;
    const moved = { ...element, groupId };
    delete moved.order;
    return moved;
  });
  const after = updateProject(root, projectId, {
    groups: [...groupsOf(before), group],
    elements: nextElements,
  });
  const project = commitMutation(root, projectId, {
    op: "createGroup",
    args_summary: { groupId, name: cleanName, members: memberIds, bounds, parentId: parentScope },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Patch a group's name/bounds/visibility/background/clip. When x or y change, translate
// the group's FULL descendant closure by the same delta — nested subgroup frames AND
// every element in the subtree — so the whole screen (and its nested widget groups)
// moves as one; resize (w/h) never moves members. `background` is the optional solid
// fill (null clears it; {type:"color", color:"#rrggbb"} sets it — validated, no silent
// fallback). `clip` is the optional Figma-frame clip flag: `true` clips members to the
// group bounds on canvas AND in the subgroup render; `false` (the default) clears it and
// is stored as an ABSENT field, so an untouched group stays clean and clip:false on an
// already-unclipped group makes no change (no journal entry). `screen` (T0332 B1) is the
// export opt-in flag: `true` makes this top-level group an exportable screen
// (exportProject/visibleScreenCount); `false` (the default) clears it to an ABSENT field,
// same convention as clip. One journal entry restores everything on undo.
export function patchGroup(root, { projectId, groupId, name, x, y, w, h, visible, background, clip, screen } = {}) {
  if (!projectId) throw new Error("patchGroup requires projectId");
  if (!groupId) throw new Error("patchGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const current = findGroup(before, groupId);

  const dx = finite(x) ? Number(x) - Number(current.x || 0) : 0;
  const dy = finite(y) ? Number(y) - Number(current.y || 0) : 0;
  // Validate background + clip + screen BEFORE any write so an invalid value throws atomically.
  const bgProvided = background !== undefined;
  const bgResolved = bgProvided ? normalizeGroupBackground(background) : undefined;
  const clipProvided = clip !== undefined;
  const clipResolved = clipProvided ? normalizeGroupClip(clip) : undefined;
  const screenProvided = screen !== undefined;
  const screenResolved = screenProvided ? normalizeGroupScreen(screen) : undefined;

  // On a move, gather the FULL descendant closure once: nested subgroup frames AND
  // every element in the subtree translate with the group.
  const moving = dx !== 0 || dy !== 0;
  const descendants = moving ? descendantsOf(before, groupId) : { groups: [], elements: [] };
  const descGroupIds = new Set(descendants.groups.map((g) => g.id));
  const descElementIds = new Set(descendants.elements.map((e) => e.id));

  const nextGroups = groupsOf(before).map((group) => {
    if (group.id === groupId) {
      const patched = { ...group };
      if (name !== undefined) patched.name = String(name);
      if (finite(x)) patched.x = Number(x);
      if (finite(y)) patched.y = Number(y);
      if (finite(w)) patched.w = Number(w);
      if (finite(h)) patched.h = Number(h);
      if (visible !== undefined) patched.visible = !(visible === false || visible === "false");
      if (bgProvided) {
        if (bgResolved === null) delete patched.background; // "None" -> absent field
        else patched.background = bgResolved;
      }
      if (clipProvided) {
        if (clipResolved === false) delete patched.clip; // unclipped -> absent field
        else patched.clip = true;
      }
      if (screenProvided) {
        if (screenResolved === false) delete patched.screen; // opted out -> absent field
        else patched.screen = true;
      }
      return patched;
    }
    // A nested subgroup frame translates with the closure (its own members are in the
    // element closure below).
    if (moving && descGroupIds.has(group.id)) {
      return { ...group, x: (Number(group.x) || 0) + dx, y: (Number(group.y) || 0) + dy };
    }
    return group;
  });

  const nextElements = moving
    ? (before.elements || []).map((element) =>
        descElementIds.has(element.id)
          ? { ...element, x: (Number(element.x) || 0) + dx, y: (Number(element.y) || 0) + dy }
          : element,
      )
    : (before.elements || []);

  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "patchGroup",
    args_summary: { groupId, name, x, y, w, h, visible, dx, dy, background: bgProvided ? bgResolved : undefined, clip: clipProvided ? clipResolved : undefined, screen: screenProvided ? screenResolved : undefined },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Patch several groups with the SAME shared field(s) in ONE journaled gesture — the
// multi-group inspector's shared toggles (Visible / Clip). Only fields that make sense to
// set uniformly across a selection are honored here: `visible` and `clip`. Per-group
// geometry (x/y/w/h/name/background) is intentionally NOT batched (moves would need the
// subtree cascade; a shared name/color is meaningless across a selection). Loud + atomic:
// every id must resolve (throws before any write), `clip` is validated once, and at least
// one of visible/clip must be provided. The whole batch is ONE commitMutation, so a single
// undo restores every group. `clip:false` clears the flag to an ABSENT field (mirrors
// patchGroup), so a no-op toggle on already-unclipped groups changes nothing.
export function patchGroups(root, { projectId, groupIds, visible, clip } = {}) {
  if (!projectId) throw new Error("patchGroups requires projectId");
  if (!Array.isArray(groupIds) || !groupIds.length) throw new Error("patchGroups requires a non-empty groupIds array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const ids = groupIds.map((value) => String(value));
  for (const groupId of ids) findGroup(before, groupId); // atomic: throws on an unknown id
  const idSet = new Set(ids);

  const visProvided = visible !== undefined;
  const clipProvided = clip !== undefined;
  if (!visProvided && !clipProvided) throw new Error("patchGroups requires at least one of visible, clip");
  const clipResolved = clipProvided ? normalizeGroupClip(clip) : undefined; // validate before any write

  const nextGroups = groupsOf(before).map((group) => {
    if (!idSet.has(group.id)) return group;
    const patched = { ...group };
    if (visProvided) patched.visible = !(visible === false || visible === "false");
    if (clipProvided) {
      if (clipResolved === false) delete patched.clip;
      else patched.clip = true;
    }
    return patched;
  });
  const after = updateProject(root, projectId, { groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "patchGroups",
    args_summary: {
      groupIds: ids,
      count: ids.length,
      visible: visProvided ? !(visible === false || visible === "false") : undefined,
      clip: clipProvided ? clipResolved : undefined,
    },
    before,
    after,
    startedAt,
  });
  return { project, groups: (project.groups || []).filter((group) => idSet.has(group.id)), count: ids.length };
}

// ---- migrateScreenFlags (T0332 B1: export opt-in inversion, one-shot) ----------------

// One-shot per-PROJECT migration for the export opt-in flip (build-spec "ЭКСПОРТ —
// ИНВЕРСИЯ НА OPT-IN"): before this increment, exportProject exported every top-level
// VISIBLE group except a recipe/style card; after it, exportProject exports ONLY a group
// carrying the explicit `screen === true` flag. This restores an EXISTING project's exact
// pre-flip export set by flagging every top-level VISIBLE group that carries none of
// recipe/style/anim/pack_run (the same objects the old filter excluded — the anim card
// is a workshop object exactly like recipe/style — plus a pack run group —
// build-spec: pack_run is provenance-only post-flip, never auto-flagged as a screen, even
// though the OLD filter would technically have exported one — see the caller's own doc for
// why this is a deliberate, not an accidental, byte-exact gap).
//
// Idempotent: a group that ALREADY carries a `screen` key (true OR false — an explicit
// prior choice, this migration or a hand-set patchGroup) is left untouched, so re-running
// this on an already-migrated (or partially hand-edited) project changes nothing further.
// ONE journal entry per project even when it flags zero groups — commitMutation's own
// before-equals-after check no-ops that case silently (no empty journal noise).
export function migrateScreenFlags(root, { projectId } = {}) {
  if (!projectId) throw new Error("migrateScreenFlags requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const flagged = [];
  const nextGroups = groupsOf(before).map((group) => {
    if (group.parentId != null) return group; // nested: never a top-level screen candidate
    if (group.visible === false) return group; // hidden: the old filter excluded it too
    if ("screen" in group) return group; // an explicit prior choice (migration or manual) stands
    if (group.recipe || group.style || group.anim || group.pack_run) return group; // workshop/run object (recipe/style/anim card or pack run)
    flagged.push(group.id);
    return { ...group, screen: true };
  });
  const after = updateProject(root, projectId, { groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "migrateScreenFlags",
    args_summary: { flagged, count: flagged.length },
    before,
    after,
    startedAt,
  });
  return { project, flagged };
}

// Resize a group's frame to fit its content (Figma "Resize to fit"). The new frame is
// the union bounding box of the group's FULL descendant closure — every descendant
// element AND every nested subgroup frame (both carry x/y/w/h; reuses the same
// elementsBBox math createGroup/sliceRegions use) — expanded by `padding` on all sides
// (default 24, the shared slice/group-create pad). Children NEVER move: only the group's
// own x/y/w/h change, so with clip=true the new frame re-evaluates the clip (the whole
// point of the button). An empty group (no descendant content) is a loud error, as is a
// non-finite or negative padding (no silent fallback). One journal entry; undo restores
// the old frame. `background`/`clip`/`parentId`/`order`/`name`/`visible` are preserved.
export function fitGroup(root, { projectId, groupId, padding } = {}) {
  if (!projectId) throw new Error("fitGroup requires projectId");
  if (!groupId) throw new Error("fitGroup requires groupId");
  const pad = padding === undefined || padding === null ? 24 : Number(padding);
  if (!Number.isFinite(pad) || pad < 0) {
    throw new Error(`fitGroup padding must be a finite number >= 0, got ${JSON.stringify(padding)}`);
  }
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  findGroup(before, groupId); // loud error on an unknown group
  const descendants = descendantsOf(before, groupId);
  const boxes = [...descendants.elements, ...descendants.groups];
  if (!boxes.length) throw new Error(`group ${groupId} has nothing to fit (no descendant content)`);
  const { minX, minY, maxX, maxY } = elementsBBox(boxes);
  const frame = { x: minX - pad, y: minY - pad, w: maxX - minX + pad * 2, h: maxY - minY + pad * 2 };

  const nextGroups = groupsOf(before).map((group) =>
    group.id === groupId ? { ...group, x: frame.x, y: frame.y, w: frame.w, h: frame.h } : group,
  );
  const after = updateProject(root, projectId, { groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "fitGroup",
    args_summary: { groupId, padding: pad, x: frame.x, y: frame.y, w: frame.w, h: frame.h },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Scale a group's FULL subtree to a new frame in ONE journaled gesture (T0271: the lead's
// override of T0232 Q2's shipped default — dragging a group's scale handles now scales its
// CONTENT by default; a Ctrl-held drag keeps the old frame-only `patchGroup` path). Unlike
// `patchGroup` (which only ever moves/resizes the group's OWN frame, pinning members),
// this computes the group's own frame AND every descendant element/nested-group's box from
// the PURE mapping `tree.scaleGroupMoves` gives (the group's CURRENT frame -> the requested
// `{x,y,w,h}`) and commits them all together — the page never has to compute/send
// descendant patches itself (so page and op can never disagree on the math). A text
// descendant's `fontSize` scales instead of its box (see `scaleGroupMoves`); every other
// node's box is remapped as-is, `rotation` untouched. Loud + atomic: an unknown group, a
// non-finite `x`/`y`, or a non-positive `w`/`h` throws before any write (`scaleGroupMoves`
// itself validates, before `updateProject` is ever called). A same-frame call writes no
// journal entry (commitMutation's own no-op guard — `scaleGroupMoves`' patches come out
// identical to the current state). One commitMutation, so a single undo restores the group
// AND every descendant exactly.
export function scaleGroup(root, { projectId, groupId, x, y, w, h } = {}) {
  if (!projectId) throw new Error("scaleGroup requires projectId");
  if (!groupId) throw new Error("scaleGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const patches = scaleGroupMoves(before, groupId, { x, y, w, h }); // pure; throws before any write

  const groupPatchById = new Map();
  const elementPatchById = new Map();
  for (const patch of patches) {
    if (patch.kind === "group") groupPatchById.set(patch.id, patch);
    else elementPatchById.set(patch.id, patch);
  }

  const nextGroups = groupsOf(before).map((group) => {
    const patch = groupPatchById.get(group.id);
    return patch ? { ...group, x: patch.x, y: patch.y, w: patch.w, h: patch.h } : group;
  });
  const nextElements = (before.elements || []).map((element) => {
    const patch = elementPatchById.get(element.id);
    if (!patch) return element;
    const next = { ...element, x: patch.x, y: patch.y };
    if (patch.fontSize !== undefined) {
      next.style = { ...next.style, fontSize: patch.fontSize };
    } else {
      next.w = patch.w;
      next.h = patch.h;
    }
    return next;
  });

  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "scaleGroup",
    args_summary: { groupId, x, y, w, h, count: patches.length },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Assign elements to a group (groupId) or clear their group (groupId=null). One
// journal entry.
export function assignToGroup(root, { projectId, elementIds, groupId } = {}) {
  if (!projectId) throw new Error("assignToGroup requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const ids = Array.isArray(elementIds) ? elementIds.map(String) : [];
  if (!ids.length) throw new Error("assignToGroup requires elementIds");
  const target = groupId == null || groupId === "" ? null : String(groupId);
  if (target) findGroup(before, target);
  const idSet = new Set(ids);
  for (const id of ids) {
    if (!(before.elements || []).some((item) => item.id === id)) throw new Error(`element not found: ${id}`);
  }
  // Scope-change order rule (scopes never go half-explicit): when the destination scope
  // is explicitly ordered, each moved element gets a fresh FRONT order (stacked in
  // elements[] order); otherwise its `order` is dropped so it sorts by the v1 fallback in
  // the new scope and never leaves a stale key from its previous scope behind.
  let fo = frontOrder(before, target);
  const nextElements = (before.elements || []).map((element) => {
    if (!idSet.has(element.id)) return element;
    const moved = { ...element, groupId: target };
    if (fo === null) delete moved.order;
    else moved.order = fo++;
    return moved;
  });
  // Auto-ref (R1 increment 3, applyStyleAutoRef — defined in the style-card section below;
  // hoisted): moving an IMAGE INTO a STYLE CARD whose ref is still null claims that ref, in
  // this SAME commit. A no-op when leaving a group (target null) — leaving never sets a ref.
  let nextGroups = groupsOf(before);
  if (target) {
    const membershipChanges = new Map(
      ids.map((id) => [id, { groupId: target, type: (before.elements || []).find((item) => item.id === id).type }]),
    );
    nextGroups = applyStyleAutoRef(nextGroups, membershipChanges);
  }
  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "assignToGroup",
    args_summary: { elementIds: ids, groupId: target },
    before,
    after,
    startedAt,
  });
  return { project, count: ids.length, groupId: target };
}

// Deleting a group deletes its ENTIRE SUBTREE with it (lead 2026-07-02: a group is a
// container — dissolving one without deleting content is Ungroup). The full closure —
// nested subgroups AND every element in the subtree — goes in ONE journal entry; undo
// restores all of it. Member image files stay in files/ (non-destructive storage).
export function deleteGroup(root, { projectId, groupId } = {}) {
  if (!projectId) throw new Error("deleteGroup requires projectId");
  if (!groupId) throw new Error("deleteGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  findGroup(before, groupId);
  const descendants = descendantsOf(before, groupId);
  const removedGroupIds = new Set([groupId, ...descendants.groups.map((group) => group.id)]);
  const removedElementIds = new Set(descendants.elements.map((element) => element.id));
  const nextGroups = groupsOf(before).filter((group) => !removedGroupIds.has(group.id));
  const removedElements = (before.elements || []).filter((element) => removedElementIds.has(element.id));
  const nextElements = (before.elements || []).filter((element) => !removedElementIds.has(element.id));
  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "deleteGroup",
    args_summary: {
      groupId,
      deletedGroups: [...removedGroupIds],
      deletedElements: removedElements.map((element) => element.id),
    },
    before,
    after,
    startedAt,
  });
  return {
    project,
    removed: groupId,
    removedGroups: [...removedGroupIds],
    removedElements: removedElements.map((element) => element.id),
  };
}

// Move a group under a new parent (null = root) at an optional merged-sibling `index`
// (default = front of the destination scope). CYCLE GUARD: reject a parent that is the
// group itself or any group in its subtree (tree.wouldCycle) — a loud error, never a
// silent no-op. Order handling mirrors the "scopes never go half-explicit" invariant:
//   - with an explicit `index`, assign contiguous order 0..N over the destination's new
//     arrangement (destination becomes explicit — the reorderNode normalization);
//   - without an index (front), give the group a FRONT order iff the destination scope
//     is already explicit, else drop its (now-stale) order so the scope stays implicit.
// The group's old scope keeps its remaining siblings' orders (still explicit, gaps are
// harmless); the moved group never leaves a stale order behind. One journal entry.
export function reparentGroup(root, { projectId, groupId, parentId, index } = {}) {
  if (!projectId) throw new Error("reparentGroup requires projectId");
  if (!groupId) throw new Error("reparentGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  findGroup(before, groupId); // loud error on an unknown group
  const target = parentId == null || parentId === "" ? null : String(parentId);
  if (target != null) findGroup(before, target); // loud error on an unknown parent
  if (wouldCycle(before, groupId, target)) {
    throw new Error(
      `reparentGroup would create a cycle: cannot move ${groupId} under ${
        target === groupId ? "itself" : `its own descendant ${target}`
      }`,
    );
  }

  const hasIndex = index !== undefined && index !== null && Number.isFinite(Number(index));

  // The moved group's new parentId (root => drop the field).
  const withParent = (group) => {
    const next = { ...group };
    if (target == null) delete next.parentId;
    else next.parentId = target;
    return next;
  };

  let nextGroups;
  let nextElements = before.elements || [];
  if (hasIndex) {
    // Explicit placement: contiguous order 0..N over the destination's new arrangement
    // (destination merged siblings BEFORE the move, excluding the group itself).
    const destSiblings = orderedChildren(before, target).filter((node) => node.id !== groupId);
    const clampedIndex = Math.max(0, Math.min(destSiblings.length, Math.round(Number(index))));
    const arranged = destSiblings.slice();
    arranged.splice(clampedIndex, 0, { kind: "group", id: groupId });
    const orderByNodeId = new Map(arranged.map((node, order) => [node.id, order]));
    nextGroups = groupsOf(before).map((group) => {
      if (group.id === groupId) return { ...withParent(group), order: orderByNodeId.get(groupId) };
      return orderByNodeId.has(group.id) ? { ...group, order: orderByNodeId.get(group.id) } : group;
    });
    nextElements = (before.elements || []).map((element) =>
      orderByNodeId.has(element.id) ? { ...element, order: orderByNodeId.get(element.id) } : element,
    );
  } else {
    const fo = frontOrder(before, target);
    nextGroups = groupsOf(before).map((group) => {
      if (group.id !== groupId) return group;
      const moved = withParent(group);
      if (fo === null) delete moved.order;
      else moved.order = fo;
      return moved;
    });
  }

  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "reparentGroup",
    args_summary: { groupId, parentId: target, index: hasIndex ? Math.round(Number(index)) : undefined },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Dissolve ONE group level in ONE journaled gesture (Figma Ungroup): the group's DIRECT
// children — elements AND direct subgroups — move up into the group's OWN parent scope
// (root when the group was top-level, preserving nesting depth otherwise), landing AT the
// group's former sibling z-slot in their internal relative order, and the now-empty group
// is removed. The parent scope is rewritten with contiguous order (the children occupy the
// vacated slot, everything else keeps its relative order), so z-order is exact — not the
// old page-composed "children jump to the front". Grandchildren keep pointing at the
// surviving subgroups (only one level dissolves). One commitMutation; a single undo
// restores the group and every child's scope + order exactly. Backing image files stay on
// disk. A loud error on an unknown group; an empty group simply dissolves (its slot closes).
export function ungroupGroup(root, { projectId, groupId } = {}) {
  if (!projectId) throw new Error("ungroupGroup requires projectId");
  if (!groupId) throw new Error("ungroupGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const group = findGroup(before, groupId); // loud error on an unknown group
  // The group's resolved parent scope (null = root; a dangling parent resolves to root,
  // mirroring tree scope resolution) — where the children land.
  const scope = nodeScope(before, group);

  // Parent-scope arrangement with the group node REPLACED by its direct children (kept in
  // their own internal back->front order), so the children take the group's exact z-slot.
  const parentSiblings = orderedChildren(before, scope);
  const children = orderedChildren(before, groupId);
  const slot = parentSiblings.findIndex((node) => node.id === groupId);
  const arranged = parentSiblings.slice();
  arranged.splice(slot, 1, ...children);
  const orderByNodeId = new Map(arranged.map((node, order) => [node.id, order]));

  const childElementIds = new Set(children.filter((node) => node.kind === "element").map((node) => node.id));
  const childGroupIds = new Set(children.filter((node) => node.kind === "group").map((node) => node.id));

  const nextGroups = groupsOf(before)
    .filter((item) => item.id !== groupId) // remove the dissolved group
    .map((item) => {
      if (!childGroupIds.has(item.id) && !orderByNodeId.has(item.id)) return item;
      const next = { ...item };
      if (childGroupIds.has(item.id)) {
        if (scope == null) delete next.parentId;
        else next.parentId = scope;
      }
      if (orderByNodeId.has(item.id)) next.order = orderByNodeId.get(item.id);
      return next;
    });
  const nextElements = (before.elements || []).map((item) => {
    if (!childElementIds.has(item.id) && !orderByNodeId.has(item.id)) return item;
    const next = { ...item };
    if (childElementIds.has(item.id)) {
      if (scope == null) delete next.groupId;
      else next.groupId = scope;
    }
    if (orderByNodeId.has(item.id)) next.order = orderByNodeId.get(item.id);
    return next;
  });

  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "ungroupGroup",
    args_summary: {
      groupId,
      parentId: scope,
      movedElements: [...childElementIds],
      movedGroups: [...childGroupIds],
    },
    before,
    after,
    startedAt,
  });
  return {
    project,
    ungrouped: groupId,
    parentId: scope,
    movedElements: [...childElementIds],
    movedGroups: [...childGroupIds],
  };
}
