// Canvas project persistence.
//
// One project is one folder under the configured canvas projects root:
//   <projectsRoot>/<project-id>/
//     project.json           schema ai_studio.canvas.project.v1
//     files/<file>           immutable, content-addressed image bytes
//
// Every write is atomic (temp file + rename). Image files are never mutated or
// deleted from disk: removeElement only drops the element from project.json so
// prior tool runs and references stay valid. All ids/filenames are path-confined.
//
// The projects root is resolved lazily via studio config (CANVAS_PROJECTS_ROOT
// env overrides for tests) and is only created on first project create.
import { randomUUID } from "node:crypto";
import {
  appendFileSync,
  closeSync,
  copyFileSync,
  existsSync,
  fstatSync,
  mkdirSync,
  openSync,
  readFileSync,
  readSync,
  readdirSync,
  renameSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, extname, join, resolve, sep } from "node:path";
import { canvasProjectsRoot } from "../../core_harness/tool_lib/studio_config.mjs";
import { sha256Hex } from "../../core_harness/tool_lib/hash.mjs";

export const CANVAS_PROJECT_SCHEMA = "ai_studio.canvas.project.v1";

const IMAGE_EXTENSIONS = ["png", "jpg", "jpeg", "gif", "webp"];

function projectsRoot(root) {
  return canvasProjectsRoot(root);
}

function nowIso() {
  return new Date().toISOString();
}

function slugify(value) {
  const slug = String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 48);
  return slug || "project";
}

// Reject anything that could escape the projects root (separators, traversal,
// dotfiles) and confine the resolved path under `parent`.
function confineChild(parent, name, label) {
  if (!name || typeof name !== "string" || /[\\/]/.test(name) || name.includes("..") || name.startsWith(".")) {
    throw new Error(`unsafe ${label}: ${JSON.stringify(name)}`);
  }
  const base = resolve(parent);
  const full = resolve(base, name);
  if (full !== base && !full.startsWith(base + sep)) {
    throw new Error(`${label} escapes its root: ${JSON.stringify(name)}`);
  }
  return full;
}

function projectDir(root, id) {
  return confineChild(projectsRoot(root), id, "project id");
}

function projectJsonPath(root, id) {
  return join(projectDir(root, id), "project.json");
}

function writeAtomic(filePath, contents) {
  const tmp = `${filePath}.tmp-${randomUUID().slice(0, 8)}`;
  writeFileSync(tmp, contents);
  renameSync(tmp, filePath);
}

function writeProjectFile(root, project) {
  const dir = projectDir(root, project.id);
  mkdirSync(dir, { recursive: true });
  writeAtomic(join(dir, "project.json"), `${JSON.stringify(project, null, 2)}\n`);
  return project;
}

function readProjectFile(root, id) {
  const path = projectJsonPath(root, id);
  if (!existsSync(path)) throw new Error(`canvas project not found: ${id}`);
  return JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
}

// PNG/JPEG/GIF intrinsic dimensions from header bytes only. Pure, no deps.
export function imageSize(buffer) {
  if (
    buffer.length >= 24 &&
    buffer[0] === 0x89 && buffer[1] === 0x50 && buffer[2] === 0x4e && buffer[3] === 0x47
  ) {
    return { width: buffer.readUInt32BE(16), height: buffer.readUInt32BE(20) };
  }
  if (buffer.length >= 10 && buffer.toString("ascii", 0, 3) === "GIF") {
    return { width: buffer.readUInt16LE(6), height: buffer.readUInt16LE(8) };
  }
  if (buffer.length >= 4 && buffer[0] === 0xff && buffer[1] === 0xd8) {
    let offset = 2;
    while (offset + 9 < buffer.length) {
      if (buffer[offset] !== 0xff) {
        offset += 1;
        continue;
      }
      const marker = buffer[offset + 1];
      const isSof = marker >= 0xc0 && marker <= 0xcf && ![0xc4, 0xc8, 0xcc].includes(marker);
      if (isSof) {
        return { width: buffer.readUInt16BE(offset + 7), height: buffer.readUInt16BE(offset + 5) };
      }
      const length = buffer.readUInt16BE(offset + 2);
      if (length < 2) break;
      offset += 2 + length;
    }
  }
  throw new Error("unsupported image: expected PNG, JPEG, or GIF header bytes");
}

function sniffExtension(buffer) {
  if (buffer.length >= 4 && buffer[0] === 0x89 && buffer[1] === 0x50) return "png";
  if (buffer.length >= 3 && buffer.toString("ascii", 0, 3) === "GIF") return "gif";
  if (buffer.length >= 2 && buffer[0] === 0xff && buffer[1] === 0xd8) return "jpg";
  if (buffer.length >= 12 && buffer.toString("ascii", 8, 12) === "WEBP") return "webp";
  return "png";
}

function imageExtension(name, buffer) {
  const fromName = extname(String(name || "")).toLowerCase().replace(/^\./, "");
  if (IMAGE_EXTENSIONS.includes(fromName)) return fromName === "jpeg" ? "jpg" : fromName;
  return sniffExtension(buffer);
}

export function createProject(root, { title } = {}) {
  const cleanTitle = String(title || "").trim() || "Untitled canvas";
  const id = `${slugify(cleanTitle)}-${randomUUID().slice(0, 6)}`;
  const now = nowIso();
  const project = {
    schema: CANVAS_PROJECT_SCHEMA,
    id,
    title: cleanTitle,
    created: now,
    updated: now,
    // history_seq is the undo/redo head: the seq of the currently applied journal
    // mutation (0 = base/empty). The op layer owns the journal semantics; the
    // store just carries this pointer through updateProject like any other field.
    history_seq: 0,
    // Groups are Figma-frame-like named screen regions (one level, no nesting).
    // Additive to ai_studio.canvas.project.v1: older projects load with no groups
    // and every reader tolerates a missing/empty groups array.
    groups: [],
    elements: [],
    tool_runs: [],
  };
  // mkdir is recursive so this also creates the projects root on first use.
  mkdirSync(join(projectDir(root, id), "files"), { recursive: true });
  return writeProjectFile(root, project);
}

export function listProjects(root) {
  const base = projectsRoot(root);
  if (!existsSync(base)) return [];
  const projects = [];
  for (const entry of readdirSync(base, { withFileTypes: true })) {
    if (!entry.isDirectory() || entry.name.startsWith(".")) continue;
    try {
      projects.push(readProjectFile(root, entry.name));
    } catch {
      // Tolerate broken/foreign project folders: skip rather than fail the list.
    }
  }
  projects.sort((a, b) => String(b.updated || "").localeCompare(String(a.updated || "")));
  return projects;
}

export function getProject(root, id) {
  return readProjectFile(root, id);
}

// Merge a shallow patch onto the project, always bumping `updated`. Immutable
// fields (schema/id/created) are never overwritten.
export function updateProject(root, id, patch = {}) {
  const project = readProjectFile(root, id);
  const { schema, id: _id, created, ...rest } = patch;
  const next = { ...project, ...rest, schema: project.schema, id: project.id, created: project.created, updated: nowIso() };
  return writeProjectFile(root, next);
}

function finiteOr(value, fallback) {
  return value !== undefined && value !== null && Number.isFinite(Number(value)) ? Number(value) : fallback;
}

// Content-addressed write of image bytes into files/ WITHOUT creating an element.
// Returns { src, fileName, width, height }. Immutable + deduped exactly like addImage's
// own file write (identical bytes reuse the same file, so this never mutates or orphans
// an existing file). The alphaCutout op uses this to point an element at a NEW alpha PNG
// while the previous file stays on disk (the non-destructive src-swap pattern). Shared by
// addImage below so there is one content-addressing path.
export function addFile(root, id, { bytes, name } = {}) {
  const buffer = Buffer.isBuffer(bytes) ? bytes : Buffer.from(bytes || []);
  if (!buffer.length) throw new Error("addFile requires non-empty image bytes");
  const { width, height } = imageSize(buffer);
  const ext = imageExtension(name, buffer);
  const fileName = `${sha256Hex(buffer)}.${ext}`;
  const filesDir = join(projectDir(root, id), "files");
  mkdirSync(filesDir, { recursive: true });
  const filePath = join(filesDir, fileName);
  // Content-addressed and immutable: identical bytes reuse the same file.
  if (!existsSync(filePath)) writeAtomic(filePath, buffer);
  return { src: `files/${fileName}`, fileName, width, height };
}

export function addImage(root, id, { name, bytes, x = 0, y = 0, meta } = {}) {
  const { src, fileName, width, height } = addFile(root, id, { bytes, name });

  const project = readProjectFile(root, id);
  const element = {
    id: `el_${randomUUID().slice(0, 8)}`,
    type: "image",
    src,
    x: finiteOr(x, 0),
    y: finiteOr(y, 0),
    w: width,
    h: height,
    // Intrinsic source pixels, kept immutable so region overlays and future
    // resizes can scale by w/source_w even after the element box is resized.
    source_w: width,
    source_h: height,
    name: String(name || fileName),
    meta: meta && typeof meta === "object" ? meta : {},
  };
  project.elements = [...(project.elements || []), element];
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, element };
}

function findElement(project, elementId) {
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  return element;
}

const NUMERIC_ELEMENT_FIELDS = ["x", "y", "w", "h"];

// Apply the mutable element fields from a patch onto an element IN PLACE: numeric
// x/y/w/h (finite values only), name (string), and the optional explicit visibility
// boolean (stored so renderGroup/the page can hide with `element.visible !== false`).
// For a TEXT element it also applies `content` (string) and `style` (a fully
// validated/normalized style OBJECT — the op layer does the fonts-manifest validation
// before this runs, so the store only stores the object verbatim). `rotation`/`flipH`/
// `flipV` (T0232 increment 3a — additive transform schema) are likewise pre-validated by
// the op layer (ops.sanitizeTransformPatch: finite degrees normalized to [0,360), real
// booleans, flip rejected on a text element) — this just stores them, mirroring
// `group.clip`'s "absent means the default" rule: `rotation === 0` and `flipH`/`flipV
// === false` are stored as an ABSENT field (an untouched/reset element stays clean; the
// stored shape never carries a redundant zero/false). Shared by patchElement and the
// batched patchElements so both honor identical rules.
function applyElementFields(element, patch = {}) {
  for (const field of NUMERIC_ELEMENT_FIELDS) {
    if (patch[field] !== undefined && patch[field] !== null && Number.isFinite(Number(patch[field]))) {
      element[field] = Number(patch[field]);
    }
  }
  if (patch.name !== undefined) element.name = String(patch.name);
  if (patch.visible !== undefined) {
    element.visible = !(patch.visible === false || patch.visible === "false");
  }
  if (patch.content !== undefined) element.content = String(patch.content);
  if (patch.style !== undefined && patch.style && typeof patch.style === "object") {
    element.style = patch.style;
  }
  if (patch.rotation !== undefined) {
    if (Number(patch.rotation) === 0) delete element.rotation;
    else element.rotation = Number(patch.rotation);
  }
  if (patch.flipH !== undefined) {
    if (patch.flipH === false) delete element.flipH;
    else element.flipH = true;
  }
  if (patch.flipV !== undefined) {
    if (patch.flipV === false) delete element.flipV;
    else element.flipV = true;
  }
  return element;
}

// Append a TEXT element (parallel to addImage, but no backing file: the content +
// style ARE the element). `style` is the fully validated/normalized style object and
// `name` the layer name — the op layer computes both before calling this. `w`/`h` are
// the nominal box (bookkeeping only; both renderers re-measure). Optional `groupId`
// drops the text straight into a group.
export function addText(root, id, { x = 0, y = 0, w = 0, h = 0, content = "Text", style = {}, name, groupId, meta } = {}) {
  const project = readProjectFile(root, id);
  const element = {
    id: `el_${randomUUID().slice(0, 8)}`,
    type: "text",
    x: finiteOr(x, 0),
    y: finiteOr(y, 0),
    w: finiteOr(w, 0),
    h: finiteOr(h, 0),
    content: String(content),
    style,
    name: String(name || "Text"),
    meta: meta && typeof meta === "object" ? meta : {},
  };
  if (groupId != null && groupId !== "") element.groupId = String(groupId);
  project.elements = [...(project.elements || []), element];
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, element };
}

export function patchElement(root, id, elementId, patch = {}) {
  const project = readProjectFile(root, id);
  const element = findElement(project, elementId);
  applyElementFields(element, patch);
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, element };
}

// Patch several elements in ONE project write. Every patch's `elementId` must
// resolve (findElement throws before updateProject, so a bad id aborts the batch
// with NO partial write). Returns the saved project and the touched elements.
export function patchElements(root, id, patches = []) {
  const project = readProjectFile(root, id);
  const touched = [];
  for (const patch of patches) {
    const element = findElement(project, patch.elementId);
    applyElementFields(element, patch);
    touched.push(element);
  }
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, elements: touched };
}

// Move a project folder to <projectsRoot>/.trash/<id>-<stamp>/ instead of deleting
// it (safety: the folder is recoverable, never rm'd). listProjects skips
// dot-prefixed entries, so a trashed project disappears from the list. This is a
// project-level action and is intentionally not journaled (the per-project journal
// lives inside the folder being moved).
export function deleteProject(root, id) {
  const dir = projectDir(root, id);
  if (!existsSync(dir)) throw new Error(`canvas project not found: ${id}`);
  const base = projectsRoot(root);
  const trashDir = join(base, ".trash");
  mkdirSync(trashDir, { recursive: true });
  const stamp = nowIso().replace(/[:.]/g, "-");
  const dest = join(trashDir, `${id}-${stamp}`);
  renameSync(dir, dest);
  return { id, trashed: dest };
}

// Remove the element from project.json. The backing file under files/ stays on
// disk on purpose (immutability + prior tool_runs may still reference it).
export function removeElement(root, id, elementId) {
  const project = readProjectFile(root, id);
  findElement(project, elementId);
  project.elements = (project.elements || []).filter((item) => item.id !== elementId);
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, removed: elementId };
}

// Remove several elements in ONE project write. Every id must exist (findElement
// throws before updateProject, so a bad id aborts the batch with NO partial delete);
// duplicate ids are de-duplicated. Backing files stay on disk (immutable storage).
export function removeElements(root, id, elementIds = []) {
  const project = readProjectFile(root, id);
  const wanted = new Set(elementIds.map(String));
  for (const elementId of wanted) findElement(project, elementId);
  project.elements = (project.elements || []).filter((item) => !wanted.has(item.id));
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, removed: [...wanted] };
}

// Confined absolute path for a project-relative file reference (e.g. an element
// `src` like "files/<hash>.png" or a bare file name). Used by the API to serve
// image bytes and by ops to read an element's source for pipeline steps.
export function resolveProjectFile(root, id, relName) {
  const filesDir = join(projectDir(root, id), "files");
  // Strip a single leading "files/" (element.src form); confineChild then rejects
  // any remaining separators or traversal instead of silently sanitizing them.
  const bare = String(relName || "").replace(/^files[\\/]/, "");
  return confineChild(filesDir, bare, "file name");
}

export function readElementBytes(root, id, elementId) {
  const project = readProjectFile(root, id);
  const element = findElement(project, elementId);
  if (element.type !== "image" || !element.src) {
    throw new Error(`element ${elementId} is not an image`);
  }
  const filePath = resolveProjectFile(root, id, element.src);
  if (!existsSync(filePath)) throw new Error(`image file missing for element ${elementId}`);
  return { buffer: readFileSync(filePath), fileName: basename(filePath), element };
}

// ---- operation journal -------------------------------------------------------
//
// One append-only journal.jsonl sits next to project.json. Each line is a small
// JSON object of op METADATA only: mutation entries
// ({seq, at, op, args_summary, parent, duration_ms, has_snapshot:true}) and
// undo/redo markers ({seq, at, op, target_seq, duration_ms}). The fat before/after
// project snapshots live OUT of the line, one file per mutation under
// <project>/snapshots/<seq>.json ({undo_patch, state}). This keeps every journal
// line O(1) in project size, so appendJournal, readHistory, and undo/redo scan
// only tiny lines and load exactly the one snapshot they need by seq.
//
// The store owns the plumbing: read, atomic append, O(1) monotonic seq (tail
// read), sidecar snapshot read/write/delete, thin-journal rewrite/archive, a
// transparent one-time migration of legacy fat lines, capped tool_runs with a
// spill sidecar, and the per-project errors.jsonl sink.

function journalPath(root, id) {
  return join(projectDir(root, id), "journal.jsonl");
}

function snapshotsDir(root, id) {
  return join(projectDir(root, id), "snapshots");
}

function snapshotPath(root, id, seq) {
  // seq is an integer we allocate, so it can never contain a path separator.
  return join(snapshotsDir(root, id), `${Number(seq)}.json`);
}

export function readJournal(root, id) {
  const path = journalPath(root, id);
  if (!existsSync(path)) return [];
  const text = readFileSync(path, "utf8").replace(/^\uFEFF/, "");
  const entries = [];
  for (const line of text.split("\n")) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    try {
      entries.push(JSON.parse(trimmed));
    } catch {
      // Tolerate a torn/foreign line rather than losing the whole history.
    }
  }
  return entries;
}

// O(1) max-seq via a tail read: seq is monotonic and appends are physical-append
// only, so the last complete line always carries the max seq. Read only the file
// tail (not the whole journal), scan backward for the last parseable line, and
// fall back to a full parse only if the tail holds no complete line (e.g. a single
// giant legacy fat line larger than the tail window \u2014 rare and one-time).
const SEQ_TAIL_BYTES = 65536;

function lastJournalSeq(root, id) {
  const path = journalPath(root, id);
  if (!existsSync(path)) return 0;
  const fd = openSync(path, "r");
  try {
    const size = fstatSync(fd).size;
    if (size === 0) return 0;
    const window = Math.min(size, SEQ_TAIL_BYTES);
    const buffer = Buffer.alloc(window);
    readSync(fd, buffer, 0, window, size - window);
    const lines = buffer.toString("utf8").split("\n");
    for (let i = lines.length - 1; i >= 0; i -= 1) {
      const trimmed = lines[i].trim();
      if (!trimmed) continue;
      try {
        const parsed = JSON.parse(trimmed);
        if (Number.isFinite(Number(parsed.seq))) return Number(parsed.seq);
      } catch {
        // Partial fragment at the chunk boundary or a torn tail: keep scanning back.
      }
    }
  } finally {
    closeSync(fd);
  }
  // Tail held no complete line: fall back to a full parse (correctness over speed).
  return readJournal(root, id).reduce((max, item) => Math.max(max, Number(item.seq) || 0), 0);
}

export function nextJournalSeq(root, id) {
  return lastJournalSeq(root, id) + 1;
}

// Append one pre-built journal line verbatim (single O_APPEND write, atomic for
// this single-writer local tool; the file is never rewritten here, so a crash at
// worst drops the last partial line \u2014 tolerated on read).
export function appendJournalLine(root, id, line) {
  const dir = projectDir(root, id);
  mkdirSync(dir, { recursive: true });
  appendFileSync(journalPath(root, id), `${JSON.stringify(line)}\n`);
  return line;
}

// Convenience append that allocates seq (O(1) tail read) + timestamp. Used for the
// undo/redo markers (which carry no snapshot).
export function appendJournal(root, id, entry) {
  const line = { seq: nextJournalSeq(root, id), at: nowIso(), ...entry };
  return appendJournalLine(root, id, line);
}

// ---- sidecar snapshots -------------------------------------------------------

export function writeSnapshot(root, id, seq, snapshot) {
  const dir = snapshotsDir(root, id);
  mkdirSync(dir, { recursive: true });
  writeAtomic(snapshotPath(root, id, seq), `${JSON.stringify(snapshot)}\n`);
}

export function readSnapshot(root, id, seq) {
  const path = snapshotPath(root, id, seq);
  if (!existsSync(path)) return null;
  try {
    return JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
  } catch {
    return null;
  }
}

export function deleteSnapshot(root, id, seq) {
  rmSync(snapshotPath(root, id, seq), { force: true });
}

// Atomic rewrite of the whole journal (used only by compaction). Kept lines are
// serialized in order; the last kept line still carries the max seq, so the tail
// seq read stays correct afterward.
export function rewriteJournal(root, id, lines) {
  const dir = projectDir(root, id);
  mkdirSync(dir, { recursive: true });
  const body = lines.map((line) => JSON.stringify(line)).join("\n");
  writeAtomic(journalPath(root, id), body ? `${body}\n` : "");
}

// Append dropped journal lines to the compaction archive (append-only audit trail
// of what fell past the history horizon; the fat snapshots themselves are dropped).
export function appendArchive(root, id, lines) {
  if (!lines.length) return;
  const dir = projectDir(root, id);
  mkdirSync(dir, { recursive: true });
  const body = lines.map((line) => JSON.stringify(line)).join("\n");
  appendFileSync(join(dir, "journal.archive.jsonl"), `${body}\n`);
}

// ---- transparent fat-journal migration --------------------------------------
//
// Legacy journals inlined {undo_patch, state} on every line (O(project) per line,
// O(n^2) per session). On the first mutating open we move those blobs into sidecar
// snapshots and rewrite the journal thin, keeping the ORIGINAL as journal.jsonl.bak
// (non-destructive: the lead's history is never deleted). The gate is O(1): once a
// snapshots/ dir exists the project is already v2, so this never re-scans. Migration
// is idempotent \u2014 a re-run finds no inline blobs and only ensures snapshots/ exists.

function lineHasInlineSnapshot(line) {
  return line && (line.undo_patch !== undefined || line.state !== undefined);
}

export function ensureThinJournal(root, id) {
  const dir = projectDir(root, id);
  // Fast O(1) gate: a snapshots/ dir means this project was already created or
  // migrated under the sidecar format; nothing to do.
  if (existsSync(snapshotsDir(root, id))) return;
  const jp = journalPath(root, id);
  if (!existsSync(jp)) return; // brand-new project: the first snapshot write creates snapshots/.

  const journal = readJournal(root, id);
  const fatLines = journal.filter(lineHasInlineSnapshot);
  if (!fatLines.length) {
    // Journal exists but is already thin (or markers-only): just mark it v2 so the
    // O(1) gate stops re-reading it every op.
    mkdirSync(snapshotsDir(root, id), { recursive: true });
    return;
  }

  // Back up the original fat journal once (never clobber an existing backup).
  const bak = `${jp}.bak`;
  if (!existsSync(bak)) copyFileSync(jp, bak);

  const thin = journal.map((line) => {
    if (!lineHasInlineSnapshot(line)) return line; // markers pass through unchanged.
    writeSnapshot(root, id, line.seq, { undo_patch: line.undo_patch, state: line.state });
    const { undo_patch, state, ...meta } = line;
    return { ...meta, has_snapshot: true };
  });
  mkdirSync(snapshotsDir(root, id), { recursive: true });
  rewriteJournal(root, id, thin);
}

// ---- tool_runs cap + spill ---------------------------------------------------
//
// tool_runs ride inside every snapshot and every project.json read/write, so an
// unbounded array quietly inflates P. Keep the last N in project.json and spill the
// overflow to an append-only <project>/tool_runs.jsonl provenance sidecar.
const DEFAULT_TOOL_RUNS_CAP = 50;

function toolRunsCap() {
  const raw = Number(process.env.CANVAS_TOOL_RUNS_CAP);
  return Number.isFinite(raw) && raw >= 0 ? raw : DEFAULT_TOOL_RUNS_CAP;
}

export function capToolRuns(root, id, runs) {
  const list = Array.isArray(runs) ? runs : [];
  const cap = toolRunsCap();
  if (list.length <= cap) return list;
  const spill = list.slice(0, list.length - cap);
  const dir = projectDir(root, id);
  mkdirSync(dir, { recursive: true });
  appendFileSync(join(dir, "tool_runs.jsonl"), `${spill.map((run) => JSON.stringify(run)).join("\n")}\n`);
  return list.slice(list.length - cap);
}

export function readToolRunsArchive(root, id) {
  const path = join(projectDir(root, id), "tool_runs.jsonl");
  if (!existsSync(path)) return [];
  const out = [];
  for (const line of readFileSync(path, "utf8").replace(/^\uFEFF/, "").split("\n")) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    try {
      out.push(JSON.parse(trimmed));
    } catch {
      // tolerate a torn line
    }
  }
  return out;
}

// ---- errors sink -------------------------------------------------------------
//
// Failed ops append one line to <project>/errors.jsonl. Only project-resolvable
// failures land here (a missing/unsafe project id can't be logged \u2014 there is no
// folder to write to), so this never throws for the caller's original error.
export function projectExists(root, id) {
  try {
    return existsSync(projectDir(root, id));
  } catch {
    return false; // unsafe id: confineChild threw
  }
}

export function appendError(root, id, entry) {
  if (!projectExists(root, id)) return false;
  try {
    const dir = projectDir(root, id);
    mkdirSync(dir, { recursive: true });
    const line = { at: nowIso(), ...entry };
    appendFileSync(join(dir, "errors.jsonl"), `${JSON.stringify(line)}\n`);
    return true;
  } catch {
    return false; // logging must never mask the real error
  }
}

export function readErrors(root, id) {
  const path = join(projectDir(root, id), "errors.jsonl");
  if (!existsSync(path)) return [];
  const out = [];
  for (const line of readFileSync(path, "utf8").replace(/^\uFEFF/, "").split("\n")) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    try {
      out.push(JSON.parse(trimmed));
    } catch {
      // tolerate a torn line
    }
  }
  return out;
}

// Confined absolute path built from nested project-relative segments (e.g.
// "export", "<stamp>", "file.png"). Every segment is rejected if it contains a
// separator, traversal, or dotfile, so export writes stay under the project dir.
export function resolveProjectPath(root, id, ...segments) {
  let full = projectDir(root, id);
  for (const segment of segments) {
    full = confineChild(full, segment, "path segment");
  }
  return full;
}

// Atomic write of arbitrary bytes to a confined absolute path, creating parent
// dirs as needed. Used by the export op to copy immutable image files and write
// the manifest without ever leaving a half-written file.
export function writeProjectBytes(absPath, bytes) {
  mkdirSync(dirname(absPath), { recursive: true });
  writeAtomic(absPath, bytes);
}
