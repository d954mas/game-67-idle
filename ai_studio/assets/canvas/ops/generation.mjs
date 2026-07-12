// Canvas generation operation domain. Public API is ../ops.mjs.
import { randomUUID } from "node:crypto";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";
import { performance } from "node:perf_hooks";
import { runPython as runToolPython } from "../../tools/image/_bridge/bridge.mjs";
import { ENGINE_PARAMS_USED, generateImageCodex, generateImageGemini } from "../tools/recipe_generate.mjs";
import { defaultAnimGenerators } from "../tools/anim_generate.mjs";
import { expandPrompt, extractFromImage } from "../tools/prompt_assist.mjs";
import { runAnimateFromText } from "../tools/animation_assist.mjs";
import { frontOrder } from "../tree.mjs";
import { validateAnimation } from "../animation.mjs";
import { addFile as storeAddFile, addImage as storeAddImage, capToolRuns, getProject, resolveProjectFile, updateProject, withProjectLock } from "../store.mjs";
import { commitMutation, finite, groupsOf, hexColor, refuseIfHeadMoved } from "./core.mjs";
import { findGroup } from "./groups.mjs";

// Recipe/style/animation cards are groups with additive metadata. Card refs are
// ordinary group members; generation results land beside the card in its parent
// scope and the final state change remains one journal entry.

export const RECIPE_ENGINES = new Set(["codex", "gemini", "both"]); // R2/R3: engine choice + compare mode

// Default recipe blob for a freshly-minted card (design doc §4.1). `engine` defaults to
// "codex" (R2/R3 adds "gemini"/"both"); `style_ref` is a reserved nullable by-id pointer
// (R1 — style cards land in increment 3); `expanded`/`use_expanded`/`last_run` stay inert
// placeholders until increments 2-3 write/consume them. `params` was write-once-at-creation
// too, until T0332 v2 unfroze four of its fields (bg_key/n_candidates/size/quality — see
// normalizeRecipePatch's own `params` handling; `model` remains immutable). `supersample`
// was REMOVED from the defaults 2026-07-07 (лид: "без техдолга") — nothing anywhere ever
// consumed it (not generate_image.py, not either command builder, not the expander); cards
// minted before then still carry `supersample: true` in their stored blob, which is harmless
// legacy (snapshots are allow-listed via snapshotParamsForEngine, so it never leaks into
// provenance, and patching it stays a loud unknown-key error below).
// `model`/`quality` are consumed by the CODEX generator only; gemini/agy consumes only
// `size` (ENGINE_PARAMS_USED in tools/recipe_generate.mjs — the seam owns that list).
// `pack` (T0332 v2, build-spec build_spec_pack_card_2026-07-07.md — lead decision "Слить":
// pack mode is NOT a third card type, it is an optional field on the recipe blob) defaults to
// null — "no pack mode, a single Generate mints one image" (unchanged single-image behavior);
// non-null turns Generate into a sheet-generation run instead (see
// normalizeRecipePack/packPreview below).
export function defaultRecipe() {
  return {
    v: 1,
    prompt: "",
    expanded: null,
    use_expanded: true,
    engine: "codex",
    params: {
      size: "1024x1024",
      quality: "high",
      model: "gpt-image-2",
      bg_key: "#ff00ff",
      n_candidates: 1,
    },
    style_ref: null,
    pack: null,
    last_run: null,
  };
}

// Validate + normalize a PARTIAL recipe patch (loud, no silent coercion — mirrors
// normalizeGroupBackground/normalizeGroupClip). Editable fields: `prompt` (a string; empty
// is fine — that is the "draft" state), `expanded` (T0239 increment 4: null, or a string —
// the Expand-prompt result the lead edits by hand; null = no expansion / "discarded"),
// `use_expanded` (a boolean — Generate sends `expanded` when this is true AND `expanded` is
// set, else the short `prompt`; see resolveRecipePromptText, unchanged by increment 4),
// `engine` (one of RECIPE_ENGINES), `style_ref` (null, or a non-empty string id — the
// reserved R1 pointer; resolving/remapping it across canvases is increment 3, not this op),
// `pack` (T0332 v2: null to turn pack mode off, or a FULL pack object — see
// normalizeRecipePack below). Unlike every other field here, a given `pack` REPLACES the
// stored `recipe.pack` WHOLESALE rather than merging: the map in patchRecipe below only
// merges the recipe blob one level deep (`{...group.recipe, ...resolved}`), so
// `resolved.pack`, when present, IS the entire next value, never a delta onto the old one —
// documented here since it is the one exception to this function's "partial patch" framing
// (the caller — cli.mjs's recipe-set, a future UI — is responsible for assembling the full
// object when it only means to change one pack field). `params` (T0332 v2 — two lead
// decisions on top of the build-spec's focus-review, 2026-07-07: pack mode does NOT duplicate
// background/candidates inside its own blob; instead recipe.params, previously immutable via
// this op, is UNFROZEN for exactly four fields: `bg_key` (a hex color string — validated
// generically via the shared `hexColor` helper, the SAME format check group backgrounds use;
// the magenta/green PAIRING restriction pack mode actually needs is enforced later, in
// packPreview/generateFromRecipe's pack branch, not here — bg_key remains free-form hex for
// the single-image cutout path it already served), `n_candidates` (a positive integer),
// `size`/`quality` (strings) — `model` is loud (immutable), same law patchPack applied to a
// pack's own params in phase A. A `params` patch is PARTIAL and MERGES onto the stored
// `recipe.params` one level deep (patchRecipe below, mirroring phase A's own
// `pack.params` merge precedent) — unlike `pack`, which replaces wholesale; this file's one
// precedent for "a patch that touches a nested object" is deep-merge, not replace, so params
// follows it and pack (a deliberately different, all-or-nothing config surface) is the
// documented exception, not params.
// Returns the subset of resolved fields actually provided; throws before any write on
// anything else (bad type, unknown key value, or an empty patch).
export function normalizeRecipePatch(patch) {
  if (!patch || typeof patch !== "object" || Array.isArray(patch)) {
    throw new Error(`recipe patch must be an object, got ${JSON.stringify(patch)}`);
  }
  const out = {};
  if (patch.prompt !== undefined) {
    if (typeof patch.prompt !== "string") throw new Error(`recipe prompt must be a string, got ${JSON.stringify(patch.prompt)}`);
    out.prompt = patch.prompt;
  }
  if (patch.expanded !== undefined) {
    if (patch.expanded !== null && typeof patch.expanded !== "string") {
      throw new Error(`recipe expanded must be null or a string, got ${JSON.stringify(patch.expanded)}`);
    }
    out.expanded = patch.expanded;
  }
  if (patch.use_expanded !== undefined) {
    if (typeof patch.use_expanded !== "boolean") {
      throw new Error(`recipe use_expanded must be a boolean, got ${JSON.stringify(patch.use_expanded)}`);
    }
    out.use_expanded = patch.use_expanded;
  }
  if (patch.engine !== undefined) {
    if (!RECIPE_ENGINES.has(patch.engine)) {
      throw new Error(`recipe engine must be one of ${[...RECIPE_ENGINES].join("/")}, got ${JSON.stringify(patch.engine)}`);
    }
    out.engine = patch.engine;
  }
  if (patch.style_ref !== undefined) {
    if (patch.style_ref !== null && typeof patch.style_ref !== "string") {
      throw new Error(`recipe style_ref must be null or a string id, got ${JSON.stringify(patch.style_ref)}`);
    }
    out.style_ref = patch.style_ref;
  }
  // T0332 v2: pack is null (pack mode off) or a FULL pack object — see normalizeRecipePack's
  // own doc comment for why this one field replaces wholesale instead of merging.
  if (patch.pack !== undefined) {
    out.pack = patch.pack === null ? null : normalizeRecipePack(patch.pack);
  }
  // T0332 v2 (two lead decisions on top of the build-spec's focus-review): recipe.params is
  // no longer fully immutable — bg_key/n_candidates/size/quality are patchable now (the root
  // fix for pack mode needing an editable background/candidate-count, instead of duplicating
  // them inside `pack`). `model` stays immutable (loud, even set to its current value) — the
  // one field pack's own phase-A params-patch also refused. Returns only the given subset;
  // patchRecipe below merges it one level deep onto the stored recipe.params (a PARTIAL patch,
  // unlike `pack` above).
  if (patch.params !== undefined) {
    if (!patch.params || typeof patch.params !== "object" || Array.isArray(patch.params)) {
      throw new Error(`recipe params must be an object, got ${JSON.stringify(patch.params)}`);
    }
    const params = {};
    if (patch.params.bg_key !== undefined) {
      // Generic hex validation only (any color) — the magenta/green PAIRING pack mode needs
      // is checked later, in packPreview/generateFromRecipe's pack branch (build-spec: "иной
      // hex — громкая ошибка в packPreview/pack-ветке generate, НЕ на patch-time").
      const hex = hexColor(patch.params.bg_key);
      if (!hex) throw new Error(`recipe params.bg_key must be a 6-digit hex color (e.g. #ff00ff), got ${JSON.stringify(patch.params.bg_key)}`);
      params.bg_key = hex;
    }
    if (patch.params.n_candidates !== undefined) {
      const nCandidates = Number(patch.params.n_candidates);
      if (!Number.isInteger(nCandidates) || nCandidates < 1) {
        throw new Error(`recipe params.n_candidates must be a positive integer, got ${JSON.stringify(patch.params.n_candidates)}`);
      }
      params.n_candidates = nCandidates;
    }
    if (patch.params.size !== undefined) {
      if (typeof patch.params.size !== "string") throw new Error(`recipe params.size must be a string, got ${JSON.stringify(patch.params.size)}`);
      params.size = patch.params.size;
    }
    if (patch.params.quality !== undefined) {
      if (typeof patch.params.quality !== "string") throw new Error(`recipe params.quality must be a string, got ${JSON.stringify(patch.params.quality)}`);
      params.quality = patch.params.quality;
    }
    // model (and any typo, and legacy supersample) is immutable/unknown — a loud error, never
    // a silent ignore (mirrors phase A's own pack.params rule verbatim).
    const unknownParamKeys = Object.keys(patch.params).filter((key) => !["bg_key", "n_candidates", "size", "quality"].includes(key));
    if (unknownParamKeys.length) {
      throw new Error(`recipe params.${unknownParamKeys[0]} is immutable or unknown — only bg_key/n_candidates/size/quality are patchable (model is fixed)`);
    }
    if (!Object.keys(params).length) {
      throw new Error("recipe params patch requires at least one of bg_key, n_candidates, size, quality");
    }
    out.params = params;
  }
  if (!Object.keys(out).length) {
    throw new Error("patchRecipe requires at least one of prompt, expanded, use_expanded, engine, style_ref, pack, params");
  }
  return out;
}

// Default frame size for a freshly-minted card with no explicit bounds — a workshop
// widget, comfortably smaller than a screen (DEFAULT_GROUP_SIZE-equivalent on the page is
// 960x540). Purely cosmetic (decision 4: the frame never feeds generation).
export const DEFAULT_RECIPE_CARD_SIZE = { w: 360, h: 280 };

// Padding around the copied image when a PROMOTION mints a card fit to its content (lead
// 2026-07-03: "карточкам при создании можно сразу сделать fit to content") — the empty
// context-menu/CLI cards above keep the fixed default size, they have no content to fit.
export const CARD_FIT_PADDING = 16;

// Mint a recipe card: a group carrying a fresh `recipe` blob (defaultRecipe). Explicit
// bounds are optional (unlike createGroup, a card is never `fromElements`) — omitted w/h
// fall back to DEFAULT_RECIPE_CARD_SIZE so the context-menu/CLI entry point can mint one
// with just a placement point. Optional `parentId` nests the card like any group (null/
// absent = top level; validated). Renders as a plain group frame today — the canvas
// badge + prompt-preview land in increment 2 alongside generation. A card is a workshop
// object, not an exportable screen BY CONSTRUCTION (T0332 B1): `createRecipeCard` never
// sets `group.screen`, and only `group.screen === true` makes a top-level group an
// exportable screen (see exportProject's own doc) — no special recipe/style skip needed.
// One journal entry.
export function createRecipeCard(root, { projectId, name, x, y, w, h, parentId } = {}) {
  if (!projectId) throw new Error("createRecipeCard requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const cleanName = String(name || "").trim() || "Recipe card";

  const bounds = {
    x: finite(x) ? Number(x) : 0,
    y: finite(y) ? Number(y) : 0,
    w: finite(w) && Number(w) > 0 ? Number(w) : DEFAULT_RECIPE_CARD_SIZE.w,
    h: finite(h) && Number(h) > 0 ? Number(h) : DEFAULT_RECIPE_CARD_SIZE.h,
  };

  const parentScope = parentId == null || parentId === "" ? null : String(parentId);
  if (parentScope != null) {
    const parent = findGroup(before, parentScope); // loud error on an unknown parent
    // Cards do not nest inside cards (R1 increment 3's symmetric guard, mirrored on
    // createStyleCard below): a recipe card inside a style card frame would blur the two
    // widget types, so it is refused loudly rather than silently allowed.
    if (parent.style) {
      throw new Error(`cannot create a recipe card inside a style card (parent ${parentScope} carries style) — cards do not nest inside cards`);
    }
    // F6: the guard was asymmetric — only a style parent was refused, so a recipe/anim parent
    // silently allowed a card to nest inside a card. Refuse those too (new messages; the style
    // one above stays verbatim — style.test asserts it word-for-word).
    if (parent.recipe) {
      throw new Error(`cannot create a recipe card inside another recipe card (parent ${parentScope} carries recipe) — cards do not nest inside cards`);
    }
    if (parent.anim) {
      throw new Error(`cannot create a recipe card inside an animation card (parent ${parentScope} carries anim) — cards do not nest inside cards`);
    }
  }

  const group = {
    id: groupId,
    name: cleanName,
    x: bounds.x,
    y: bounds.y,
    w: bounds.w,
    h: bounds.h,
    visible: true,
    recipe: defaultRecipe(),
  };
  if (parentScope != null) group.parentId = parentScope;
  const groupFront = frontOrder(before, parentScope);
  if (groupFront !== null) group.order = groupFront;

  const after = updateProject(root, projectId, { groups: [...groupsOf(before), group] });
  const project = commitMutation(root, projectId, {
    op: "createRecipeCard",
    args_summary: { groupId, name: cleanName, bounds, parentId: parentScope },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Partial update of a card's `recipe` blob (prompt/expanded/use_expanded/engine/style_ref/pack/
// params — see normalizeRecipePatch; `last_run` is written by generateFromRecipe, not this
// general patch; `pack`, uniquely, REPLACES rather than merges, while `params` — the one other
// field that itself nests — merges one level deeper than the rest of the blob, mirroring phase
// A's own `pack.params` precedent; see normalizeRecipePatch's own doc comment for both). Loud
// on a group that carries no `recipe` at all — a plain group is not a card, so patching one
// here is a caller bug, not a silent no-op. One journal entry; undo restores the prior recipe
// blob byte-exact (free via the group snapshot).
export function patchRecipe(root, { projectId, groupId, patch } = {}) {
  if (!projectId) throw new Error("patchRecipe requires projectId");
  if (!groupId) throw new Error("patchRecipe requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const current = findGroup(before, groupId);
  if (!current.recipe || typeof current.recipe !== "object") {
    throw new Error(`group is not a recipe card (no recipe blob): ${groupId}`);
  }
  const resolved = normalizeRecipePatch(patch); // validates BEFORE any write (type-level)
  // R1 increment 3: style_ref stopped being a free-form reserved pointer — a non-null value
  // must now resolve to an actual STYLE CARD group in THIS project (loud otherwise). Project-
  // aware, so it lives here rather than in normalizeRecipePatch (which stays a pure type check).
  if (resolved.style_ref !== undefined && resolved.style_ref !== null) {
    const styleCard = groupsOf(before).find((group) => group.id === resolved.style_ref);
    if (!styleCard || !styleCard.style || typeof styleCard.style !== "object") {
      throw new Error(`recipe style_ref must be null or the id of an existing style-card group, got ${JSON.stringify(resolved.style_ref)}`);
    }
  }

  const nextGroups = groupsOf(before).map((group) => {
    if (group.id !== groupId) return group;
    // `params` merges one level deeper than the rest of the blob (T0332 v2: bg_key/
    // n_candidates/size/quality are individually patchable now — a PARTIAL params patch must
    // not silently wipe out the OTHER params fields it didn't mention); `pack` needs no such
    // special-casing — a plain spread already replaces it wholesale, which is the documented,
    // intentional behavior for that one field (see normalizeRecipePatch's doc comment).
    const { params: paramsPatch, ...rest } = resolved;
    const nextRecipe = { ...group.recipe, ...rest };
    if (paramsPatch) nextRecipe.params = { ...group.recipe.params, ...paramsPatch };
    return { ...group, recipe: nextRecipe };
  });
  const after = updateProject(root, projectId, { groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "patchRecipe",
    args_summary: { groupId, patch: resolved },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// ---- style card (T0239 increment 3) -------------------------------------------
//
// A style card is a GROUP carrying an additive `style` object (design R1, lead-accepted
// verbatim) — the SAME "group + additive blob" shape as a recipe card, not a new element
// type. Blob: name (= group name) + a STYLE PROMPT + exactly ONE ref image (the only member
// image SENT to generation) + any number of example images (member images that are NOT the
// ref — eyes-only, never sent; see generateFromRecipe's style-mixing section below). A group
// must never carry BOTH `recipe` and `style` (createRecipeCard/createStyleCard's symmetric
// parent-nesting guards above/below); patchRecipe/patchStyle's own "no <blob>" checks make it
// structurally impossible for the SAME group to answer to both patch ops. `updateProject`
// spreads `groups` verbatim (store.mjs), so `group.style` round-trips through every snapshot/
// undo/redo/buildNodesSpec copy-paste with zero store/tree changes, exactly like `recipe`.

// Default style blob for a freshly-minted card (design R1): no prompt, no ref yet — the
// FIRST image later dropped in auto-claims the ref (see applyStyleAutoRef below).
export function defaultStyle() {
  return { v: 1, prompt: "", ref: null };
}

// Validate + normalize a PARTIAL style patch (loud, no silent coercion — mirrors
// normalizeRecipePatch). Only `prompt` (a string) and `ref` (null, or the id of an IMAGE
// element that is already a MEMBER of THIS card — the "Make ref" gesture) are patchable;
// `ref` needs the project + this card's id to validate membership, so unlike
// normalizeRecipePatch this helper is NOT pure of the project. Returns the subset of
// resolved fields actually provided; throws before any write on anything else.
export function normalizeStylePatch(project, groupId, patch) {
  if (!patch || typeof patch !== "object" || Array.isArray(patch)) {
    throw new Error(`style patch must be an object, got ${JSON.stringify(patch)}`);
  }
  const out = {};
  if (patch.prompt !== undefined) {
    if (typeof patch.prompt !== "string") throw new Error(`style prompt must be a string, got ${JSON.stringify(patch.prompt)}`);
    out.prompt = patch.prompt;
  }
  if (patch.ref !== undefined) {
    if (patch.ref === null) {
      out.ref = null;
    } else {
      const refId = String(patch.ref);
      const element = (project.elements || []).find((item) => item.id === refId);
      if (!element) throw new Error(`style ref must be null or an existing element id, got ${JSON.stringify(patch.ref)}`);
      if (element.type !== "image") {
        throw new Error(`style ref must be an IMAGE element, got type ${JSON.stringify(element.type)}: ${refId}`);
      }
      if (element.groupId !== groupId) {
        throw new Error(`style ref must be a member of this style card (${groupId}): ${refId}`);
      }
      out.ref = refId;
    }
  }
  if (!Object.keys(out).length) {
    throw new Error("patchStyle requires at least one of prompt, ref");
  }
  return out;
}

// Same default frame as a recipe card — a workshop widget, purely cosmetic (the frame never
// feeds generation, same as decision 4 for the recipe card).
export const DEFAULT_STYLE_CARD_SIZE = { w: 360, h: 280 };

// Mint a style card: a group carrying a fresh `style` blob (defaultStyle). Mirrors
// createRecipeCard exactly (bounds fallback, optional parentId, one journal entry) except
// for the blob itself and its symmetric "no nesting inside a recipe card" guard.
export function createStyleCard(root, { projectId, name, x, y, w, h, parentId } = {}) {
  if (!projectId) throw new Error("createStyleCard requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const cleanName = String(name || "").trim() || "New style";

  const bounds = {
    x: finite(x) ? Number(x) : 0,
    y: finite(y) ? Number(y) : 0,
    w: finite(w) && Number(w) > 0 ? Number(w) : DEFAULT_STYLE_CARD_SIZE.w,
    h: finite(h) && Number(h) > 0 ? Number(h) : DEFAULT_STYLE_CARD_SIZE.h,
  };

  const parentScope = parentId == null || parentId === "" ? null : String(parentId);
  if (parentScope != null) {
    const parent = findGroup(before, parentScope); // loud error on an unknown parent
    // Symmetric to createRecipeCard's own guard: cards do not nest inside cards.
    if (parent.recipe) {
      throw new Error(`cannot create a style card inside a recipe card (parent ${parentScope} carries recipe) — cards do not nest inside cards`);
    }
    // F6: symmetric to createRecipeCard — refuse a style/anim parent too (only a recipe parent
    // was caught before; the recipe message above stays verbatim for style.test).
    if (parent.style) {
      throw new Error(`cannot create a style card inside another style card (parent ${parentScope} carries style) — cards do not nest inside cards`);
    }
    if (parent.anim) {
      throw new Error(`cannot create a style card inside an animation card (parent ${parentScope} carries anim) — cards do not nest inside cards`);
    }
  }

  const group = {
    id: groupId,
    name: cleanName,
    x: bounds.x,
    y: bounds.y,
    w: bounds.w,
    h: bounds.h,
    visible: true,
    style: defaultStyle(),
  };
  if (parentScope != null) group.parentId = parentScope;
  const groupFront = frontOrder(before, parentScope);
  if (groupFront !== null) group.order = groupFront;

  const after = updateProject(root, projectId, { groups: [...groupsOf(before), group] });
  const project = commitMutation(root, projectId, {
    op: "createStyleCard",
    args_summary: { groupId, name: cleanName, bounds, parentId: parentScope },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Partial update of a card's `style` blob (prompt/ref). Loud on a group that carries no
// `style` at all — a plain group (or a recipe card) is not a style card. One journal entry;
// undo restores the prior style blob byte-exact (free via the group snapshot). Mirrors
// patchRecipe exactly, minus the reserved-pointer validation (ref validates against THIS
// card's own membership instead, in normalizeStylePatch).
export function patchStyle(root, { projectId, groupId, patch } = {}) {
  if (!projectId) throw new Error("patchStyle requires projectId");
  if (!groupId) throw new Error("patchStyle requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const current = findGroup(before, groupId);
  if (!current.style || typeof current.style !== "object") {
    throw new Error(`group is not a style card (no style blob): ${groupId}`);
  }
  const resolved = normalizeStylePatch(before, groupId, patch); // validates BEFORE any write

  const nextGroups = groupsOf(before).map((group) =>
    group.id === groupId ? { ...group, style: { ...group.style, ...resolved } } : group,
  );
  const after = updateProject(root, projectId, { groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "patchStyle",
    args_summary: { groupId, patch: resolved },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Auto-ref (design R1: "первое изображение, попавшее в карточку, само становится ref"): the
// FIRST image element that becomes a member of a STYLE CARD whose `ref` is still null
// auto-claims that ref. Pure helper shared by every membership-change op (assignToGroup,
// pasteNodes — the two real paths that can drop an image into an existing/freshly-copied
// style card; addImage/addImageFromFile mint top-level only today, no groupId to cover).
// `membershipChanges` is a Map<elementId, {groupId, type}> of elements that just landed at
// their FINAL groupId in this SAME gesture; iteration order (Map preserves insertion order)
// decides "first" when several land at once. Never overwrites an existing (non-null) ref, and
// a non-style group (or a style card whose ref is already set) passes through unchanged.
// Called from the SAME commit as the membership change — never a second journal entry.
// ---- generateFromRecipe (T0239 increment 2: generation end-to-end) -----------
//
// The Recipe inspector's Generate button / `recipe-generate` CLI verb / POST .../generate
// route. An action on a RECIPE CARD (`group.recipe`), not on an existing image element —
// structurally it mirrors alphaDualPlateGenerate (validate loudly, generate OUTSIDE the
// journal, mint via storeAddImage, ONE commitMutation) but the generator seam is
// tools/recipe_generate.mjs's TWO engines (codex/gemini, R2) with an R3 "both" compare mode.
//
// Refs (decision 3) = the card's member IMAGE elements (`groupId === cardId`, visible only),
// resolved to abs paths at generate time — never the group's frame w/h (decision 4: never
// read here). `recipe.style_ref`, when set, resolves to a STYLE CARD group in this project
// (R1 increment 3): its prompt is APPENDED to the recipe prompt and its ref image (if any) is
// attached ALONGSIDE the recipe card's own member refs, last — see resolveStyleMix below.
// Example images (the style card's non-ref members) never travel (design R1).
//
// Placement (R1): the result(s) land in the card's PARENT scope (`groupId =
// card.parentId ?? null`) — NEVER `groupId = cardId`, so a result can never become a ref
// feeding a future run of the SAME card. Positioned to the RIGHT of the card frame (16px
// gap); a second result (engine="both") stacks BELOW the first (16px gap).
//
// Engine (R2/R3): "codex" (default) -> one generation; "gemini" -> one, refs plumbed through
// exactly like codex (agy ref support is VERIFIED — T0251, 2026-07-03; the .seen.txt
// silent-divergence guard lives in tools/recipe_generate.mjs's generateImageGemini, not here);
// "both" -> runs BOTH engines with the SAME refPaths, mints TWO elements ("<card> codex" /
// "<card> agy"), one journal entry for the pair, and allows PARTIAL success (one engine
// failing — including agy's own .seen.txt guard tripping — still lands the other). Only when
// EVERY attempted engine fails (including a single-engine run) does the op throw; nothing is
// written in that case (no journal entry, no card mutation) — mirrors alphaDualPlateGenerate's
// atomic refusal.
export const MAX_RECIPE_REFS = 5; // generate_image.py --input-image cap (research: <=5)
// R3: the minted element name suffix per engine ("<card> agy", never "<card> gemini" — the
// agy/Antigravity CLI is what actually runs behind the "gemini" engine choice, R2).
export const RECIPE_ENGINE_SUFFIX = { codex: "codex", gemini: "agy" };

// Provenance truth (lead, 2026-07-07, "без техдолга"): params_snapshot/tool_runs record ONLY
// the params the engine that ran actually consumed (ENGINE_PARAMS_USED — the generator seam's
// own contract, owned next to the command builders in tools/recipe_generate.mjs) plus the
// caller-named CANVAS-level params that genuinely shaped the run regardless of engine
// (`extraKeys`: bg_key — baked into a pack sheet's prompt / cutout advisory on the
// single-image path; n_candidates — the pack expander's overgen count, pack branch only).
// A gemini run must never claim a gpt-image model or a quality it had no knob for.
// `recipe.params` itself stays whole on the card — flipping the engine back loses nothing.
// "both" is the caller's job to resolve per element/attempt; this helper takes ONE engine.
export function snapshotParamsForEngine(engine, params, extraKeys = []) {
  const source = params || {};
  const out = {};
  for (const key of [...(ENGINE_PARAMS_USED[engine] || []), ...extraKeys]) {
    if (source[key] !== undefined) out[key] = source[key];
  }
  return out;
}

// Resolve the text a generate call actually sends: `expanded` (increment 3's Expand-prompt
// output) when present AND enabled, else the base `prompt` — trimmed either way. Returns ""
// on an all-whitespace/missing value so the caller's emptiness check is a single truthiness
// test.
export function resolveRecipePromptText(recipe) {
  const source = recipe.use_expanded && recipe.expanded ? recipe.expanded : recipe.prompt;
  return typeof source === "string" ? source.trim() : "";
}

// The card's member IMAGE elements (decision 3's refs), visible ones only — an explicitly
// hidden member never travels with the call, mirroring how a hidden element is skipped
// everywhere else in the renderer/exporter.
export function recipeCardMembers(project, cardId) {
  return (project.elements || []).filter((el) => el.groupId === cardId && el.type === "image" && el.visible !== false);
}

export async function generateFromRecipe(root, { projectId, groupId, generators, runGroupId, sheetSlug } = {}) {
  if (!projectId) throw new Error("generateFromRecipe requires projectId");
  if (!groupId) throw new Error("generateFromRecipe requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const card = findGroup(before, groupId); // loud "group not found" on an unknown id
  if (!card.recipe || typeof card.recipe !== "object") {
    throw new Error(`group is not a recipe card (no recipe blob): ${groupId}`);
  }
  const recipe = card.recipe;
  const cardLabel = card.name || groupId;

  // T0332 v2 (build_spec_pack_card_2026-07-07.md §3): recipe.pack turns Generate into a
  // SHEET-GENERATION run instead of a single mint — see generatePackSheets below (defined
  // alongside packPreview, which shares its config-assembly helper, buildPackConfig).
  // `runGroupId`/`sheetSlug` are pack-only options (resume / force-regen-one-sheet); the
  // single-image branch below never reads them. Everything from here down this function is
  // the ORIGINAL single-image branch, UNCHANGED by this increment.
  if (recipe.pack) {
    return generatePackSheets(root, { projectId, groupId, generators, runGroupId, sheetSlug, before, card, recipe, cardLabel, startedAt });
  }

  const promptText = resolveRecipePromptText(recipe);
  if (!promptText) {
    throw new Error(`recipe card "${cardLabel}" (${groupId}) has an empty prompt — set one before generating`);
  }

  const members = recipeCardMembers(before, groupId);
  if (members.length > MAX_RECIPE_REFS) {
    throw new Error(
      `recipe card "${cardLabel}" (${groupId}) has ${members.length} reference images — generate_image.py accepts at most ${MAX_RECIPE_REFS} (--input-image)`,
    );
  }
  const refPaths = members.map((el) => resolveProjectFile(root, projectId, el.src));
  const refSrcs = members.map((el) => el.src);

  // R1 increment 3: style mixing. `recipe.style_ref`, when set, MUST resolve to a style-card
  // group in THIS project (patchRecipe already enforces this on write, but a hand-edited
  // project.json is still possible — loud here too, defensively). A style card with a prompt
  // but ref===null is valid (prompt-only style: append text, attach no extra image). A SET
  // ref that no longer exists or left the card's membership is loud (never silently dropped).
  let effectivePromptText = promptText;
  let finalRefPaths = refPaths;
  let finalRefSrcs = refSrcs;
  let styleSnapshot; // stays undefined (key omitted, mirrors meta.alpha's "absent") when unset
  if (recipe.style_ref) {
    const styleCard = groupsOf(before).find((group) => group.id === recipe.style_ref);
    if (!styleCard || !styleCard.style || typeof styleCard.style !== "object") {
      throw new Error(
        `recipe card "${cardLabel}" (${groupId}) has a style_ref that is not a style-card group: ${recipe.style_ref}`,
      );
    }
    const styleLabel = styleCard.name || styleCard.id;
    const stylePromptTrimmed = typeof styleCard.style.prompt === "string" ? styleCard.style.prompt.trim() : "";
    if (stylePromptTrimmed) effectivePromptText = `${promptText}\n\nStyle: ${stylePromptTrimmed}`;
    if (styleCard.style.ref) {
      const refElement = (before.elements || []).find((el) => el.id === styleCard.style.ref);
      if (!refElement || refElement.groupId !== styleCard.id) {
        throw new Error(
          `style card "${styleLabel}" (${styleCard.id}) ref points at a missing/non-member element: ${styleCard.style.ref}`,
        );
      }
      finalRefPaths = [...refPaths, resolveProjectFile(root, projectId, refElement.src)];
      finalRefSrcs = [...refSrcs, refElement.src];
    }
    styleSnapshot = { cardId: styleCard.id, name: styleLabel, prompt: stylePromptTrimmed };
  }

  const requestedEngine = recipe.engine;
  if (!RECIPE_ENGINES.has(requestedEngine)) {
    // Defensive only — patchRecipe already validates on write; a hand-edited project.json
    // is the only way to reach this. Loud, not a silent "codex" coercion.
    throw new Error(`recipe card "${cardLabel}" (${groupId}) has an invalid engine: ${JSON.stringify(requestedEngine)}`);
  }
  // R2/R3 ref rule: agy ref support is VERIFIED (T0251, 2026-07-03) — engine="gemini" and
  // engine="both" both forward refPaths to the gemini generator exactly like codex always
  // has; the silent-divergence guard (agy can generate from text alone and exit 0 without
  // reading a ref) lives in tools/recipe_generate.mjs's generateImageGemini/verifyAgyRefProof,
  // not here.
  const gens = { codex: generateImageCodex, gemini: generateImageGemini, ...(generators || {}) };
  const attempts = [];
  if (requestedEngine === "both") {
    attempts.push({ engine: "codex" });
    attempts.push({ engine: "gemini" });
  } else {
    attempts.push({ engine: requestedEngine });
  }

  const at = new Date().toISOString();
  const results = []; // {engine, bytes}
  const failed = []; // {engine, error}
  for (const attempt of attempts) {
    try {
      const generated = await gens[attempt.engine]({ prompt: effectivePromptText, refPaths: finalRefPaths, params: recipe.params });
      const bytes = Buffer.isBuffer(generated) ? generated : readFileSync(generated);
      results.push({ engine: attempt.engine, bytes });
    } catch (error) {
      failed.push({ engine: attempt.engine, error: error.message });
    }
  }

  if (!results.length) {
    const reasons = failed.map((f) => `${f.engine}: ${f.error}`).join("; ");
    throw new Error(`generateFromRecipe: every engine failed for recipe card "${cardLabel}" (${groupId}) — ${reasons}`);
  }

  // Re-read to avoid clobbering a concurrent edit (mirrors alphaDualPlateGenerate's re-read
  // before the final mint) — generation above may have taken minutes. Locked (T0254 Tier 1
  // #1) around just this final critical section (re-read through commit): the slow codex/
  // agy calls already ran OUTSIDE it above, so a multi-minute generate never blocks other
  // mutations on this project — see withProjectLock's doc in store.mjs. refuseIfHeadMoved
  // runs BEFORE the storeAddImage loop below (not just at commitMutation time) — see
  // expandRecipePrompt's comment for why that matters (an intermediate write must never
  // land un-journaled just because the FINAL commit refuses).
  const { project, minted, run } = await withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("generateFromRecipe", before, current);
    const currentCard = (current.groups || []).find((g) => g.id === groupId);
    if (!currentCard) throw new Error(`group not found: ${groupId}`);

    // Placement (R1): PARENT scope of the card, right of the frame, second result stacks below.
    const parentScope = currentCard.parentId == null || currentCard.parentId === "" ? null : String(currentCard.parentId);
    const gap = 16;
    const startX = currentCard.x + currentCard.w + gap;
    let cursorY = currentCard.y;

    let workingProject = current;
    const minted = []; // {engine, element, bytes}
    for (const result of results) {
      const added = storeAddImage(root, projectId, {
        name: `${cardLabel} ${RECIPE_ENGINE_SUFFIX[result.engine] || result.engine}`,
        bytes: result.bytes,
        x: startX,
        y: cursorY,
        meta: {
          recipe: {
            cardId: groupId,
            engine: result.engine,
            at,
            prompt_snapshot: effectivePromptText,
            refs_snapshot: finalRefSrcs,
            // Per ELEMENT, not per call — under engine="both" the codex element records
            // size/quality/model while its agy sibling records only size (+ bg_key).
            params_snapshot: snapshotParamsForEngine(result.engine, recipe.params, ["bg_key"]),
            ...(styleSnapshot ? { style_snapshot: styleSnapshot } : {}),
          },
        },
      });
      workingProject = added.project;
      minted.push({ engine: result.engine, element: added.element, bytes: result.bytes.length });
      cursorY = added.element.y + added.element.h + gap;
    }

    // Front-order hook (mirrors addImages' multi-insert version): stack the minted element(s)
    // at the FRONT of the destination scope, in mint order, when that scope is already
    // explicitly ordered; a no-op on a never-reordered scope.
    const mintedIds = new Set(minted.map((m) => m.element.id));
    let fo = frontOrder(before, parentScope);
    const nextElements = (workingProject.elements || []).map((element) => {
      if (!mintedIds.has(element.id)) return element;
      let next = element;
      if (parentScope != null) next = { ...next, groupId: parentScope };
      if (fo !== null) {
        next = { ...next, order: fo };
        fo += 1;
      }
      return next;
    });

    const verdict = failed.length ? "partial" : "ok";
    const lastRun = { at, result_element_id: minted[0].element.id, verdict };
    const nextGroups = (workingProject.groups || []).map((group) =>
      group.id === groupId ? { ...group, recipe: { ...group.recipe, last_run: lastRun } } : group,
    );

    const run = {
      id: `run_${randomUUID().slice(0, 8)}`,
      op: "generate_from_recipe",
      cardId: groupId,
      at,
      params: {
        prompt_snapshot: effectivePromptText,
        engine: requestedEngine,
        refs: finalRefSrcs,
        // ONE entry per call, so "both" records the codex superset (size/quality/model —
        // the union of what its two halves consumed); each element's own meta.recipe
        // params_snapshot above is the per-engine exact record.
        ...snapshotParamsForEngine(requestedEngine === "both" ? "codex" : requestedEngine, recipe.params, ["bg_key"]),
      },
      result_summary: {
        results: minted.map((m) => ({ engine: m.engine, elementId: m.element.id, bytes: m.bytes })),
        failed,
      },
    };

    const after = updateProject(root, projectId, {
      elements: nextElements,
      groups: nextGroups,
      tool_runs: capToolRuns(root, projectId, [...(workingProject.tool_runs || []), run]),
    });

    // ONE journal entry for the whole gesture (generation itself ran OUTSIDE the journal) —
    // one undo removes every minted element AND reverts the card's recipe.last_run together.
    const project = commitMutation(root, projectId, {
      op: "generateFromRecipe",
      args_summary: {
        groupId,
        engine: requestedEngine,
        elementIds: minted.map((m) => m.element.id),
        failedEngines: failed.map((f) => f.engine),
      },
      before,
      after,
      startedAt,
    });
    return { project, minted, run };
  });

  const mintedIds = new Set(minted.map((m) => m.element.id));
  const elements = (project.elements || []).filter((el) => mintedIds.has(el.id));
  const group = (project.groups || []).find((g) => g.id === groupId);
  return { project, elements, group, failed, run };
}

// ---- expandRecipePrompt (T0239 increment 4: Expand-prompt, the codex TEXT half of the
// prompt-assist seam) ------------------------------------------------------------------
//
// The Recipe inspector's "Expand prompt" button / `recipe-expand` CLI verb / POST
// .../recipe-cards/<gid>/expand route. Reads the card's short `recipe.prompt`, resolves an
// optional style block (recipe.style_ref, when set — same loud validation
// generateFromRecipe's style-mixing section uses), calls the injectable
// `assistant.expand` (default tools/prompt_assist.mjs's expandPrompt) BEFORE any write (the
// slow codex call runs OUTSIDE the journal, mirrors alphaDualPlateGenerate's own shape),
// then ONE commitMutation writes `recipe.expanded`. The lead edits the result himself in the
// Expanded textarea/large-editor modal, or discards it back to null (patchRecipe); Generate
// already resolves which text to send via resolveRecipePromptText (unchanged by this op —
// `use_expanded && expanded ? expanded : prompt`). Loud on a non-card group, an empty
// `recipe.prompt`, and an empty assistant result (no silent fallback to the un-expanded
// prompt) — none of those write anything.
export async function expandRecipePrompt(root, { projectId, groupId, assistant } = {}) {
  if (!projectId) throw new Error("expandRecipePrompt requires projectId");
  if (!groupId) throw new Error("expandRecipePrompt requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const card = findGroup(before, groupId); // loud "group not found" on an unknown id
  if (!card.recipe || typeof card.recipe !== "object") {
    throw new Error(`group is not a recipe card (no recipe blob): ${groupId}`);
  }
  const recipe = card.recipe;
  const cardLabel = card.name || groupId;

  const promptText = typeof recipe.prompt === "string" ? recipe.prompt.trim() : "";
  if (!promptText) {
    throw new Error(`recipe card "${cardLabel}" (${groupId}) has an empty prompt — set one before expanding`);
  }

  // Style-block resolution mirrors generateFromRecipe's own style-mixing validation exactly
  // (a set style_ref MUST resolve to a real style-card group in this project) — only the
  // PROMPT is used here (Expand embeds it as [STYLE] source material); no image ref travels
  // with a text-only codex call.
  let styleBlock = "";
  if (recipe.style_ref) {
    const styleCard = groupsOf(before).find((group) => group.id === recipe.style_ref);
    if (!styleCard || !styleCard.style || typeof styleCard.style !== "object") {
      throw new Error(`recipe card "${cardLabel}" (${groupId}) has a style_ref that is not a style-card group: ${recipe.style_ref}`);
    }
    styleBlock = typeof styleCard.style.prompt === "string" ? styleCard.style.prompt.trim() : "";
  }

  const expand = assistant && typeof assistant.expand === "function" ? assistant.expand : expandPrompt;
  const expandedRaw = await expand({ prompt: promptText, styleBlock });
  if (typeof expandedRaw !== "string" || !expandedRaw.trim()) {
    throw new Error(`expandRecipePrompt: assistant returned an empty result for recipe card "${cardLabel}" (${groupId})`);
  }
  const expanded = expandedRaw.trim();

  // Re-read to avoid clobbering a concurrent edit (mirrors generateFromRecipe/
  // alphaDualPlateGenerate — the codex call above may have taken real time). Locked
  // (T0254 Tier 1 #1) around just this final critical section — the slow codex call
  // above already ran OUTSIDE the lock, so a multi-minute expand never blocks other
  // mutations on this project; see withProjectLock's doc in store.mjs. A concurrent
  // edit that landed during the codex call is caught HERE, loud (HEAD_CONFLICT), BEFORE
  // the write below — checking only at commitMutation time would be too late: the
  // updateProject a few lines down would already have landed on disk, unjournaled.
  const project = await withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("expandRecipePrompt", before, current);
    const currentCard = (current.groups || []).find((g) => g.id === groupId);
    if (!currentCard) throw new Error(`group not found: ${groupId}`);

    const nextGroups = (current.groups || []).map((group) =>
      group.id === groupId ? { ...group, recipe: { ...group.recipe, expanded } } : group,
    );
    const after = updateProject(root, projectId, { groups: nextGroups });
    return commitMutation(root, projectId, {
      op: "expandRecipePrompt",
      args_summary: { groupId },
      before,
      after,
      startedAt,
    });
  });
  const group = (project.groups || []).find((item) => item.id === groupId);
  return { project, group, expanded };
}

// ---- pack mode (T0332 v2: build_spec_pack_card_2026-07-07.md — lead decision "Слить"/
// "Merge") -------------------------------------------------------------------------------
//
// `recipe.pack`, when non-null, turns Generate into a SHEET GENERATION run instead of a
// single mint: the SAME recipe.prompt (subject template) + wildcard axes + one "vary" axis
// that fills a grid of cells per sheet (build-spec §Принцип). There is no third card type, no
// new element type, no new UI surface — every recipe widget (chrome/chip, layers panel,
// inspectorSig, export-filter, paste-remap) already works for `group.recipe`; pack mode just
// rides inside it. Prompt EXPANSION is not reimplemented here a second time:
// `.codex/skills/nt-asset-image-generation/scripts/expand_jobs.py` is the ONE expander (T0330,
// proven on the swords pilot) — this file only assembles its flat JSON config and calls it
// through the shared warm-worker bridge (runToolPython), the same seam every other canvas
// Python tool in this file uses. "One system" invariant: style/refs go through the SAME
// resolve the single-image branch uses (recipe.style_ref's prompt as `style_prefix`, its ref
// image travels as a generation ref exactly like the single-image branch) — the lead's
// requirement that a style card's ref image work in packs is satisfied BY CONSTRUCTION, not a
// separate code path.
//
// This is a TRANSPLANT, not a reimplementation: the build-spec's earlier v1 (a separate
// pack-card TYPE, dual-reviewed, implemented, then superseded by the lead's merge decision)
// proved the mechanics — normalizePackPatch's per-field rules -> normalizeRecipePack below
// (axes/vary/grid/max_jobs, byte-for-byte the same validation); expandPack -> packPreview below
// (reading recipe.* instead of a separate card's own `pack.*`; the old "picture is not sent"
// warning is DELETED — the picture IS sent in the generate branch, see the "One system" note
// above — replaced with an info flag, `style_ref_image`). `recipe.pack` itself is SLIM (v1,
// axes, vary, grid, max_jobs only) — a first review draft put background/candidates inside it
// too, but that was superseded by a SECOND lead decision (2026-07-07, on top of the
// focus-review): rather than duplicate a background/candidate-count concept the recipe already
// half-has (`params.bg_key`/`params.n_candidates`, previously write-once-at-creation, dead
// weight since patchRecipe never touched `params` at all), `params` itself is UNFROZEN for
// exactly those two fields (plus size/quality) — see normalizeRecipePatch's own `params`
// handling. `subject_template`/`style_ref` are likewise NOT part of `recipe.pack` — they were
// never duplicated in the first place, recipe's own `prompt`/`style_ref` cover them.

export const PACK_KNOWN_FIELDS = ["axes", "vary", "grid", "max_jobs"]; // `v` is a stamped constant
// (like recipe's own top-level `v`) — never a patchable field.

// Validate + normalize a FULL (non-null) `recipe.pack` value — NOT a partial patch.
// normalizeRecipePatch's `pack` field REPLACES `recipe.pack` WHOLESALE (build-spec: "patch
// ЗАМЕНЯЕТ pack целиком" — recipe itself only merges shallow, see patchRecipe's
// `{...group.recipe, ...resolved}`), so every one of these four fields must be present in
// full; a caller that only wants to tweak ONE field (cli.mjs's recipe-set, a future UI) is
// responsible for reading the project's CURRENT recipe.pack first and merging its own change
// on top BEFORE calling patchRecipe — a CLI/UI convenience, this op itself never merges onto
// the stored value (missing a field here is a loud error, not a silent carry-forward from the
// old blob). Field-level rules transplanted verbatim from phase A's normalizePackPatch: axes
// (object of axisName -> non-empty array of non-empty strings, insertion order preserved for
// expand_jobs.py's own cartesian-product order); grid ([rows, cols] integers in 1..3); max_jobs
// (a positive integer). Unknown keys (including phase A's now-removed subject_template/
// style_ref/params/background/candidates) are a loud error. Throws before any write.
export function normalizeRecipePack(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error(`recipe pack must be null or an object, got ${JSON.stringify(value)}`);
  }
  const unknownKeys = Object.keys(value).filter((key) => key !== "v" && !PACK_KNOWN_FIELDS.includes(key));
  if (unknownKeys.length) {
    throw new Error(`recipe pack: unknown field ${JSON.stringify(unknownKeys[0])} — known fields are ${PACK_KNOWN_FIELDS.join(", ")}`);
  }
  const missingKeys = PACK_KNOWN_FIELDS.filter((key) => value[key] === undefined);
  if (missingKeys.length) {
    throw new Error(
      `recipe pack: missing field ${JSON.stringify(missingKeys[0])} — patch replaces pack wholesale, send the full object ` +
        `(known fields: ${PACK_KNOWN_FIELDS.join(", ")})`,
    );
  }

  if (!value.axes || typeof value.axes !== "object" || Array.isArray(value.axes)) {
    throw new Error(`pack axes must be an object of axisName -> array of values, got ${JSON.stringify(value.axes)}`);
  }
  const axes = {};
  // Object.entries walks insertion order — the given key order is preserved verbatim
  // (expand_jobs.py's own big-axis cartesian product order, and axes_slug's file-path
  // convention, both depend on that order being stable/intentional, not silently resorted).
  for (const [axisName, values] of Object.entries(value.axes)) {
    if (!Array.isArray(values) || !values.length || !values.every((v) => typeof v === "string" && v.trim())) {
      throw new Error(`pack axes.${axisName} must be a non-empty array of non-empty strings, got ${JSON.stringify(values)}`);
    }
    axes[axisName] = [...values];
  }

  if (typeof value.vary !== "string") throw new Error(`pack vary must be a string, got ${JSON.stringify(value.vary)}`);

  if (!Array.isArray(value.grid) || value.grid.length !== 2) {
    throw new Error(`pack grid must be [rows, cols], got ${JSON.stringify(value.grid)}`);
  }
  const [rows, cols] = value.grid.map(Number);
  if (!Number.isInteger(rows) || !Number.isInteger(cols) || rows < 1 || rows > 3 || cols < 1 || cols > 3) {
    throw new Error(`pack grid must be two integers in 1..3, got ${JSON.stringify(value.grid)}`);
  }

  const maxJobs = Number(value.max_jobs);
  if (!Number.isInteger(maxJobs) || maxJobs < 1) {
    throw new Error(`pack max_jobs must be a positive integer, got ${JSON.stringify(value.max_jobs)}`);
  }

  return { v: 1, axes, vary: value.vary, grid: [rows, cols], max_jobs: maxJobs };
}

// bg_key -> expand_jobs.py's `background` enum — pack mode's own narrower pairing on top of
// `params.bg_key`'s generic hex validation (normalizeRecipePatch accepts ANY hex there; this
// mapping is where "must be exactly the magenta or green key" is actually enforced, loudly, at
// preview/generate time — build-spec: "иной hex — громкая ошибка в packPreview/pack-ветке
// generate, НЕ на patch-time"). Keys are lowercase to match hexColor's own normalization.
export const BG_KEY_BACKGROUND = { "#ff00ff": "magenta", "#00ff00": "green" };

// Path-safe slug (mirrors expand_jobs.py's own `slugify`, JS-side, for the config's `pack`
// field only — the expander re-slugifies internally for file stems regardless, so this does
// not need to be byte-identical, just filesystem/display-safe on the canvas side).
export function slugifyPackName(value, fallback = "pack") {
  const text = String(value ?? "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9_-]+/g, "-")
    .replace(/-{2,}/g, "-")
    .replace(/^-+|-+$/g, "");
  return (text || fallback).slice(0, 80);
}

// The ONE expander (T0330) — never reimplemented on the canvas side. Path is relative to the
// repo root, exactly like every other runToolPython call in this file.
export const PACK_EXPANDER_SCRIPT = ".codex/skills/nt-asset-image-generation/scripts/expand_jobs.py";

// Shared pack-mode config assembly (T0332 v2 build-spec §3: "экспандер вызывается СВЕЖИМ" for
// BOTH packPreview and generateFromRecipe's pack branch — extracted here so the two paths can
// never disagree on what recipe.* maps to expand_jobs.py's flat config; the build-spec's own
// config-assembly table lives in exactly ONE place, this function). Assembles `subject_template`
// = `recipe.prompt` VERBATIM — NEVER resolveRecipePromptText/`expanded` (pack mode ignores
// Expand-prompt entirely, by design: a stale/hand-edited `expanded` string must never silently
// leak into an axes-driven sheet the lead did not review); `style_prefix` = a style card's
// prompt, verbatim, "" when `style_ref` is null (unlike the single-image branch, pack mode never
// appends "Style: ..." — that framing is the single-image branch's alone); `axes`/`vary`/`grid`/
// `max_jobs` from `recipe.pack`; `background` DERIVED from `recipe.params.bg_key` via
// BG_KEY_BACKGROUND (loud if it is not EXACTLY the magenta/green pair — bg_key itself is generic
// hex at patch-time, this is where the pack-specific pairing is actually enforced) and
// `candidates` from `recipe.params.n_candidates`; `gen` (size/quality/model) is `recipe.params`
// wholesale. Every RECIPE_ENGINES value is legal here — codex, gemini/agy (grid adherence
// smoke-checked 2026-07-07), and "both" (lead decision 2026-07-07: compare mode on packs —
// every sheet generates on BOTH engines at 2x the paid calls, sheet identity = (sheet_axes,
// engine); see generatePackSheets). Only a hand-edited unknown engine is loud (defensive,
// checked HERE, i.e. in both packPreview and the generate pack branch, never at patch-time,
// since a cross-field refusal at patch-time would depend on edit order) and
// on a SET `style_ref` that doesn't resolve to a real style-card group (defensive re-check;
// patchRecipe already validates this at write time — a hand-edited project.json is the only way
// to reach this). Callers add their OWN `out_dir` (a required expander key, but otherwise a stub
// on the canvas path — job.out/job.input_image are dead fields here either way, since generation
// goes through the codex-seam directly, never gen_batch.py). Does not call the expander itself
// (no filesystem/subprocess work) — pure assembly + validation, so it never needs a workDir.
// Returns `{ config, styleRefImage, styleSnapshot }`: `styleRefImage` is an INFO flag (the style
// card's ref image IS sent as a generation ref in the pack branch, never phase A's old
// "not sent" warning); `styleSnapshot`, present only when style_ref is set, mirrors the
// single-image branch's own `{cardId, name, prompt}` shape for meta.pack's style_snapshot parity.
export function buildPackConfig(project, card) {
  const recipe = card.recipe;
  const cardLabel = card.name || card.id;
  if (!RECIPE_ENGINES.has(recipe.engine)) {
    // Defensive only (mirrors the single-image branch's own check) — patchRecipe validates
    // engine on write; a hand-edited project.json is the only way to reach this. "both" IS
    // legal for packs (lead, 2026-07-07: «почему если я выбрал значит мне это нужно» — cost
    // is the lead's call): every sheet generates on BOTH engines, identity = (sheet_axes,
    // engine) — see generatePackSheets.
    throw new Error(`recipe card "${cardLabel}" (${card.id}) has an invalid engine: ${JSON.stringify(recipe.engine)}`);
  }
  const pack = recipe.pack;
  // background: derived from params.bg_key, NOT a pack field. A hex that isn't exactly one of
  // the two known keys is loud HERE (not at patch-time — bg_key itself stays generic hex for
  // the single-image cutout path).
  const background = BG_KEY_BACKGROUND[String(recipe.params && recipe.params.bg_key).toLowerCase()];
  if (!background) {
    throw new Error(
      `recipe card "${cardLabel}" (${card.id}) has params.bg_key ${JSON.stringify(recipe.params && recipe.params.bg_key)} — pack mode requires ` +
        `exactly ${Object.entries(BG_KEY_BACKGROUND).map(([hex, name]) => `${hex} (${name})`).join(" or ")}`,
    );
  }
  const candidates = Number(recipe.params && recipe.params.n_candidates);
  if (!Number.isInteger(candidates) || candidates < 1) {
    throw new Error(`recipe card "${cardLabel}" (${card.id}) has an invalid params.n_candidates: ${JSON.stringify(recipe.params && recipe.params.n_candidates)}`);
  }

  // style_ref resolution: `style_ref === null` is a legal "no style" pack — expand_jobs.py's
  // `_require(config, "style_prefix")` only checks the KEY is present, not that it is non-empty
  // — so "" is accepted, not a loud error.
  let stylePrefix = "";
  let styleRefImage = false;
  let styleSnapshot; // stays undefined (key omitted) when style_ref is unset — mirrors meta.recipe
  if (recipe.style_ref) {
    const styleCard = groupsOf(project).find((group) => group.id === recipe.style_ref);
    if (!styleCard || !styleCard.style || typeof styleCard.style !== "object") {
      throw new Error(`recipe card "${cardLabel}" (${card.id}) has a style_ref that is not a style-card group: ${recipe.style_ref}`);
    }
    stylePrefix = typeof styleCard.style.prompt === "string" ? styleCard.style.prompt.trim() : "";
    if (styleCard.style.ref) styleRefImage = true;
    styleSnapshot = { cardId: styleCard.id, name: styleCard.name || styleCard.id, prompt: stylePrefix };
  }

  const config = {
    pack: slugifyPackName(card.name || card.id),
    style_prefix: stylePrefix,
    subject_template: recipe.prompt, // VERBATIM — never resolveRecipePromptText/`expanded`
    axes: pack.axes,
    sheet: { vary: pack.vary, grid: pack.grid },
    background,
    candidates,
    max_jobs: pack.max_jobs,
    gen: recipe.params,
  };

  return { config, styleRefImage, styleSnapshot };
}

// packPreview: an EPHEMERAL preview, not a mutation — no commitMutation, no journal entry, no
// write to the card at all (build-spec: "экспандер чист -> превью полностью деривативно от
// блоба"). Config assembly + engine/bg_key/candidates/style_ref validation is shared with
// generateFromRecipe's pack branch via buildPackConfig above; this function only adds its OWN
// `out_dir` (an ephemeral tmp dir, torn down before returning) and strips the expander's raw
// job objects down to `{name, prompt, cells}` (job.out/job.input_image are dead fields on the
// canvas preview path). Loud on `recipe.pack === null` (pack mode is off — nothing to preview,
// checked BEFORE buildPackConfig's own engine gate, so the two loud cases stay in a stable,
// tested order). A bad/incomplete axes setup, a `vary` that is not a key of `axes`, too many
// vary values — every one of those is expand_jobs.py's OWN law (SystemExit), surfaced verbatim
// by runToolPython (bridge.mjs) as the thrown Error; nothing here re-validates or re-wraps that
// message.
export async function packPreview(root, { projectId, groupId } = {}) {
  if (!projectId) throw new Error("packPreview requires projectId");
  if (!groupId) throw new Error("packPreview requires groupId");
  const project = getProject(root, projectId);
  const card = findGroup(project, groupId); // loud "group not found" on an unknown id
  if (!card.recipe || typeof card.recipe !== "object") {
    throw new Error(`group is not a recipe card (no recipe blob): ${groupId}`);
  }
  const recipe = card.recipe;
  const cardLabel = card.name || groupId;
  if (!recipe.pack) {
    throw new Error(`recipe card "${cardLabel}" (${groupId}) has no pack config — set recipe.pack before previewing (pack mode is off)`);
  }

  const { config: baseConfig, styleRefImage } = buildPackConfig(project, card);

  const workDir = mkdtempSync(join(tmpdir(), "canvas-pack-"));
  try {
    const config = {
      ...baseConfig,
      // `out_dir` is a required key for the expander (it builds each job's `out` path from
      // it) but is otherwise a stub here: the canvas preview path never generates or writes
      // images through gen_batch, so job.out/job.input_image below are dead fields, stripped
      // from what this op returns.
      out_dir: join(workDir, "out").replaceAll("\\", "/"),
    };
    const cfgPath = join(workDir, "pack_config.json");
    const jobsPath = join(workDir, "jobs.json");
    writeFileSync(cfgPath, `${JSON.stringify(config, null, 2)}\n`, "utf8");

    await runToolPython(root, [PACK_EXPANDER_SCRIPT, "--config", cfgPath, "--out", jobsPath]);

    const jobs = JSON.parse(readFileSync(jobsPath, "utf8"));
    // expand_jobs.py's own jobs.json is a FLAT list (sheets * candidates); "sheets" for the
    // lead's readout is the sheet count alone (matches the expander's own stdout summary,
    // `_summary`'s `len(jobs) // candidates`), not the raw job count.
    const sheets = config.candidates > 0 ? Math.round(jobs.length / config.candidates) : jobs.length;
    return {
      sheets,
      style_ref_image: styleRefImage,
      jobs: jobs.map((job) => ({ name: job.name, prompt: job.prompt, cells: job.cells })),
    };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// The big-axis-only identity of a job (build-spec's `sheet_axes`): every cell in job.cells
// shares the same big-axis values — only `vary` differs per cell (expand_jobs.py's own
// row-major fill, see its `cells` comprehension) — so cells[0]'s axes minus the vary key IS
// the sheet identity RESUME/`--sheet` key off of. Pure, no I/O.
export function sheetAxesFromJob(job, varyAxis) {
  const axes = { ...(((job && job.cells) || [])[0] || {}).axes };
  delete axes[varyAxis];
  return axes;
}

// Order-independent equality for two flat string-valued axes objects (RESUME's own dedup key).
// A plain JSON.stringify comparison would also work today (expand_jobs.py always builds a
// job's axes in the SAME `recipe.pack.axes` key order), but this is robust even if the pack's
// axes were edited/reordered between runs.
export function axesEqual(a, b) {
  const keysA = Object.keys(a || {}).sort();
  const keysB = Object.keys(b || {}).sort();
  if (keysA.length !== keysB.length) return false;
  return keysA.every((key, index) => key === keysB[index] && a[key] === b[keysB[index]]);
}

// Deep-review поправка (build-spec's "Поправка по deep-ревью", 2026-07-07): the artifacts a
// forced `--sheet` regen REPLACES, scoped to `runGroupId` ONLY (build-spec: "продвинутые за
// пределы run-группы копии не затрагиваются" — a promoted/copied cut living outside the run
// group is untouched even though its `meta.pack.sheet_element_id` still points at the old
// sheet). Two kinds of artifact fall out of one lookup:
//   - the prior sheet element(s) themselves — this card's sheets inside `runGroupId` whose
//     `meta.pack.sheet_axes` matches the forced job's sheet_axes (axesEqual — the SAME key the
//     RESUME dedup check above already uses);
//   - each such sheet's own slice subgroup, if packSlice already cut it. A slice subgroup
//     carries no meta of its own — sliceRegions mints it bare, name + parentId only (its own
//     doc comment, ~4809-4836) — so the durable link back to the ORIGINATING sheet lives on
//     the subgroup's CHILD cut elements' `meta.pack.sheet_element_id` (packSlice's own
//     perRegionMeta assembly, ~4986-4988: `{pack: {cardId, sheet_element_id: sheet.id, cell,
//     axes}}`). A cut still living directly in the run group with no wrapper (T0246's
//     single-crop edge case, sliceRegions ~4860-4863) has no subgroup to remove, but the lone
//     cut is still this sheet's own artifact and is removed too. Only cuts/subgroups CURRENTLY
//     inside `runGroupId`'s scope qualify (`cut.groupId === runGroupId` directly, or a group
//     whose `parentId === runGroupId`) — a cut (or its group) moved out of that scope keeps its
//     meta.pack pointer but falls outside this walk, so it survives untouched, matching the
//     build-spec's "copy placed outside the run group survives".
export function findForcedSheetReplacementTargets(project, runGroupId, cardId, jobSheetAxes, engine) {
  const elements = project.elements || [];
  const groups = groupsOf(project);
  const oldSheets = elements.filter(
    (el) =>
      el.groupId === runGroupId &&
      el.meta &&
      el.meta.pack &&
      Array.isArray(el.meta.pack.cells) &&
      el.meta.pack.cardId === cardId &&
      // Same (axes, ENGINE) identity the resume dedup uses — a "both" regen must replace only
      // ITS OWN engine's prior sheet, never the sibling's; a pre-engine legacy sheet is codex.
      (el.meta.pack.engine || "codex") === engine &&
      axesEqual(el.meta.pack.sheet_axes, jobSheetAxes),
  );
  const elementIds = new Set(oldSheets.map((el) => el.id));
  const groupIds = new Set();
  for (const oldSheet of oldSheets) {
    const cuts = elements.filter((el) => el.meta && el.meta.pack && el.meta.pack.sheet_element_id === oldSheet.id);
    for (const cut of cuts) {
      if (cut.groupId === runGroupId) {
        elementIds.add(cut.id); // T0246 single-crop edge case: no wrapper — the lone cut IS in-scope
        continue;
      }
      const cutGroup = cut.groupId ? groups.find((g) => g.id === cut.groupId) : null;
      if (cutGroup && cutGroup.parentId === runGroupId) {
        elementIds.add(cut.id);
        groupIds.add(cutGroup.id);
      }
      // else: the cut (or its group) lives outside runGroupId's scope — promoted/copied
      // elsewhere — never touched.
    }
  }
  return { elementIds, groupIds };
}

// One per-sheet commit (build-spec §3): optionally mints the run GROUP (first successful sheet
// of a fresh run only) + an image element (`mintPayload` present), always updates
// `recipe.last_run` to the CURRENT cumulative verdict/failed[] ("обновляется ПОСЛЕ КАЖДОГО
// листа" — an unattended agent watching the project sees partial progress even mid-run), and
// appends ONE tool_runs summary entry when `isLast` (parity with the single-image branch's own
// one-entry-per-call tool_runs write — never one per sheet). `sheetBefore` is the snapshot taken
// right before whatever (possibly slow, possibly skipped) work preceded THIS sheet only —
// refuseIfHeadMoved's tolerance is scoped to ONE sheet, not the whole run: a HEAD_CONFLICT here
// fails only this commit (the caller records it as this sheet's own failure), never voids
// earlier already-minted sheets or blocks later ones, which re-snapshot fresh. A pure "nothing
// changed" iteration (a skip that isn't the last sheet) safely no-ops via commitMutation's own
// before/after diff — this function does not special-case that itself.
export async function commitPackSheetOutcome(root, projectId, {
  groupId, sheetBefore, job, sheetName, engine, jobSheetAxes, mintPayload, currentRunGroupId, styleSnapshot, varyAxis,
  at, failedSoFar, isLast, priorResults, outcomeStatus, outcomeError, refSrcs, recipe, startedAt, forced,
}) {
  return withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("generateFromRecipe", sheetBefore, current);
    const currentCard = (current.groups || []).find((g) => g.id === groupId);
    if (!currentCard) throw new Error(`group not found: ${groupId}`);

    let workingProject = current;
    let targetGroupId = currentRunGroupId;
    let mintedElementId = null;

    if (mintPayload) {
      if (!targetGroupId) {
        // First successful sheet of a fresh run mints the result-group RIGHT of the card
        // (build-spec: "первый лист минтит result-группу рядом с картой"), carrying the
        // pack_run PROVENANCE marker (resolve --run, gate the Slice-pack button — no export
        // role, that is `group.screen`'s alone). Name: "<style card name | 'no-style'>/<vary>
        // <ts>" — several runs stay distinguishable without digging through meta.
        const styleNamePart = styleSnapshot ? styleSnapshot.name : "no-style";
        const newGroup = {
          id: `grp_${randomUUID().slice(0, 8)}`,
          name: `${styleNamePart}/${varyAxis} ${at}`,
          x: currentCard.x + currentCard.w + 16,
          y: currentCard.y,
          w: DEFAULT_RECIPE_CARD_SIZE.w,
          h: DEFAULT_RECIPE_CARD_SIZE.h,
          visible: true,
          pack_run: { v: 1, cardId: groupId, at },
        };
        const parentScope = currentCard.parentId == null || currentCard.parentId === "" ? null : String(currentCard.parentId);
        if (parentScope != null) newGroup.parentId = parentScope;
        const groupFront = frontOrder(current, parentScope);
        if (groupFront !== null) newGroup.order = groupFront;
        // A small non-journaled write (like storeAddImage's own file writes below) — only the
        // FINAL updateProject/commitMutation at the bottom of this function is journaled.
        workingProject = updateProject(root, projectId, { groups: [...groupsOf(current), newGroup] });
        targetGroupId = newGroup.id;
      }

      if (forced) {
        // Deep-review поправка: REPLACE, not duplicate — drop the prior sheet(s) sharing this
        // job's sheet_axes (and their own slice subgroup, if already cut) from the run group,
        // in this SAME commit, right before minting the fresh sheet below. See
        // findForcedSheetReplacementTargets's own doc comment for the identification rule.
        const targets = findForcedSheetReplacementTargets(workingProject, targetGroupId, groupId, jobSheetAxes, engine);
        if (targets.elementIds.size || targets.groupIds.size) {
          workingProject = updateProject(root, projectId, {
            elements: (workingProject.elements || []).filter((el) => !targets.elementIds.has(el.id)),
            groups: groupsOf(workingProject).filter((g) => !targets.groupIds.has(g.id)),
          });
        }
      }

      const added = storeAddImage(root, projectId, { name: sheetName, bytes: mintPayload.bytes, meta: { pack: mintPayload.meta } });
      workingProject = added.project;
      mintedElementId = added.element.id;

      // Place inside the run group, stacked top-to-bottom (16px gap) in mint order — mirrors
      // the single-image branch's own placement convention.
      const runGroupNow = (workingProject.groups || []).find((g) => g.id === targetGroupId);
      const siblingCount = (workingProject.elements || []).filter((el) => el.groupId === targetGroupId && el.id !== added.element.id).length;
      const fo = frontOrder(workingProject, targetGroupId);
      workingProject = {
        ...workingProject,
        elements: (workingProject.elements || []).map((element) => {
          if (element.id !== added.element.id) return element;
          const next = { ...element, groupId: targetGroupId, x: runGroupNow.x, y: runGroupNow.y + siblingCount * (element.h + 16) };
          if (fo !== null) next.order = fo;
          return next;
        }),
      };
    }

    const verdict = isLast && failedSoFar.length === 0 ? "ok" : "partial";
    const lastRun = { at, verdict, run_group_id: targetGroupId, failed: [...failedSoFar] };
    const nextGroups = (workingProject.groups || []).map((group) =>
      group.id === groupId ? { ...group, recipe: { ...group.recipe, last_run: lastRun } } : group,
    );

    const resultRow = {
      name: sheetName, // display name (engine-suffixed on a "both" run) — job.name lives in meta.pack.job
      engine,
      sheet_axes: jobSheetAxes,
      status: outcomeStatus,
      ...(outcomeStatus === "ok" ? { elementId: mintedElementId } : {}),
      ...(outcomeStatus === "failed" ? { error: outcomeError } : {}),
    };

    const patch = { groups: nextGroups };
    if (mintPayload) patch.elements = workingProject.elements;
    if (isLast) {
      // ONE tool_runs entry for the WHOLE pack run (parity with the single-image branch's own
      // one-entry-per-call write — never one per sheet), landing in the FINAL sheet's commit.
      const run = {
        id: `run_${randomUUID().slice(0, 8)}`,
        op: "generate_from_recipe_pack",
        cardId: groupId,
        at,
        params: {
          subject_template: recipe.prompt,
          engine: recipe.engine,
          refs: refSrcs,
          // Same engine-filtered record the sheets' params_snapshot carries — never a
          // hardcoded codex triple (a gemini pack run must not claim a gpt-image model).
          // "both" records the codex superset in this ONE aggregate row (single-image
          // branch's own convention); each sheet's meta.pack is the per-engine exact record.
          ...snapshotParamsForEngine(recipe.engine === "both" ? "codex" : recipe.engine, recipe.params, ["bg_key", "n_candidates"]),
          pack: recipe.pack,
        },
        result_summary: { run_group_id: targetGroupId, results: [...priorResults, resultRow], failed: failedSoFar },
      };
      patch.tool_runs = capToolRuns(root, projectId, [...(workingProject.tool_runs || []), run]);
    }

    const after = updateProject(root, projectId, patch);
    const project = commitMutation(root, projectId, {
      op: "generateRecipePackSheet",
      args_summary: { groupId, sheet: job.name, engine, runGroupId: targetGroupId, verdict, minted: !!mintPayload },
      before: current,
      after,
      startedAt,
    });
    return { project, targetGroupId, resultRow };
  });
}

// generateFromRecipe's pack branch (T0332 v2 build-spec §3): `recipe.pack` set -> generate the
// expander's flat job list as a SEQUENCE of sheets instead of one single mint. Loud, upfront,
// BEFORE any generation call: the engine gate + bg_key/candidates/style_ref validation
// (buildPackConfig, shared with packPreview), the ref-count cap (MAX_RECIPE_REFS, shared with
// the single-image branch), an unresolvable `runGroupId` (resume) or `sheetSlug` (force-regen)
// that doesn't match any expanded job. Refs (member images + the style card's ref image, when
// set) are resolved ONCE here — the SAME resolve the single-image branch uses — and travel to
// EVERY sheet's generation call unchanged. The expander is invoked FRESH (its own ephemeral tmp
// dir, torn down before generation even starts) — never packPreview's, and never stale: build-
// spec "никакого стейла по построению". Sheets generate SEQUENTIALLY via the card's own engine
// seam (`generators` injection respected, exactly like the single-image branch); each finished sheet
// mints under its OWN short commit (commitPackSheetOutcome) as soon as it lands, so a crash on
// sheet 3 never loses sheets 1-2 and per-sheet HEAD_CONFLICT tolerance never voids the whole
// run. Never throws once the sheet loop starts (unlike the single-image branch's "every engine
// failed" throw) — failures land in `failed`/`recipe.last_run.failed` instead (each row naming
// its sheet_axes AND engine), since a partial pack run is a normal, resumable outcome, not an
// all-or-nothing gesture. The generator(s) are the card's OWN recipe.engine — codex, gemini, or
// "both" (fan every job out to both engines, 2x the paid calls — lead decision 2026-07-07) —
// same `{prompt, refPaths, params}` seam as the single-image branch; each sheet records the
// engine that ACTUALLY generated it in meta.pack, and sheet identity everywhere (resume dedup,
// forced replace) is the (sheet_axes, engine) pair, legacy engineless sheets counting as codex.
export async function generatePackSheets(root, { projectId, groupId, generators, runGroupId, sheetSlug, before, card, recipe, cardLabel, startedAt }) {
  const { config: baseConfig, styleSnapshot } = buildPackConfig(before, card);

  // opts.sheetSlug WITHOUT an explicit opts.runGroupId (deep-review поправка, 2026-07-07): a
  // silent new-group fork is forbidden — "принудительный реген" only ever means "into an
  // EXISTING pack run", so resolve `recipe.last_run.run_group_id` here, loud if there is none
  // (a forced regen with nowhere to land is a caller mistake, not "start a fresh run of one").
  // An explicit opts.runGroupId always wins over this resolution.
  let resolvedRunGroupId = runGroupId;
  if (sheetSlug != null && !resolvedRunGroupId) {
    resolvedRunGroupId = recipe.last_run && recipe.last_run.run_group_id;
    if (!resolvedRunGroupId) {
      throw new Error(
        `generateFromRecipe: --sheet requires an existing pack run; pass --run or generate the pack first`,
      );
    }
  }

  // RESUME (opts.runGroupId, or the --sheet resolution above): validate the group up front
  // (existence + carries pack_run for THIS card) — loud before any generation call AND before
  // the (Python) expander spawn, since this check depends only on metadata already in `before`,
  // never on the expanded job list.
  let runGroup = null;
  if (resolvedRunGroupId) {
    runGroup = (before.groups || []).find((g) => g.id === resolvedRunGroupId);
    if (!runGroup || !runGroup.pack_run || typeof runGroup.pack_run !== "object") {
      throw new Error(`generateFromRecipe: run group not found or does not carry a pack_run marker: ${resolvedRunGroupId}`);
    }
    if (runGroup.pack_run.cardId !== groupId) {
      throw new Error(
        `generateFromRecipe: run group ${resolvedRunGroupId} belongs to a different recipe card (${runGroup.pack_run.cardId}), not this card (${groupId})`,
      );
    }
  }

  // Refs (build-spec: "референсы = те же refPaths, что собрала бы одиночная ветка") — resolved
  // ONCE, before the sheet loop, exactly like the single-image branch's own members + style-ref-
  // image resolution (never re-resolved per sheet).
  const members = recipeCardMembers(before, groupId);
  if (members.length > MAX_RECIPE_REFS) {
    throw new Error(
      `recipe card "${cardLabel}" (${groupId}) has ${members.length} reference images — generate_image.py accepts at most ${MAX_RECIPE_REFS} (--input-image)`,
    );
  }
  let refPaths = members.map((el) => resolveProjectFile(root, projectId, el.src));
  let refSrcs = members.map((el) => el.src);
  if (recipe.style_ref) {
    // buildPackConfig above already validated this resolves to a real style-card group.
    const styleCard = groupsOf(before).find((group) => group.id === recipe.style_ref);
    if (styleCard.style.ref) {
      const refElement = (before.elements || []).find((el) => el.id === styleCard.style.ref);
      if (!refElement || refElement.groupId !== styleCard.id) {
        throw new Error(
          `style card "${styleCard.name || styleCard.id}" (${styleCard.id}) ref points at a missing/non-member element: ${styleCard.style.ref}`,
        );
      }
      refPaths = [...refPaths, resolveProjectFile(root, projectId, refElement.src)];
      refSrcs = [...refSrcs, refElement.src];
    }
  }

  // Expand FRESH (build-spec: "пересобрать config и вызвать экспандер СВЕЖИМ") — a brand-new
  // tmp workDir + config, never packPreview's (torn down before generation even starts here;
  // out_dir/job.out/job.input_image are dead fields on the canvas path either way).
  const workDir = mkdtempSync(join(tmpdir(), "canvas-pack-gen-"));
  let jobs;
  try {
    const config = { ...baseConfig, out_dir: join(workDir, "out").replaceAll("\\", "/") };
    const cfgPath = join(workDir, "pack_config.json");
    const jobsPath = join(workDir, "jobs.json");
    writeFileSync(cfgPath, `${JSON.stringify(config, null, 2)}\n`, "utf8");
    await runToolPython(root, [PACK_EXPANDER_SCRIPT, "--config", cfgPath, "--out", jobsPath]);
    jobs = JSON.parse(readFileSync(jobsPath, "utf8"));
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }

  const varyAxis = recipe.pack.vary;

  // opts.sheetSlug (build-spec: "принудительный реген именно этого листа ... по имени/слагу
  // джобы экспандера") — matches the expander's own job.name verbatim, the only stable,
  // human-legible identifier a caller could have copied off a prior run/preview. Forced jobs
  // NEVER skip (even when their sheet_axes already exist in the group) — that IS the point of
  // --sheet.
  let jobsToRun = jobs;
  const forcedNames = new Set();
  if (sheetSlug != null) {
    jobsToRun = jobs.filter((job) => job.name === sheetSlug);
    if (!jobsToRun.length) {
      throw new Error(
        `generateFromRecipe: --sheet ${JSON.stringify(sheetSlug)} does not match any expanded job name for recipe card "${cardLabel}" (${groupId}) — known names: ${jobs.map((j) => j.name).join(", ")}`,
      );
    }
    for (const job of jobsToRun) forcedNames.add(job.name);
  }

  const at = new Date().toISOString(); // ONE timestamp for the whole run — pack_run.at, every
  // sheet's meta.pack.at, recipe.last_run.at, and the run group's name's <ts> all share it.
  const gens = { codex: generateImageCodex, gemini: generateImageGemini, ...(generators || {}) };

  // The run's ATTEMPT engines: "both" (lead decision 2026-07-07) fans every job out to codex
  // AND gemini — 2x the paid calls, the lead's explicit choice, exactly like the single-image
  // compare mode. The unit of everything below (dedup, forced replace, mint, failed[], result
  // rows) is a (job, engine) PAIR, never the bare job: sheet identity is (sheet_axes, engine).
  // A sheet minted before engines were recorded (no meta.pack.engine, pre-2026-07-07) counts
  // as codex — factually true, codex was the only pack engine then. Deliberate consequence:
  // resuming --run after FLIPPING the card's engine regenerates every sheet on the new engine
  // (the old engine's sheets stay put) — that IS the cheap "get the agy versions side by side"
  // gesture, not double-billing: per-sheet skip lines still print for anything already landed.
  const attemptEngines = recipe.engine === "both" ? ["codex", "gemini"] : [recipe.engine];
  // Engine-filtered per attempt engine (see snapshotParamsForEngine): bg_key/n_candidates ride
  // along on the pack branch because both genuinely shaped the run (bg baked into every sheet's
  // prompt; overgen count expanded the job list) — unlike model/quality, which agy never consumed.
  const paramsSnapshots = Object.fromEntries(
    attemptEngines.map((engine) => [engine, snapshotParamsForEngine(engine, recipe.params, ["bg_key", "n_candidates"])]),
  );

  let currentRunGroupId = runGroup ? runGroup.id : null;
  const failed = []; // {sheet_axes, engine, error}
  const results = []; // {name, engine, sheet_axes, status, elementId?, error?}
  let finalProject = before;
  const totalUnits = jobsToRun.length * attemptEngines.length;
  let unitIndex = 0;

  for (const job of jobsToRun) {
    const jobSheetAxes = sheetAxesFromJob(job, varyAxis);
    const forced = forcedNames.has(job.name);
    for (const engine of attemptEngines) {
      unitIndex += 1;
      const isLast = unitIndex === totalUnits;
      // Sheet element NAME: single-engine runs keep the expander's job.name verbatim
      // (unchanged contract); a "both" run suffixes the engine (single-image branch's own
      // "<card> codex"/"<card> agy" convention) so the two siblings stay distinguishable on
      // the canvas. meta.pack.job below always carries the bare job.name — the stable regen
      // pointer regardless of display name.
      const sheetName = attemptEngines.length > 1 ? `${job.name} ${RECIPE_ENGINE_SUFFIX[engine]}` : job.name;

      // RESUME dedup: a sheet whose (axes, ENGINE) pair is already represented among the run
      // group's minted sheet elements is SKIPPED (gen_batch's own skip-if-exists parity) —
      // unless this exact job was explicitly forced via --sheet.
      if (!forced && currentRunGroupId) {
        const liveNow = getProject(root, projectId);
        const already = (liveNow.elements || []).some(
          (el) => el.groupId === currentRunGroupId && el.meta && el.meta.pack && el.meta.pack.cardId === groupId
            && (el.meta.pack.engine || "codex") === engine && axesEqual(el.meta.pack.sheet_axes, jobSheetAxes),
        );
        if (already) {
          const sheetBefore = getProject(root, projectId);
          const outcome = await commitPackSheetOutcome(root, projectId, {
            groupId, sheetBefore, job, sheetName, engine, jobSheetAxes, mintPayload: null, currentRunGroupId, styleSnapshot, varyAxis, at,
            failedSoFar: failed, isLast, priorResults: results, outcomeStatus: "skipped", refSrcs, recipe, startedAt,
          });
          results.push(outcome.resultRow);
          currentRunGroupId = outcome.targetGroupId;
          finalProject = outcome.project;
          continue;
        }
      }

      const sheetBefore = getProject(root, projectId); // fresh snapshot right before the slow call
      let bytes = null;
      let genError = null;
      try {
        const generated = await gens[engine]({ prompt: job.prompt, refPaths, params: recipe.params });
        bytes = Buffer.isBuffer(generated) ? generated : readFileSync(generated);
      } catch (error) {
        genError = error;
      }

      if (genError) {
        failed.push({ sheet_axes: jobSheetAxes, engine, error: genError.message });
        const outcome = await commitPackSheetOutcome(root, projectId, {
          groupId, sheetBefore, job, sheetName, engine, jobSheetAxes, mintPayload: null, currentRunGroupId, styleSnapshot, varyAxis, at,
          failedSoFar: failed, isLast, priorResults: results, outcomeStatus: "failed", outcomeError: genError.message, refSrcs, recipe, startedAt,
        });
        results.push(outcome.resultRow);
        currentRunGroupId = outcome.targetGroupId;
        finalProject = outcome.project;
        continue;
      }

      const meta = {
        cardId: groupId,
        engine,
        job: job.name, // stable regen pointer: --sheet matches the expander's job.name, never the (possibly suffixed) display name
        at,
        sheet_axes: jobSheetAxes,
        cells: job.cells,
        prompt_snapshot: job.prompt,
        refs_snapshot: refSrcs,
        params_snapshot: paramsSnapshots[engine],
        ...(styleSnapshot ? { style_snapshot: styleSnapshot } : {}),
      };
      const outcome = await commitPackSheetOutcome(root, projectId, {
        groupId, sheetBefore, job, sheetName, engine, jobSheetAxes, mintPayload: { bytes, meta }, currentRunGroupId, styleSnapshot, varyAxis, at,
        failedSoFar: failed, isLast, priorResults: results, outcomeStatus: "ok", refSrcs, recipe, startedAt, forced,
      });
      results.push(outcome.resultRow);
      currentRunGroupId = outcome.targetGroupId;
      finalProject = outcome.project;
    }
  }

  const finalGroup = (finalProject.groups || []).find((g) => g.id === groupId);
  return {
    project: finalProject,
    group: finalGroup,
    run_group_id: currentRunGroupId,
    results,
    failed,
    last_run: finalGroup ? finalGroup.recipe.last_run : null,
  };
}

// ---- animation card (T0265 increment 1, video route) --------------------------
//
// An animation card is a GROUP carrying an additive `anim` object — the SAME "group + additive
// blob" shape as a recipe/style card, not a new element type (design
// docs/design_video_anim_canvas_2026-07-05.md §1.1). Keyframes (source art + storyboard) are the
// card's ordinary member IMAGE elements (assignToGroup / drag-in), ordered left-to-right by X
// (§5); the card blob itself owns only the motion/profile/seed/matte/loop settings. Generate
// mints a FLIPBOOK element (`element.flipbook`, §1.2) beside the card in its PARENT scope —
// the per-frame RGBA matte sequence, editable frame-by-frame later (increment 2); the sprite
// SHEET is a derived export (increment 2), never baked here. `updateProject` spreads `groups`
// verbatim (store.mjs), so `group.anim` round-trips through every snapshot/undo/redo/copy-paste
// with zero store/tree changes, exactly like `recipe`/`style`.

export const ANIM_PROFILES = new Set(["draft", "final"]); // WAN draft/final workflow choice
// Matte TOOL choice (video/matte stage). Default "corridorkey" (lead decision, design open
// question 1): soft glow/translucency; "key_matte" is the clean-licence opaque-sprite cutout.
export const ANIM_MATTES = new Set(["corridorkey", "key_matte"]);

// Default anim blob for a freshly-minted card (design §1.1, schema ai_studio.canvas.anim_card.v1).
// `seed:null` = a fresh random seed on every Generate; `gen_fps:null` = the workflow's own fps.
// `loop:true` -> the flipbook plays as a loop; `style_ref`/`accepted_ref` are reserved nullable
// by-id pointers (style mixing = increment 4, Accept = increment 2); `columns`/`trim` feed the
// derived sheet export (increment 2); `last_run` is written by generateAnimFromCard.
export function defaultAnim() {
  return {
    v: 1,
    motion: "",
    profile: "draft",
    seed: null,
    matte: "corridorkey",
    gen_fps: null,
    loop: true,
    columns: null,
    trim: false,
    style_ref: null,
    accepted_ref: null,
    last_run: null,
  };
}

// Validate + normalize a PARTIAL anim patch (loud, no silent coercion — mirrors
// normalizeRecipePatch). PURE of the project (type-level only): `motion` (a string; empty is
// the draft state), `profile` (ANIM_PROFILES), `seed` (null or a number), `matte`
// (ANIM_MATTES), `gen_fps` (null or a positive number), `loop`/`trim` (booleans), `columns`
// (null or a positive integer), `style_ref`/`accepted_ref` (null or a string id — the
// project-level resolution lives in patchAnim, like patchRecipe's style_ref). Returns the
// subset of resolved fields actually provided; throws before any write on anything else.
export function normalizeAnimPatch(patch) {
  if (!patch || typeof patch !== "object" || Array.isArray(patch)) {
    throw new Error(`anim patch must be an object, got ${JSON.stringify(patch)}`);
  }
  const out = {};
  if (patch.motion !== undefined) {
    if (typeof patch.motion !== "string") throw new Error(`anim motion must be a string, got ${JSON.stringify(patch.motion)}`);
    out.motion = patch.motion;
  }
  if (patch.profile !== undefined) {
    if (!ANIM_PROFILES.has(patch.profile)) {
      throw new Error(`anim profile must be one of ${[...ANIM_PROFILES].join("/")}, got ${JSON.stringify(patch.profile)}`);
    }
    out.profile = patch.profile;
  }
  if (patch.seed !== undefined) {
    if (patch.seed !== null && !Number.isFinite(Number(patch.seed))) {
      throw new Error(`anim seed must be null or a number, got ${JSON.stringify(patch.seed)}`);
    }
    out.seed = patch.seed === null ? null : Number(patch.seed);
  }
  if (patch.matte !== undefined) {
    if (!ANIM_MATTES.has(patch.matte)) {
      throw new Error(`anim matte must be one of ${[...ANIM_MATTES].join("/")}, got ${JSON.stringify(patch.matte)}`);
    }
    out.matte = patch.matte;
  }
  if (patch.gen_fps !== undefined) {
    if (patch.gen_fps !== null && (!Number.isFinite(Number(patch.gen_fps)) || Number(patch.gen_fps) <= 0)) {
      throw new Error(`anim gen_fps must be null or a positive number, got ${JSON.stringify(patch.gen_fps)}`);
    }
    out.gen_fps = patch.gen_fps === null ? null : Number(patch.gen_fps);
  }
  if (patch.loop !== undefined) {
    if (typeof patch.loop !== "boolean") throw new Error(`anim loop must be a boolean, got ${JSON.stringify(patch.loop)}`);
    out.loop = patch.loop;
  }
  if (patch.columns !== undefined) {
    if (patch.columns !== null && (!Number.isInteger(Number(patch.columns)) || Number(patch.columns) < 1)) {
      throw new Error(`anim columns must be null or a positive integer, got ${JSON.stringify(patch.columns)}`);
    }
    out.columns = patch.columns === null ? null : Number(patch.columns);
  }
  if (patch.trim !== undefined) {
    if (typeof patch.trim !== "boolean") throw new Error(`anim trim must be a boolean, got ${JSON.stringify(patch.trim)}`);
    out.trim = patch.trim;
  }
  if (patch.style_ref !== undefined) {
    if (patch.style_ref !== null && typeof patch.style_ref !== "string") {
      throw new Error(`anim style_ref must be null or a string id, got ${JSON.stringify(patch.style_ref)}`);
    }
    out.style_ref = patch.style_ref;
  }
  if (patch.accepted_ref !== undefined) {
    if (patch.accepted_ref !== null && typeof patch.accepted_ref !== "string") {
      throw new Error(`anim accepted_ref must be null or a string id, got ${JSON.stringify(patch.accepted_ref)}`);
    }
    out.accepted_ref = patch.accepted_ref;
  }
  if (!Object.keys(out).length) {
    throw new Error(
      "patchAnim requires at least one of motion, profile, seed, matte, gen_fps, loop, columns, trim, style_ref, accepted_ref",
    );
  }
  return out;
}

// Same default frame as a recipe/style card — a workshop widget, purely cosmetic (the frame
// never feeds generation; the flipbook box comes from the generated frame pixels).
export const DEFAULT_ANIM_CARD_SIZE = { w: 360, h: 280 };
// Fit-to-content margin for the "Animate this image" promotion (F4): the card frame hugs the
// member image by this much on every side. Was applied CLIENT-side (actions.js PAD=24); moved
// into the op so the whole gesture is ONE journal entry.
export const ANIM_CARD_MEMBER_PAD = 24;

// Mint an animation card: a group carrying a fresh `anim` blob (defaultAnim). Mirrors
// createRecipeCard/createStyleCard exactly (bounds fallback, optional parentId, one journal
// entry) except for the blob itself and its symmetric "cards do not nest inside cards" guard
// (refuses a parent that carries recipe/style/anim).
//
// F4 — the "Animate this image" PROMOTION path: with `memberId`, the card FITS around that
// image (member box + 24px pad) and the image is MOVED INSIDE as the first keyframe in this
// SAME commit — ONE journal entry (the law "one gesture = one record", precedent
// promoteExtractedRecipe/Style). The box is owned by fit, so explicit x/y/w/h alongside
// memberId is a loud refusal (never a silent geometry fight). Refuses a member that is not an
// image, is missing, or is already committed to another card (a member of a recipe/style/anim
// card, or a claimed style ref — assignToGroup/applyStyleAutoRef claim refs on membership):
// stealing it would silently strip the other card, so the lead duplicates the image first.
export function createAnimCard(root, { projectId, name, x, y, w, h, parentId, memberId } = {}) {
  if (!projectId) throw new Error("createAnimCard requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const cleanName = String(name || "").trim() || "Animation card";

  const member = memberId == null || memberId === "" ? null : String(memberId);
  let memberElement = null;
  let bounds;
  if (member != null) {
    if (x !== undefined || y !== undefined || w !== undefined || h !== undefined) {
      throw new Error("createAnimCard: pass memberId OR explicit x/y/w/h, not both — memberId fits the card around the image");
    }
    memberElement = (before.elements || []).find((el) => el.id === member);
    if (!memberElement) throw new Error(`element not found: ${member}`);
    if (memberElement.type !== "image" || !memberElement.src) {
      throw new Error(`element ${member} is not an image — an animation card keyframe must be an image`);
    }
    const memberGroup = memberElement.groupId ? groupsOf(before).find((g) => g.id === memberElement.groupId) : null;
    const inCard = !!(memberGroup && (memberGroup.recipe || memberGroup.style || memberGroup.anim));
    const claimedStyleRef = groupsOf(before).some(
      (g) => g.style && typeof g.style === "object" && g.style.ref === member,
    );
    if (inCard || claimedStyleRef) {
      throw new Error(`element ${member} is already a member of a card (or a claimed style ref) — duplicate the image first`);
    }
    bounds = {
      x: Math.round(Number(memberElement.x) - ANIM_CARD_MEMBER_PAD),
      y: Math.round(Number(memberElement.y) - ANIM_CARD_MEMBER_PAD),
      w: Math.round(Number(memberElement.w) + ANIM_CARD_MEMBER_PAD * 2),
      h: Math.round(Number(memberElement.h) + ANIM_CARD_MEMBER_PAD * 2),
    };
  } else {
    bounds = {
      x: finite(x) ? Number(x) : 0,
      y: finite(y) ? Number(y) : 0,
      w: finite(w) && Number(w) > 0 ? Number(w) : DEFAULT_ANIM_CARD_SIZE.w,
      h: finite(h) && Number(h) > 0 ? Number(h) : DEFAULT_ANIM_CARD_SIZE.h,
    };
  }

  const parentScope = parentId == null || parentId === "" ? null : String(parentId);
  if (parentScope != null) {
    const parent = findGroup(before, parentScope); // loud error on an unknown parent
    if (parent.recipe || parent.style || parent.anim) {
      throw new Error(
        `cannot create an animation card inside another card (parent ${parentScope} carries a card blob) — cards do not nest inside cards`,
      );
    }
  }

  const group = {
    id: groupId,
    name: cleanName,
    x: bounds.x,
    y: bounds.y,
    w: bounds.w,
    h: bounds.h,
    visible: true,
    anim: defaultAnim(),
  };
  if (parentScope != null) group.parentId = parentScope;
  const groupFront = frontOrder(before, parentScope);
  if (groupFront !== null) group.order = groupFront;

  // Move the member into the fresh card scope as its first keyframe (same commit). The card has
  // no children yet, so its scope is implicit (frontOrder null) — drop the member's old order so
  // it sorts by the v1 fallback in the new scope (scopes never go half-explicit; mirrors
  // assignToGroup). The member keeps its world position; the fitted frame surrounds it.
  let nextElements = before.elements || [];
  if (memberElement != null) {
    nextElements = nextElements.map((el) => {
      if (el.id !== member) return el;
      const moved = { ...el, groupId };
      delete moved.order;
      return moved;
    });
  }

  const after = updateProject(root, projectId, { groups: [...groupsOf(before), group], elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "createAnimCard",
    args_summary: { groupId, name: cleanName, bounds, parentId: parentScope, ...(member != null ? { memberId: member } : {}) },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Partial update of a card's `anim` blob (see normalizeAnimPatch; `last_run` is written by
// generateAnimFromCard, not this general patch). Loud on a group that carries no `anim` at all
// — a plain group (or a recipe/style card) is not an animation card. Project-aware pointer
// validation (mirrors patchRecipe's style_ref) lives here, not in normalizeAnimPatch: a
// non-null `style_ref` must resolve to a STYLE CARD group; a non-null `accepted_ref` must
// resolve to an element carrying a flipbook blob. One journal entry; undo restores the prior
// anim blob byte-exact (free via the group snapshot).
export function patchAnim(root, { projectId, groupId, patch } = {}) {
  if (!projectId) throw new Error("patchAnim requires projectId");
  if (!groupId) throw new Error("patchAnim requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const current = findGroup(before, groupId);
  if (!current.anim || typeof current.anim !== "object") {
    throw new Error(`group is not an animation card (no anim blob): ${groupId}`);
  }
  const resolved = normalizeAnimPatch(patch); // validates BEFORE any write (type-level)
  if (resolved.style_ref !== undefined && resolved.style_ref !== null) {
    const styleCard = groupsOf(before).find((group) => group.id === resolved.style_ref);
    if (!styleCard || !styleCard.style || typeof styleCard.style !== "object") {
      throw new Error(`anim style_ref must be null or the id of an existing style-card group, got ${JSON.stringify(resolved.style_ref)}`);
    }
  }
  if (resolved.accepted_ref !== undefined && resolved.accepted_ref !== null) {
    const el = (before.elements || []).find((item) => item.id === resolved.accepted_ref);
    if (!el || !el.flipbook || typeof el.flipbook !== "object") {
      throw new Error(`anim accepted_ref must be null or the id of an existing flipbook element, got ${JSON.stringify(resolved.accepted_ref)}`);
    }
  }

  const nextGroups = groupsOf(before).map((group) =>
    group.id === groupId ? { ...group, anim: { ...group.anim, ...resolved } } : group,
  );
  const after = updateProject(root, projectId, { groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "patchAnim",
    args_summary: { groupId, patch: resolved },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// The card's keyframe IMAGE members in X order (tie-break Y, then id — design §5), visible ones
// only (a hidden member never feeds generation, mirroring recipeCardMembers). The canvas hands
// this ordered path list to the generator; FLF/piecewise is the generate stage's concern.
export function animCardKeyframes(project, cardId) {
  return (project.elements || [])
    .filter((el) => el.groupId === cardId && el.type === "image" && el.visible !== false)
    .sort((a, b) => (a.x - b.x) || (a.y - b.y) || String(a.id).localeCompare(String(b.id)));
}

// The animation card's Generate button / `anim-generate` CLI verb / POST .../generate route.
// An action on an ANIMATION CARD (`group.anim`) — structurally the twin of generateFromRecipe:
// validate loudly, generate OUTSIDE the journal/lock (minutes; ComfyUI is GPU-exclusive), then
// lock + re-read + refuseIfHeadMoved only around the final import+commit. Unlike
// generateFromRecipe it imports the per-frame RGBA matte sequence (store.addFile, ONE file per
// frame — NEVER a packed sheet, design decision 2) and mints a FLIPBOOK element beside the card
// in its PARENT scope (never inside — a result can never become a keyframe feeding a future run
// of the SAME card). Increment 1 is PLAIN I2V: exactly ONE keyframe (0 or empty motion / >1
// keyframe are loud refusals). The generator seam is injectable (tests pass a fake
// `generators.run`); the default runs the Track B generate->frames->matte stages
// (tools/anim_generate.mjs). One commitMutation; one undo removes the flipbook element AND
// reverts the card's anim.last_run together.
export async function generateAnimFromCard(root, { projectId, groupId, generators } = {}) {
  if (!projectId) throw new Error("generateAnimFromCard requires projectId");
  if (!groupId) throw new Error("generateAnimFromCard requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const card = findGroup(before, groupId); // loud "group not found" on an unknown id
  if (!card.anim || typeof card.anim !== "object") {
    throw new Error(`group is not an animation card (no anim blob): ${groupId}`);
  }
  const anim = card.anim;
  const cardLabel = card.name || groupId;

  const motion = typeof anim.motion === "string" ? anim.motion.trim() : "";
  if (!motion) {
    throw new Error(`animation card "${cardLabel}" (${groupId}) has an empty motion — set one before generating`);
  }

  const keyframes = animCardKeyframes(before, groupId);
  if (!keyframes.length) {
    throw new Error(
      `animation card "${cardLabel}" (${groupId}) has no keyframes — add a source image member before generating`,
    );
  }
  if (keyframes.length > 1) {
    throw new Error(
      `animation card "${cardLabel}" (${groupId}) has ${keyframes.length} keyframes — increment 1 is plain I2V (1 keyframe); multi-keyframe FLF/piecewise is increment 3`,
    );
  }
  const keyframePaths = keyframes.map((el) => resolveProjectFile(root, projectId, el.src));
  const keyframeSrcs = keyframes.map((el) => el.src);

  // Generation runs OUTSIDE the journal + lock (minutes; ComfyUI is GPU-exclusive), exactly
  // like generateFromRecipe. The generator seam is injectable — the default orchestrates the
  // Track B video stages (tools/anim_generate.mjs), tests inject a fake `run`.
  const gen = generators && typeof generators.run === "function" ? generators : defaultAnimGenerators;
  const at = new Date().toISOString();
  const runResult = await gen.run({
    keyframePaths,
    motion,
    profile: anim.profile,
    seed: anim.seed,
    matte: anim.matte,
    gen_fps: anim.gen_fps,
  });

  const framePaths = runResult && Array.isArray(runResult.framePaths) ? runResult.framePaths : null;
  if (!framePaths || !framePaths.length) {
    throw new Error(`generateAnimFromCard: generator returned no frames for animation card "${cardLabel}" (${groupId})`);
  }
  const genMeta = (runResult && runResult.meta) || {};
  const fps = Number(genMeta.fps);
  if (!Number.isFinite(fps) || fps <= 0) {
    throw new Error(`generateAnimFromCard: generator meta.fps must be a positive number, got ${JSON.stringify(genMeta.fps)}`);
  }
  // The RESOLVED seed the generator actually used — runGenerate rolls a random one when the
  // card left seed=null, so provenance freezes the reproducible value, not `null`. The card's
  // REQUESTED seed (anim.seed) stays untouched. Fall back to the requested seed only when a
  // generator omits meta.seed (a refusal-path fake) — such a run never persists.
  const resolvedSeed = Number.isFinite(Number(genMeta.seed)) ? Number(genMeta.seed) : anim.seed;
  const runDir = runResult.runDir ?? null;
  const playMode = anim.loop ? "loop" : "once"; // ping-pong is increment 2
  const frameCount = framePaths.length;

  // Locked around ONLY the final import+commit — the multi-minute generate above ran OUTSIDE
  // it (see withProjectLock's doc in store.mjs). refuseIfHeadMoved runs BEFORE the
  // store.addFile writes below, not just at commitMutation time (generateFromRecipe's law): an
  // intermediate file/element write must never land un-journaled just because the FINAL commit
  // refuses on a concurrent edit.
  const { project, element, run } = await withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("generateAnimFromCard", before, current);
    const currentCard = (current.groups || []).find((g) => g.id === groupId);
    if (!currentCard) throw new Error(`group not found: ${groupId}`);

    // Placement (§2): PARENT scope of the card, right of the frame (16px gap) — NEVER inside.
    const parentScope = currentCard.parentId == null || currentCard.parentId === "" ? null : String(currentCard.parentId);
    const gap = 16;
    const x = currentCard.x + currentCard.w + gap;
    const y = currentCard.y;

    // Frozen per-run provenance written onto the minted element.
    const animRun = {
      cardId: groupId,
      at,
      motion,
      profile: anim.profile,
      seed: resolvedSeed,
      matte: anim.matte,
      gen_fps: anim.gen_fps,
      keyframes: keyframeSrcs,
      frame_count: frameCount,
      runDir,
      meta: { frame_w: genMeta.frame_w ?? null, frame_h: genMeta.frame_h ?? null, fps },
    };

    // Import frame 0 as the element's own file (src = frame 0, the fallback/thumbnail; its
    // pixel dims ARE one frame -> the element box). Frames 1..N-1 import via store.addFile
    // (content-addressed, no element). ONE file per frame, never a packed sheet (decision 2).
    const frame0Bytes = readFileSync(framePaths[0]);
    const added = storeAddImage(root, projectId, {
      name: cardLabel,
      bytes: frame0Bytes,
      x,
      y,
      meta: { anim_run: animRun },
    });
    const elementId = added.element.id;
    const frameW = added.element.w;
    const frameH = added.element.h;
    const frames = [{ src: added.element.src, kept: true }];
    for (let i = 1; i < framePaths.length; i += 1) {
      const bytes = readFileSync(framePaths[i]);
      const file = storeAddFile(root, projectId, { bytes, name: basename(framePaths[i]) });
      frames.push({ src: file.src, kept: true });
    }

    const flipbook = { v: 1, frames, fps, play_mode: playMode, frame_w: frameW, frame_h: frameH };

    // Attach the flipbook blob + move the element into the PARENT scope, front-ordered when
    // that scope is already explicitly ordered (mirrors generateFromRecipe's own remap).
    const fo = frontOrder(before, parentScope);
    const nextElements = (added.project.elements || []).map((el) => {
      if (el.id !== elementId) return el;
      let next = { ...el, flipbook };
      if (parentScope != null) next = { ...next, groupId: parentScope };
      if (fo !== null) next = { ...next, order: fo };
      return next;
    });

    const lastRun = { at, result_element_id: elementId, verdict: "ok" };
    const nextGroups = (added.project.groups || []).map((group) =>
      group.id === groupId ? { ...group, anim: { ...group.anim, last_run: lastRun } } : group,
    );

    const run = {
      id: `run_${randomUUID().slice(0, 8)}`,
      op: "generate_anim_from_card",
      cardId: groupId,
      at,
      params: {
        motion_snapshot: motion,
        profile: anim.profile,
        seed: resolvedSeed,
        matte: anim.matte,
        gen_fps: anim.gen_fps,
        keyframes: keyframeSrcs,
      },
      result_summary: { elementId, frame_count: frameCount, fps, play_mode: playMode, runDir },
    };

    const after = updateProject(root, projectId, {
      elements: nextElements,
      groups: nextGroups,
      tool_runs: capToolRuns(root, projectId, [...(added.project.tool_runs || []), run]),
    });

    const project = commitMutation(root, projectId, {
      op: "generateAnimFromCard",
      args_summary: { groupId, elementId, frame_count: frameCount },
      before,
      after,
      startedAt,
    });
    const element = (project.elements || []).find((el) => el.id === elementId);
    return { project, element, run };
  });

  const group = (project.groups || []).find((g) => g.id === groupId);
  return { project, element, group, run };
}

// ---- extractFromElement / promoteExtractedRecipe / promoteExtractedStyle (T0239
// increment 4: the codex VISION half of the prompt-assist seam) -------------------------
//
// FINAL shape (lead, 2026-07-03, supersedes two earlier drafts of this increment):
// extraction is a SINGLE vision call that writes IMAGE META ONLY (`element.meta.extracted`)
// — no card is minted by the vision call itself. Minting a card is a SEPARATE, CHEAP,
// non-codex "promotion" gesture (promoteExtractedRecipe / promoteExtractedStyle) that just
// re-slices the ALREADY-STORED `meta.extracted` blob, so the lead can extract ONCE and mint
// as many recipe/style cards from the same extraction as he likes, at zero extra codex cost.
// extractFromElement mirrors alphaDualPlateGenerate's own shape for the call itself: the
// slow external call runs BEFORE the single commitMutation ("generation outside the
// journal, only the mint commits"); the two promotion ops are plain synchronous mutations
// (no external call at all — same speed class as createRecipeCard/createStyleCard).
//
// Division of labor: a promoted STYLE card feeds the COMPOSABLE style-card path (link it
// into a recipe card's `style_ref` to reuse it across many generations); a promoted RECIPE
// card's prompt is deliberately the SUBJECT-ONLY text (`prompt_subject`, not `prompt_full`)
// so it composes cleanly with a separately-linked style card instead of doubling up style
// wording. `prompt_full` (the complete, standalone, paste-elsewhere prompt — lead: "точный
// промпт и уйти попробовать в другом месте") never lands in a card; it is a read/copy-only
// row in the inspector. If the lead later links a style card into a promoted recipe card and
// their style wording overlaps, that is his edit to make — neither op guards against it.

// The Extract button / `extract` CLI verb / POST .../elements/<eid>/extract route. Loud on a
// non-image element. Calls the injectable `assistant.extract` (default
// tools/prompt_assist.mjs's extractFromImage) BEFORE any write, then loudly requires the
// four keys the meta write depends on (mirrors the default impl's own validation — enforced
// here too so a fake test assistant that skips a key is caught the same way). ONE
// commitMutation writes `element.meta.extracted`; re-running OVERWRITES it (the regenerate
// ability) — a fresh journal entry each time, never silently merged with the prior result.
export async function extractFromElement(root, { projectId, elementId, assistant } = {}) {
  if (!projectId) throw new Error("extractFromElement requires projectId");
  if (!elementId) throw new Error("extractFromElement requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} is not an image`);

  const imagePath = resolveProjectFile(root, projectId, element.src);
  const extract = assistant && typeof assistant.extract === "function" ? assistant.extract : extractFromImage;
  const result = await extract({ imagePath });
  if (!result || typeof result !== "object") {
    throw new Error(`extractFromElement: assistant returned no result for element ${elementId}`);
  }
  const requiredKeys = ["prompt_full", "prompt_subject", "style_block", "description"];
  for (const key of requiredKeys) {
    if (typeof result[key] !== "string" || !result[key].trim()) {
      throw new Error(`extractFromElement: assistant result is missing a non-empty "${key}" for element ${elementId}`);
    }
  }

  const at = new Date().toISOString();
  const extracted = {
    prompt_full: result.prompt_full.trim(),
    prompt_subject: result.prompt_subject.trim(),
    style: {
      style_block: result.style_block.trim(),
      palette: Array.isArray(result.palette) ? result.palette.map((item) => String(item)) : [],
      materials: typeof result.materials === "string" ? result.materials.trim() : "",
      lighting: typeof result.lighting === "string" ? result.lighting.trim() : "",
      composition: typeof result.composition === "string" ? result.composition.trim() : "",
      constraints_block: typeof result.constraints_block === "string" ? result.constraints_block.trim() : "",
    },
    description: result.description.trim(),
    at,
  };

  // Re-read to avoid clobbering a concurrent edit (mirrors generateFromRecipe/
  // alphaDualPlateGenerate — the vision call above may have taken real time). Locked
  // (T0254 Tier 1 #1) around just this final critical section — see
  // expandRecipePrompt's identical comment / withProjectLock's doc in store.mjs. The
  // refuseIfHeadMoved check runs BEFORE the write below (not just at commitMutation
  // time) for the same reason expandRecipePrompt's does — see its comment.
  const project = await withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("extractFromElement", before, current);
    const currentElement = (current.elements || []).find((item) => item.id === elementId);
    if (!currentElement) throw new Error(`element not found: ${elementId}`);

    const nextElements = (current.elements || []).map((el2) =>
      el2.id === elementId ? { ...el2, meta: { ...(el2.meta || {}), extracted } } : el2,
    );
    const after = updateProject(root, projectId, { elements: nextElements });
    return commitMutation(root, projectId, {
      op: "extractFromElement",
      args_summary: { elementId },
      before,
      after,
      startedAt,
    });
  });
  const resultElement = (project.elements || []).find((item) => item.id === elementId);
  return { project, element: resultElement, extracted };
}

// Mint a RECIPE card BELOW the element (`y = element.y + element.h + 16`, `x = element.x`,
// frame fit to the copied image) from its ALREADY-STORED `meta.extracted` — loud when absent (run
// Extract first). NO codex call. `recipe.prompt` = the SUBJECT-ONLY text (`prompt_subject`,
// see the division-of-labor note above); card name from the prompt's first ~40 chars,
// fallback "Extracted prompt". The source element is copied in as a member (fresh id,
// centered in the frame) — member images ARE a recipe card's refs (decision 3), so the copy
// immediately feeds generation as a reference. Composed directly (never via
// createRecipeCard/pasteNodes, each of which would commit its own journal entry) so the
// whole mint is ONE journal entry; promoting the SAME element twice mints two independent
// cards (no dedup — each promotion is its own gesture).
export function promoteExtractedRecipe(root, { projectId, elementId } = {}) {
  if (!projectId) throw new Error("promoteExtractedRecipe requires projectId");
  if (!elementId) throw new Error("promoteExtractedRecipe requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const extracted = element.meta && element.meta.extracted;
  if (!extracted || typeof extracted !== "object") {
    throw new Error(`element ${elementId} has no extracted data — run Extract first`);
  }

  const promptSubject = String(extracted.prompt_subject || "").trim();
  const cardName = promptSubject.slice(0, 40).trim() || "Extracted prompt";

  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const refElementId = `el_${randomUUID().slice(0, 8)}`;
  // Fit-to-content (lead 2026-07-03: "карточкам при создании можно сразу сделать fit to
  // content"): the frame hugs the copied image + padding instead of a fixed default size —
  // a big source (e.g. 1254px) would otherwise overflow a 360x280 frame on every side.
  const cardBounds = {
    x: element.x,
    y: element.y + element.h + 16,
    w: element.w + CARD_FIT_PADDING * 2,
    h: element.h + CARD_FIT_PADDING * 2,
  };
  const refCopy = {
    id: refElementId,
    type: "image",
    src: element.src,
    x: cardBounds.x + CARD_FIT_PADDING,
    y: cardBounds.y + CARD_FIT_PADDING,
    w: element.w,
    h: element.h,
    source_w: element.source_w,
    source_h: element.source_h,
    name: element.name,
    groupId,
    meta: {},
  };
  const recipeCardGroup = {
    id: groupId,
    name: cardName,
    x: cardBounds.x,
    y: cardBounds.y,
    w: cardBounds.w,
    h: cardBounds.h,
    visible: true,
    recipe: { ...defaultRecipe(), prompt: promptSubject },
  };
  const groupFront = frontOrder(before, null);
  if (groupFront !== null) recipeCardGroup.order = groupFront;

  const after = updateProject(root, projectId, {
    elements: [...(before.elements || []), refCopy],
    groups: [...groupsOf(before), recipeCardGroup],
  });
  const project = commitMutation(root, projectId, {
    op: "promoteExtractedRecipe",
    args_summary: { elementId, cardId: groupId, refElementId },
    before,
    after,
    startedAt,
  });
  const card = (project.groups || []).find((g) => g.id === groupId);
  const refElement = (project.elements || []).find((el2) => el2.id === refElementId);
  return { project, card, refElement };
}

// Mint a STYLE card to the RIGHT of the element (`x = element.x + element.w + 16`, `y =
// element.y`, frame fit to the copied image) from its ALREADY-STORED `meta.extracted` — loud when
// absent (run Extract first). NO codex call. `style.prompt` = `style_block` (+ "\n\n" +
// `constraints_block` when non-empty); `style.ref` = the minted copy (the ONE ref image sent
// to generation, R1). Mirrors promoteExtractedRecipe exactly except for placement side and
// blob shape — see its own comment for the shared rationale (composed directly, ONE journal
// entry, no dedup on repeated promotion).
export function promoteExtractedStyle(root, { projectId, elementId } = {}) {
  if (!projectId) throw new Error("promoteExtractedStyle requires projectId");
  if (!elementId) throw new Error("promoteExtractedStyle requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const extracted = element.meta && element.meta.extracted;
  if (!extracted || typeof extracted !== "object" || !extracted.style || typeof extracted.style !== "object") {
    throw new Error(`element ${elementId} has no extracted data — run Extract first`);
  }

  const styleBlock = String(extracted.style.style_block || "").trim();
  const constraintsBlock = String(extracted.style.constraints_block || "").trim();
  const stylePrompt = constraintsBlock ? `${styleBlock}\n\n${constraintsBlock}` : styleBlock;
  const description = String(extracted.description || "").trim();
  const cardName = description.slice(0, 40).trim() || "Extracted style";

  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const refElementId = `el_${randomUUID().slice(0, 8)}`;
  // Fit-to-content — same rationale as promoteExtractedRecipe above.
  const cardBounds = {
    x: element.x + element.w + 16,
    y: element.y,
    w: element.w + CARD_FIT_PADDING * 2,
    h: element.h + CARD_FIT_PADDING * 2,
  };
  const refCopy = {
    id: refElementId,
    type: "image",
    src: element.src,
    x: cardBounds.x + CARD_FIT_PADDING,
    y: cardBounds.y + CARD_FIT_PADDING,
    w: element.w,
    h: element.h,
    source_w: element.source_w,
    source_h: element.source_h,
    name: element.name,
    groupId,
    meta: {},
  };
  const styleCardGroup = {
    id: groupId,
    name: cardName,
    x: cardBounds.x,
    y: cardBounds.y,
    w: cardBounds.w,
    h: cardBounds.h,
    visible: true,
    style: { ...defaultStyle(), prompt: stylePrompt, ref: refElementId },
  };
  const groupFront = frontOrder(before, null);
  if (groupFront !== null) styleCardGroup.order = groupFront;

  const after = updateProject(root, projectId, {
    elements: [...(before.elements || []), refCopy],
    groups: [...groupsOf(before), styleCardGroup],
  });
  const project = commitMutation(root, projectId, {
    op: "promoteExtractedStyle",
    args_summary: { elementId, cardId: groupId, refElementId },
    before,
    after,
    startedAt,
  });
  const card = (project.groups || []).find((g) => g.id === groupId);
  const refElement = (project.elements || []).find((el2) => el2.id === refElementId);
  return { project, card, refElement };
}

// ---- animateElementFromText (T0264: the text->animation bridge) ----------------------------
//
// The Animation inspector's [Animate] input / `animate` CLI verb / POST .../elements/<eid>/animate
// route. The lead selects art, types a description ("крылья медленно машут"), and the codex seam
// authors (or minimally patches) the element's `ai_studio.canvas.animation.v1` spec so the preview
// can play it immediately. Slow-op shape copied from extractFromElement: the codex call runs BEFORE
// any write, OUTSIDE the journal (mirrors alphaDualPlateGenerate — a multi-minute call must never
// hold the project lock); then ONE commitMutation writes the spec inside a re-read + refuseIfHeadMoved
// locked tail. The write itself is setElementAnimation's shape (element.animation = the validated
// spec). Additive provenance: element.meta.animation_request = {text, at} rides alongside so the NEXT
// "make it slower" call can hand codex both the current spec AND the phrasing that produced it.
//
// Works on IMAGE and TEXT elements (animation is geometry/opacity-level, never pixel-level — same as
// setElementAnimation). An image element passes its files/<src> absolute path so the codex call is a
// VISION call (the model SEES the art and can size amplitudes to it); a text element has no source
// image, so it is a TEXT-only call (the model still gets the name + w/h + description).
//
// Loud, no-fallback, atomic: an empty/whitespace `text`, a missing element, a non-image/text element,
// a non-JSON codex reply, OR a reply that fails validateAnimation all throw and write NOTHING — no
// element.animation, no meta, no journal entry. `runner` is the injectable codex-spawn seam
// (default runAnimateFromText's own codex runner); tests pass a fake so codex never spawns.
export async function animateElementFromText(root, { projectId, elementId, text, runner } = {}) {
  if (!projectId) throw new Error("animateElementFromText requires projectId");
  if (!elementId) throw new Error("animateElementFromText requires elementId");
  const description = typeof text === "string" ? text.trim() : "";
  if (!description) throw new Error("animateElementFromText requires a non-empty text");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" && element.type !== "text") {
    throw new Error(`element ${elementId} is not an image or text element (cannot animate)`);
  }

  // Image elements feed their source pixels to a VISION call; text elements have no source image, so
  // imagePath stays null and the tool falls back to a TEXT-only call. `runner` is the injectable
  // codex-SPAWN seam forwarded to runAnimateFromText (the prompt_assist precedent): the op ALWAYS
  // runs the real instruction-build + strict JSON parse, only the codex call itself is faked in tests.
  const imagePath = element.type === "image" && element.src ? resolveProjectFile(root, projectId, element.src) : null;
  const currentSpec = element.animation && typeof element.animation === "object" ? element.animation : null;
  const raw = await runAnimateFromText({
    element: { name: element.name, w: element.w, h: element.h, type: element.type },
    imagePath,
    currentSpec,
    text: description,
    runner,
  });
  const animation = validateAnimation(raw); // loud: an invalid spec writes NOTHING, no journal entry

  // Re-read + refuseIfHeadMoved + write inside the lock — see expandRecipePrompt's identical comment /
  // withProjectLock's doc in store.mjs. The codex call above already ran OUTSIDE the lock, so a
  // multi-minute authoring never blocks other mutations; a concurrent edit that landed during it is
  // caught HERE, loud (HEAD_CONFLICT), BEFORE the write.
  const at = new Date().toISOString();
  const project = await withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("animateElementFromText", before, current);
    const currentElement = (current.elements || []).find((item) => item.id === elementId);
    if (!currentElement) throw new Error(`element not found: ${elementId}`);

    const nextElements = (current.elements || []).map((el2) =>
      el2.id === elementId
        ? { ...el2, animation, meta: { ...(el2.meta || {}), animation_request: { text: description, at } } }
        : el2,
    );
    const after = updateProject(root, projectId, { elements: nextElements });
    return commitMutation(root, projectId, {
      op: "animateElementFromText",
      args_summary: { elementId, text: description.slice(0, 120), channels: animation.channels.length },
      before,
      after,
      startedAt,
    });
  });
  const resultElement = (project.elements || []).find((item) => item.id === elementId);
  return { project, element: resultElement, animation };
}
