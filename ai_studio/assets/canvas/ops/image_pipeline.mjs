// Canvas image pipeline operation domain. Public API is ../ops.mjs.
import { randomUUID } from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { extname, join } from "node:path";
import { performance } from "node:perf_hooks";
import { runPython as runToolPython } from "../../tools/image/_bridge/bridge.mjs";
import { detectImageRegions } from "../../tools/image/regions/api.mjs";
import { uploadImageSource } from "../../tools/image/sources/api.mjs";
import { buildBlackPlatePrompt, buildWhitePlatePrompt, generatePlate } from "../tools/dual_plate_generate.mjs";
import { runCorridorKey } from "../../tools/video/matte/matte.mjs";
import { resolveRepoPython, runProcess } from "../../tools/video/_lib.mjs";
import { frontOrder, isNodeHidden, isNodeTransformed, orderedChildren } from "../tree.mjs";
import { defaultTextStyle, resolveFontEntry, splitTextLines } from "../fonts.mjs";
import { addFile as storeAddFile, addImage as storeAddImage, capToolRuns, getProject, imageSize, readElementBytes, resolveProjectFile, resolveProjectPath, updateProject, withProjectLock, writeProjectBytes } from "../store.mjs";
import { zipStore } from "../zip.mjs";
import { parseScaleSpec, resolveExportScale } from "./export_scale.mjs";
import { commitMutation, finite, groupsOf, mimeForExt, readFontsManifest, refuseIfHeadMoved, resolveFontFileAbs, slug } from "./core.mjs";
import { DEFAULT_EXPORT_ROW, cleanExportRows } from "./elements.mjs";
import { elementsBBox, findGroup } from "./groups.mjs";

// Source-space detection, slicing, and alpha operations refuse transformed
// elements because stored source pixels no longer align with displayed geometry.

export function refuseIfTransformed(element, elementId) {
  if (!isNodeTransformed(element)) return;
  throw new Error(
    `element ${elementId} is rotated/flipped — reset rotation/flip to edit regions or slice (the source is untransformed)`,
  );
}

// T0265 increment 1: a flipbook element's `element.src` is a plain COPY of `frames[0].src`
// with no back-link. Any pixel op that SWAPS element.src (cleanup, alpha cutout, bake filters)
// would leave frame 0 pointing at the OLD bytes forever — the static render and the first
// animation frame would silently diverge. Per-frame editing arrives in increment 2; until then
// these ops refuse a flipbook element loudly rather than corrupt it.
export function refuseIfFlipbook(element, elementId) {
  if (element && element.flipbook && typeof element.flipbook === "object") {
    throw new Error(`element ${elementId} has a flipbook — frame-level editing lands in increment 2`);
  }
}

// Meaningful numbered names for freshly detected regions: "<base> 1..N" (base =
// element name, else "Region"). Regions that already carry a name keep it.
export function nameDetectedRegions(regions, baseName) {
  const base = String(baseName || "").trim() || "Region";
  return (regions || []).map((region, index) =>
    region && !region.name ? { ...region, name: `${base} ${index + 1}` } : region,
  );
}

// Read the element's stored image, run it through the image tools upload +
// detect pipeline (imported unmodified from ../tools/image/{sources,regions}/api.mjs), then
// persist the detected regions on the element and record a tool_runs entry. One
// journal entry makes the detection undoable.
export async function detectRegions(root, { projectId, elementId, params = {} } = {}) {
  if (!projectId) throw new Error("detectRegions requires projectId");
  if (!elementId) throw new Error("detectRegions requires elementId");
  const startedAt = performance.now();
  // Fail fast (R7): check BEFORE reading bytes/spawning Python. A missing/non-image
  // elementId is left to readElementBytes' own error below (no message drift).
  const precheck = (getProject(root, projectId).elements || []).find((item) => item.id === elementId);
  if (precheck) refuseIfTransformed(precheck, elementId);
  const { buffer, fileName } = readElementBytes(root, projectId, elementId);
  const dims = imageSize(buffer);

  const dataUrl = `data:${mimeForExt(fileName)};base64,${buffer.toString("base64")}`;
  const uploaded = await uploadImageSource(root, { fileName, dataUrl });
  const detected = await detectImageRegions(root, {
    sourcePath: uploaded.sourcePath,
    options: params || {},
  });
  const rawRegions = Array.isArray(detected.regions && detected.regions.regions)
    ? detected.regions.regions
    : [];

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "detect_regions",
    elementId,
    at: new Date().toISOString(),
    params: params || {},
    result_summary: {
      region_count: rawRegions.length,
      session_id: detected.sessionId,
      background_mode: (detected.regions && detected.regions.mode) || "",
    },
  };

  // Re-read to avoid clobbering concurrent edits, snapshot before, then persist
  // regions (and backfill source dimensions) + the tool_runs entry atomically.
  const before = getProject(root, projectId);
  const target = (before.elements || []).find((item) => item.id === elementId);
  if (!target) {
    throw new Error(`element not found: ${elementId}`);
  }
  // Detected regions get meaningful numbered names ("<element name> 1..N") so
  // region rows read as content instead of raw sizes; sliced crops inherit them.
  const regions = nameDetectedRegions(rawRegions, target.name);
  const nextElements = (before.elements || []).map((item) =>
    item.id === elementId
      ? { ...item, source_w: item.source_w || dims.width, source_h: item.source_h || dims.height, regions }
      : item,
  );
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(before.tool_runs || []), run]),
  });
  const project = commitMutation(root, projectId, {
    op: "detectRegions",
    args_summary: { elementId, region_count: regions.length },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === elementId);
  return { project, element, run, regions };
}

// ---- sliceRegions (own crop tool) --------------------------------------------

// Slice an element's stored regions into new immutable image elements. Cropping is
// done by our OWN Python tool (tools/crop_regions.py, PIL): ops writes a crop spec
// (absolute source path + the element's regions with their exact rects) and spawns
// the script once. Each region is cropped from the element's own pixels by the
// STORED rect — verbatim, no re-detection — so user-moved, resized, and hand-drawn
// regions all crop exactly where they sit (unlike a detect-then-export bridge,
// which would key/normalize the pixels and re-derive geometry). Each crop becomes a
// content-addressed file + a new image element placed in a grid to the right of the
// parent, with provenance in meta.parent. The whole slice is one journal entry
// (undo removes every crop). detectRegions still uses the image tools bridge; only
// slice is ours. Per-region spec entries are objects carrying the rect and, for a
// polygonal region, its vertex ring (the crop tool then alpha-masks outside it);
// mixed rect + polygon sets stay one spawn.
//
// Two optional opts (T0332 B3, packSlice — build-spec §4), both additive/backward-
// compatible (absent -> today's exact behavior): `perRegionMeta` is an array of plain
// objects, ONE per SELECTED region (same order — see below), shallow-merged onto each
// crop's stored `meta` ALONGSIDE the existing `parent` provenance (never replacing it —
// packSlice's own minimal `{pack: {...}}` entries ride here); `targetParentId` is an
// existing group id that becomes the minted slices-group's parent (packSlice: the pack
// run group), or — when the slice is a SINGLE crop with no wrapper group (T0246) — the
// lone crop's own groupId directly, so a 1-cell pack sheet still lands inside the run
// group. Order stability: crop_regions.py's own report preserves `spec.regions` order
// (its `enumerate` never reorders), and `spec.regions` is built from `selected` in order,
// so `selected[i]` <-> `created[i]` is a stable identity — perRegionMeta[i] belongs to
// created[i].
export async function sliceRegions(root, { projectId, elementId, regionIds, perRegionMeta, targetParentId } = {}) {
  if (!projectId) throw new Error("sliceRegions requires projectId");
  if (!elementId) throw new Error("sliceRegions requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const parent = (before.elements || []).find((item) => item.id === elementId);
  if (!parent) throw new Error(`element not found: ${elementId}`);
  if (parent.type !== "image" || !parent.src) throw new Error(`element ${elementId} is not an image`);
  refuseIfTransformed(parent, elementId);
  const allRegions = Array.isArray(parent.regions) ? parent.regions : [];
  if (!allRegions.length) {
    throw new Error(`element ${elementId} has no regions; run detectRegions first`);
  }

  let selected = allRegions;
  if (Array.isArray(regionIds) && regionIds.length) {
    const wanted = new Set(regionIds.map(String));
    selected = allRegions.filter((region) => wanted.has(String(region.id)));
    const found = new Set(selected.map((region) => String(region.id)));
    const missing = [...wanted].filter((id) => !found.has(id));
    if (missing.length) throw new Error(`unknown region id(s): ${missing.join(", ")}`);
  }
  if (!selected.length) throw new Error("no regions selected to slice");

  // Validate both new opts loudly, BEFORE any python spawn (same "fail before work" law
  // every other loud check in this op already follows).
  if (perRegionMeta !== undefined && (!Array.isArray(perRegionMeta) || perRegionMeta.length !== selected.length)) {
    throw new Error(
      `sliceRegions: perRegionMeta must be an array aligned with the ${selected.length} selected region(s), got ${Array.isArray(perRegionMeta) ? `${perRegionMeta.length} entries` : typeof perRegionMeta}`,
    );
  }
  if (targetParentId !== undefined && targetParentId !== null) {
    findGroup(before, targetParentId); // loud "group not found" on an unknown id
  }

  const sourceAbs = resolveProjectFile(root, projectId, parent.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-slice-"));
  const created = [];
  try {
    const specPath = join(workDir, "crop_spec.json");
    const reportPath = join(workDir, "crop_report.json");
    const spec = {
      schema: "ai_studio.canvas.crop_regions_spec.v1",
      source: sourceAbs,
      output_dir: workDir,
      report: reportPath,
      // Objects (not bare rects): a polygonal region also carries its vertex ring, so
      // the crop tool masks alpha outside the polygon (bbox crop + ImageDraw.polygon).
      regions: selected.map((region) => {
        const rect = region.rect || region.content_bbox;
        if (!Array.isArray(rect) || rect.length !== 4) {
          throw new Error(`region ${region.id} has no rect to slice`);
        }
        const entry = { id: String(region.id), rect };
        if (Array.isArray(region.polygon) && region.polygon.length >= 3) entry.polygon = region.polygon;
        return entry;
      }),
    };
    writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
    await runToolPython(root, ["ai_studio/assets/canvas/tools/crop_regions.py", "--spec", specPath]);
    const report = JSON.parse(readFileSync(reportPath, "utf8"));
    const crops = (report && report.crops) || [];
    if (!crops.length) throw new Error("crop_regions produced no crops");

    // Place crops in a neat grid to the right of the parent (gap in source pixels).
    const gap = 16;
    const startX = parent.x + parent.w + gap;
    const columns = Math.max(1, Math.ceil(Math.sqrt(crops.length)));
    let col = 0;
    let cursorX = startX;
    let rowY = parent.y;
    let rowMaxH = 0;
    for (const crop of crops) {
      const bytes = readFileSync(join(workDir, crop.file));
      // Name the crop after the region's name when set (sanitized/trimmed), else
      // fall back to the <parent-name>#<region-id> provenance scheme.
      const region = selected.find((item) => String(item.id) === String(crop.id));
      const cropName = region && region.name ? String(region.name).trim() : "";
      const added = storeAddImage(root, projectId, {
        name: cropName || `${parent.name}#${crop.id}`,
        bytes,
        x: cursorX,
        y: rowY,
        meta: { parent: { elementId, regionId: crop.id, sheetSrc: parent.src } },
      });
      created.push(added.element);
      cursorX += added.element.w + gap;
      rowMaxH = Math.max(rowMaxH, added.element.h);
      col += 1;
      if (col >= columns) {
        col = 0;
        cursorX = startX;
        rowY += rowMaxH + gap;
        rowMaxH = 0;
      }
    }
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }

  // Wrap the crops in a fresh "<sheet name> slices" group so a big slice never dumps N
  // loose elements onto the scene (lead 2026-07-02) — but a SINGLE crop skips the wrapper
  // entirely (T0246, lead 2026-07-03: "если я делаю один вырез региона из картинки, то
  // группа не нужна" — a one-element group is noise) and stays a loose element at its
  // grid spot. Same journal entry either way: one undo removes the group (when present)
  // AND every crop together.
  let group = null;
  const targetParentScope = targetParentId != null ? String(targetParentId) : null;
  if (created.length > 1) {
    const pad = 24;
    const { minX, minY, maxX, maxY } = elementsBBox(created);
    group = {
      id: `grp_${randomUUID().slice(0, 8)}`,
      name: `${String(parent.name || "sheet").trim()} slices`,
      x: minX - pad,
      y: minY - pad,
      w: maxX - minX + pad * 2,
      h: maxY - minY + pad * 2,
      visible: true,
    };
    // targetParentId (T0332 B3): nest the fresh slices-group under the caller's group
    // (packSlice: the pack run group) instead of leaving it top-level.
    if (targetParentScope != null) group.parentId = targetParentScope;
    // Front order keeps an explicitly-ordered destination scope explicit (no-op on a
    // never-reordered scope). The crops sit in the group's own fresh scope either way.
    const sliceGroupFront = frontOrder(before, targetParentScope);
    if (sliceGroupFront !== null) group.order = sliceGroupFront;
  }
  const createdIds = new Set(created.map((element) => element.id));
  // perRegionMeta[i] belongs to created[i] (see the doc comment above for why that index
  // alignment holds) — mapped here so the final element patch below can merge it in.
  const metaByCreatedId = new Map();
  if (perRegionMeta !== undefined) {
    created.forEach((element, index) => metaByCreatedId.set(element.id, perRegionMeta[index]));
  }

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "slice_regions",
    elementId,
    at: new Date().toISOString(),
    params: { regionIds: selected.map((region) => String(region.id)) },
    result_summary: { slice_count: created.length, group_id: group ? group.id : null },
  };
  const current = getProject(root, projectId);
  const withRun = updateProject(root, projectId, {
    groups: group ? [...groupsOf(current), group] : groupsOf(current),
    elements: (current.elements || []).map((element) => {
      if (!createdIds.has(element.id)) return element;
      let next = element;
      if (group) next = { ...next, groupId: group.id };
      // No wrapper group (single crop, T0246) but a targetParentId was given: land the
      // lone crop directly in that group (packSlice's 1-cell-sheet edge case) instead of
      // leaving it loose — absent targetParentId, this is a no-op (today's behavior).
      else if (targetParentScope != null) next = { ...next, groupId: targetParentScope };
      if (metaByCreatedId.has(element.id)) next = { ...next, meta: { ...(next.meta || {}), ...metaByCreatedId.get(element.id) } };
      return next;
    }),
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
  });
  const project = commitMutation(root, projectId, {
    op: "slice",
    args_summary: {
      elementId,
      regionIds: selected.map((region) => String(region.id)),
      created: created.map((element) => element.id),
      count: created.length,
      groupId: group ? group.id : null,
    },
    before,
    after: withRun,
    startedAt,
  });
  // Hand back the STORED crops (they carry groupId when grouped), not the pre-group copies.
  const fresh = (project.elements || []).filter((element) => createdIds.has(element.id));
  return {
    project,
    created: created.map((element) => fresh.find((item) => item.id === element.id) || element),
    group: group ? (project.groups || []).find((item) => item.id === group.id) : null,
    run,
    regions: selected,
  };
}

// ---- packSlice (T0332 B3: slice every sheet of a pack run) --------------------------

// The pack-mode counterpart to a manual detect+slice: for EVERY sheet element of a pack
// run (build-spec §4), detect its regions, gate the count against the sheet's own
// manifest (`meta.pack.cells`), and — only on a match — slice it, reparenting the fresh
// cuts into the run group with a MINIMAL per-cut meta (`meta.pack = {cardId,
// sheet_element_id, cell, axes}` — the full manifest/prompt stay on the sheet, the
// provenance anchor; duplicating them onto every cut would balloon a 21-cut pack to
// 100+KB of repeated JSON). Never throws mid-loop: every sheet lands in the returned
// per-sheet contract with its own verdict, so a rejected/errored sheet never blocks its
// siblings (build-spec: "остальные режутся"/"never throws mid-loop").
//
// Verdict meanings (the build-spec's prose only names the count gate explicitly as
// REJECT; MISSING is this implementation's own, clearly-scoped reading, called out here
// since the spec text does not fully enumerate it):
//   - "OK": detection ran AND region_count === cells_len -> sliced; cut_ids populated.
//   - "REJECT": detection ran but region_count !== cells_len (the build-spec's hard
//     count gate) — including zero regions detected. `region_count`/`cells_len` ARE the
//     got/expected pair the build-spec requires ("REJECT обязан называть got/expected").
//   - "MISSING": the sheet's own data could not even be turned into a verdict — detection
//     itself threw (unreadable/corrupt image, a rotated/transformed sheet, element
//     deleted mid-run, an image-tools pipeline error) or a post-gate slice unexpectedly
//     failed. `region_count`/`cut_ids` are 0/[] since neither ran to completion.
// A sheet element is identified by carrying `meta.pack.cells` (an ARRAY — the full
// manifest) as opposed to a CUT's own `meta.pack.cell` (singular) after a prior
// packSlice run — this positively distinguishes "a sheet still to slice" from "a cut
// already sliced" even in the single-cut-no-wrapper edge case (T0246), where a cut can
// land directly in the run group next to sheets.
export async function packSlice(root, { projectId, groupId, runGroupId } = {}) {
  if (!projectId) throw new Error("packSlice requires projectId");
  if (!groupId) throw new Error("packSlice requires groupId");
  const project = getProject(root, projectId);
  const card = findGroup(project, groupId); // loud "group not found" on an unknown id
  if (!card.recipe || typeof card.recipe !== "object") {
    throw new Error(`group is not a recipe card (no recipe blob): ${groupId}`);
  }

  // Resolve the run group: an explicit --run MUST carry a pack_run marker for THIS card
  // (same validation generateFromRecipe's own --run resume path uses); omitted resolves
  // recipe.last_run.run_group_id (the most recent run), loud if that is unset or dangling.
  let resolvedRunGroupId = runGroupId;
  if (resolvedRunGroupId) {
    const runGroup = (project.groups || []).find((g) => g.id === resolvedRunGroupId);
    if (!runGroup || !runGroup.pack_run || typeof runGroup.pack_run !== "object") {
      throw new Error(`packSlice: run group not found or does not carry a pack_run marker: ${resolvedRunGroupId}`);
    }
    if (runGroup.pack_run.cardId !== groupId) {
      throw new Error(
        `packSlice: run group ${resolvedRunGroupId} belongs to a different recipe card (${runGroup.pack_run.cardId}), not this card (${groupId})`,
      );
    }
  } else {
    resolvedRunGroupId = card.recipe.last_run && card.recipe.last_run.run_group_id;
    if (!resolvedRunGroupId) {
      throw new Error(`packSlice: recipe card "${card.name || groupId}" (${groupId}) has no last_run.run_group_id — generate a pack run first, or pass --run`);
    }
    const runGroup = (project.groups || []).find((g) => g.id === resolvedRunGroupId);
    if (!runGroup || !runGroup.pack_run || typeof runGroup.pack_run !== "object") {
      throw new Error(`packSlice: last_run.run_group_id ${resolvedRunGroupId} does not resolve to a pack_run group (was it deleted?)`);
    }
  }

  const sheetElements = (project.elements || []).filter(
    (el) => el.groupId === resolvedRunGroupId && el.meta && el.meta.pack && Array.isArray(el.meta.pack.cells),
  );
  if (!sheetElements.length) {
    throw new Error(`packSlice: run group ${resolvedRunGroupId} has no sheet elements (meta.pack with a cells manifest) to slice`);
  }

  const contract = [];
  for (const sheet of sheetElements) {
    const cells = sheet.meta.pack.cells;
    let detected;
    try {
      detected = await detectRegions(root, { projectId, elementId: sheet.id });
    } catch {
      // Detection itself could not run to completion for this sheet — MISSING (see the
      // doc comment above). The next sheet is still attempted (never throws mid-loop).
      contract.push({ sheet_element_id: sheet.id, verdict: "MISSING", region_count: 0, cells_len: cells.length, cut_ids: [] });
      continue;
    }
    const regionCount = (detected.regions || []).length;
    if (regionCount !== cells.length) {
      // The build-spec's hard count gate, named REJECT regardless of WHY the counts
      // differ (0 detected is as much a mismatch as too many/too few).
      contract.push({ sheet_element_id: sheet.id, verdict: "REJECT", region_count: regionCount, cells_len: cells.length, cut_ids: [] });
      continue;
    }
    try {
      // cells[i] <-> the i-th detected region, ROW-MAJOR (build-spec: expand_jobs.py
      // already emits `cells` in row-major cell order; the region detector's own
      // left-to-right/top-to-bottom scan order is assumed to line up — an accepted v1
      // approximation, not re-litigated here).
      const perRegionMeta = cells.map((cell) => ({
        pack: { cardId: groupId, sheet_element_id: sheet.id, cell: cell.cell, axes: cell.axes },
      }));
      const sliced = await sliceRegions(root, {
        projectId,
        elementId: sheet.id,
        perRegionMeta,
        targetParentId: resolvedRunGroupId,
      });
      contract.push({
        sheet_element_id: sheet.id,
        verdict: "OK",
        region_count: regionCount,
        cells_len: cells.length,
        cut_ids: sliced.created.map((el) => el.id),
      });
    } catch {
      // Detection matched but slicing itself failed (e.g. a crop_regions.py spawn
      // error) — the sheet's data could not be turned into cuts, so MISSING, not
      // REJECT (which is reserved for the count gate specifically).
      contract.push({ sheet_element_id: sheet.id, verdict: "MISSING", region_count: regionCount, cells_len: cells.length, cut_ids: [] });
    }
  }
  return { contract, run_group_id: resolvedRunGroupId };
}

// ---- alphaCutout (own alpha tool) --------------------------------------------

// Accepted alpha methods on the canvas: the auto route (soft_score router picks key_matte, and
// refuses a wide soft zone that would need a dual-plate pair), an explicit key_matte force, and
// three EXPLICIT-ONLY neural methods — "corridorkey", "vitmatte", "birefnet" — the auto router
// NEVER yields any of them (it routes only key_matte|dual_plate). Their niches (lead-ratified,
// alpha-methods-portfolio bench 2026-07-07):
//   - "corridorkey" — neural CorridorKey green-screen matte for soft glow / translucent / soft-edge
//     art (T0261, magenta shim + regions T0262). GREEN native + MAGENTA via a hue+180 shim; any
//     OTHER key is a LOUD refusal; ~13-16s cold GPU load; region scoping composites the whole-frame
//     CK result into the requested regions (CK itself has no per-region pass). FIRST choice for glow.
//   - "vitmatte" — ViTMatte alpha matting for THIN detail (spider-web / mesh / fur / hair) on a flat
//     GREEN/MAGENTA key, and the SECOND-priority glow keyer after corridorkey. Its OWN GPU-torch
//     venv (never the shared repo venv); a key that is neither green nor magenta is a LOUD refusal
//     pointing at matte/alphaDualPlate; ~1-3s GPU; WHOLE-ELEMENT only in v1 (region-scoped is a loud
//     refusal). Weights carry an Adobe-DIM noncommercial caveat (local-only, T0335 gate).
//   - "birefnet" — BiRefNet-general SOD cutout for an ARBITRARY/unknown background with NO chroma
//     key at all (its niche is exactly where route_cutout finds no flat key, so it has NO key gate);
//     shared repo venv (CPU onnxruntime, ~10-30s); MIT; weak on flat line-art; WHOLE-ELEMENT only in
//     v1 (region-scoped is a loud refusal).
// alpha_dualplate needs a white+black plate PAIR — a single elementId call can't provide one, so
// asking for it here is a loud error that points at the separate alphaDualPlate op (T0237), which
// takes 2 elementIds instead; a translucent uniform interior (ghost/glass) is dual-plate ONLY.
export const ALPHA_METHODS = new Set(["auto", "matte", "corridorkey", "vitmatte", "birefnet"]);

// Resolve the optional region-id selection against the element's STORED regions (same
// model as slice's regionIds), so moved/resized/hand-drawn regions key exactly where they
// sit. Each spec entry carries the rect and, for a polygonal region, its vertex ring.
// Returns null for "whole element" (no regions requested).
export function resolveAlphaRegions(element, regions) {
  const requested = Array.isArray(regions) ? regions.map(String) : [];
  if (!requested.length) return null;
  const allRegions = Array.isArray(element.regions) ? element.regions : [];
  const wanted = new Set(requested);
  const selected = allRegions.filter((region) => wanted.has(String(region.id)));
  const found = new Set(selected.map((region) => String(region.id)));
  const missing = requested.filter((id) => !found.has(id));
  if (missing.length) throw new Error(`unknown region id(s): ${missing.join(", ")}`);
  const specRegions = selected.map((region) => {
    const rect = region.rect || region.content_bbox;
    if (!Array.isArray(rect) || rect.length !== 4) throw new Error(`region ${region.id} has no rect for alpha`);
    const entry = { id: String(region.id), rect };
    if (Array.isArray(region.polygon) && region.polygon.length >= 3) entry.polygon = region.polygon;
    return entry;
  });
  if (!specRegions.length) throw new Error("no regions selected for alpha");
  return specRegions;
}

// Run ONE element's CURRENT pixels through alpha_cutout.py (own worker spawn + own temp
// dir) and return the new content-addressed src + the tool's report, WITHOUT touching
// project.json — the caller (single or batch) commits. Shared so there is exactly one
// implementation of the actual keying step for both paths.
export async function runAlphaCutoutTool(root, projectId, element, chosen, specRegions) {
  const sourceAbs = resolveProjectFile(root, projectId, element.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-alpha-"));
  try {
    const specPath = join(workDir, "alpha_spec.json");
    const reportPath = join(workDir, "alpha_report.json");
    const outPath = join(workDir, "alpha_out.png");
    const spec = {
      schema: "ai_studio.canvas.alpha_cutout_spec.v1",
      source: sourceAbs,
      output: outPath,
      report: reportPath,
      method: chosen,
    };
    if (specRegions) spec.regions = specRegions;
    writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
    await runToolPython(root, ["ai_studio/assets/canvas/tools/alpha_cutout.py", "--spec", specPath]);
    const report = JSON.parse(readFileSync(reportPath, "utf8"));
    const bytes = readFileSync(outPath);
    // Content-addressed write WITHOUT a new element: the caller swaps the element's src.
    const newSrc = storeAddFile(root, projectId, { bytes, name: `${slug(element.name)}.png` }).src;
    return { newSrc, report };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// ---- corridorkey (neural green-screen matte; T0261, magenta shim + regions T0262) --------------
//
// CorridorKey is a THIRD, EXPLICIT alpha method scoped to GREEN/MAGENTA-key soft/glow/translucent
// art (the same niche it fills in the video Track-B pipeline). It is architecturally green/blue only
// (no magenta checkpoint) — the canvas gets magenta support via a preprocessing SHIM, not a second
// model. The invocation reuses the video pipeline's runCorridorKey() verbatim (one source of truth);
// this file adds the canvas seam: the border-key gate, the hue180 shim (magenta only), the 1-frame
// staging, the region composite, the content-addressed src swap, and provenance — identical contract
// to key_matte so the single/batch/undo paths are method-agnostic. There is NO auto-router entry and
// NO silent fallback to key_matte: a key that is neither green nor magenta, or a missing CK venv, is
// a LOUD refusal.
//
// Magenta via hue180 shim (T0262, research_corridorkey_magenta_2026-07-05.md): rotate the input's
// hue +180 (magenta 300deg -> green 120deg, value-preserving HSV rotation — S/V untouched, so dark
// subjects survive), stage THAT as the frame, run CorridorKey's shipped GREEN checkpoint exactly as
// the native green path does, then rotate the reconstructed FG's RGB back -180 (its own inverse mod
// 360); the alpha channel is copied through byte-exact, never touched by the color shim. Measured a
// STRICT upgrade over the blue-on-magenta path on the research's hard-edge fixtures (subject dE76
// 2.7 -> 2.0, rim contamination -> 0%) — runner precedent
// C:\projects\video_gen_experiment\static_eval\trick_run.py, ported into
// tools/ck_pixel_ops.py (hue_shift_image) since this repo's venv has no cv2. UNTESTED on soft/glow
// magenta so far (only hard-edge fixtures were measured) — key_matte remains the recommended
// DEFAULT for FLAT magenta art (it beats CK there outright, exact color, ~200ms); the shim only
// matters for an explicit CK choice on magenta glow/soft art. Provenance records
// meta.alpha.shim = "hue180" when the shim ran.
//
// Regions (T0262): CorridorKey itself is always whole-frame (no per-region neural pass); a
// region-scoped request runs CK ONCE on the whole element (with the shim first, if magenta) and
// then composites the result into the requested regions via tools/ck_pixel_ops.py's
// compose_regions — reusing alpha_cutout.py's region_mask/clamp_rect verbatim (imported, not
// duplicated), the exact mask/paste contract key_matte's own region path uses.

// CorridorKey's shipped checkpoints key GREEN (and blue); the canvas supports GREEN natively. This
// green test mirrors corridorkey_prep.py's rough_chroma_hint dominance rule EXACTLY (g - max(r,b) >
// 40 AND g > 110) so the op-level gate and the coarse AlphaHint agree on what "green" means (single
// source of the green definition; the two constants below are the only duplication, and are pinned
// to that file).
export const CK_GREEN_DOMINANCE = 40;
export const CK_GREEN_MIN = 110;
export function isCorridorKeyGreenKey(key) {
  if (!Array.isArray(key) || key.length < 3) return false;
  const [r, g, b] = key.map((value) => Number(value));
  return g - Math.max(r, b) > CK_GREEN_DOMINANCE && g > CK_GREEN_MIN;
}

// MAGENTA border-key test, via the hue180 shim (not a second CorridorKey checkpoint). Mirrors
// trick_run.py's magenta_hint dominance rule EXACTLY (min(r,b) - g > 40 AND min(r,b) > 110) — the
// same dominance/floor pair as the green rule above, mirrored across the opposite channels (r,b
// dominant over g instead of g dominant over r,b).
export const CK_MAGENTA_DOMINANCE = 40;
export const CK_MAGENTA_MIN = 110;
export function isCorridorKeyMagentaKey(key) {
  if (!Array.isArray(key) || key.length < 3) return false;
  const [r, g, b] = key.map((value) => Number(value));
  return Math.min(r, b) - g > CK_MAGENTA_DOMINANCE && Math.min(r, b) > CK_MAGENTA_MIN;
}

// Classify the element's border key for CorridorKey: "green" (native checkpoint), "magenta" (hue180
// shim), or null (neither — a LOUD refusal, no silent fallback).
export function classifyCorridorKeyBorder(key) {
  if (isCorridorKeyGreenKey(key)) return "green";
  if (isCorridorKeyMagentaKey(key)) return "magenta";
  return null;
}

// LOUD refusal (before the ~15s GPU call) for a key CorridorKey cannot process: names the detected
// key and the methods to use instead — never a silent fringe. Returns the classification ("green" |
// "magenta") for the caller to route on.
export function assertCorridorKeySupportedKey(key) {
  const classified = classifyCorridorKeyBorder(key);
  if (classified) return classified;
  const [r, g, b] = Array.isArray(key) ? key : [];
  throw new Error(
    `corridorkey supports green screens natively and magenta via a hue180 shim — the border key of ` +
      `this element is rgb(${r}, ${g}, ${b}), which is neither. Use method "matte" (key_matte) for ` +
      `other flat keys, or the alphaDualPlate op for a neutral (non-chroma) background.`,
  );
}

// Write a ck_pixel_ops.py spec JSON into workDir and return its path — the shared plumbing for both
// hue_shift (the magenta shim, applied twice: staging in, un-rotating FG out) and compose_regions
// (region-scoped corridorkey) calls into the ONE new canvas-side Python pixel helper.
export function writeCkPixelOpsSpec(workDir, name, fields) {
  const specPath = join(workDir, `${name}.json`);
  writeFileSync(
    specPath,
    `${JSON.stringify({ schema: "ai_studio.canvas.ck_pixel_ops_spec.v1", ...fields }, null, 2)}\n`,
  );
  return specPath;
}

// Estimate the element's flat-key colour the SAME way the auto router does: route_cutout.py returns
// the border-estimated key (lib/color.estimate_border_key — the one shared convention), through the
// warm repo-venv worker (no GPU, sub-second) — the cheap gate BEFORE the expensive neural call.
// `gate` labels the error with the METHOD that asked (corridorkey or vitmatte both gate through
// here) so a failure is never blamed on a method the caller did not request.
export async function estimateBorderKeyRgb(root, sourceAbs, gate = "corridorkey green gate") {
  const stdout = await runToolPython(root, [
    "ai_studio/assets/tools/image/route/route_cutout.py",
    "--image",
    sourceAbs,
    "--json",
  ]);
  const line = String(stdout).trim().split(/\r?\n/).filter(Boolean).pop();
  let decision;
  try {
    decision = JSON.parse(line);
  } catch (error) {
    throw new Error(`${gate}: could not parse route_cutout output (${error.message}): ${stdout}`);
  }
  const key = decision && decision.key;
  if (!Array.isArray(key) || key.length < 3) {
    throw new Error(`${gate}: route_cutout returned no border key`);
  }
  return [Number(key[0]), Number(key[1]), Number(key[2])];
}

// The DEFAULT (real, GPU-backed) CorridorKey invocation. Injectable via the op's `corridorKey`
// option so tests fake the ~15s GPU run with no GPU/venv in the suite — the same seam shape as
// alphaDualPlateGenerate's `generator`. Contract: writes a straight-RGBA PNG to `outAbs` and returns
// a canvas alpha report, ALWAYS whole-frame (region composition, when requested, happens one layer
// up in runCorridorKeyCutoutTool). Steps: (1) KEY GATE — estimate the border key and LOUD-refuse a
// key that is neither green nor magenta, before spending the model load; (2) if magenta, hue180-shim
// the source into the staged frame (ck_pixel_ops.py hue_shift) so it reads as green to the shipped
// checkpoint — otherwise stage the source verbatim; (3) reuse runCorridorKey() (video Track-B) on
// the 1-frame shot, ALWAYS screenColor "green" (the shim already did the color work for magenta); (4)
// if magenta, hue180-shim the reconstructed frame back (RGB only, alpha byte-exact) into `outAbs` —
// otherwise copy it verbatim. `corridorkey_prep.py` inside runCorridorKey builds the coarse
// green-dominance AlphaHint from whatever frame it is given (the shim makes a magenta source read as
// that green dominance too), so the whole subject (incl. its soft glow halo) is kept either way.
export async function defaultCorridorKeyInvoke(root, { sourceAbs, outAbs }) {
  const key = await estimateBorderKeyRgb(root, sourceAbs);
  const classified = assertCorridorKeySupportedKey(key);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-ck-"));
  try {
    const framesDir = join(workDir, "frames");
    const outDir = join(workDir, "matte");
    mkdirSync(framesDir, { recursive: true });
    const stagedFrame = join(framesDir, "frame_000.png");
    if (classified === "magenta") {
      const shimInSpec = writeCkPixelOpsSpec(workDir, "hue_shift_in", {
        op: "hue_shift",
        source: sourceAbs,
        output: stagedFrame,
      });
      await runToolPython(root, ["ai_studio/assets/canvas/tools/ck_pixel_ops.py", "--spec", shimInSpec]);
    } else {
      copyFileSync(sourceAbs, stagedFrame);
    }
    const { report: ck } = await runCorridorKey({ root, framesDir, outDir, runDir: workDir, screenColor: "green" });
    const outFrame = join(outDir, "frame_000.png");
    if (!existsSync(outFrame)) throw new Error(`CorridorKey produced no RGBA frame at ${outFrame}`);
    if (classified === "magenta") {
      const shimOutSpec = writeCkPixelOpsSpec(workDir, "hue_shift_out", {
        op: "hue_shift",
        source: outFrame,
        output: outAbs,
      });
      await runToolPython(root, ["ai_studio/assets/canvas/tools/ck_pixel_ops.py", "--spec", shimOutSpec]);
    } else {
      copyFileSync(outFrame, outAbs);
    }
    return {
      schema: "ai_studio.canvas.alpha_cutout_report.v1",
      method: "corridorkey",
      tool: "corridorkey",
      key_color: key,
      screen_color: "green",
      ...(classified === "magenta" ? { shim: "hue180" } : {}),
      commit: ck.commit,
      license: ck.license,
      settings: ck.settings,
      wall_seconds: ck.wall_seconds,
      per_frame_seconds: ck.per_frame_seconds,
      region_count: 0,
      regions: [{ id: "*element*", method: "corridorkey", routed: "corridorkey", key }],
    };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Run ONE element's CURRENT pixels through CorridorKey (own temp dir) and return the new
// content-addressed src + the canvas alpha report — the corridorkey sibling of runAlphaCutoutTool,
// so the single AND batch paths swap src / record provenance / undo BYTE-IDENTICALLY to key_matte.
// `invoke` is the injectable CK seam (default defaultCorridorKeyInvoke; tests fake it) and ALWAYS
// runs whole-frame (CorridorKey has no per-region neural pass). `specRegions`, when given (T0262),
// composites the whole-frame CK result into just those regions via ck_pixel_ops.py's
// compose_regions (reusing alpha_cutout.py's region_mask/clamp_rect verbatim) — everywhere outside
// the requested regions keeps the ORIGINAL source pixels, exactly like key_matte's region path.
export async function runCorridorKeyCutoutTool(root, projectId, element, invoke, specRegions) {
  const sourceAbs = resolveProjectFile(root, projectId, element.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-alpha-ck-"));
  try {
    const ckOutPath = specRegions ? join(workDir, "alpha_ck_full.png") : join(workDir, "alpha_out.png");
    const report = await (invoke || defaultCorridorKeyInvoke)(root, { sourceAbs, outAbs: ckOutPath });
    let finalPath = ckOutPath;
    if (specRegions) {
      finalPath = join(workDir, "alpha_out.png");
      const composeSpec = writeCkPixelOpsSpec(workDir, "compose_regions", {
        op: "compose_regions",
        source: sourceAbs,
        keyed: ckOutPath,
        regions: specRegions,
        output: finalPath,
      });
      await runToolPython(root, ["ai_studio/assets/canvas/tools/ck_pixel_ops.py", "--spec", composeSpec]);
    }
    const bytes = readFileSync(finalPath);
    const newSrc = storeAddFile(root, projectId, { bytes, name: `${slug(element.name)}.png` }).src;
    const finalReport = specRegions
      ? {
          ...report,
          region_count: specRegions.length,
          regions: specRegions.map((region) => ({
            id: region.id,
            method: "corridorkey",
            routed: "corridorkey",
            key: report.key_color,
          })),
        }
      : report;
    return { newSrc, report: finalReport };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// ---- vitmatte (neural ViTMatte alpha matting; T0335, alpha-methods-portfolio bench 2026-07-07) ---
//
// ViTMatte is an EXPLICIT alpha method scoped to THIN detail (spider-web / mesh / fur / hair) on a
// flat GREEN/MAGENTA key, and the SECOND-priority glow keyer after corridorkey. Two hard rules from
// the tool contract: it runs in its OWN GPU-torch venv (never the shared repo .venv — cu128 torch
// must not enter it), and a missing/broken venv is a LOUD error naming the exact setup script (no
// fallback). Like corridorkey it has a KEY GATE (green/magenta only) and NO auto-router entry.
export const VITMATTE_DIR = "ai_studio/assets/tools/image/vitmatte_matte";
export const VITMATTE_SCRIPT = `${VITMATTE_DIR}/vitmatte_matte.py`;
export const VITMATTE_SETUP = `node ${VITMATTE_DIR}/setup_python.mjs`;

// Resolve ViTMatte's OWN venv interpreter; a missing venv is a LOUD refusal naming the setup script
// — surface the tool's own no-fallback message, never key with a different method silently.
export function resolveVitmattePython(root) {
  const abs = join(root, VITMATTE_DIR, ".venv", "Scripts", "python.exe");
  if (!existsSync(abs)) {
    throw new Error(
      `vitmatte requires its OWN GPU-torch venv (not the shared repo .venv) and it is not installed ` +
        `at ${abs}. Create it: run \`${VITMATTE_SETUP}\` from the repo root, then retry. No fallback ` +
        `— vitmatte is a deliberate, GPU-backed choice.`,
    );
  }
  return abs;
}

// LOUD refusal (before the GPU model load) for a key ViTMatte cannot auto-trimap: names the detected
// key and the methods to use instead. Reuses corridorkey's green/magenta classifier — the same two
// keys the canvas conveyor keys against. NOTE: ViTMatte's auto-trimap is chroma-DISTANCE-to-key math
// (matte_math.build_auto_trimap), which is itself key-AGNOSTIC (works for any flat key), but its
// T1=70 / T2=150 trimap thresholds were tuned once on a MAGENTA fixture (opaque_hard_scavenger) and
// frozen — so green + magenta are the two keys we gate to here.
export function assertVitmatteSupportedKey(key) {
  if (classifyCorridorKeyBorder(key)) return;
  const [r, g, b] = Array.isArray(key) ? key : [];
  throw new Error(
    `vitmatte auto-trimaps against a flat GREEN or MAGENTA key — the border key of this element is ` +
      `rgb(${r}, ${g}, ${b}), which is neither. Use method "matte" (key_matte) for other flat keys, ` +
      `"birefnet" for an arbitrary/unknown background, or the alphaDualPlate op for a neutral ` +
      `(non-chroma) background.`,
  );
}

// The DEFAULT (real, GPU-backed) ViTMatte invocation. Injectable via the op's `vitmatte` option so
// tests fake the GPU run with no GPU/venv in the suite — the same seam shape as `corridorKey`.
// Steps: (1) KEY GATE — estimate the border key (route_cutout, warm repo venv, sub-second) and
// LOUD-refuse a non-green/non-magenta key BEFORE the model load; (2) spawn the TOOL-VENV python on
// the staged element pixels with `--key R,G,B` (the detected key) + `--report`; despill is on by
// default (the tool's default). Writes a straight RGBA PNG to `outAbs` and returns a canvas alpha
// report mirroring corridorkey's shape.
export async function defaultVitmatteInvoke(root, { sourceAbs, outAbs }) {
  const key = await estimateBorderKeyRgb(root, sourceAbs, "vitmatte key gate");
  assertVitmatteSupportedKey(key);
  const python = resolveVitmattePython(root); // LOUD if the tool venv is missing
  const workDir = mkdtempSync(join(tmpdir(), "canvas-vitmatte-"));
  try {
    const reportPath = join(workDir, "vitmatte_report.json");
    const argv = [
      join(root, VITMATTE_SCRIPT),
      "--in", sourceAbs,
      "--key", key.join(","),
      "--out", outAbs,
      "--report", reportPath,
    ];
    // quiet:true — the tool's stdout must never reach the CLI's JSON channel; we read the report file.
    const { code, stdout, stderr } = await runProcess(python, argv, { cwd: root, quiet: true });
    if (code !== 0) {
      // Surface the tool's OWN message (setup-script pointer on a broken venv, license refusal,
      // argparse error) as ONE clean line — never a swallowed or partial failure (no-fallbacks law).
      throw new Error((stderr || stdout || `vitmatte exited with code ${code}`).trim());
    }
    if (!existsSync(outAbs)) throw new Error(`vitmatte produced no RGBA output at ${outAbs}`);
    const tool = JSON.parse(readFileSync(reportPath, "utf8"));
    return {
      schema: "ai_studio.canvas.alpha_cutout_report.v1",
      method: "vitmatte",
      tool: "vitmatte",
      model: tool.model,
      device: tool.device,
      despill: tool.despill !== false,
      license: "code MIT; weights local-only (Adobe-DIM caveat, T0335 gate)",
      seconds: tool.seconds,
      key_color: key,
      region_count: 0,
      regions: [{ id: "*element*", method: "vitmatte", routed: "vitmatte", key }],
    };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Run ONE element's CURRENT pixels through ViTMatte (own temp dir) — the vitmatte sibling of
// runCorridorKeyCutoutTool, so the single AND batch paths swap src / record provenance / undo
// BYTE-IDENTICALLY to key_matte. `invoke` is the injectable seam (default defaultVitmatteInvoke).
// v1 is WHOLE-ELEMENT ONLY: a region-scoped request is a loud refusal (ViTMatte has no per-region
// trimap pass here — corridorkey is the region-scoped neural path, or matte for a region key cut).
export async function runVitmatteCutoutTool(root, projectId, element, invoke, specRegions) {
  if (specRegions) {
    throw new Error(
      'vitmatte is whole-element only (v1) — use method "corridorkey" for region-scoped neural ' +
        'keying, or "matte" for a region-scoped key_matte cut.',
    );
  }
  const sourceAbs = resolveProjectFile(root, projectId, element.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-alpha-vm-"));
  try {
    const outPath = join(workDir, "alpha_out.png");
    const report = await (invoke || defaultVitmatteInvoke)(root, { sourceAbs, outAbs: outPath });
    const bytes = readFileSync(outPath);
    const newSrc = storeAddFile(root, projectId, { bytes, name: `${slug(element.name)}.png` }).src;
    return { newSrc, report };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// ---- birefnet (BiRefNet-general SOD cutout; T0335, alpha-methods-portfolio bench 2026-07-07) -----
//
// BiRefNet is the EXPLICIT alpha method for an ARBITRARY/unknown background with NO chroma key at
// all — its niche is exactly where route_cutout finds no flat key, so it has NO key gate (calling
// the key detector would be wrong). It runs in the SHARED repo venv (studio.config pythonPath) but
// as a DIRECT spawn, NOT through the serialized warm worker: its ~10-30s CPU inference (plus a
// first-run ~930MB checkpoint download) would hold every other queued canvas python op (slice,
// export, quantize, key_matte) hostage behind one keying — the warm-worker path is for sub-second
// tools only (review T0335 finding 2). MIT; weak on flat line-art (a documented routing nuance,
// not a bug). No auto-router entry.

// The DEFAULT BiRefNet invocation. Injectable via the op's `birefnet` option so tests fake the
// ~10-30s CPU run — the same seam shape as `corridorKey`. NO key gate. Writes a straight RGBA PNG to
// `outAbs` and returns a canvas alpha report mirroring corridorkey's shape (no key_color — no key).
export async function defaultBirefnetInvoke(root, { sourceAbs, outAbs }) {
  const python = resolveRepoPython(root); // LOUD if the shared venv is missing (names the setup cmd)
  const workDir = mkdtempSync(join(tmpdir(), "canvas-birefnet-"));
  try {
    const reportPath = join(workDir, "birefnet_report.json");
    const argv = [
      join(root, "ai_studio/assets/tools/image/birefnet_cutout/birefnet_cutout.py"),
      "--in", sourceAbs,
      "--out", outAbs,
      "--report", reportPath,
    ];
    // quiet:true — the tool's stdout must never reach the CLI's JSON channel; we read the report file.
    const { code, stdout, stderr } = await runProcess(python, argv, { cwd: root, quiet: true });
    if (code !== 0) {
      // Surface the tool's OWN message (missing-rembg setup pointer, license refusal, argparse
      // error) as ONE clean line — never a swallowed or partial failure (no-fallbacks law).
      throw new Error((stderr || stdout || `birefnet exited with code ${code}`).trim());
    }
    if (!existsSync(outAbs)) throw new Error(`birefnet produced no RGBA output at ${outAbs}`);
    const tool = JSON.parse(readFileSync(reportPath, "utf8"));
    return {
      schema: "ai_studio.canvas.alpha_cutout_report.v1",
      method: "birefnet",
      tool: "birefnet",
      model: tool.model,
      device: tool.device,
      license: "MIT (rembg + BiRefNet-general)",
      seconds: tool.seconds,
      region_count: 0,
      regions: [{ id: "*element*", method: "birefnet", routed: "birefnet" }],
    };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Run ONE element's CURRENT pixels through BiRefNet (own temp dir) — the birefnet sibling of
// runCorridorKeyCutoutTool. NO key gate (arbitrary-background niche). v1 is WHOLE-ELEMENT ONLY: a
// region-scoped request is a loud refusal.
export async function runBirefnetCutoutTool(root, projectId, element, invoke, specRegions) {
  if (specRegions) {
    throw new Error(
      'birefnet is whole-element only (v1) — it cuts the whole image against its own background ' +
        'model; use method "matte" for a region-scoped key_matte cut.',
    );
  }
  const sourceAbs = resolveProjectFile(root, projectId, element.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-alpha-bn-"));
  try {
    const outPath = join(workDir, "alpha_out.png");
    const report = await (invoke || defaultBirefnetInvoke)(root, { sourceAbs, outAbs: outPath });
    const bytes = readFileSync(outPath);
    const newSrc = storeAddFile(root, projectId, { bytes, name: `${slug(element.name)}.png` }).src;
    return { newSrc, report };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Route ONE element to the right keyer: key_matte via alpha_cutout.py (auto/matte, regions-aware),
// the neural CorridorKey tool (explicit, green native / magenta shim, region composite when
// requested), ViTMatte (explicit, green/magenta thin detail, own GPU venv), or BiRefNet (explicit,
// arbitrary background, no key). All return the identical { newSrc, report } contract so the single
// + batch commit paths stay method-agnostic. Each neural method has its own injectable seam.
export async function runOneAlphaKeyer(root, projectId, element, chosen, specRegions, corridorKey, vitmatte, birefnet) {
  if (chosen === "corridorkey") {
    return runCorridorKeyCutoutTool(root, projectId, element, corridorKey, specRegions);
  }
  if (chosen === "vitmatte") {
    return runVitmatteCutoutTool(root, projectId, element, vitmatte, specRegions);
  }
  if (chosen === "birefnet") {
    return runBirefnetCutoutTool(root, projectId, element, birefnet, specRegions);
  }
  return runAlphaCutoutTool(root, projectId, element, chosen, specRegions);
}

// Build the tool_runs row + element.meta.alpha provenance for one keyed element (shared by
// the single and batch paths, so the recorded shape never drifts between them). T0336: the
// provenance now lives on the NEW copy element, so it records BOTH the parent's `src`
// (parentSrc — the exact pixels that were keyed) and the parent's element id
// (parentElementId — so a side-by-side A/B copy links back to the art it came from). The
// tool_runs `elementId` stays the SOURCE (its pixels are what ran through the keyer).
export function buildAlphaProvenance(elementId, chosen, specRegions, report, parentSrc, parentElementId) {
  const at = new Date().toISOString();
  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "alpha_cutout",
    elementId,
    at,
    params: { method: chosen, regions: specRegions ? specRegions.map((region) => region.id) : [] },
    result_summary: {
      method: (report && report.method) || chosen,
      key_color: report && report.key_color,
      region_count: (report && report.region_count) || 0,
    },
  };
  const alphaMeta = {
    method: chosen,
    tool: "alpha_cutout.py",
    parentSrc,
    parentElementId,
    at,
    key_color: report && report.key_color,
    regions: run.params.regions,
    routing: (report && report.regions) || [],
  };
  // corridorkey provenance (T0261): record the neural tool + its commit/licence/timings so the
  // ~15s GPU run is auditable, mirroring how the video matte report carries them.
  if (report && report.tool === "corridorkey") {
    alphaMeta.tool = "corridorkey";
    alphaMeta.commit = report.commit;
    alphaMeta.license = report.license;
    alphaMeta.screen_color = report.screen_color;
    // T0262: the hue180 magenta shim ran — recorded so the toast/history can tell a native-green
    // CK run apart from a shimmed-magenta one.
    if (report.shim) alphaMeta.shim = report.shim;
    alphaMeta.timings = {
      wall_seconds: report.wall_seconds ?? null,
      per_frame_seconds: report.per_frame_seconds ?? null,
    };
  }
  // vitmatte provenance (T0335): the neural thin-detail / second-priority-glow keyer, its own GPU
  // venv. Record the model + license + despill flag + DEVICE + seconds so the run is auditable,
  // mirroring the CK block above. `device` matters: the tool's one sanctioned fallback is
  // CUDA-OOM -> CPU ("cpu (cuda OOM)", 10-30x slower) and the tool's own report JSON is deleted
  // with its temp dir — this committed copy is the only after-the-fact answer to "did the GPU
  // path actually run?". key_color is the detected border key (set generically above from report).
  if (report && report.tool === "vitmatte") {
    alphaMeta.tool = "vitmatte";
    alphaMeta.model = report.model;
    alphaMeta.license = report.license;
    alphaMeta.despill = report.despill;
    alphaMeta.device = report.device ?? null;
    alphaMeta.timings = { seconds: report.seconds ?? null };
  }
  // birefnet provenance (T0335): the arbitrary-background (no-key) SOD cutout, shared repo venv (CPU
  // onnxruntime). No key_color (there is no key). Record model + license + device + seconds.
  if (report && report.tool === "birefnet") {
    alphaMeta.tool = "birefnet";
    alphaMeta.model = report.model;
    alphaMeta.license = report.license;
    alphaMeta.device = report.device ?? null;
    alphaMeta.timings = { seconds: report.seconds ?? null };
  }
  return { run, alphaMeta };
}

// Mint the alpha cutout as a NEW element beside the source (T0336 — "не ломать арт + легко
// сравнивать разные методы бок о бок": the lead A/Bs key_matte vs corridorkey vs vitmatte vs
// birefnet on the SAME art, so the cutout must never modify the original's pixels). The copy
// is placed to the RIGHT of the source (16px gap, mirroring alphaDualPlate/alphaDualPlateGenerate's
// own placement), named "<source> · <method>" so a stacked A/B reads at a glance, and carries
// element.meta.alpha provenance (incl. parentElementId). storeAddImage mints it the SAME way every
// other add does (id/type/source_w/h/name/meta) — no hand-rolled element shape. The copy's DISPLAY
// box (w/h) is set to the source's exact box (the keyer output is always the source's pixel size,
// so this is a pixel-perfect side-by-side twin even when the source was scaled on the canvas);
// source_w/source_h stay the true output pixels. `before` drives the front-order hook (the copy
// lands at the FRONT of the root scope when that scope is already explicitly ordered). Returns the
// storeAddImage result + the front-order value so the caller folds the twin/order override into ONE
// updateProject. The source element is never read for a write and never appears in `elements` map
// changes — it is byte-identical before and after.
export function mintAlphaCopy(root, projectId, source, newSrc, chosen, alphaMeta) {
  const gap = 16;
  const bytes = readFileSync(resolveProjectFile(root, projectId, newSrc));
  return storeAddImage(root, projectId, {
    name: `${source.name} · ${chosen}`,
    bytes,
    x: source.x + source.w + gap,
    y: source.y,
    meta: { alpha: alphaMeta },
  });
}

// Single-element path. T0336: the cutout is minted as a NEW element beside the source (see
// mintAlphaCopy); the source element and its pixels are NEVER touched — undo removes the copy and
// leaves the original byte-identical, because it was never written.
export async function alphaCutoutSingle(root, projectId, before, elementId, chosen, regions, startedAt, corridorKey, vitmatte, birefnet) {
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} is not an image`);
  refuseIfTransformed(element, elementId);
  refuseIfFlipbook(element, elementId);
  // corridorkey regions (T0262): CorridorKey itself is always whole-frame — a region-scoped
  // request runs it once on the whole element and composites the result into the requested
  // regions (runCorridorKeyCutoutTool); no early refusal here anymore.
  const specRegions = resolveAlphaRegions(element, regions);

  const { newSrc, report } = await runOneAlphaKeyer(root, projectId, element, chosen, specRegions, corridorKey, vitmatte, birefnet);

  // Re-read to avoid clobbering concurrent edits, read the SOURCE (untouched) for its
  // placement/name, then mint the cutout as a NEW element beside it.
  const current = getProject(root, projectId);
  const source = (current.elements || []).find((item) => item.id === elementId);
  if (!source) throw new Error(`element not found: ${elementId}`);
  const { run, alphaMeta } = buildAlphaProvenance(elementId, chosen, specRegions, report, source.src, elementId);
  const added = mintAlphaCopy(root, projectId, source, newSrc, chosen, alphaMeta);

  // Fold the display-box twin (w/h) + front-order into the SAME updateProject the tool_runs
  // append rides (mirrors alphaDualPlateGenerate's post-mint map). The source element is left
  // exactly as storeAddImage saw it — no map entry touches it.
  const fo = frontOrder(before, null);
  const nextElements = (added.project.elements || []).map((item) => {
    if (item.id !== added.element.id) return item;
    const twin = { ...item, w: source.w, h: source.h };
    if (fo !== null) twin.order = fo;
    return twin;
  });
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
  });
  const project = commitMutation(root, projectId, {
    op: "alphaCutout",
    args_summary: { elementId, method: chosen, regions: run.params.regions, region_count: run.params.regions.length, newElementId: added.element.id },
    before,
    after,
    startedAt,
  });
  return { project, element: (project.elements || []).find((item) => item.id === added.element.id) || added.element, run, method: chosen };
}

// Batch path — the multi-selection "Apply to N images" gesture. Every element is
// validated up front (exists, is an image; regions are NOT allowed here — regions stay
// single-element). Each element is then keyed SEQUENTIALLY through its own worker spawn;
// if ANY element fails (dual-plate refusal, not an image mid-run, tool error), the error
// propagates immediately and NOTHING is written to project.json — no new element, no
// journal entry (any files/ bytes already written for earlier-succeeding elements are
// inert content-addressed data, never referenced by any element, so nothing is
// "mutated"). Only once EVERY element has keyed successfully does this commit ONE journal
// entry that MINTS N new copy elements beside their sources (T0336 — the sources are
// never touched); one undo removes ALL the copies and leaves every original byte-exact.
export async function alphaCutoutBatch(root, projectId, before, elementIds, chosen, startedAt, corridorKey, vitmatte, birefnet) {
  const ids = elementIds.map((value) => String(value));
  const unique = [...new Set(ids)];
  const elements = unique.map((elementId) => {
    const element = (before.elements || []).find((item) => item.id === elementId);
    if (!element) throw new Error(`element not found: ${elementId}`);
    if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} is not an image`);
    refuseIfTransformed(element, elementId);
    refuseIfFlipbook(element, elementId);
    return element;
  });

  const processed = [];
  for (const element of elements) {
    const { newSrc, report } = await runOneAlphaKeyer(root, projectId, element, chosen, null, corridorKey, vitmatte, birefnet);
    processed.push({ elementId: element.id, newSrc, report });
  }

  // Re-read once (defensive against a concurrent edit across the whole sequential run),
  // then MINT one copy per source off that SAME snapshot (each storeAddImage appends to disk,
  // exactly like addImages' sequential mint loop). twinById remembers each copy's source so the
  // display-box (w/h) twin + front-order can be folded in AFTER all mints, in ONE updateProject.
  const current = getProject(root, projectId);
  const runs = [];
  const twinById = new Map(); // newElementId -> source element (for the w/h twin override)
  const mintedIds = [];
  for (const item of processed) {
    const source = (current.elements || []).find((el) => el.id === item.elementId);
    if (!source) throw new Error(`element not found: ${item.elementId}`);
    const { run, alphaMeta } = buildAlphaProvenance(item.elementId, chosen, null, item.report, source.src, item.elementId);
    runs.push(run);
    const added = mintAlphaCopy(root, projectId, source, item.newSrc, chosen, alphaMeta);
    twinById.set(added.element.id, source);
    mintedIds.push(added.element.id);
  }

  // Re-read after all mints (they each wrote project.json), fold the w/h twin + front-order
  // for the minted copies, and append every tool_runs row — ONE updateProject, then ONE commit.
  const minted = getProject(root, projectId);
  let fo = frontOrder(before, null);
  const orderById = fo !== null ? new Map(mintedIds.map((id) => [id, fo++])) : null;
  const nextElements = (minted.elements || []).map((item) => {
    const source = twinById.get(item.id);
    if (!source) return item;
    const twin = { ...item, w: source.w, h: source.h };
    if (orderById) twin.order = orderById.get(item.id);
    return twin;
  });
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), ...runs]),
  });
  const project = commitMutation(root, projectId, {
    op: "alphaCutout",
    args_summary: { elementIds: unique, count: unique.length, method: chosen, newElementIds: mintedIds },
    before,
    after,
    startedAt,
  });
  const resultIds = new Set(mintedIds);
  return {
    project,
    elements: (project.elements || []).filter((item) => resultIds.has(item.id)),
    runs,
    method: chosen,
    count: unique.length,
  };
}

// Run the element's CURRENT pixels through the image-tools alpha pipeline and mint the cutout
// as a NEW element beside the source in ONE journaled entry (T0336 — the original element and
// its pixels are NEVER touched: "не ломать арт + легко сравнивать разные методы бок о бок", so
// the lead can A/B key_matte vs corridorkey vs vitmatte vs birefnet on the same art side by
// side). The missing bridge that puts the matte pipeline on the canvas ("готовый арт сразу в
// альфу"). Cutting is done
// by our OWN Python tool (tools/alpha_cutout.py) which REUSES the image-tools route +
// key_matte modules unmodified (no matte logic duplicated in node or a second python impl);
// ops writes an alpha spec (absolute source path + method + the element's selected regions
// with their exact rects) and spawns it once through the shared warm worker. `method` is
// "auto" (route), "matte" (force key_matte), "corridorkey" (T0261/T0262 — the explicit neural
// green-screen matte for soft glow/translucent art; green native, magenta via a hue180 shim, ~15s
// GPU cold, via runCorridorKeyCutoutTool, NOT the warm worker), "vitmatte" (T0335 — explicit neural
// thin-detail / second-priority-glow matte on a green/magenta key, its OWN GPU-torch venv, ~1-3s,
// whole-element only), or "birefnet" (T0335 — explicit SOD cutout for an arbitrary/unknown
// background with no key at all, shared repo venv CPU ~10-30s, whole-element only). vitmatte/birefnet
// are EXPLICIT-ONLY like corridorkey (the auto router never yields them); a wide soft zone under
// "auto" is a loud error (dual-plate pair needed — no silent single-plate fallback), as is a
// non-image element or an unknown method. `regions` is an optional list of the element's stored region ids: given, the
// alpha is applied ONLY inside those region masks and the rest is untouched (region-mask
// composition happens IN python, one worker call — for corridorkey this composites the whole-frame
// CK result into the requested regions, since CK itself has no per-region pass); omitted, the whole
// element is keyed. The NEW element is placed to the RIGHT of the source (16px gap, mirroring
// alphaDualPlate/alphaDualPlateGenerate), sized to the source's exact display box (a pixel-perfect
// side-by-side twin — the keyer output always equals the source's pixel dims), named
// "<source> · <method>", and carries element.meta.alpha (method, params, parentSrc,
// parentElementId, routing metrics) like slice provenance, plus a tool_runs row. The source is
// byte-identical before and after; undo removes ONLY the copy. `elementIds` (2+ images), given
// INSTEAD of `elementId`, batches a multi-selection into ONE journal entry (T0230) that mints N
// copies — see alphaCutoutBatch; `regions` is not accepted with a batch. Return: `result.element`
// (single) / `result.elements` (batch) are the NEW copy element(s).
export async function alphaCutout(root, { projectId, elementId, elementIds, method, regions, corridorKey, vitmatte, birefnet } = {}) {
  if (!projectId) throw new Error("alphaCutout requires projectId");
  const batch = elementIds !== undefined && elementIds !== null;
  if (batch && elementId != null) {
    throw new Error("alphaCutout accepts either elementId or elementIds, not both");
  }
  if (!batch && !elementId) throw new Error("alphaCutout requires elementId");
  if (batch) {
    // Structural batch checks are cheap and belong before any disk read, mirroring the
    // requires-elementId guard above (fail fast on bad shape, not on a missing project).
    if (!Array.isArray(elementIds) || !elementIds.length) {
      throw new Error("alphaCutout requires a non-empty elementIds array");
    }
    if (regions != null) {
      throw new Error(
        "alphaCutout batch (elementIds) does not support regions — regions stay single-element (pass a single elementId)",
      );
    }
  }
  const chosen = method == null || method === "" ? "auto" : String(method).trim().toLowerCase();
  if (!ALPHA_METHODS.has(chosen)) {
    if (chosen === "dualplate" || chosen === "dual_plate" || chosen === "generation") {
      throw new Error(
        `alpha method ${JSON.stringify(method)} needs a white+black plate PAIR (dual-plate) — a single ` +
          `elementId call can't provide one. Select BOTH plate elements and use the alphaDualPlate op ` +
          `(API POST /alpha-dual, CLI alpha-dual) instead, or use method "auto"/"matte" on a single image.`,
      );
    }
    throw new Error(
      `unknown alpha method ${JSON.stringify(method)} ` +
        `(expected "auto", "matte", "corridorkey", "vitmatte", or "birefnet")`,
    );
  }
  const startedAt = performance.now();
  const before = getProject(root, projectId);

  if (batch) {
    return alphaCutoutBatch(root, projectId, before, elementIds, chosen, startedAt, corridorKey, vitmatte, birefnet);
  }
  return alphaCutoutSingle(root, projectId, before, elementId, chosen, regions, startedAt, corridorKey, vitmatte, birefnet);
}

// ---- cleanup: Quantize + Denoise (own Python tools, src-swap) ----------------
//
// T0207 (lead-settled 2026-07-02/03): the Cleanup section is TWO separate interactive
// tools — Quantize (color-count + optional dither) and Denoise (strength) — never one
// monolithic "Clean up" (bg-solidify was CUT as a standalone tool; it stays an internal
// pre-pass of the alpha keyer only — no op, no button, no CLI here). Both follow the
// exact alphaCutout non-destructive shape: Preview computes the result WITHOUT touching
// the store at all (no files/ write, no journal entry, no tool_runs row) so the
// inspector's slider can recompute on every debounced drag and Cancel is free; Apply
// commits a FRESH deterministic run of the SAME tool+params (quantize/denoise carry no
// randomness, so this reproduces byte-identical bytes to what the last preview showed)
// as ONE journal entry — a new content-addressed file + the element's src swap +
// additive element.meta.cleanup — exactly like alphaCutout's src-swap. Same R7
// rotated/flipped refusal as alphaCutout/detectRegions/sliceRegions (source-space op).

export const CLEANUP_TOOLS = {
  quantize: {
    label: "Quantize",
    script: "ai_studio/assets/tools/image/quantize/quantize.py",
    // colors: integer 2..256 (Pillow's own quantize() range); dither: optional boolean
    // (Floyd-Steinberg when true, else the default exact NONE mapping).
    validate(params = {}) {
      const colors = Math.round(Number(params.colors));
      if (!Number.isFinite(colors) || colors < 2 || colors > 256) {
        throw new Error(`quantize requires colors between 2 and 256, got ${JSON.stringify(params.colors)}`);
      }
      return { colors, dither: params.dither === true };
    },
    args(clean, sourceAbs, outPath, reportPath) {
      const argv = ["--source", sourceAbs, "--out", outPath, "--colors", String(clean.colors), "--report", reportPath];
      if (clean.dither) argv.push("--dither");
      return argv;
    },
  },
  denoise: {
    label: "Denoise",
    script: "ai_studio/assets/tools/image/denoise/denoise.py",
    // strength: 1|2|3 — maps to a median-filter pass ladder in denoise.py (3px, 3px x2,
    // 5px); the alpha channel is NEVER filtered there (the halo law).
    validate(params = {}) {
      const strength = Math.round(Number(params.strength));
      if (![1, 2, 3].includes(strength)) {
        throw new Error(`denoise requires strength 1, 2, or 3, got ${JSON.stringify(params.strength)}`);
      }
      return { strength };
    },
    args(clean, sourceAbs, outPath, reportPath) {
      return ["--source", sourceAbs, "--out", outPath, "--strength", String(clean.strength), "--report", reportPath];
    },
  },
};

// Look up + normalize a cleanup tool name. Loud on anything else — validated BEFORE any
// disk/project read (fail fast on bad shape, mirroring alphaCutout's method check).
export function resolveCleanupTool(tool) {
  const name = String(tool || "").trim().toLowerCase();
  const spec = CLEANUP_TOOLS[name];
  if (!spec) throw new Error(`unknown cleanup tool ${JSON.stringify(tool)} (expected "quantize" or "denoise")`);
  return { name, ...spec };
}

// Resolve + validate the element for a cleanup op: must be an image with a src, and
// untransformed (R7 — cleanup reads/writes source-space pixels, same rule alphaCutout/
// detectRegions/sliceRegions enforce, with the identical refusal message).
export function resolveCleanupElement(project, elementId) {
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} is not an image`);
  refuseIfTransformed(element, elementId);
  refuseIfFlipbook(element, elementId);
  return element;
}

// Run ONE cleanup tool over an element's CURRENT pixels (own worker spawn + own temp dir,
// deleted before returning) and return the resulting bytes + the tool's report. WITHOUT
// touching project.json — callers decide whether to mint a file (apply) or hand the bytes
// straight back (preview). Shared so there is exactly one implementation of the actual
// spawn for both paths (mirrors runAlphaCutoutTool).
export async function runCleanupToolOnElement(root, projectId, element, spec, clean) {
  const sourceAbs = resolveProjectFile(root, projectId, element.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-cleanup-"));
  try {
    const outPath = join(workDir, "cleanup_out.png");
    const reportPath = join(workDir, "cleanup_report.json");
    await runToolPython(root, [spec.script, ...spec.args(clean, sourceAbs, outPath, reportPath)]);
    const report = JSON.parse(readFileSync(reportPath, "utf8"));
    const bytes = readFileSync(outPath);
    return { bytes, report };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Preview a cleanup tool's result on an element's CURRENT pixels — the inspector's live
// param-slider path (debouncing is the caller's job). Computes bytes + report and hands
// the PNG straight back as base64; NOTHING is written to the store (no files/ entry, no
// journal line, no tool_runs row), so Cancel is free — there is nothing to undo.
export async function cleanupPreview(root, { projectId, elementId, tool, params } = {}) {
  if (!projectId) throw new Error("cleanupPreview requires projectId");
  if (!elementId) throw new Error("cleanupPreview requires elementId");
  const spec = resolveCleanupTool(tool);
  const clean = spec.validate(params);
  const project = getProject(root, projectId);
  const element = resolveCleanupElement(project, elementId);
  const { bytes, report } = await runCleanupToolOnElement(root, projectId, element, spec, clean);
  return { elementId, tool: spec.name, params: clean, previewBase64: bytes.toString("base64"), report };
}

// Apply a cleanup tool's result as ONE journaled mutation: a new content-addressed file +
// the element's src swap + additive element.meta.cleanup ({tool, params, report,
// prev_src, at}); the previous src file stays in files/ (immutable), so undo restores the
// exact previous bytes byte-for-byte, exactly like alphaCutout. `prev_src` is additive
// provenance on top of the swap itself — the file the swap replaced never moves.
export async function cleanupApply(root, { projectId, elementId, tool, params } = {}) {
  if (!projectId) throw new Error("cleanupApply requires projectId");
  if (!elementId) throw new Error("cleanupApply requires elementId");
  const spec = resolveCleanupTool(tool);
  const clean = spec.validate(params);
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = resolveCleanupElement(before, elementId);
  const { bytes, report } = await runCleanupToolOnElement(root, projectId, element, spec, clean);
  const newSrc = storeAddFile(root, projectId, { bytes, name: `${slug(element.name)}.png` }).src;

  // Re-read to avoid clobbering concurrent edits, then swap src + record provenance
  // ADDITIVELY on meta (alpha's meta, if any, is preserved alongside cleanup's).
  const current = getProject(root, projectId);
  const target = (current.elements || []).find((item) => item.id === elementId);
  if (!target) throw new Error(`element not found: ${elementId}`);
  const at = new Date().toISOString();
  const cleanupMeta = { tool: spec.name, params: clean, report, prev_src: target.src, at };
  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: `cleanup_${spec.name}`,
    elementId,
    at,
    params: clean,
    result_summary: { changed_pixel_pct: report && report.changed_pixel_pct },
  };
  const nextElements = (current.elements || []).map((item) =>
    item.id === elementId ? { ...item, src: newSrc, meta: { ...(item.meta || {}), cleanup: cleanupMeta } } : item,
  );
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
  });
  const project = commitMutation(root, projectId, {
    op: "cleanupApply",
    args_summary: { elementId, tool: spec.name, params: clean },
    before,
    after,
    startedAt,
  });
  return {
    project,
    element: (project.elements || []).find((item) => item.id === elementId),
    run,
    tool: spec.name,
    params: clean,
    report,
  };
}

// ---- bakeFilters ("Apply" — rasterize filters + opacity into pixels) ---------
//
// T0274 (lead-approved, Photoshop-rasterize semantics): "принял -> получил новый арт ->
// ползунки снова в 0". element.filters/element.opacity (T0273/T0260) are non-destructive
// so the lead can iterate, but he also wants to COMMIT a look: burn the CURRENT filters +
// opacity into a NEW content-addressed source file and reset the sliders — Photoshop's
// "Flatten"/rasterize on an adjustment layer. Mirrors alphaCutout/cleanupApply's
// non-destructive src-swap shape verbatim (own Python tool, own temp dir, one
// commitMutation, previous file kept in files/ so undo restores byte-exact).
//
// Math: tools/bake_filters.py imports the SAME apply_filters() render_group.py's paint
// path uses (extracted to tools/filters_math.py, T0274 — one implementation of the
// canonical brightness/saturate/contrast/tint chain, see README "Image filters"), then
// multiplies the alpha channel by opacity with the IDENTICAL formula paint_element uses
// (`round(a * factor)` via PIL's own `.point()`) — so the baked asset's pixels match the
// on-canvas preview exactly. Opacity-to-alpha lives ONLY in bake_filters.py; render_group.py
// still applies opacity at composite time for every OTHER (non-baked) element, so nothing
// here double-applies it.
//
// Image elements only (loud on text/note, mirrors alphaCutout's type guard). A loud error
// when the element has NOTHING to bake (no/default filters AND opacity absent-or-1) — no
// silent no-op file churn.

// True when an element carries a filters object (always non-empty when present — see
// normalizeFilters) OR a stored opacity != 1 (also always the non-default case — see
// normalizeOpacityPatch) — the "there is something to bake" gate shared by the single AND
// batch paths.
export function hasBakeableFilters(element) {
  if (!element) return false;
  if (element.filters && typeof element.filters === "object" && Object.keys(element.filters).length) return true;
  const opacity = element.opacity;
  return opacity !== undefined && opacity !== null && Number(opacity) !== 1;
}

// Resolve + validate the element for a bake: must be an image with a src, and must carry
// something to bake (loud otherwise, BEFORE any python spawn).
export function resolveBakeElement(project, elementId) {
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} is not an image`);
  refuseIfFlipbook(element, elementId);
  if (!hasBakeableFilters(element)) {
    throw new Error(`element ${elementId} has nothing to apply — filters are at defaults`);
  }
  return element;
}

// Run ONE element's CURRENT pixels through bake_filters.py (own worker spawn + own temp
// dir, deleted before returning) and return the new content-addressed src + the tool's
// report, WITHOUT touching project.json — the caller (single or batch) commits. Mirrors
// runAlphaCutoutTool/runCleanupToolOnElement exactly.
export async function runBakeFiltersTool(root, projectId, element) {
  const sourceAbs = resolveProjectFile(root, projectId, element.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-bake-"));
  try {
    const specPath = join(workDir, "bake_spec.json");
    const reportPath = join(workDir, "bake_report.json");
    const outPath = join(workDir, "bake_out.png");
    const spec = {
      schema: "ai_studio.canvas.bake_filters_spec.v1",
      source: sourceAbs,
      output: outPath,
      report: reportPath,
    };
    if (element.filters) spec.filters = element.filters;
    if (element.opacity !== undefined) spec.opacity = element.opacity;
    writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
    await runToolPython(root, ["ai_studio/assets/canvas/tools/bake_filters.py", "--spec", specPath]);
    const report = JSON.parse(readFileSync(reportPath, "utf8"));
    const bytes = readFileSync(outPath);
    const newSrc = storeAddFile(root, projectId, { bytes, name: `${slug(element.name)}.png` }).src;
    return { newSrc, report };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Build the tool_runs row + element.meta.filters_bake provenance for one baked element
// (shared by the single and batch paths). `baked` records exactly what was burned in (the
// filters object and the opacity the element carried going in), so "what did Apply just
// do" is answerable without re-deriving it from the undo state.
export function buildBakeProvenance(elementId, element, report, prevSrc) {
  const at = new Date().toISOString();
  const baked = { filters: element.filters || null, opacity: element.opacity !== undefined ? element.opacity : 1 };
  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "filters_bake",
    elementId,
    at,
    params: baked,
    result_summary: { width: report && report.width, height: report && report.height },
  };
  const bakeMeta = { prev_src: prevSrc, baked, at };
  return { run, bakeMeta };
}

// Single-element path.
export async function bakeFiltersSingle(root, projectId, before, elementId, startedAt) {
  const element = resolveBakeElement(before, elementId);
  const { newSrc, report } = await runBakeFiltersTool(root, projectId, element);

  // Re-read to avoid clobbering concurrent edits, then swap src + clear filters/opacity +
  // record provenance on the SAME element (previous src file stays in files/, so undo
  // restores the exact previous bytes AND the exact previous filters/opacity).
  const current = getProject(root, projectId);
  const target = (current.elements || []).find((item) => item.id === elementId);
  if (!target) throw new Error(`element not found: ${elementId}`);
  const { run, bakeMeta } = buildBakeProvenance(elementId, element, report, target.src);
  const nextElements = (current.elements || []).map((item) => {
    if (item.id !== elementId) return item;
    const next = { ...item, src: newSrc, meta: { ...(item.meta || {}), filters_bake: bakeMeta } };
    delete next.filters;
    delete next.opacity;
    return next;
  });
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
  });
  const project = commitMutation(root, projectId, {
    op: "bakeFilters",
    args_summary: { elementId },
    before,
    after,
    startedAt,
  });
  return { project, element: (project.elements || []).find((item) => item.id === elementId), run };
}

// Batch path — the multi-selection "Apply filters on N images" gesture. Atomic
// all-or-nothing, mirrors alphaCutoutBatch exactly: every element is validated up front
// (exists, is an image, HAS something to bake) BEFORE any python spawn; each is then baked
// SEQUENTIALLY through its own worker spawn; if ANY element fails, the error propagates
// immediately and NOTHING is written to project.json. Only once EVERY element has baked
// successfully does this commit ONE journal entry swapping every src + clearing every
// element's filters/opacity — one undo restores every element byte-exact.
export async function bakeFiltersBatch(root, projectId, before, elementIds, startedAt) {
  const ids = elementIds.map((value) => String(value));
  const unique = [...new Set(ids)];
  const elements = unique.map((elementId) => resolveBakeElement(before, elementId));

  const processed = [];
  for (const element of elements) {
    const { newSrc, report } = await runBakeFiltersTool(root, projectId, element);
    processed.push({ element, newSrc, report });
  }

  // Re-read once (defensive against a concurrent edit across the whole sequential run),
  // then swap every src + clear every element's filters/opacity off the SAME snapshot.
  const current = getProject(root, projectId);
  const runs = [];
  const swapById = new Map();
  for (const item of processed) {
    const target = (current.elements || []).find((el) => el.id === item.element.id);
    if (!target) throw new Error(`element not found: ${item.element.id}`);
    const { run, bakeMeta } = buildBakeProvenance(item.element.id, item.element, item.report, target.src);
    runs.push(run);
    swapById.set(item.element.id, { newSrc: item.newSrc, bakeMeta });
  }
  const nextElements = (current.elements || []).map((item) => {
    const swap = swapById.get(item.id);
    if (!swap) return item;
    const next = { ...item, src: swap.newSrc, meta: { ...(item.meta || {}), filters_bake: swap.bakeMeta } };
    delete next.filters;
    delete next.opacity;
    return next;
  });
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), ...runs]),
  });
  const project = commitMutation(root, projectId, {
    op: "bakeFilters",
    args_summary: { elementIds: unique, count: unique.length },
    before,
    after,
    startedAt,
  });
  const resultIds = new Set(unique);
  return {
    project,
    elements: (project.elements || []).filter((item) => resultIds.has(item.id)),
    runs,
    count: unique.length,
  };
}

// Bake the element's CURRENT filters+opacity into a NEW content-addressed source file in
// ONE journaled entry, then reset the sliders (filters/opacity cleared) — "принял -> получил
// новый арт -> ползунки снова в 0". `elementIds` (2+ images), given INSTEAD of `elementId`,
// batches a multi-selection into ONE journal entry/undo (mirrors alphaCutout).
export async function bakeFilters(root, { projectId, elementId, elementIds } = {}) {
  if (!projectId) throw new Error("bakeFilters requires projectId");
  const batch = elementIds !== undefined && elementIds !== null;
  if (batch && elementId != null) {
    throw new Error("bakeFilters accepts either elementId or elementIds, not both");
  }
  if (!batch && !elementId) throw new Error("bakeFilters requires elementId");
  if (batch && (!Array.isArray(elementIds) || !elementIds.length)) {
    throw new Error("bakeFilters requires a non-empty elementIds array");
  }
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  if (batch) return bakeFiltersBatch(root, projectId, before, elementIds, startedAt);
  return bakeFiltersSingle(root, projectId, before, elementId, startedAt);
}

// ---- alphaDualPlate (white+black plate pair -> one new cut element) ----------
//
// T0237 (lead, 2026-07-03): "до сих пор нет дуал пути для альфы?" — closes the loop the
// alphaCutout doc points at ("a pair source could come from later"). Runs the canvas's
// own tools/alpha_dualplate.py through the shared warm worker, which REUSES the
// image-tools dual_plate_alpha + dual_plate_pair_gate modules unmodified (no matte logic
// duplicated in node or a second python impl).

// Run the two plate elements' CURRENT pixels through alpha_dualplate.py (own worker spawn
// + own temp dir) and return the extracted RGBA bytes + the tool's report, WITHOUT
// touching project.json — the caller mints the new element and commits.
export async function runAlphaDualPlateTool(root, projectId, elementA, elementB) {
  const plateAAbs = resolveProjectFile(root, projectId, elementA.src);
  const plateBAbs = resolveProjectFile(root, projectId, elementB.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-dualplate-"));
  try {
    const specPath = join(workDir, "dualplate_spec.json");
    const reportPath = join(workDir, "dualplate_report.json");
    const outPath = join(workDir, "dualplate_out.png");
    const spec = {
      schema: "ai_studio.canvas.alpha_dualplate_spec.v1",
      plateA: plateAAbs,
      plateB: plateBAbs,
      output: outPath,
      report: reportPath,
    };
    writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
    await runToolPython(root, ["ai_studio/assets/canvas/tools/alpha_dualplate.py", "--spec", specPath]);
    const report = JSON.parse(readFileSync(reportPath, "utf8"));
    const bytes = readFileSync(outPath);
    return { bytes, report };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Build the tool_runs row + the new element's meta.alpha provenance from the tool's
// report: method "dual_plate", both parent srcs, and the pair gate's own metrics (so the
// lead can see exactly how clean the pair was) — mirrors buildAlphaProvenance's shape.
export function buildDualPlateProvenance(elementIdA, elementIdB, report, srcA, srcB) {
  const at = new Date().toISOString();
  const gate = (report && report.pair_gate) || {};
  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "alpha_dualplate",
    elementIds: [elementIdA, elementIdB],
    at,
    params: {},
    result_summary: {
      method: "dual_plate",
      light_plate: report && report.light_plate,
      dark_plate: report && report.dark_plate,
      pair_gate: gate,
      visible_pixels: report && report.visible_pixels,
    },
  };
  const alphaMeta = {
    method: "dual_plate",
    tool: "alpha_dualplate.py",
    parents: [srcA, srcB],
    at,
    light_plate: report && report.light_plate,
    dark_plate: report && report.dark_plate,
    pair_gate: gate,
  };
  return { run, alphaMeta };
}

// TWO selected image elements (the SAME art rendered on a white plate and a black plate,
// in either order — the tool auto-detects which is which by overall brightness) -> ONE NEW
// content-addressed cut element in ONE journaled entry. Both plate elements stay UNTOUCHED
// (non-destructive; the lead deletes them himself once happy with the result) — unlike
// alphaCutout, this never swaps an existing element's src. The new element is named
// "<first plate's name> alpha", placed at the first plate's x/y, sized to the extracted
// output (equals the plate size), and carries element.meta.alpha provenance (method,
// both parent srcs, the pair gate's verdict/metrics) plus an alpha_dualplate tool_runs
// row. Refusals are loud and specific: not exactly 2 ids, a non-image element, or the
// pair gate's own "regenerate" message (misaligned/redrawn plates, ambiguous roles) —
// travels the python worker's SystemExit path as a clean message, no traceback.
export async function alphaDualPlate(root, { projectId, elementIds } = {}) {
  if (!projectId) throw new Error("alphaDualPlate requires projectId");
  if (!Array.isArray(elementIds)) throw new Error("alphaDualPlate requires an elementIds array");
  const ids = elementIds.map((value) => String(value));
  if (ids.length !== 2) {
    throw new Error(`alphaDualPlate requires exactly 2 elementIds (a white-plate + black-plate pair), got ${ids.length}`);
  }
  const [idA, idB] = ids;
  if (idA === idB) throw new Error("alphaDualPlate requires two DIFFERENT elementIds (a white-plate + black-plate pair)");

  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const elementA = (before.elements || []).find((item) => item.id === idA);
  if (!elementA) throw new Error(`element not found: ${idA}`);
  if (elementA.type !== "image" || !elementA.src) throw new Error(`element ${idA} is not an image`);
  const elementB = (before.elements || []).find((item) => item.id === idB);
  if (!elementB) throw new Error(`element not found: ${idB}`);
  if (elementB.type !== "image" || !elementB.src) throw new Error(`element ${idB} is not an image`);

  const { bytes, report } = await runAlphaDualPlateTool(root, projectId, elementA, elementB);

  // Re-read to avoid clobbering a concurrent edit (mirrors alphaCutout's re-read-before-write).
  const current = getProject(root, projectId);
  const plateA = (current.elements || []).find((item) => item.id === idA);
  if (!plateA) throw new Error(`element not found: ${idA}`);
  const plateB = (current.elements || []).find((item) => item.id === idB);
  if (!plateB) throw new Error(`element not found: ${idB}`);

  const { run, alphaMeta } = buildDualPlateProvenance(idA, idB, report, plateA.src, plateB.src);
  // Placement: to the RIGHT of BOTH plates' union bbox (gap in canvas px, mirrors slice) —
  // never on top of a plate, so the result is immediately visible (lead complaint T0237).
  const gap = 16;
  const pairRight = Math.max(plateA.x + plateA.w, plateB.x + plateB.w);
  const pairTop = Math.min(plateA.y, plateB.y);
  // storeAddImage mints the new element (id/type/src/x/y/w/h/source_w/h/name/meta) the
  // SAME way every other add does — no hand-rolled element shape here. Like addImage, a
  // freshly minted image never carries a groupId, so the new element lands in the root
  // scope regardless of the plates' own group membership.
  const added = storeAddImage(root, projectId, {
    name: `${plateA.name} alpha`,
    bytes,
    x: pairRight + gap,
    y: pairTop,
    meta: { alpha: alphaMeta },
  });

  // Front-order hook (identical to addImage/addImages): the new element lands at the
  // FRONT of the root scope when it is already explicitly ordered; a no-op otherwise.
  const fo = frontOrder(before, null);
  const nextElements = (added.project.elements || []).map((element) =>
    fo !== null && element.id === added.element.id ? { ...element, order: fo } : element,
  );
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
  });

  const project = commitMutation(root, projectId, {
    op: "alphaDualPlate",
    args_summary: { elementIds: [idA, idB], newElementId: added.element.id },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === added.element.id) || added.element;
  return { project, element, run };
}

// ---- alphaDualPlateGenerate (AUTOMATIC: one element -> generated plate(s) -> cut) --------
//
// T0238 (lead, 2026-07-03): "Генерировать пару, проверять, и делать" — closes the manual
// gap alphaDualPlate (T0237) still has: the lead had to run gen_dual_plate.sh himself and
// drop both plates onto the canvas before running alpha-dual. This is an ACTION ON ONE
// EXISTING image element ("сделай дуал-плейт альфу этому арту"), reused REGARDLESS of how
// that art got there — generated placeholders included, per the T0239 reframe: "generation
// placeholders produce RAW art with NO alpha".
//
// T0248 (lead, same day): T0238 wrongly collapsed the reference script's white-plate step —
// it treated the element's CURRENT pixels as the light plate outright and loudly REFUSED
// any art that wasn't already flat-light. gen_dual_plate.sh never assumes that: it
// GENERATES the white plate from arbitrary source art FIRST, then generates the black plate
// as an edit of THAT white plate. This op now does the same: check_flat_background.py is
// REPORT-only (no refusal); a flat-light element skips straight to the original one-call
// path (its own pixels ARE the light plate), while any other art generates the white plate
// first (one codex call, no retry) and uses THAT as the light plate for the rest of the
// flow. Either way, the DARK plate is generated as a codex EDIT of the light plate (the
// exact subject-lock chain gen_dual_plate.sh's black-plate step uses, see
// tools/dual_plate_generate.mjs), the pair then runs through the SAME alphaDualPlate tool
// (T0237/T0243 — role detection, translation-align, the pair gate, extraction —
// unmodified, ONE engine for both the manual and automatic paths), with ONE automatic
// retry on a gate refusal (the white plate itself is generated exactly once). The whole
// gesture (generation + gating + retry) is OUTSIDE the journal; only the final mint
// commits, so ONE journal entry covers everything and one undo removes just the new
// element — the source element is NEVER touched (mirrors alphaDualPlate's own
// non-destructive stance, extended to a generated-not-selected second plate).

export const DUAL_PLATE_GENERATE_MAX_ATTEMPTS = 2; // 1 initial try + 1 automatic retry
export const ENV_ERROR_RE = /venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i;

// Border-ring flat-light-background REPORT (own Python tool, check_flat_background.py —
// reuses the pair_align border-median idea), run BEFORE any codex spend. T0248:
// report-only — no refusal; returns {flat_light, median_luma, spread, ...} and the caller
// (alphaDualPlateGenerate) ROUTES on `flat_light` instead of stopping the flow. Still
// throws loudly on a genuine tool/environment failure (missing source, broken venv) — only
// the flat/not-flat JUDGMENT stopped refusing.
export async function checkFlatBackground(root, projectId, element) {
  const sourceAbs = resolveProjectFile(root, projectId, element.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-dualgen-flatcheck-"));
  try {
    const specPath = join(workDir, "flatbg_spec.json");
    const reportPath = join(workDir, "flatbg_report.json");
    const spec = { schema: "ai_studio.canvas.check_flat_bg_spec.v1", source: sourceAbs, report: reportPath };
    writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
    await runToolPython(root, ["ai_studio/assets/canvas/tools/check_flat_background.py", "--spec", specPath]);
    return JSON.parse(readFileSync(reportPath, "utf8"));
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Build the tool_runs row + the new element's meta.alpha provenance for the AUTOMATIC
// flow. Additive to buildDualPlateProvenance's shape (method/tool/pair_gate/at stay the
// same shape) plus the fields unique to generation: `plates` with FIXED roles (we already
// KNOW which is which — the light plate is either the source element's own src (flat-light
// path) or a freshly generated white plate (T0248, non-flat path), and the stored generated
// file is always dark — unlike the manual pair op's unordered 2-element input), the
// `prompt` sent to the dark-plate generator, and `align` (T0243's translation-align delta,
// from the tool's own report). `lightGenerated` (T0248, additive) records whether the LIGHT
// plate itself cost a codex call — the dark plate always does.
export function buildDualPlateGenerateProvenance(elementId, lightSrc, darkSrc, prompt, report, lightGenerated) {
  const at = new Date().toISOString();
  const gate = (report && report.pair_gate) || {};
  const align = (report && report.align) || null;
  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "alpha_dualplate_generate",
    elementId,
    at,
    params: { prompt },
    result_summary: {
      method: "dual_plate",
      pair_gate: gate,
      align,
      visible_pixels: report && report.visible_pixels,
    },
  };
  const alphaMeta = {
    method: "dual_plate",
    tool: "alpha_dualplate.py",
    plates: [
      { src: lightSrc, role: "light", generated: !!lightGenerated },
      { src: darkSrc, role: "dark", generated: true },
    ],
    prompt,
    pair_gate: gate,
    align,
    at,
  };
  return { run, alphaMeta };
}

// Generate + key a dark plate against `lightElement` — either the source element itself
// (flat-light path, its own pixels already verified as the light plate) or a freshly
// generated white plate (T0248, non-flat path: `lightElement` is a `{src, name}` stand-in
// for the stored generated file, not a real canvas element) — with ONE automatic retry on a
// gate/extraction refusal. `generate` is the injectable {inputPngPath, prompt} -> Buffer|
// path seam (ops.alphaDualPlateGenerate's `generator` arg, or the default
// tools/dual_plate_generate.mjs generator; T0248 reuses the SAME seam for the white-plate
// step too). Every generated dark-plate attempt is stored content-addressed in files/
// REGARDLESS of outcome (storeAddFile is non-destructive/harmless even on a later throw —
// same law alphaCutoutBatch's atomic comment documents), so a final refusal can name every
// attempt for a manual retry. An environment failure (missing studio venv/Pillow — the SAME
// failure alpha_dualplate.py itself would hit) is NEVER retried: regenerating against a
// broken interpreter just wastes a codex call for the identical failure.
export async function generateAndKeyDualPlate(root, projectId, lightElement, prompt, generate) {
  const inputPngPath = resolveProjectFile(root, projectId, lightElement.src);
  const preserved = [];
  let lastError = null;
  for (let attempt = 1; attempt <= DUAL_PLATE_GENERATE_MAX_ATTEMPTS; attempt += 1) {
    const generated = await generate({ inputPngPath, prompt });
    const darkBytes = Buffer.isBuffer(generated) ? generated : readFileSync(generated);
    const darkFile = storeAddFile(root, projectId, { bytes: darkBytes, name: `${slug(lightElement.name)}_dark_try${attempt}.png` });
    preserved.push(darkFile.src);
    try {
      const { bytes, report } = await runAlphaDualPlateTool(root, projectId, { src: lightElement.src }, { src: darkFile.src });
      return { bytes, report, darkSrc: darkFile.src };
    } catch (error) {
      lastError = error;
      if (ENV_ERROR_RE.test(error.message)) throw error; // broken interpreter: surface now, no retry
    }
  }
  // Semicolon-delimited (never a bare comma/period): every src here already contains a
  // "." (its .png extension), so a comma/period separator would make the reference
  // ambiguous to parse back out. `;` never appears in a content-addressed file name.
  const attemptsList = preserved.map((src, index) => `dark_attempt_${index + 1}=${src}`).join("; ");
  throw new Error(
    `dual-plate generation failed after ${DUAL_PLATE_GENERATE_MAX_ATTEMPTS} attempt(s): ${lastError.message} — ` +
      `preserved plate files for a manual retry: light=${lightElement.src}; ${attemptsList}; ` +
      "manual path: place both plates on the canvas and run the alphaDualPlate pair op " +
      "(API POST /alpha-dual, CLI alpha-dual --elements <light>,<dark>).",
  );
}

// ONE existing image element -> generated plate(s) -> ONE new cut element, in ONE
// journaled entry (`elementId` required; `prompt?` is an optional extra subject
// description APPENDED to both the white-plate and black-plate subject-lock prompts;
// `generator?` is the injectable {inputPngPath, prompt} -> Buffer|path GENERIC plate
// generator, defaulting to tools/dual_plate_generate.mjs's codex-backed implementation —
// tests inject a fake one so codex NEVER runs in the suite). Validates like
// alphaCutout/alphaDualPlate (loud on a missing project/element id or a non-image element)
// BEFORE any Python spawn, then gets a REPORT (not a refusal, T0248) on whether the
// element's border is a flat light background (checkFlatBackground): flat -> the element's
// own pixels ARE the light plate (today's one-codex-call path, unchanged); not flat -> the
// white plate is generated FIRST from the element's own pixels (one codex call, no retry —
// gen_dual_plate.sh's white-plate step) and stored content-addressed, becoming the light
// plate for everything downstream. Either way the dark plate is generated + keyed with one
// automatic retry (generateAndKeyDualPlate) against that light plate. The new element is
// named "<source name> alpha" and placed to the RIGHT of the source element's bbox with a
// 16px gap (mirrors alphaDualPlate's own plate-pair placement); its element.meta.alpha
// carries method "dual_plate", both plates (fixed light/dark roles, each carrying an
// additive `generated` flag — T0248), the prompt, the pair gate's verdict, and the T0243
// align delta. The source element is NEVER mutated — non-destructive, exactly like the
// manual pair op.
export async function alphaDualPlateGenerate(root, { projectId, elementId, prompt, generator } = {}) {
  if (!projectId) throw new Error("alphaDualPlateGenerate requires projectId");
  if (!elementId) throw new Error("alphaDualPlateGenerate requires elementId");
  const generate = typeof generator === "function" ? generator : generatePlate;

  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} is not an image`);

  // Step 1: flat-light-background REPORT — before any codex spend; routes, never refuses.
  const flatReport = await checkFlatBackground(root, projectId, element);

  const extraPrompt = prompt != null && String(prompt).trim() ? String(prompt).trim() : undefined;
  const blackPrompt = buildBlackPlatePrompt(extraPrompt);

  // Step 2 (non-flat only): generate the WHITE plate FIRST, as an edit of the ELEMENT's own
  // (arbitrary) pixels — gen_dual_plate.sh's white-plate step. Generated exactly ONCE (no
  // retry here; only the dark plate gets the automatic retry below) and stored
  // content-addressed immediately, so it is preserved even if everything downstream fails.
  // A flat-light element skips this and reuses its own pixels as the light plate.
  let lightSrc = element.src;
  let lightGenerated = false;
  if (!flatReport.flat_light) {
    const whitePrompt = buildWhitePlatePrompt(extraPrompt);
    const elementPngPath = resolveProjectFile(root, projectId, element.src);
    const generatedWhite = await generate({ inputPngPath: elementPngPath, prompt: whitePrompt });
    const whiteBytes = Buffer.isBuffer(generatedWhite) ? generatedWhite : readFileSync(generatedWhite);
    const whiteFile = storeAddFile(root, projectId, { bytes: whiteBytes, name: `${slug(element.name)}_white.png` });
    lightSrc = whiteFile.src;
    lightGenerated = true;
  }
  const lightElement = { src: lightSrc, name: element.name };

  // Steps 3-4: generate the dark plate + gate/extract (ONE automatic retry inside), against
  // the light plate resolved above (the element itself, or the freshly generated white one).
  const { bytes, report, darkSrc } = await generateAndKeyDualPlate(root, projectId, lightElement, blackPrompt, generate);

  // Re-read to avoid clobbering a concurrent edit (mirrors alphaDualPlate's re-read).
  // Locked (T0254 Tier 1 #1) around just this final critical section — the slow codex
  // plate generation above already ran OUTSIDE the lock, so a multi-minute generate
  // never blocks other mutations on this project; see withProjectLock's doc in
  // store.mjs. refuseIfHeadMoved runs BEFORE the storeAddImage write below (not just
  // at commitMutation time) — see expandRecipePrompt's comment for why that matters.
  const { project, run, addedElement } = await withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("alphaDualPlateGenerate", before, current);
    const source = (current.elements || []).find((item) => item.id === elementId);
    if (!source) throw new Error(`element not found: ${elementId}`);

    const { run, alphaMeta } = buildDualPlateGenerateProvenance(elementId, lightSrc, darkSrc, blackPrompt, report, lightGenerated);

    // Step 5: placement — to the RIGHT of the source element's bbox, 16px gap.
    const gap = 16;
    const added = storeAddImage(root, projectId, {
      name: `${source.name} alpha`,
      bytes,
      x: source.x + source.w + gap,
      y: source.y,
      meta: { alpha: alphaMeta },
    });

    // Front-order hook (identical to addImage/alphaDualPlate): the new element lands at the
    // FRONT of the root scope when it is already explicitly ordered; a no-op otherwise.
    const fo = frontOrder(before, null);
    const nextElements = (added.project.elements || []).map((el2) =>
      fo !== null && el2.id === added.element.id ? { ...el2, order: fo } : el2,
    );
    const after = updateProject(root, projectId, {
      elements: nextElements,
      tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
    });

    // Step 6: ONE journal entry for the whole gesture — the generation itself ran OUTSIDE
    // the journal (before/after only bracket this final mint), so undo removes exactly the
    // new element and never touches the source.
    const committed = commitMutation(root, projectId, {
      op: "alphaDualPlateGenerate",
      args_summary: { elementId, newElementId: added.element.id },
      before,
      after,
      startedAt,
    });
    return { project: committed, run, addedElement: added.element };
  });
  const resultElement = (project.elements || []).find((item) => item.id === addedElement.id) || addedElement;
  return { project, element: resultElement, run };
}

// ---- exportElements (scale + encode) -----------------------------------------

// The source image's on-disk format (png/jpg/webp/gif) from its element.src ext,
// and the output file extension for an export format.
export function sourceFormat(element) {
  const ext = extname(String(element.src || "")).toLowerCase().replace(/^\./, "");
  return ext === "jpeg" ? "jpg" : ext || "png";
}
export function formatExt(format) {
  return format; // png -> png, jpg -> jpg, webp -> webp (already normalized)
}

// Figma-style scale marker for an export file name (T0229 replaces the manual suffix;
// T0235 added the base tag; lead flipped the DEFAULT base to CANVAS same day — "я
// поскейлил арт и хочу на выходе размер как на канвасе"). A 1x CANVAS-base multiplier
// is the baseline -> no marker (clean "name.png"); any other scale gets "@<token>"
// (e.g. "@2x", "@0.5x", "@512w"). A SOURCE-base row always gets a marker, even at 1x
// ("@1x-source"), because the canvas-base 1x row already claims the unmarked baseline
// name — a source-base row must never collide with it. Only applied when an element
// has SEVERAL rows (a single row is always the clean base name); the tokens are
// filename-safe (digits, "x"/"w"/"h", "." and "@"), so they never escape the confined
// export folder.
export function scaleMarker(scaleToken, base) {
  const spec = parseScaleSpec(scaleToken);
  const isSource = base === "source";
  if (spec.kind === "mul" && spec.value === 1 && !isSource) return "";
  return isSource ? `@${spec.token}-source` : `@${spec.token}`;
}

// The rows an element exports with: its persisted export settings, an explicit
// override applied to every element (CLI ad-hoc / one-off), or the default single
// 1x-png row when the layer has no settings — matching Figma's implicit 1x.
export function rowsForElement(element, overrideRows) {
  if (overrideRows) return overrideRows;
  if (Array.isArray(element.export) && element.export.length) return cleanExportRows(element.export);
  return [DEFAULT_EXPORT_ROW];
}

// The element's on-canvas size (Math.round(w)/Math.round(h)) for a "canvas"-base export
// row (T0235) — the CURRENT size on the canvas at export time, not frozen into the row's
// scale token, so a later resize is picked up automatically. Loud when either dimension
// is missing or rounds to zero: a canvas-base row can't resolve without real geometry.
export function canvasDimsFor(element) {
  const w = Math.round(Number(element.w));
  const h = Math.round(Number(element.h));
  if (!(w > 0) || !(h > 0)) {
    throw new Error(
      `element ${element.id} (${element.name || element.id}) has no on-canvas size for a "canvas" base export row (w=${element.w}, h=${element.h})`,
    );
  }
  return { w, h };
}

// Export selected elements to <project>/export/<utc-stamp>/, one file per element x
// export row, plus a manifest.json. Each row scales (resolveExportScale) + encodes
// (png/jpg/webp with quality/resample) via ONE Python spawn for the whole batch
// (tools/export_images.py, spec-file pattern). A 1x-png export of a png source is a
// byte-identical file COPY done in Node (no re-encode, no spawn) so the lead's
// original pixels are preserved exactly. Export makes no project mutation, so it is
// NOT journaled/undoable; it only records a tool_runs entry. `rows`, when given,
// overrides every element's settings for this one run (agent one-shots / the CLI's
// inline --scale/--format flags); omit it to honor each element's persisted rows.
// Each row's base (T0235, default "source") picks which dims its scale token resolves
// against: "source" -> the element's original source_w/h (unchanged v1 behavior);
// "canvas" -> the element's CURRENT on-canvas w/h (canvasDimsFor).
export async function exportElements(root, { projectId, elementIds, rows } = {}) {
  if (!projectId) throw new Error("exportElements requires projectId");
  const project = getProject(root, projectId);
  const ids = Array.isArray(elementIds) ? elementIds.map(String) : [];
  if (!ids.length) throw new Error("exportElements requires elementIds");
  const overrideRows = rows === undefined || rows === null ? null : cleanExportRows(rows);

  const elements = [];
  for (const id of ids) {
    const element = (project.elements || []).find((item) => String(item.id) === id);
    if (!element) throw new Error(`element not found: ${id}`);
    if (element.type === "text") {
      // Standalone per-element text-PNG export is a v1.1 feature (see T0222). Text bakes
      // into a screen today: put it in a group and export the screen (renderGroup) or run
      // a project export, which composites text with PIL through render_group.py.
      throw new Error(
        `element ${id} is a text element — standalone text export is not in v1. Put it in a group and export the screen (Render group / project export) to bake the text into the PNG.`,
      );
    }
    if (element.type === "note") {
      // A note is a work annotation, NOT render content (T0268): it never reaches a PNG in
      // ANY path — renderGroup/exportProject prune it and this standalone export refuses it
      // loudly, same spirit as the text refusal above.
      throw new Error(
        `element ${id} is a note element — notes are canvas annotations and never export to a PNG (they are excluded from Render group / project export too).`,
      );
    }
    if (element.type !== "image" || !element.src) throw new Error(`element ${id} is not an exportable image`);
    elements.push(element);
  }

  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const folder = resolveProjectPath(root, projectId, "export", stamp);
  const used = new Set();
  const items = [];
  const copyJobs = []; // Node byte-copies (png -> png, no resize): verbatim originals.
  const encodeJobs = []; // one batched Python spawn scales + encodes the rest.

  for (const element of elements) {
    const elementRows = rowsForElement(element, overrideRows);
    const srcAbs = resolveProjectFile(root, projectId, element.src);
    const srcFmt = sourceFormat(element);
    const sourceW = Number(element.source_w) || Number(element.w) || 0;
    const sourceH = Number(element.source_h) || Number(element.h) || 0;
    const base = slug(element.name || element.id);
    // Automatic naming (T0229): a single row is the clean base name; several rows get a
    // Figma scale marker ("name@2x.png") so they never overwrite each other. Any name
    // that still collides (same scale+format twice, or two elements sharing a name) is
    // disambiguated deterministically with a numeric "_NN" against the run-wide `used`.
    const multiRow = elementRows.length > 1;

    for (const row of elementRows) {
      // Lead 2026-07-03: CANVAS is the default base — a scaled-on-canvas sprite exports
      // "as seen" unless the row explicitly asks for the source pixels.
      const baseDims = row.base === "source" ? { w: sourceW, h: sourceH } : canvasDimsFor(element);
      const { width, height } = resolveExportScale(row.scale, baseDims.w, baseDims.h);
      const marker = multiRow ? scaleMarker(row.scale, row.base) : "";
      let file = `${base}${marker}.${formatExt(row.format)}`;
      let counter = 2;
      while (used.has(file)) {
        file = `${base}${marker}_${String(counter).padStart(2, "0")}.${formatExt(row.format)}`;
        counter += 1;
      }
      used.add(file);
      const outAbs = resolveProjectPath(root, projectId, "export", stamp, file);
      // needsResize/pureCopy compare against the SOURCE FILE's actual pixels (sourceW/H,
      // not baseDims) — that decides whether the on-disk bytes can be copied verbatim,
      // regardless of which base the row's target dims were resolved against.
      const needsResize = width !== sourceW || height !== sourceH;
      const pureCopy = !needsResize && row.format === "png" && srcFmt === "png";

      const item = {
        elementId: element.id,
        name: element.name || element.id,
        file,
        src: element.src,
        scale: row.scale,
        base: row.base === "source" ? "source" : "canvas",
        format: row.format,
        resample: row.resample,
        w: width,
        h: height,
        meta: element.meta || {},
      };
      if (row.quality !== undefined) item.quality = row.quality;
      items.push(item);

      if (pureCopy) {
        copyJobs.push({ srcAbs, outAbs });
      } else {
        encodeJobs.push({
          src: srcAbs,
          out: outAbs,
          target_w: width,
          target_h: height,
          format: row.format,
          quality: row.quality === undefined ? null : row.quality,
          resample: row.resample,
        });
      }
    }
  }

  // Byte-identical copies never touch Python (offline + preserves the exact bytes).
  for (const job of copyJobs) writeProjectBytes(job.outAbs, readFileSync(job.srcAbs));

  // One Python spawn for the whole encode batch (spec-file pattern like slice). Uses
  // the config-only bridge interpreter, so a missing venv/PIL is a loud named error.
  if (encodeJobs.length) {
    const workDir = mkdtempSync(join(tmpdir(), "canvas-export-"));
    try {
      const specPath = join(workDir, "export_spec.json");
      const reportPath = join(workDir, "export_report.json");
      const spec = {
        schema: "ai_studio.canvas.export_images_spec.v1",
        report: reportPath,
        jobs: encodeJobs,
      };
      writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
      await runToolPython(root, ["ai_studio/assets/canvas/tools/export_images.py", "--spec", specPath]);
    } finally {
      rmSync(workDir, { recursive: true, force: true });
    }
  }

  const manifest = {
    schema: "ai_studio.canvas.export.v1",
    project: project.id,
    at: new Date().toISOString(),
    items,
  };
  writeProjectBytes(
    resolveProjectPath(root, projectId, "export", stamp, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
  );

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "export_elements",
    at: new Date().toISOString(),
    params: { elementIds: ids, rows: overrideRows ? "override" : "per-element" },
    result_summary: { item_count: items.length, encoded: encodeJobs.length, copied: copyJobs.length, folder },
  };
  updateProject(root, projectId, { tool_runs: capToolRuns(root, projectId, [...(getProject(root, projectId).tool_runs || []), run]) });
  return { folder, stamp, items, manifest, run };
}

// ---- zipExport (bundle a finished run into one .zip) -------------------------

// Bundle a finished export run's image files into ONE STORE-mode zip — the page's
// "several outputs -> one archive" save-dialog delivery (Figma behavior) and the CLI
// --zip flag, so both clients archive identically (tool parity). The run already
// materialized its files under the confined <project>/export/<stamp>/; this reads that
// run's manifest.json to learn the produced file names, reads each file, and hands them
// to the pure zip writer. STORE mode = no compression (PNG/JPG/WebP are already
// compressed). Loud on a bad/unknown stamp, a corrupt manifest, or a file gone missing
// (never a silent empty archive). Makes no project mutation.
export function zipExport(root, { projectId, stamp } = {}) {
  if (!projectId) throw new Error("zipExport requires projectId");
  if (!stamp) throw new Error("zipExport requires stamp");
  const manifestPath = resolveProjectPath(root, projectId, "export", stamp, "manifest.json");
  if (!existsSync(manifestPath)) throw new Error(`export run not found: ${stamp}`);
  let manifest;
  try {
    manifest = JSON.parse(readFileSync(manifestPath, "utf8").replace(/^﻿/, ""));
  } catch (error) {
    throw new Error(`export manifest is not valid JSON (${stamp}): ${error.message}`);
  }
  // Collect the run's output image files: a single-screen render carries manifest.file;
  // element/project runs list them under manifest.items[].file. De-duplicate, preserving
  // order, and archive nothing else (specs/reports/manifest stay out of the bundle).
  const files = [];
  if (manifest.file) files.push(manifest.file);
  for (const item of manifest.items || []) if (item && item.file) files.push(item.file);
  const seen = new Set();
  const entries = [];
  for (const file of files) {
    if (seen.has(file)) continue;
    seen.add(file);
    const abs = resolveProjectPath(root, projectId, "export", stamp, file); // confines each segment
    if (!existsSync(abs)) throw new Error(`export file missing for zip: ${file}`);
    entries.push({ name: file, data: readFileSync(abs) });
  }
  if (!entries.length) throw new Error(`export run ${stamp} has no files to zip`);
  return { bytes: zipStore(entries), files: entries.map((entry) => entry.name) };
}

// ---- renderGroup (screen compositing) ----------------------------------------
//
// Composite a group's VISIBLE member elements (element.visible !== false), in
// element array order (z-order), clipped to the group bounds, into ONE PNG at
// the requested scale over a transparent (or solid) background. The pixel work
// is done by our own Python tool (tools/render_group.py, PIL) because there is
// no dependency-free pure-Node compositor. This tool is OURS, so ops hands the full
// render spec to render_group.py through the shared bridge (runToolPython → warm
// worker), the same path detect/slice/export use; renderGroup makes no undoable
// geometry change, so like exportElements it is NOT journaled — it only records a
// render_group tool_runs entry.

export function hexColor(value) {
  const text = String(value || "").trim();
  return /^#[0-9a-fA-F]{6}$/.test(text) ? text.toLowerCase() : null;
}

// Build the recursive z-ordered paint tree for a scope. Each VISIBLE child is either
// an element paint node (absolute box) or a group node (its background fill + its own
// recursively-built children painted inside the parent band). Hidden subtrees are
// pruned (isNodeHidden cascade). `leaves` accumulates every painted image element ref
// for the manifest + member count. `clip` rides along per group: render_group.py composites
// a clip:true subgroup's subtree onto its OWN box-sized layer (cropping overflow) before
// pasting it into the parent; a clip:false subgroup paints into the same layer at absolute
// offsets (overflow preserved).
export function buildRenderNodes(root, projectId, project, scopeId, leaves, fonts) {
  const nodes = [];
  for (const child of orderedChildren(project, scopeId)) {
    if (isNodeHidden(project, child.ref)) continue;
    if (child.kind === "group") {
      const group = child.ref;
      const groupBg = group.background && group.background.type === "color" ? hexColor(group.background.color) : null;
      nodes.push({
        kind: "group",
        clip: group.clip === true,
        x: Number(group.x) || 0,
        y: Number(group.y) || 0,
        w: Number(group.w) || 0,
        h: Number(group.h) || 0,
        background: groupBg,
        children: buildRenderNodes(root, projectId, project, group.id, leaves, fonts),
      });
    } else {
      const element = child.ref;
      // T0268: a NOTE is a canvas-only work annotation — prune it from the render spec
      // BEFORE it is written so render_group.py never sees it (renderGroup + exportProject,
      // which composites through here, both inherit the skip; no Python change needed). This
      // is the explicit belt to the natural fall-through's suspenders below.
      if (element.type === "note") continue;
      if (element.type === "text") {
        // A text node carries the ABSOLUTE font file (render_group.py loads the same
        // .ttf the page @font-faces) plus the split lines + style; the painter
        // re-measures each line for auto-width alignment, so no width is baked here.
        const style = element.style || defaultTextStyle();
        const entry = resolveFontEntry(fonts, {
          family: style.fontFamily,
          weight: style.fontWeight,
          style: style.fontStyle,
        });
        const stroke = style.stroke && Number(style.stroke.width) > 0
          ? { width: Number(style.stroke.width), color: style.stroke.color || "#000000" }
          : null;
        const shadow = style.shadow
          ? { dx: Number(style.shadow.dx) || 0, dy: Number(style.shadow.dy) || 0, color: style.shadow.color || "#000000" }
          : null;
        leaves.push(element);
        nodes.push({
          kind: "text",
          x: Number(element.x) || 0,
          y: Number(element.y) || 0,
          // T0232 increment 3a: rotation is a valid field on a text element ("rotates the
          // box") and is forwarded here for forward-compat, but render_group.py's
          // paint_text does not yet rotate glyph pixels — that lands in a follow-up
          // increment; only IMAGE elements get real pixel rotation/flip in 3a.
          rotation: Number(element.rotation) || 0,
          fontFile: resolveFontFileAbs(root, entry),
          fontSize: Number(style.fontSize) || 24,
          lineHeight: Number(style.lineHeight) || 1.2,
          align: style.align || "left",
          color: style.color || "#111111",
          lines: splitTextLines(element.content),
          stroke,
          shadow,
        });
        continue;
      }
      if (element.type !== "image" || !element.src) continue;
      leaves.push(element);
      nodes.push({
        kind: "element",
        src: resolveProjectFile(root, projectId, element.src),
        x: Number(element.x) || 0,
        y: Number(element.y) || 0,
        w: Number(element.w) || 0,
        h: Number(element.h) || 0,
        // T0232 increment 3a: rotation (degrees CW about the box center) + flip, forwarded
        // verbatim for render_group.py's paint_element (R2 exact math) and the canvas's own
        // paintElement — see README "Rotation & flip" for the shared parity contract.
        rotation: Number(element.rotation) || 0,
        flipH: element.flipH === true,
        flipV: element.flipV === true,
        // T0233: forwarded verbatim so render_group.py's paint_element can build the
        // sliced box BEFORE its flip/rotate chain (design section 4.2). `undefined`
        // when absent, so JSON.stringify drops the key entirely — zero shape change
        // for a non-slice9 element.
        slice9: element.slice9 || undefined,
        // T0260: static element.opacity in [0,1] (absent = 1, JSON drops the key), so
        // render_group.py's paint_element multiplies the element alpha by it before
        // compositing — parity with the canvas's ctx.globalAlpha. `element.opacity`
        // directly (NOT `|| undefined`) so a valid 0 is forwarded, not turned absent.
        // Image-only render integration in increment 1: text nodes do not carry it (a
        // translucent text element renders opaque in BOTH renderers today).
        opacity: element.opacity,
        // Image filters (brightness/saturation/contrast/tint): forwarded verbatim so
        // render_group.py's paint_element can apply the SAME canonical formulas (see
        // README "Image filters") right after the resize/slice9 step, BEFORE flip/rotate.
        // `undefined` when absent, so JSON.stringify drops the key — zero shape change for
        // an unfiltered element. Image-only, same as slice9/rotation/flip above.
        filters: element.filters || undefined,
      });
    }
  }
  return nodes;
}

// Composite one group's visible SUBTREE into a screen PNG at the given absolute
// output/spec/report paths (spawns render_group.py once). Shared by renderGroup
// (single screen, own folder) and exportProject (every visible top-level screen into
// one folder). Runs render_group.py through the shared bridge warm worker (runToolPython),
// the same config-only interpreter every canvas Python tool now uses.
export async function compositeGroup(root, projectId, project, group, { scale, background, outputAbs, specAbs, reportAbs } = {}) {
  const renderScale = finite(scale) && Number(scale) > 0 ? Number(scale) : 1;
  // Precedence: an explicit render-time background arg OVERRIDES group.background;
  // else the group's own stored background; else transparent. render_group.py fills
  // this hex as the bottom layer, so the group background composites behind children.
  const explicit = background === undefined || background === null || background === "" ? null : hexColor(background);
  if (background && explicit === null) throw new Error(`background must be #rrggbb, got ${JSON.stringify(background)}`);
  const groupBg = group.background && group.background.type === "color" ? hexColor(group.background.color) : null;
  const bg = explicit || groupBg || null;

  // Recursive paint tree of the group's VISIBLE subtree, in COMPUTED z-order per scope
  // (nested subgroups included; each subgroup's background composites inside the parent
  // band). `members` is every painted image element (leaf) for the manifest + count.
  const members = [];
  // Load the fonts manifest once per composite so text nodes resolve to absolute .ttf
  // paths (a no-op file read when the group has no text).
  const fonts = readFontsManifest(root);
  const children = buildRenderNodes(root, projectId, project, group.id, members, fonts);
  const spec = {
    schema: "ai_studio.canvas.render_group_spec.v1",
    scale: renderScale,
    background: bg,
    group: { x: Number(group.x) || 0, y: Number(group.y) || 0, w: Number(group.w) || 0, h: Number(group.h) || 0 },
    output: outputAbs,
    report: reportAbs,
    children,
  };
  writeProjectBytes(specAbs, `${JSON.stringify(spec, null, 2)}\n`);
  await runToolPython(root, ["ai_studio/assets/canvas/tools/render_group.py", "--spec", specAbs]);

  let report = {};
  try {
    report = JSON.parse(readFileSync(reportAbs, "utf8"));
  } catch {
    // The PNG is the real product; a missing/foreign report is non-fatal.
  }
  const width = report.width || Math.max(1, Math.round((Number(group.w) || 0) * renderScale));
  const height = report.height || Math.max(1, Math.round((Number(group.h) || 0) * renderScale));
  return { renderScale, bg, members, width, height };
}

export async function renderGroup(root, { projectId, groupId, scale, background } = {}) {
  if (!projectId) throw new Error("renderGroup requires projectId");
  if (!groupId) throw new Error("renderGroup requires groupId");
  const project = getProject(root, projectId);
  const group = groupsOf(project).find((item) => item.id === groupId);
  if (!group) throw new Error(`group not found: ${groupId}`);

  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const fileName = `screen_${slug(group.name || group.id)}.png`;
  const folder = resolveProjectPath(root, projectId, "export", stamp);
  const outputAbs = resolveProjectPath(root, projectId, "export", stamp, fileName);

  const composited = await compositeGroup(root, projectId, project, group, {
    scale,
    background,
    outputAbs,
    specAbs: resolveProjectPath(root, projectId, "export", stamp, "render_spec.json"),
    reportAbs: resolveProjectPath(root, projectId, "export", stamp, "render_report.json"),
  });

  const manifest = {
    schema: "ai_studio.canvas.export.v1",
    kind: "screen",
    project: project.id,
    at: new Date().toISOString(),
    group: { id: group.id, name: group.name || group.id },
    scale: composited.renderScale,
    background: composited.bg,
    file: fileName,
    width: composited.width,
    height: composited.height,
    items: composited.members.map((element) => ({ elementId: element.id, name: element.name || element.id, src: element.src })),
  };
  writeProjectBytes(
    resolveProjectPath(root, projectId, "export", stamp, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
  );

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "render_group",
    at: new Date().toISOString(),
    params: { groupId, scale: composited.renderScale, background: composited.bg },
    result_summary: { file: fileName, folder, member_count: composited.members.length, width: manifest.width, height: manifest.height },
  };
  updateProject(root, projectId, { tool_runs: capToolRuns(root, projectId, [...(getProject(root, projectId).tool_runs || []), run]) });
  return { folder, file: fileName, path: outputAbs, manifest, run, members: composited.members.length };
}

// ---- exportProject (no selection -> export every screen) ---------------------

// Project-level export used when nothing is selected: render every EXPLICITLY-FLAGGED
// (`group.screen === true`) TOP-LEVEL group at its own default 1x png into ONE
// <project>/export/<utc-stamp>/ folder plus a combined manifest. A nested group is a
// component INSIDE its root screen (composited by compositeGroup's recursion), never a
// separate screen, so only parentId-less groups are considered at all here. T0332 B1
// (lead 2026-07-07, "ЭКСПОРТ — ИНВЕРСИЯ НА OPT-IN"): a group is a screen ONLY when the
// lead explicitly ticks it — `screen` is absent by default (patchGroup/group-set), so a
// freshly created top-level group does NOT export until flagged. This REPLACES the old
// implicit rule ("every visible top-level group except a recipe/style card"); there is no
// special-case skip for `group.recipe`/`group.style`/`group.pack_run` any more — they are
// simply never auto-flagged (createRecipeCard/createStyleCard/the pack-run mint never set
// `screen`), so they stay unflagged BY CONSTRUCTION, same as any other group the lead never
// ticked. An EXISTING project migrates via tools/migrate_screen_flags.mjs (one-shot,
// preserves today's export set). Like the other export ops it makes no project mutation, so
// it is NOT journaled; it records one export_project tool_runs entry.
export async function exportProject(root, { projectId } = {}) {
  if (!projectId) throw new Error("exportProject requires projectId");
  const project = getProject(root, projectId);
  const visibleGroups = groupsOf(project).filter(
    (group) => group.parentId == null && group.visible !== false && group.screen === true,
  );
  if (!visibleGroups.length) throw new Error("no visible screens to export (project export renders every screen-flagged top-level group)");

  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const folder = resolveProjectPath(root, projectId, "export", stamp);
  const usedFiles = new Set();
  const screens = [];
  for (const group of visibleGroups) {
    const base = `screen_${slug(group.name || group.id)}`;
    let fileName = `${base}.png`;
    let counter = 2;
    while (usedFiles.has(fileName)) {
      fileName = `${base}_${String(counter).padStart(2, "0")}.png`;
      counter += 1;
    }
    usedFiles.add(fileName);
    const stem = fileName.replace(/\.png$/, "");
    const composited = await compositeGroup(root, projectId, project, group, {
      scale: 1,
      background: null,
      outputAbs: resolveProjectPath(root, projectId, "export", stamp, fileName),
      specAbs: resolveProjectPath(root, projectId, "export", stamp, `${stem}.spec.json`),
      reportAbs: resolveProjectPath(root, projectId, "export", stamp, `${stem}.report.json`),
    });
    screens.push({
      groupId: group.id,
      name: group.name || group.id,
      file: fileName,
      w: composited.width,
      h: composited.height,
      members: composited.members.length,
    });
  }

  const manifest = {
    schema: "ai_studio.canvas.export.v1",
    kind: "project",
    project: project.id,
    at: new Date().toISOString(),
    items: screens,
  };
  writeProjectBytes(
    resolveProjectPath(root, projectId, "export", stamp, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
  );

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "export_project",
    at: new Date().toISOString(),
    params: { screenCount: screens.length },
    result_summary: { screen_count: screens.length, folder },
  };
  updateProject(root, projectId, { tool_runs: capToolRuns(root, projectId, [...(getProject(root, projectId).tool_runs || []), run]) });
  return { folder, stamp, screens, manifest, run };
}
