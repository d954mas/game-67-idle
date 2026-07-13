// Canvas HTTP API adapter.
//
// Studio Shell mounts this handler on the /api/canvas/ prefix. It only marshals
// HTTP <-> the shared ops layer (ops.mjs); it holds no canvas logic. Method verbs
// follow one rule (T0254 Tier 1 #2, previously true but unwritten): PUT replaces a
// sub-document wholesale (regions/slice9/export rows), PATCH partial-updates a blob
// (project/group/recipe-card/style-card/element), POST is an action/mint/tool-run
// (cleanup, generate, slice, alpha, nodes-*, …). Routes:
//   GET    /api/canvas/projects
//   POST   /api/canvas/projects                    {title}
//   GET    /api/canvas/projects/<id>
//   PATCH  /api/canvas/projects/<id>               {title?,ownership?,gameId?,archived?}
//   DELETE /api/canvas/projects/<id>                                (move to .trash)
//   POST   /api/canvas/projects/<id>/images        {name, bytes_base64, x?, y?}
//   POST   /api/canvas/projects/<id>/images-batch  {images:[{name, bytes_base64, x?, y?}]} (one entry)
//   POST   /api/canvas/projects/<id>/images-from-file {src, name?, x?, y?}   (mint an element from an EXISTING project file; no re-upload)
//   POST   /api/canvas/projects/<id>/text          {x?, y?, content?, style?, groupId?}
//   POST   /api/canvas/projects/<id>/note          {x?, y?, w?, h?, content?, style?, background?, groupId?}  (T0268: a sticky-note annotation; fixed clipped box + wrap; excluded from renderGroup/exportProject)
//   POST   /api/canvas/projects/<id>/detect-regions {elementId, params?}
//   POST   /api/canvas/projects/<id>/slice          {elementId, regionIds?}
//   POST   /api/canvas/projects/<id>/alpha          {elementId, method?, regions?} | {elementIds, method?} (batch) -> NEW element(s) beside the source(s); original(s) untouched; one entry
//   POST   /api/canvas/projects/<id>/alpha-dual     {elementIds:[a,b]}   (white+black plate pair -> new element; one entry)
//   POST   /api/canvas/projects/<id>/alpha-dual-generate {elementId, prompt?}   (AUTOMATIC: element = light plate, generates the dark plate, gates, cuts; new element; one entry)
//   POST   /api/canvas/projects/<id>/export         {elementIds, rows?} | {project:true}
//   PUT    /api/canvas/projects/<id>/elements/<eid>/export {rows}  (export settings)
//   POST   /api/canvas/projects/<id>/groups         {name, x?,y?,w?,h?, fromElements?, parentId?}
//   PATCH  /api/canvas/projects/<id>/groups/<gid>   {name?,x?,y?,w?,h?,visible?,background?,clip?,screen?}   (T0332 B1: screen is the export opt-in flag — see exportProject/group-set --screen)
//   POST   /api/canvas/projects/<id>/groups-set     {groupIds, visible?, clip?} (batched shared toggles)
//   DELETE /api/canvas/projects/<id>/groups/<gid>
//   POST   /api/canvas/projects/<id>/groups/<gid>/render {scale?, background?}
//   POST   /api/canvas/projects/<id>/groups/<gid>/fit {padding?}   (resize frame to content)
//   POST   /api/canvas/projects/<id>/groups/<gid>/scale {x,y,w,h}   (T0271: scale the group's full subtree -- frame + descendants + text fontSize -- to a new frame; one entry)
//   POST   /api/canvas/projects/<id>/groups/<gid>/reparent {parentId|null, index?}
//   POST   /api/canvas/projects/<id>/groups/<gid>/ungroup  (dissolve one level, keep children)
//   POST   /api/canvas/projects/<id>/recipe-cards          {name?, x?,y?,w?,h?, parentId?}   (T0239 increment 1: mint a recipe card — a group with an additive `recipe` blob)
//   PATCH  /api/canvas/projects/<id>/recipe-cards/<gid>    {prompt?, expanded?, use_expanded?, engine?, style_ref?}     (partial recipe blob update; 400 on a group with no `recipe`; style_ref must be null or an existing style-card group id)
//   POST   /api/canvas/projects/<id>/recipe-cards/<gid>/generate  {runGroupId?, sheetSlug?}   (T0239 increment 2/3: generate — mints 1 (codex/gemini) or 2 (both, compare mode) new RAW elements beside the card, in its PARENT scope; one entry; partial success allowed on engine=both; recipe.style_ref, when set, mixes the style card's prompt + ref image in. T0332 v2: recipe.pack set -> the SAME op branches into a per-sheet pack run instead; runGroupId resumes an existing run, sheetSlug force-regenerates exactly one sheet — both ignored by the single-image branch)
//   POST   /api/canvas/projects/<id>/recipe-cards/<gid>/pack-preview  {}   (T0332 v2 phase C: EPHEMERAL — sheet count + per-sheet {name,prompt,cells} from the real expand_jobs.py expander + a style_ref_image info flag; never journals/mutates recipe.pack)
//   POST   /api/canvas/projects/<id>/recipe-cards/<gid>/pack-slice  {runGroupId?}   (T0332 B3/phase C: slice every sheet of a pack run — detect + hard region-count gate + slice, per-sheet {sheet_element_id,verdict,region_count,cells_len,cut_ids} contract; omitted runGroupId resolves recipe.last_run.run_group_id)
//   POST   /api/canvas/projects/<id>/recipe-cards/<gid>/expand    {}   (T0239 increment 4: Expand-prompt — ONE codex TEXT call; writes recipe.expanded only, no card minted; Generate sends it when use_expanded is true, else the short prompt)
//   POST   /api/canvas/projects/<id>/anim-cards            {name?, x?,y?,w?,h?, parentId?, memberId?}   (T0265 increment 1: mint an animation card — a group with an additive `anim` blob; keyframes are its member images. memberId = "Animate this image" promotion: fit around that image + move it in as the first keyframe, ONE entry; not combinable with x/y/w/h)
//   PATCH  /api/canvas/projects/<id>/anim-cards/<gid>      {motion?, profile?, seed?, matte?, gen_fps?, loop?, columns?, trim?, style_ref?, accepted_ref?}   (partial anim blob update; 400 on a group with no `anim`)
//   POST   /api/canvas/projects/<id>/anim-cards/<gid>/generate  {}   (T0265 increment 1: generate via the Track B video route — mints ONE flipbook element beside the card in its PARENT scope; one entry; 1 keyframe = plain I2V)
//   POST   /api/canvas/projects/<id>/style-cards           {name?, x?,y?,w?,h?, parentId?}   (T0239 increment 3: mint a style card — a group with an additive `style` blob: prompt + ONE ref image; no generate route, style cards never generate)
//   PATCH  /api/canvas/projects/<id>/style-cards/<gid>     {prompt?, ref?}                    (partial style blob update; ref must be null or a member IMAGE element id — the "Make ref" gesture; 400 on a group with no `style`)
//   POST   /api/canvas/projects/<id>/elements/<eid>/cleanup-preview {tool, params}  (T0207: quantize|denoise LIVE preview against CURRENT pixels; writes NOTHING to the store)
//   POST   /api/canvas/projects/<id>/elements/<eid>/cleanup        {tool, params}  (T0207: apply — new file + element.src swap; one journal entry)
//   POST   /api/canvas/projects/<id>/elements/<eid>/filters-bake  {}   (T0274 "Apply": rasterize the element's CURRENT filters+opacity into a new source file, then clear both; one journal entry)
//   POST   /api/canvas/projects/<id>/filters-bake  {elementIds}   (batch: 2+ images baked into ONE journal entry/undo, atomic)
//   POST   /api/canvas/projects/<id>/elements/<eid>/extract        {}   (T0239 increment 4: ONE codex VISION call -> element.meta.extracted {prompt_full, prompt_subject, style, description}; no card minted here — re-running overwrites)
//   POST   /api/canvas/projects/<id>/elements/<eid>/animate        {text}   (T0264: ONE codex TEXT/VISION call -> element.animation (the ai_studio.canvas.animation.v1 spec); authors fresh or minimally patches an existing spec; image + text; loud on a non-JSON reply / invalid spec)
//   POST   /api/canvas/projects/<id>/elements/<eid>/promote-recipe {}   (mint a RECIPE card BELOW the element from its ALREADY-STORED meta.extracted; NO codex call; loud without meta.extracted first)
//   POST   /api/canvas/projects/<id>/elements/<eid>/promote-style  {}   (mint a STYLE card RIGHT of the element from its ALREADY-STORED meta.extracted; NO codex call; loud without meta.extracted first)
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
//   PATCH  /api/canvas/projects/<id>/elements/<eid> {x,y,w,h,name,visible,rotation?,flipH?,flipV?,opacity?,filters?,content?,style?,background?} (T0232 3a: rotation = degrees CW about the box center; flip is image-only. T0260: opacity in [0,1], stored only when != 1. T0273: filters = {brightness?,saturation?,contrast?,tint?} non-destructive image color adjustments, image-only, whole-object replace, null/{} clears — see README "Image filters". T0222/T0268: content/style patch a text OR note; background patches a note only — all loud on the wrong type)
//   PUT    /api/canvas/projects/<id>/elements/<eid>/regions {regions}   (replace)
//   PUT    /api/canvas/projects/<id>/elements/<eid>/slice9  {insets}    (T0233: set 9-slice insets {left,top,right,bottom,scale?}; {insets:null} clears; image-only)
//   PUT    /api/canvas/projects/<id>/elements/<eid>/animation {animation} (T0260: set the ai_studio.canvas.animation.v1 spec; {animation:null} clears; image + text)
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
  addNote,
  addText,
  alignNodes,
  alphaCutout,
  alphaDualPlate,
  alphaDualPlateGenerate,
  animateElementFromText,
  assignToGroup,
  bakeFilters,
  cleanupApply,
  cleanupPreview,
  createAnimCard,
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
  expandRecipePrompt,
  exportElements,
  exportProject,
  extractFromElement,
  fitGroup,
  generateAnimFromCard,
  generateFromRecipe,
  getProject,
  historyFlags,
  jumpHistory,
  listHistory,
  listProjects,
  moveNodes,
  opsStats,
  packPreview,
  packSlice,
  pasteNodes,
  patchAnim,
  patchElement,
  patchElements,
  patchGroup,
  patchGroups,
  patchProject,
  patchRecipe,
  patchStyle,
  promoteExtractedRecipe,
  promoteExtractedStyle,
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
  scaleGroup,
  setElementAnimation,
  setExportSettings,
  setRegions,
  setSlice9,
  sliceRegions,
  undoOp,
  ungroupGroup,
  withProjectLock,
  zipExport,
} from "./ops.mjs";
import {
  canvasStoreSummary,
  canvasStoresForQuery,
  decorateCanvasProject,
  selectCanvasStore,
  withCanvasStore,
} from "./stores.mjs";

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

function boolQueryParam(value) {
  return value === "true" || value === "1";
}

function headerValue(req, name) {
  const value = req.headers && req.headers[name.toLowerCase()];
  return Array.isArray(value) ? value[0] : (value || "");
}

function canvasStoreRequestArgs(req, url) {
  const headerStore = headerValue(req, "x-ai-studio-store");
  const queryStore = url.searchParams.get("store") || "";
  if (headerStore && queryStore && headerStore !== queryStore) {
    throw new Error(`Canvas store mismatch between header and query: ${headerStore} != ${queryStore}`);
  }
  return {
    store: headerStore || queryStore,
    game: url.searchParams.get("game") || "",
    includePrivate: boolQueryParam(url.searchParams.get("include-private")) || boolQueryParam(url.searchParams.get("includePrivate")),
  };
}

// Map a thrown op error to an HTTP status (T0254 Tier 1 #2 — previously everything
// fell through to a catch-all 400, so the page couldn't tell "reload, the project
// moved" from "bad input"). Ops throw deliberate `new Error(...)` calls (the "loud
// errors" law), so matching their message shape is the tested contract, not
// incidental prose — EXCEPT the head/seq conflict family, which gets a stable
// `error.code = "HEAD_CONFLICT"` marker at its two throw sites (ops.mjs:
// checkExpectHead, commitMutation's stale-before check) so this never regex-matches
// prose for that one. 404: a resolvable id that doesn't exist ("<noun> not found").
// 409: the project moved underneath the caller — re-read and retry, never a "fix your
// input" 400. 500: a genuinely unexpected error (a bug, not a caller mistake). 400:
// everything else thrown on purpose (validation).
export function statusForError(error) {
  if (error && error.code === "HEAD_CONFLICT") return 409;
  if (error instanceof TypeError || error instanceof ReferenceError) return 500;
  const message = error && error.message ? error.message : String(error);
  if (/not found/i.test(message)) return 404;
  return 400;
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
    let storeArgs;
    let activeStore;
    try {
      storeArgs = canvasStoreRequestArgs(req, url);
      activeStore = selectCanvasStore(root, storeArgs);
    } catch (error) {
      sendJson(res, statusForError(error), { error: error && error.message ? error.message : String(error) });
      return true;
    }
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
        payload.project = decorateCanvasProject(project, activeStore);
      }
      sendJson(res, status, payload);
    };
    // T0254 Tier 1 #1: serialize every mutating call for one project. Page + chat
    // share this one server process, and the CLI is a separate process — withProjectLock
    // covers both (in-process queue + cross-process lockfile), so two concurrent
    // mutations on the SAME project queue instead of racing. The five slow codex/agy
    // generation routes (recipe generate/expand, element extract, element animate,
    // alpha-dual-generate) deliberately do NOT call `locked` here — they lock only their
    // own final commit internally (ops.mjs), so a multi-minute generation never blocks
    // other mutations on the project. See withProjectLock's doc in store.mjs.
    const locked = (projectId, fn) => withProjectLock(root, projectId, fn);
    return await withCanvasStore(activeStore, async () => {
    try {
      if (parts[0] !== "api" || parts[1] !== "canvas" || parts[2] !== "projects") {
        sendJson(res, 404, { error: "not found" });
        return true;
      }

      // /api/canvas/projects
      if (parts.length === 3) {
        if (req.method === "GET") {
          const stores = canvasStoresForQuery(root, storeArgs);
          const ownerGame = url.searchParams.get("owner-game") || url.searchParams.get("ownerGame") || "";
          const includeArchived = boolQueryParam(url.searchParams.get("include-archived"));
          let projects = stores.flatMap((store) =>
            withCanvasStore(store, () => listProjects(root, { includeArchived }).map((project) => decorateCanvasProject(project, store)))
          );
          if (ownerGame) {
            projects = projects.filter((project) =>
              project.ownership?.kind === "game" && project.ownership.gameId === ownerGame
            );
          }
          sendJson(res, 200, { stores: stores.map(canvasStoreSummary), projects });
          return true;
        }
        if (req.method === "POST") {
          const body = await readJsonBody(req);
          sendMutation(201, { project: createProject(root, { title: body.title, ownership: body.ownership, gameId: body.gameId }) });
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      const id = decodeURIComponent(parts[3]);

      // /api/canvas/projects/<id>
      if (parts.length === 4) {
        if (req.method === "GET") {
          sendJson(res, 200, { project: decorateCanvasProject(getProject(root, id), activeStore) });
          return true;
        }
        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          const patch = { projectId: id };
          if (Object.hasOwn(body, "title")) patch.title = body.title;
          if (Object.hasOwn(body, "ownership")) patch.ownership = body.ownership;
          if (Object.hasOwn(body, "gameId")) patch.gameId = body.gameId;
          if (Object.hasOwn(body, "archived")) patch.archived = body.archived;
          sendMutation(200, await locked(id, () => patchProject(root, patch)));
          return true;
        }
        if (req.method === "DELETE") {
          sendMutation(200, await locked(id, () => deleteProject(root, { projectId: id })));
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
        sendMutation(201, await locked(id, () => addImage(root, id, { name: body.name, bytes, x: body.x, y: body.y })));
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
        sendMutation(201, await locked(id, () => addImages(root, id, { images })));
        return true;
      }

      // /api/canvas/projects/<id>/images-from-file  (mint an element from an EXISTING
      // project file src — no re-upload, no duplicate bytes; backs the inspector's
      // per-plate "Add to canvas" button, T0238)
      if (parts.length === 5 && sub === "images-from-file" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => addImageFromFile(root, id, { src: body.src, name: body.name, x: body.x, y: body.y })));
        return true;
      }

      // /api/canvas/projects/<id>/text  (add a text element)
      // x/y place it at a world point; content/style/groupId are optional (style is
      // validated against the fonts manifest by the op — a loud 400 on bad input).
      if (parts.length === 5 && sub === "text" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => addText(root, id, {
          x: body.x,
          y: body.y,
          content: body.content,
          style: body.style,
          groupId: body.groupId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/note  (add a note element — T0268)
      // x/y place it at a world point; content/style/background/w/h/groupId are optional
      // (style + background are validated by the op — a loud 400 on bad input). A note is a
      // canvas annotation and is excluded from renderGroup/exportProject.
      if (parts.length === 5 && sub === "note" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => addNote(root, id, {
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          content: body.content,
          style: body.style,
          background: body.background,
          groupId: body.groupId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/detect-regions
      if (parts.length === 5 && sub === "detect-regions" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => detectRegions(root, {
          projectId: id,
          elementId: body.elementId,
          params: body.params || {},
        })));
        return true;
      }

      // /api/canvas/projects/<id>/slice
      if (parts.length === 5 && sub === "slice" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => sliceRegions(root, {
          projectId: id,
          elementId: body.elementId,
          regionIds: body.regionIds,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/alpha
      // Alpha-cutout the element's current pixels (whole element, or only the given stored
      // region ids) via the image-tools matte pipeline; mints the cutout as a NEW element
      // beside the source (original untouched, for side-by-side A/B — T0336), one journal
      // entry. method "auto" (route), "matte" (force key_matte), "corridorkey"
      // (T0261/T0262 — neural GREEN-screen matte for soft glow art; green native, magenta via a
      // hue180 shim, ~15s GPU; a key that is neither is a loud refusal; regions composite the
      // whole-frame CK result into the requested regions), "vitmatte" (T0335 — neural thin-detail /
      // 2nd-choice-glow matte on a green/magenta key, own GPU venv, ~1-3s, whole-element only), or
      // "birefnet" (T0335 — SOD cutout for an arbitrary/unknown background with no key, shared repo
      // venv CPU ~10-30s, whole-element only). elementIds (2+ images) batches a multi-selection into
      // ONE journal entry/undo (regions are not allowed with a batch). Body is a passthrough — the op
      // validates the method + surfaces every refusal.
      if (parts.length === 5 && sub === "alpha" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => alphaCutout(root, {
          projectId: id,
          elementId: body.elementId,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : undefined,
          method: body.method,
          regions: body.regions,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/alpha-dual
      // Dual-plate alpha cutout (T0237): exactly 2 image elements (the SAME art on a
      // white plate and a black plate, either order) -> ONE new cut element via the
      // image-tools dual_plate_alpha + pair-gate modules. Both plates stay untouched
      // (non-destructive); one journal entry, one undo removes the new element.
      if (parts.length === 5 && sub === "alpha-dual" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => alphaDualPlate(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
        })));
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
          sendMutation(200, await locked(id, () => exportProject(root, { projectId: id })));
          return true;
        }
        sendMutation(200, await locked(id, () => exportElements(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
          rows: body.rows,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/groups  (create)
      if (parts.length === 5 && sub === "groups" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => createGroup(root, {
          projectId: id,
          name: body.name,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          fromElements: body.fromElements,
          parentId: body.parentId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/elements-set   (batched multi-element patch)
      // One journal entry for the whole gesture (marquee/multi-select move commit).
      if (parts.length === 5 && sub === "elements-set" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => patchElements(root, {
          projectId: id,
          patches: Array.isArray(body.patches) ? body.patches : [],
        })));
        return true;
      }

      // /api/canvas/projects/<id>/elements-remove   (batched multi-element delete)
      // One journal entry for the whole gesture; a single undo restores every element.
      if (parts.length === 5 && sub === "elements-remove" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => removeElements(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
        })));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-move   (batched mixed element+group move)
      // One journal entry for the whole gesture (marquee/multi-select move of loose
      // elements AND group frames); group moves cascade their subtree.
      if (parts.length === 5 && sub === "nodes-move" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => moveNodes(root, {
          projectId: id,
          moves: Array.isArray(body.moves) ? body.moves : [],
        })));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-reorder   (batched multi-node z-order)
      // One journal entry: the selected same-scope siblings move as a block (Figma
      // semantics) via {direction} or an absolute {index}; cross-scope applies per scope.
      if (parts.length === 5 && sub === "nodes-reorder" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => reorderNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
          direction: body.direction,
          index: body.index,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-align   (align 2+ nodes, or 1 node inside a group)
      // One journal entry: the reference frame defaults to "auto" (Figma semantics — union
      // bbox of 2+ selected nodes, or the parent group frame for exactly 1 node).
      if (parts.length === 5 && sub === "nodes-align" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => alignNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
          align: body.align,
          reference: body.reference,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-distribute   (equal-gap distribute; 3+ nodes)
      // One journal entry: sorted by position along the axis, endpoints fixed.
      if (parts.length === 5 && sub === "nodes-distribute" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => distributeNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
          axis: body.axis,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-paste   (instantiate a copied node spec)
      // One journal entry: new ids for the whole subtree, internal structure + relative
      // order preserved, shifted by dx/dy, pasted into scopeId (null/absent = root).
      if (parts.length === 5 && sub === "nodes-paste" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => pasteNodes(root, {
          projectId: id,
          spec: body.spec,
          dx: body.dx,
          dy: body.dy,
          scopeId: body.scopeId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-duplicate   (duplicate live nodes in place)
      // One journal entry: builds the spec from the current project, then pastes it at
      // +offset (default +16,+16) into scopeId (default = the originals' common scope).
      if (parts.length === 5 && sub === "nodes-duplicate" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => duplicateNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
          dx: body.dx,
          dy: body.dy,
          scopeId: body.scopeId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/nodes-delete   (batched mixed element+group subtree delete)
      // One journal entry deleting loose elements AND whole group subtrees together; a
      // single undo deep-restores everything at its exact z-slot.
      if (parts.length === 5 && sub === "nodes-delete" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => deleteNodes(root, {
          projectId: id,
          nodeIds: Array.isArray(body.nodeIds) ? body.nodeIds : [],
        })));
        return true;
      }

      // /api/canvas/projects/<id>/groups-set   (batched shared group toggles)
      // One journal entry for the whole gesture: set Visible / Clip on several groups at
      // once (the multi-group inspector's shared toggles). A single undo restores all.
      if (parts.length === 5 && sub === "groups-set" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => patchGroups(root, {
          projectId: id,
          groupIds: Array.isArray(body.groupIds) ? body.groupIds : [],
          visible: body.visible,
          clip: body.clip,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/assign-group
      if (parts.length === 5 && sub === "assign-group" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => assignToGroup(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
          groupId: body.groupId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>  (patch/delete)
      if (parts.length === 6 && sub === "groups") {
        const groupId = decodeURIComponent(parts[5]);
        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          sendMutation(200, await locked(id, () => patchGroup(root, { projectId: id, groupId, ...body })));
          return true;
        }
        if (req.method === "DELETE") {
          sendMutation(200, await locked(id, () => deleteGroup(root, { projectId: id, groupId })));
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/render
      if (parts.length === 7 && sub === "groups" && parts[6] === "render" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => renderGroup(root, {
          projectId: id,
          groupId,
          scale: body.scale,
          background: body.background,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/fit  (resize the frame to fit its
      // content: union of the descendant closure + padding; children never move).
      if (parts.length === 7 && sub === "groups" && parts[6] === "fit" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => fitGroup(root, { projectId: id, groupId, padding: body.padding })));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/scale  (T0271: scale the group's FULL
      // subtree -- the group's own frame AND every descendant element/nested-group box,
      // proportionally, text fontSize scaled too -- to the given {x,y,w,h} in ONE entry.
      // Children are computed SERVER-SIDE by scaleGroup/tree.scaleGroupMoves, so the page
      // never sends descendant patches itself. Distinct from PATCH .../groups/<gid> {w,h},
      // which is FRAME-ONLY (children pinned) -- the page picks one route or the other per
      // drag depending on whether Ctrl is held.)
      if (parts.length === 7 && sub === "groups" && parts[6] === "scale" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => scaleGroup(root, {
          projectId: id,
          groupId,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/reparent  (nest a group under another
      // group, or {parentId:null} = top level; optional merged-sibling index).
      if (parts.length === 7 && sub === "groups" && parts[6] === "reparent" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => reparentGroup(root, {
          projectId: id,
          groupId,
          parentId: body.parentId ?? null,
          index: body.index,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/ungroup  (dissolve one level: direct
      // children land in the parent scope at the group's former z-slot; one journal entry).
      if (parts.length === 7 && sub === "groups" && parts[6] === "ungroup" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(200, await locked(id, () => ungroupGroup(root, { projectId: id, groupId })));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards  (create — T0239 increment 1)
      // A recipe card is a group carrying an additive `recipe` blob; no generation yet.
      if (parts.length === 5 && sub === "recipe-cards" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => createRecipeCard(root, {
          projectId: id,
          name: body.name,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          parentId: body.parentId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards/<gid>  (partial recipe blob update)
      // Body is the recipe PATCH itself ({prompt?, engine?, style_ref?}), not wrapped —
      // patchRecipe validates loudly and 400s on a group with no `recipe` at all.
      if (parts.length === 6 && sub === "recipe-cards" && req.method === "PATCH") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => patchRecipe(root, { projectId: id, groupId, patch: body })));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards/<gid>/generate  (T0239 increment 2; T0332 v2
      // pack branch)
      // Generation runs OUTSIDE the journal (a codex/agy spawn, minutes); only the final
      // mint commits. Mints 1 element (engine codex/gemini) or 2 (engine both, R3 compare
      // mode) beside the card frame, in its PARENT scope — one journal entry either way.
      // Partial success on engine=both surfaces as 200 with a `failed` array, never a 4xx.
      // T0332 v2: `recipe.pack` set -> the SAME op branches into a per-sheet pack run instead
      // (ops.generatePackSheets, one short commit PER SHEET); `runGroupId`/`sheetSlug` (both
      // optional, pack-only — the single-image branch above ignores them, see
      // generateFromRecipe's own doc) resume an existing run / force-regenerate exactly one
      // sheet — the same body fields the inspector's Pack "Generate"/"Regenerate" buttons send.
      if (parts.length === 7 && sub === "recipe-cards" && parts[6] === "generate" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(201, await generateFromRecipe(root, {
          projectId: id,
          groupId,
          runGroupId: body.runGroupId,
          sheetSlug: body.sheetSlug,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards/<gid>/pack-preview  (T0332 v2 phase C)
      // EPHEMERAL preview of the pack's expanded sheets (packPreview: sheet count + per-sheet
      // {name,prompt,cells}, a style_ref_image info flag) — runs the real expand_jobs.py
      // expander but NEVER journals/mutates the blob (mirrors the elements/<eid>/cleanup-
      // preview route above: plain sendJson, not sendMutation — there is no project/history to
      // fold in; not wrapped in `locked` either, for the same "never writes" reason
      // cleanup-preview already relies on).
      if (parts.length === 7 && sub === "recipe-cards" && parts[6] === "pack-preview" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendJson(res, 200, await packPreview(root, { projectId: id, groupId }));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards/<gid>/pack-slice  (T0332 B3/phase C)
      // Slice every sheet of a pack run (detect -> hard region-count gate -> slice, per-sheet
      // contract) — real region-detector/crop_regions.py spawns, no fake seam (same law as the
      // plain detect-regions/slice routes above). Optional body.runGroupId selects an explicit
      // run group; omitted resolves recipe.last_run.run_group_id. UNLIKE the codex/agy routes
      // above, packSlice does not lock its own commits (its detectRegions/sliceRegions calls
      // don't either — mirrors plain detect-regions/slice) — wrapped in `locked` here so the
      // whole multi-sheet pass runs under ONE outer lock (mirrors cli.mjs's own
      // SELF_LOCKING_COMMANDS exclusion + doc comment for recipe-pack-slice).
      if (parts.length === 7 && sub === "recipe-cards" && parts[6] === "pack-slice" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => packSlice(root, { projectId: id, groupId, runGroupId: body.runGroupId })));
        return true;
      }

      // /api/canvas/projects/<id>/recipe-cards/<gid>/expand  (T0239 increment 4)
      // Expand-prompt runs OUTSIDE the journal (a codex TEXT spawn); only the final write
      // commits. Writes recipe.expanded only — no card is minted by this route.
      if (parts.length === 7 && sub === "recipe-cards" && parts[6] === "expand" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(200, await expandRecipePrompt(root, { projectId: id, groupId }));
        return true;
      }

      // /api/canvas/projects/<id>/anim-cards  (create — T0265 increment 1, video route)
      // An animation card is a group carrying an additive `anim` blob; keyframes are its
      // member images. With `memberId` (F4) this is the "Animate this image" promotion: the card
      // fits around that image and the image moves in as the first keyframe in ONE journal entry
      // (createAnimCard refuses memberId combined with explicit x/y/w/h — fit owns the box).
      if (parts.length === 5 && sub === "anim-cards" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => createAnimCard(root, {
          projectId: id,
          name: body.name,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          parentId: body.parentId,
          memberId: body.memberId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/anim-cards/<gid>  (partial anim blob update)
      // Body is the anim PATCH itself ({motion?, profile?, seed?, matte?, gen_fps?, loop?,
      // columns?, trim?, style_ref?, accepted_ref?}), not wrapped — patchAnim validates loudly
      // and 400s on a group with no `anim` at all.
      if (parts.length === 6 && sub === "anim-cards" && req.method === "PATCH") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => patchAnim(root, { projectId: id, groupId, patch: body })));
        return true;
      }

      // /api/canvas/projects/<id>/anim-cards/<gid>/generate  (T0265 increment 1)
      // Generation runs OUTSIDE the journal (the Track B video pipeline, minutes); only the
      // final import+mint commits. Mints ONE flipbook element beside the card frame, in its
      // PARENT scope — one journal entry. Not wrapped in `locked` (self-locks its own final
      // commit internally, like recipe generate), so a multi-minute run never blocks the project.
      if (parts.length === 7 && sub === "anim-cards" && parts[6] === "generate" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(201, await generateAnimFromCard(root, { projectId: id, groupId }));
        return true;
      }

      // /api/canvas/projects/<id>/style-cards  (create — T0239 increment 3)
      // A style card is a group carrying an additive `style` blob (prompt + ONE ref image);
      // it never generates on its own — no generate route here.
      if (parts.length === 5 && sub === "style-cards" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(201, await locked(id, () => createStyleCard(root, {
          projectId: id,
          name: body.name,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          parentId: body.parentId,
        })));
        return true;
      }

      // /api/canvas/projects/<id>/style-cards/<gid>  (partial style blob update)
      // Body is the style PATCH itself ({prompt?, ref?}), not wrapped — patchStyle validates
      // loudly and 400s on a group with no `style` at all; `ref` must be null or a member
      // IMAGE element id (the "Make ref" gesture).
      if (parts.length === 6 && sub === "style-cards" && req.method === "PATCH") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => patchStyle(root, { projectId: id, groupId, patch: body })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/cleanup-preview  (T0207)
      // Quantize/Denoise LIVE preview: runs the tool against the element's CURRENT pixels
      // and hands back the resulting PNG as base64 + the before/after report. Writes
      // NOTHING to the store (no files/ entry, no journal line) — Cancel is free, nothing
      // to undo. The inspector's slider calls this on every debounced param change.
      if (parts.length === 7 && sub === "elements" && parts[6] === "cleanup-preview" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        const result = await cleanupPreview(root, { projectId: id, elementId, tool: body.tool, params: body.params });
        sendJson(res, 200, {
          preview_base64: result.previewBase64,
          report: result.report,
          tool: result.tool,
          params: result.params,
        });
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/cleanup  (T0207)
      // Apply: commits a FRESH deterministic run of the SAME tool+params (quantize/denoise
      // carry no randomness, so this reproduces the exact bytes the last preview showed) as
      // ONE journal entry — new content-addressed file + element.src swap + additive
      // element.meta.cleanup; undo restores the previous src byte-exact. (Cleanup keeps the
      // in-place src-swap; alphaCutout since T0336 mints a NEW element beside the source.)
      if (parts.length === 7 && sub === "elements" && parts[6] === "cleanup" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => cleanupApply(root, { projectId: id, elementId, tool: body.tool, params: body.params })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/filters-bake  (T0274 "Apply")
      // Rasterize the element's CURRENT non-destructive filters+opacity (T0273/T0260) into a
      // NEW content-addressed source file, then clear both (the sliders reset) — ONE journal
      // entry; undo restores the previous src + filters + opacity byte-exact, like cleanupApply.
      // Loud (400) when the element has nothing to bake (filters/opacity already at defaults).
      if (parts.length === 7 && sub === "elements" && parts[6] === "filters-bake" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(200, await locked(id, () => bakeFilters(root, { projectId: id, elementId })));
        return true;
      }

      // /api/canvas/projects/<id>/filters-bake  (T0274 "Apply", batch)
      // The multi-selection "Apply filters on N images" gesture: elementIds (2+ images) baked
      // into ONE journal entry/undo, atomic (any refusal — non-image, nothing to bake — rejects
      // the whole batch, nothing mutated). Mirrors /alpha's project-level batch shape.
      if (parts.length === 5 && sub === "filters-bake" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => bakeFilters(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
        })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/extract  (T0239 increment 4)
      // ONE codex VISION call -> element.meta.extracted; runs OUTSIDE the journal, only the
      // final meta write commits. No card is minted by this route — see promote-recipe/
      // promote-style below.
      if (parts.length === 7 && sub === "elements" && parts[6] === "extract" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(200, await extractFromElement(root, { projectId: id, elementId }));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/animate  {text}  (T0264: the text->animation
      // bridge) — ONE codex TEXT/VISION call authors/minimally-patches element.animation; runs
      // OUTSIDE the journal, only the final spec write commits. Action verb -> POST. EXCLUDED
      // from `locked` like the other slow codex routes above (the op self-locks its own commit).
      if (parts.length === 7 && sub === "elements" && parts[6] === "animate" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await animateElementFromText(root, { projectId: id, elementId, text: body.text }));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/promote-recipe  (T0239 increment 4)
      // Mint a RECIPE card BELOW the element from its ALREADY-STORED meta.extracted; NO
      // codex call (fast, metadata-only — 400s loudly when meta.extracted is absent).
      if (parts.length === 7 && sub === "elements" && parts[6] === "promote-recipe" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(201, await locked(id, () => promoteExtractedRecipe(root, { projectId: id, elementId })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/promote-style  (T0239 increment 4)
      // Mint a STYLE card RIGHT of the element from its ALREADY-STORED meta.extracted; NO
      // codex call (fast, metadata-only — 400s loudly when meta.extracted is absent).
      if (parts.length === 7 && sub === "elements" && parts[6] === "promote-style" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        await readJsonBody(req);
        sendMutation(201, await locked(id, () => promoteExtractedStyle(root, { projectId: id, elementId })));
        return true;
      }

      // /api/canvas/projects/<id>/undo | /redo   {expectHead?}  (T0234: optional
      // concurrency guard; the page does not send it today — undefined, so behavior
      // is unchanged there. An agent driving the API directly can pass it like the CLI.)
      if (parts.length === 5 && sub === "undo" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => undoOp(root, { projectId: id, expectHead: body.expectHead })));
        return true;
      }
      if (parts.length === 5 && sub === "redo" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => redoOp(root, { projectId: id, expectHead: body.expectHead })));
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
        sendMutation(200, await locked(id, () => jumpHistory(root, { projectId: id, seq: body.seq, expectHead: body.expectHead })));
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
        sendMutation(200, await locked(id, () => setRegions(root, { projectId: id, elementId, regions })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/slice9  (T0233: set/clear 9-slice
      // insets; isolated self-contained block — see setSlice9 in ops.mjs)
      if (parts.length === 7 && sub === "elements" && parts[6] === "slice9" && req.method === "PUT") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => setSlice9(root, { projectId: id, elementId, insets: body.insets })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/animation  (T0260: PUT replaces the
      // animation sub-document — the ai_studio.canvas.animation.v1 spec; {animation:null}
      // clears it. setElementAnimation validates loudly and journals one entry.)
      if (parts.length === 7 && sub === "elements" && parts[6] === "animation" && req.method === "PUT") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => setElementAnimation(root, { projectId: id, elementId, animation: body.animation })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/export  (replace export rows)
      if (parts.length === 7 && sub === "elements" && parts[6] === "export" && req.method === "PUT") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        const rows = Array.isArray(body) ? body : body.rows;
        sendMutation(200, await locked(id, () => setExportSettings(root, { projectId: id, elementId, rows })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>/reorder  (z-order: move to sibling index)
      if (parts.length === 7 && sub === "elements" && parts[6] === "reorder" && req.method === "POST") {
        const elementId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => reorderElement(root, { projectId: id, elementId, index: body.index })));
        return true;
      }

      // /api/canvas/projects/<id>/nodes/<nodeId>/reorder  (z-order: move an element OR a
      // group to a target index among its MERGED same-scope siblings).
      if (parts.length === 7 && sub === "nodes" && parts[6] === "reorder" && req.method === "POST") {
        const nodeId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendMutation(200, await locked(id, () => reorderNode(root, { projectId: id, nodeId, index: body.index })));
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>
      if (parts.length === 6 && sub === "elements") {
        const elementId = decodeURIComponent(parts[5]);
        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          sendMutation(200, await locked(id, () => patchElement(root, id, elementId, body)));
          return true;
        }
        if (req.method === "DELETE") {
          sendMutation(200, await locked(id, () => removeElement(root, id, elementId)));
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
      sendJson(res, statusForError(error), { error: error && error.message ? error.message : String(error) });
      return true;
    }
    });
  };
}
