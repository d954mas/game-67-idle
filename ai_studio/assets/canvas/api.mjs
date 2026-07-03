// Canvas HTTP API adapter.
//
// Studio Shell mounts this handler on the /api/canvas/ prefix. It only marshals
// HTTP <-> the shared ops layer (ops.mjs); it holds no canvas logic. Routes:
//   GET    /api/canvas/projects
//   POST   /api/canvas/projects                    {title}
//   GET    /api/canvas/projects/<id>
//   PATCH  /api/canvas/projects/<id>               {title}          (rename)
//   DELETE /api/canvas/projects/<id>                                (move to .trash)
//   POST   /api/canvas/projects/<id>/images        {name, bytes_base64, x?, y?}
//   POST   /api/canvas/projects/<id>/images-batch  {images:[{name, bytes_base64, x?, y?}]} (one entry)
//   POST   /api/canvas/projects/<id>/images-from-file {src, name?, x?, y?}   (mint an element from an EXISTING project file; no re-upload)
//   POST   /api/canvas/projects/<id>/text          {x?, y?, content?, style?, groupId?}
//   POST   /api/canvas/projects/<id>/detect-regions {elementId, params?}
//   POST   /api/canvas/projects/<id>/slice          {elementId, regionIds?}
//   POST   /api/canvas/projects/<id>/alpha          {elementId, method?, regions?} | {elementIds, method?} (batch, one entry)
//   POST   /api/canvas/projects/<id>/alpha-dual     {elementIds:[a,b]}   (white+black plate pair -> new element; one entry)
//   POST   /api/canvas/projects/<id>/alpha-dual-generate {elementId, prompt?}   (AUTOMATIC: element = light plate, generates the dark plate, gates, cuts; new element; one entry)
//   POST   /api/canvas/projects/<id>/export         {elementIds, rows?} | {project:true}
//   PUT    /api/canvas/projects/<id>/elements/<eid>/export {rows}  (export settings)
//   POST   /api/canvas/projects/<id>/groups         {name, x?,y?,w?,h?, fromElements?, parentId?}
//   PATCH  /api/canvas/projects/<id>/groups/<gid>   {name?,x?,y?,w?,h?,visible?,background?}
//   POST   /api/canvas/projects/<id>/groups-set     {groupIds, visible?, clip?} (batched shared toggles)
//   DELETE /api/canvas/projects/<id>/groups/<gid>
//   POST   /api/canvas/projects/<id>/groups/<gid>/render {scale?, background?}
//   POST   /api/canvas/projects/<id>/groups/<gid>/fit {padding?}   (resize frame to content)
//   POST   /api/canvas/projects/<id>/groups/<gid>/reparent {parentId|null, index?}
//   POST   /api/canvas/projects/<id>/groups/<gid>/ungroup  (dissolve one level, keep children)
//   POST   /api/canvas/projects/<id>/recipe-cards          {name?, x?,y?,w?,h?, parentId?}   (T0239 increment 1: mint a recipe card — a group with an additive `recipe` blob)
//   PATCH  /api/canvas/projects/<id>/recipe-cards/<gid>    {prompt?, engine?, style_ref?}     (partial recipe blob update; 400 on a group with no `recipe`; style_ref must be null or an existing style-card group id)
//   POST   /api/canvas/projects/<id>/recipe-cards/<gid>/generate  {}   (T0239 increment 2/3: generate — mints 1 (codex/gemini) or 2 (both, compare mode) new RAW elements beside the card, in its PARENT scope; one entry; partial success allowed on engine=both; recipe.style_ref, when set, mixes the style card's prompt + ref image in)
//   POST   /api/canvas/projects/<id>/style-cards           {name?, x?,y?,w?,h?, parentId?}   (T0239 increment 3: mint a style card — a group with an additive `style` blob: prompt + ONE ref image; no generate route, style cards never generate)
//   PATCH  /api/canvas/projects/<id>/style-cards/<gid>     {prompt?, ref?}                    (partial style blob update; ref must be null or a member IMAGE element id — the "Make ref" gesture; 400 on a group with no `style`)
//   POST   /api/canvas/projects/<id>/nodes-move    {moves:[{nodeId,x,y}...]} (mixed element+group move)
//   POST   /api/canvas/projects/<id>/nodes-reorder {nodeIds, direction|index} (multi-node z-order)
//   POST   /api/canvas/projects/<id>/nodes-align   {nodeIds, align, reference?} (align to selection bbox or parent frame; one entry)
//   POST   /api/canvas/projects/<id>/nodes-distribute {nodeIds, axis} (equal-gap distribute; 3+ nodes; one entry)
//   POST   /api/canvas/projects/<id>/nodes-paste     {spec, dx?, dy?, scopeId?}   (instantiate a node spec; one entry)
//   POST   /api/canvas/projects/<id>/nodes-duplicate {nodeIds, dx?, dy?, scopeId?} (duplicate live nodes; one entry)
//   POST   /api/canvas/projects/<id>/nodes-delete    {nodeIds}                     (mixed element+group subtree delete)
//   POST   /api/canvas/projects/<id>/assign-group   {elementIds, groupId|null}
//   POST   /api/canvas/projects/<id>/undo           {expectHead?}   (T0234 guard; page omits it)
//   POST   /api/canvas/projects/<id>/redo           {expectHead?}
//   GET    /api/canvas/projects/<id>/history
//   GET    /api/canvas/projects/<id>/history-list
//   POST   /api/canvas/projects/<id>/history-jump   {seq, expectHead?}
//   PATCH  /api/canvas/projects/<id>/elements/<eid> {x,y,w,h,name,visible,rotation?,flipH?,flipV?} (T0232 3a: rotation = degrees CW about the box center; flip is image-only)
//   PUT    /api/canvas/projects/<id>/elements/<eid>/regions {regions}   (replace)
//   POST   /api/canvas/projects/<id>/elements/<eid>/reorder {index}     (z-order)
//   POST   /api/canvas/projects/<id>/nodes/<nodeId>/reorder {index}     (z-order: element or group)
//   DELETE /api/canvas/projects/<id>/elements/<eid>
//   GET    /api/canvas/projects/<id>/files/<name>  (image bytes, path-confined)
//   GET    /api/canvas/projects/<id>/export/<...>  (export files, path-confined)
//   GET    /api/canvas/projects/<id>/export-zip/<stamp>  (STORE-mode zip of the run's images)
import { existsSync, readFileSync, statSync } from "node:fs";
import { basename, extname } from "node:path";
import { performance } from "node:perf_hooks";
import {
  addImage,
  addImageFromFile,
  addImages,
  addText,
  alignNodes,
  alphaCutout,
  alphaDualPlate,
  alphaDualPlateGenerate,
  assignToGroup,
  createGroup,
  createProject,
  createRecipeCard,
  createStyleCard,
  deleteGroup,
  deleteNodes,
  deleteProject,
  detectRegions,
  distributeNodes,
  duplicateNodes,
  exportElements,
  exportProject,
  fitGroup,
  generateFromRecipe,
  getProject,
  historyFlags,
  jumpHistory,
  listHistory,
  listProjects,
  moveNodes,
  opsStats,
  pasteNodes,
  patchElement,
  patchElements,
  patchGroup,
  patchGroups,
  patchProject,
  patchRecipe,
  patchStyle,
  readHistory,
  recordOpFailure,
  redoOp,
  removeElement,
  removeElements,
  renderGroup,
  reorderElement,
  reorderNode,
  reorderNodes,
  reparentGroup,
  resolveProjectFile,
  resolveProjectPath,
  setExportSettings,
  setRegions,
  sliceRegions,
  undoOp,
  ungroupGroup,
  zipExport,
} from "./ops.mjs";

// Images are the big payload here; allow up to ~20MB for a base64 upload body.
const maxBodyBytes = 20 * 1024 * 1024;

const mimeByExt = {
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".gif": "image/gif",
  ".webp": "image/webp",
  ".zip": "application/zip",
};

function sendJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data));
}

function readJsonBody(req) {
  return new Promise((resolveBody, rejectBody) => {
    let size = 0;
    let data = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => {
      size += Buffer.byteLength(chunk);
      if (size > maxBodyBytes) {
        rejectBody(new Error("request body too large"));
        req.destroy();
        return;
      }
      data += chunk;
    });
    req.on("end", () => {
      try {
        resolveBody(data ? JSON.parse(data) : {});
      } catch {
        rejectBody(new Error("invalid JSON body"));
      }
    });
    req.on("error", rejectBody);
  });
}

function serveFile(res, filePath, { immutable = false } = {}) {
  if (!existsSync(filePath) || !statSync(filePath).isFile()) {
    sendJson(res, 404, { error: "file not found" });
    return;
  }
  // Local single-user tool: read the confined project image into memory and end.
  // Files here are small canvas assets, so streaming is unnecessary complexity.
  const headers = { "content-type": mimeByExt[extname(filePath).toLowerCase()] || "application/octet-stream" };
  if (immutable) {
    // files/ are content-addressed (sha256 filename) and never rewritten, so the
    // browser may cache them for a year and reuse the decoded image across ops — no
    // re-download of unrelated layer thumbnails on every mutation. The sha256 file
    // name is itself a strong validator (ETag); Last-Modified is a secondary one.
    headers["cache-control"] = "public, max-age=31536000, immutable";
    headers.etag = `"${basename(filePath)}"`;
    headers["last-modified"] = statSync(filePath).mtime.toUTCString();
  }
  res.writeHead(200, headers);
  res.end(readFileSync(filePath));
}

export function createCanvasApi(root) {
  return async function handleCanvasApi(req, res, url) {
    const parts = url.pathname.split("/").filter(Boolean); // ["api","canvas","projects", id, ...]
    const t0 = performance.now();
    // Mutating responses carry the API-observed duration_ms AND the folded history
    // flags (canUndo/canRedo/seq) whenever the op returned a project, so the page
    // drives its re-render from the response alone — no reload GET, no /history GET.
    // Additive transport enrichment (like duration_ms): existing fields are untouched
    // and history never fails an otherwise-successful mutation.
    const sendMutation = (status, data) => {
      const payload = { ...data, duration_ms: Math.round((performance.now() - t0) * 1000) / 1000 };
      const project = data && data.project;
      if (project && project.id) {
        try {
          payload.history = historyFlags(root, { projectId: project.id });
        } catch {
          // history is a convenience; a successful mutation must still 200.
        }
      }
      sendJson(res, status, payload);
    };
    try {
      if (parts[0] !== "api" || parts[1] !== "canvas" || parts[2] !== "projects") {
        sendJson(res, 404, { error: "not found" });
        return true;
      }

      // /api/canvas/projects
      if (parts.length === 3) {
        if (req.method === "GET") {
          sendJson(res, 200, { projects: listProjects(root) });
          return true;
        }
        if (req.method === "POST") {
          const body = await readJsonBody(req);
          sendMutation(201, { project: createProject(root, { title: body.title }) });
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      const id = decodeURIComponent(parts[3]);

      // /api/canvas/projects/<id>
      if (parts.length === 4) {
        if (req.method === "GET") {
          sendJson(res, 200, { project: getProject(root, id) });
          return true;
        }
        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          sendMutation(200, patchProject(root, { projectId: id, title: body.title }));
          return true;
        }
        if (req.method === "DELETE") {
          sendMutation(200, deleteProject(root, { projectId: id }));
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      const sub = parts[4];

      // /api/canvas/projects/<id>/images
      if (parts.length === 5 && sub === "images" && req.method === "POST") {
        const body = await readJsonBody(req);
        const bytes = Buffer.from(String(body.bytes_base64 || ""), "base64");
        // x/y let the page drop an image at a world point; addImage defaults to 0,0.
        sendMutation(201, addImage(root, id, { name: body.name, bytes, x: body.x, y: body.y }));
        return true;
      }

      // /api/canvas/projects/<id>/images-batch   (batched multi-image add)
      // One journal entry for the whole gesture (multi-file drop / paste of several
      // images); each image is {name, bytes_base64, x?, y?}. A single-image add stays on
      // POST /images.
      if (parts.length === 5 && sub === "images-batch" && req.method === "POST") {
        const body = await readJsonBody(req);
        const images = (Array.isArray(body.images) ? body.images : []).map((image) => ({
          name: image && image.name,
          bytes: Buffer.from(String((image && image.bytes_base64) || ""), "base64"),
          x: image && image.x,
          y: image && image.y,
        }));
        sendMutation(201, addImages(root, id, { images }));
        return true;
      }

      // /api/canvas/projects/<id>/images-from-file  (mint an element from an EXISTING
      // project file src — no re-upload, no duplicate bytes; backs the inspector's
      // per-plate "Add to canvas" button, T0238)
      if (parts.length === 5 && sub === "images-from-file" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, addImageFromFile(root, id, { src: body.src, name: body.name, x: body.x, y: body.y }));
        return true;
      }

      // /api/canvas/projects/<id>/text  (add a text element)
      // x/y place it at a world point; content/style/groupId are optional (style is
      // validated against the fonts manifest by the op — a loud 400 on bad input).
      if (parts.length === 5 && sub === "text" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, addText(root, id, {
          x: body.x,
          y: body.y,
          content: body.content,
          style: body.style,
          groupId: body.groupId,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/detect-regions
      if (parts.length === 5 && sub === "detect-regions" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await detectRegions(root, {
          projectId: id,
          elementId: body.elementId,
          params: body.params || {},
        }));
        return true;
      }

      // /api/canvas/projects/<id>/slice
      if (parts.length === 5 && sub === "slice" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await sliceRegions(root, {
          projectId: id,
          elementId: body.elementId,
          regionIds: body.regionIds,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/alpha
      // Alpha-cutout the element's current pixels (whole element, or only the given stored
      // region ids) via the image-tools matte pipeline; swaps the element to a new alpha PNG
      // (one journal entry). method "auto" (route) or "matte" (force key_matte).
      // elementIds (2+ images) batches a multi-selection into ONE journal entry/undo
      // (regions are not allowed with a batch — regions stay single-element).
      if (parts.length === 5 && sub === "alpha" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await alphaCutout(root, {
          projectId: id,
          elementId: body.elementId,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : undefined,
          method: body.method,
          regions: body.regions,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/alpha-dual
      // Dual-plate alpha cutout (T0237): exactly 2 image elements (the SAME art on a
      // white plate and a black plate, either order) -> ONE new cut element via the
      // image-tools dual_plate_alpha + pair-gate modules. Both plates stay untouched
      // (non-destructive); one journal entry, one undo removes the new element.
      if (parts.length === 5 && sub === "alpha-dual" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await alphaDualPlate(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
        }));
        return true;
      }

      // /api/canvas/projects/<id>/alpha-dual-generate
      // AUTOMATIC dual-plate alpha (T0238/T0248): works from ANY art — a flat-light
      // element's own pixels are the LIGHT plate; any other background generates the
      // WHITE plate first (codex edit of the element), then the DARK plate as a codex
      // edit of the light plate, gates the pair through the SAME alphaDualPlate tool
      // (one automatic retry on the dark plate only), and mints ONE new cut element
      // beside the source. The source element stays untouched (non-destructive); one
      // journal entry, one undo removes just the new element.
      if (parts.length === 5 && sub === "alpha-dual-generate" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await alphaDualPlateGenerate(root, {
          projectId: id,
          elementId: body.elementId,
          prompt: body.prompt,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/export
      // {project:true} exports every visible screen; otherwise the selected
      // elements, each honoring its persisted export rows (or an explicit rows
      // override applied to every element for this run).
      if (parts.length === 5 && sub === "export" && req.method === "POST") {
        const body = await readJsonBody(req);
        if (body.project === true || body.project === "true") {
          sendMutation(200, await exportProject(root, { projectId: id }));
          return true;
        }
        sendMutation(200, await exportElements(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
          rows: body.rows,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/groups  (create)
      if (parts.length === 5 && sub === "groups" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, createGroup(root, {
          projectId: id,
          name: body.name,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          fromElements: body.fromElements,
          parentId: body.parentId,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/elements-set   (batched multi-element patch)
      // One journal entry for the whole gesture (marquee/multi-select move commit).
      if (parts.length === 5 && sub === "elements-set" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, patchElements(root, {
          projectId: id,
          patches: Array.isArray(body.patches) ? body.patches : [],
        }));
        return true;
      }

      // /api/canvas/projects/<id>/elements-remove   (batched multi-element delete)
      // One journal entry for the whole gesture; a single undo restores every element.
      if (parts.length === 5 && sub === "elements-remove" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, removeElements(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
        }));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-move   (batched mixed element+group move)
      // One journal entry for the whole gesture (marquee/multi-select move of loose
      // elements AND group frames); group moves cascade their subtree.
      if (parts.length === 5 && sub === "nodes-move" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, moveNodes(root, {
          projectId: id,
          moves: Array.isArray(body.moves) ? body.moves : [],
        }));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-reorder   (batched multi-node z-order)
      // One journal entry: the selected same-scope siblings move as a block (Figma
      // semantics) via {direction} or an absolute {index}; cross-scope applies per scope.
      if (parts.length === 5 && sub === "nodes-reorder" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, reorderNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
          direction: body.direction,
          index: body.index,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-align   (align 2+ nodes, or 1 node inside a group)
      // One journal entry: the reference frame defaults to "auto" (Figma semantics — union
      // bbox of 2+ selected nodes, or the parent group frame for exactly 1 node).
      if (parts.length === 5 && sub === "nodes-align" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, alignNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
          align: body.align,
          reference: body.reference,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-distribute   (equal-gap distribute; 3+ nodes)
      // One journal entry: sorted by position along the axis, endpoints fixed.
      if (parts.length === 5 && sub === "nodes-distribute" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, distributeNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
          axis: body.axis,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-paste   (instantiate a copied node spec)
      // One journal entry: new ids for the whole subtree, internal structure + relative
      // order preserved, shifted by dx/dy, pasted into scopeId (null/absent = root).
      if (parts.length === 5 && sub === "nodes-paste" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, pasteNodes(root, {
          projectId: id,
          spec: body.spec,
          dx: body.dx,
          dy: body.dy,
          scopeId: body.scopeId,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-duplicate   (duplicate live nodes in place)
      // One journal entry: builds the spec from the current project, then pastes it at
      // +offset (default +16,+16) into scopeId (default = the originals' common scope).
      if (parts.length === 5 && sub === "nodes-duplicate" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, duplicateNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
          dx: body.dx,
          dy: body.dy,
          scopeId: body.scopeId,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-delete   (batched mixed element+group subtree delete)
      // One journal entry deleting loose elements AND whole group subtrees together; a
      // single undo deep-restores everything at its exact z-slot.
      if (parts.length === 5 && sub === "nodes-delete" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, deleteNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
        }));
        return true;
      }

      // /api/canvas/projects/<id>/groups-set   (batched shared group toggles)
      // One journal entry for the whole gesture: set Visible / Clip on several groups at
      // once (the multi-group inspector's shared toggles). A single undo restores all.
      if (parts.length === 5 && sub === "groups-set" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, patchGroups(root, {
          projectId: id,
          groupIds: Array.isArray(body.groupIds) ? body.groupIds : [],
          visible: body.visible,
          clip: body.clip,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/assign-group
      if (parts.length === 5 && sub === "assign-group" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, assignToGroup(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
          groupId: body.groupId,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>  (patch/delete)
      if (parts.length === 6 && sub === "groups") {
        const groupId = decodeURIComponent(parts[5]);
        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          sendMutation(200, patchGroup(root, { projectId: id, groupId, ...body }));
          return true;
        }
        if (req.method === "DELETE") {
          sendMutation(200, deleteGroup(root, { projectId: id, groupId }));
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/render
      if (parts.length === 7 && sub === "groups" && parts[6] === "render" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await renderGroup(root, {
          projectId: id,
          groupId,
          scale: body.scale,
          background: body.background,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/fit  (resize the frame to fit its
      // content: union of the descendant closure + padding; children never move).
      if (parts.length === 7 && sub === "groups" && parts[6] === "fit" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, fitGroup(root, { projectId: id, groupId, padding: body.padding }));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/reparent  (nest a group under another
      // group, or {parentId:null} = top level; optional merged-sibling index).
      if (parts.length === 7 && sub === "groups" && parts[6] === "reparent" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, reparentGroup(root, {
          projectId: id,
          groupId,
          parentId: body.parentId ?? null,
          index: body.index,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/ungroup  (dissolve one level: direct
      // children land in the parent scope at the group's former z-slot; one journal entry).
      if (parts.length === 7 && sub === "groups" && parts[6] === "ungroup" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(200, ungroupGroup(root, { projectId: id, groupId }));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards  (create — T0239 increment 1)
      // A recipe card is a group carrying an additive `recipe` blob; no generation yet.
      if (parts.length === 5 && sub === "recipe-cards" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, createRecipeCard(root, {
          projectId: id,
          name: body.name,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          parentId: body.parentId,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards/<gid>  (partial recipe blob update)
      // Body is the recipe PATCH itself ({prompt?, engine?, style_ref?}), not wrapped —
      // patchRecipe validates loudly and 400s on a group with no `recipe` at all.
      if (parts.length === 6 && sub === "recipe-cards" && req.method === "PATCH") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, patchRecipe(root, { projectId: id, groupId, patch: body }));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards/<gid>/generate  (T0239 increment 2)
      // Generation runs OUTSIDE the journal (a codex/agy spawn, minutes); only the final
      // mint commits. Mints 1 element (engine codex/gemini) or 2 (engine both, R3 compare
      // mode) beside the card frame, in its PARENT scope — one journal entry either way.
      // Partial success on engine=both surfaces as 200 with a `failed` array, never a 4xx.
      if (parts.length === 7 && sub === "recipe-cards" && parts[6] === "generate" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(201, await generateFromRecipe(root, { projectId: id, groupId }));
        return true;
      }

      // /api/canvas/projects/<id>/style-cards  (create — T0239 increment 3)
      // A style card is a group carrying an additive `style` blob (prompt + ONE ref image);
      // it never generates on its own — no generate route here.
      if (parts.length === 5 && sub === "style-cards" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, createStyleCard(root, {
          projectId: id,
          name: body.name,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          parentId: body.parentId,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/style-cards/<gid>  (partial style blob update)
      // Body is the style PATCH itself ({prompt?, ref?}), not wrapped — patchStyle validates
      // loudly and 400s on a group with no `style` at all; `ref` must be null or a member
      // IMAGE element id (the "Make ref" gesture).
      if (parts.length === 6 && sub === "style-cards" && req.method === "PATCH") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, patchStyle(root, { projectId: id, groupId, patch: body }));
        return true;
      }

      // /api/canvas/projects/<id>/undo | /redo   {expectHead?}  (T0234: optional
      // concurrency guard; the page does not send it today — undefined, so behavior
      // is unchanged there. An agent driving the API directly can pass it like the CLI.)
      if (parts.length === 5 && sub === "undo" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, undoOp(root, { projectId: id, expectHead: body.expectHead }));
        return true;
      }
      if (parts.length === 5 && sub === "redo" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, redoOp(root, { projectId: id, expectHead: body.expectHead }));
        return true;
      }

      // /api/canvas/projects/<id>/history
      if (parts.length === 5 && sub === "history" && req.method === "GET") {
        sendJson(res, 200, readHistory(root, { projectId: id }));
        return true;
      }

      // /api/canvas/projects/<id>/history-list  (labeled linear spine for the panel)
      if (parts.length === 5 && sub === "history-list" && req.method === "GET") {
        sendJson(res, 200, listHistory(root, { projectId: id }));
        return true;
      }

      // /api/canvas/projects/<id>/history-jump  {seq, expectHead?}  (jump the applied
      // head to a spine seq; one journaled nav marker, folds history flags like undo/
      // redo. expectHead is T0234's optional concurrency guard — see undo/redo above.)
      if (parts.length === 5 && sub === "history-jump" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, jumpHistory(root, { projectId: id, seq: body.seq, expectHead: body.expectHead }));
        return true;
      }

      // /api/canvas/projects/<id>/ops-stats  (per-op timing rollup + error count)
      if (parts.length === 5 && sub === "ops-stats" && req.method === "GET") {
        sendJson(res, 200, opsStats(root, { projectId: id }));
        return true;
      }

      // /api/canvas/projects/<id>/files/<name>  (content-addressed, immutable-cached)
      if (parts.length === 6 && sub === "files" && req.method === "GET") {
        const filePath = resolveProjectFile(root, id, decodeURIComponent(parts[5]));
        serveFile(res, filePath, { immutable: true });
        return true;
      }

      // /api/canvas/projects/<id>/export/<stamp>/<file>  (download an export)
      // Each URL segment is confined individually by resolveProjectPath, so ".."
      // or separators in any segment throw before a file is read.
      if (parts.length >= 6 && sub === "export" && req.method === "GET") {
        const segments = parts.slice(5).map((part) => decodeURIComponent(part));
        const filePath = resolveProjectPath(root, id, "export", ...segments);
        serveFile(res, filePath);
        return true;
      }

      // /api/canvas/projects/<id>/export-zip/<stamp>  (STORE-mode zip of the run's images)
      // The page's "several outputs -> one .zip" save-dialog delivery: builds the archive
      // in memory from the run's manifest (zipExport confines each file name) and returns
      // application/zip. A bad/unknown stamp throws -> the outer catch turns it into a 400.
      if (parts.length === 6 && sub === "export-zip" && req.method === "GET") {
        const stamp = decodeURIComponent(parts[5]);
        const { bytes } = zipExport(root, { projectId: id, stamp });
        res.writeHead(200, {
          "content-type": "application/zip",
          "content-length": bytes.length,
          "content-disposition": `attachment; filename="export-${stamp}.zip"`,
        });
        res.end(bytes);
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/regions  (replace regions)
      if (parts.length === 7 && sub === "elements" && parts[6] === "regions" && req.method === "PUT") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        const regions = Array.isArray(body) ? body : body.regions;
        sendMutation(200, setRegions(root, { projectId: id, elementId, regions }));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/export  (replace export rows)
      if (parts.length === 7 && sub === "elements" && parts[6] === "export" && req.method === "PUT") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        const rows = Array.isArray(body) ? body : body.rows;
        sendMutation(200, setExportSettings(root, { projectId: id, elementId, rows }));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/reorder  (z-order: move to sibling index)
      if (parts.length === 7 && sub === "elements" && parts[6] === "reorder" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, reorderElement(root, { projectId: id, elementId, index: body.index }));
        return true;
      }

      // /api/canvas/projects/<id>/nodes/<nodeId>/reorder  (z-order: move an element OR a
      // group to a target index among its MERGED same-scope siblings).
      if (parts.length === 7 && sub === "nodes" && parts[6] === "reorder" && req.method === "POST") {
        const nodeId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, reorderNode(root, { projectId: id, nodeId, index: body.index }));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>
      if (parts.length === 6 && sub === "elements") {
        const elementId = decodeURIComponent(parts[5]);
        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          sendMutation(200, patchElement(root, id, elementId, body));
          return true;
        }
        if (req.method === "DELETE") {
          sendMutation(200, removeElement(root, id, elementId));
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      sendJson(res, 404, { error: "not found" });
      return true;
    } catch (error) {
      // Log project-resolvable failures to <project>/errors.jsonl (recordOpFailure
      // no-ops when the id can't resolve, e.g. project-not-found). The op name and a
      // coarse summary are derived from the route since the parsed body is gone here.
      const projectId = parts.length >= 4 ? decodeURIComponent(parts[3]) : "";
      const opName = `${req.method} ${parts.slice(4).join("/") || "project"}`;
      recordOpFailure(root, projectId, {
        op: opName,
        args_summary: { method: req.method, path: url.pathname },
        error,
        duration_ms: performance.now() - t0,
      });
      sendJson(res, 400, { error: error && error.message ? error.message : String(error) });
      return true;
    }
  };
}
