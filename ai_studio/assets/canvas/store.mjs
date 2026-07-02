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
  existsSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  renameSync,
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

export function addImage(root, id, { name, bytes, x = 0, y = 0, meta } = {}) {
  const buffer = Buffer.isBuffer(bytes) ? bytes : Buffer.from(bytes || []);
  if (!buffer.length) throw new Error("addImage requires non-empty image bytes");
  const { width, height } = imageSize(buffer);
  const ext = imageExtension(name, buffer);
  const fileName = `${sha256Hex(buffer)}.${ext}`;
  const dir = projectDir(root, id);
  const filesDir = join(dir, "files");
  mkdirSync(filesDir, { recursive: true });
  const filePath = join(filesDir, fileName);
  // Content-addressed and immutable: identical bytes reuse the same file.
  if (!existsSync(filePath)) writeAtomic(filePath, buffer);

  const project = readProjectFile(root, id);
  const element = {
    id: `el_${randomUUID().slice(0, 8)}`,
    type: "image",
    src: `files/${fileName}`,
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

export function patchElement(root, id, elementId, patch = {}) {
  const project = readProjectFile(root, id);
  const element = findElement(project, elementId);
  for (const field of NUMERIC_ELEMENT_FIELDS) {
    if (patch[field] !== undefined && patch[field] !== null && Number.isFinite(Number(patch[field]))) {
      element[field] = Number(patch[field]);
    }
  }
  if (patch.name !== undefined) element.name = String(patch.name);
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, element };
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
// One append-only journal.jsonl sits next to project.json. Each line is a JSON
// object; the op layer writes mutation entries ({seq, at, op, args_summary,
// undo_patch, state, parent}) and undo/redo markers ({seq, at, op, target_seq}).
// The store only owns the plumbing: read, atomic append, monotonic seq.

function journalPath(root, id) {
  return join(projectDir(root, id), "journal.jsonl");
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

// Append one journal line. `seq` is assigned monotonically (max existing + 1) so
// every physical entry has a unique, ever-increasing id. The append is a single
// O_APPEND write, which is atomic for this single-writer local tool; the whole
// file is never rewritten, so a crash can at worst drop the last partial line
// (tolerated by readJournal).
export function appendJournal(root, id, entry) {
  const dir = projectDir(root, id);
  mkdirSync(dir, { recursive: true });
  const seq = readJournal(root, id).reduce((max, item) => Math.max(max, Number(item.seq) || 0), 0) + 1;
  const line = { seq, at: nowIso(), ...entry };
  appendFileSync(journalPath(root, id), `${JSON.stringify(line)}\n`);
  return line;
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
