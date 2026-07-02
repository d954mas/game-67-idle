// Export destination delivery — the page's "where do the files land" layer.
//
// This is delivery-to-disk only (an allowed page concern, like rendering/input):
// the shared export op already produced the bytes in the confined
// <project>/export/<stamp>/ folder and the server serves them back over the
// download route. Here we hand those bytes to the lead's chosen folder.
//
// Behavior (Figma-style, lead-confirmed 2026-07-02):
//   - Export ALWAYS opens the directory picker (showDirectoryPicker), every time.
//   - The picker OPENS AT the last-used folder for this project via `startIn`; the
//     newly picked handle is remembered per project in IndexedDB after each export.
//   - Cancel in the picker cancels the export (the caller shows a status message);
//     there is NO silent fallback on cancel.
//   - The browser-download fallback runs ONLY when the File System Access API is
//     unavailable in this browser (never as a cancel path) so files are never
//     silently dropped.
//
// No node/business logic lives here; it is 100% browser plumbing.

const DB_NAME = "canvas-export";
const STORE = "dirHandles";

export function supportsFsa() {
  return typeof window !== "undefined" && typeof window.showDirectoryPicker === "function";
}

function openDb() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, 1);
    request.onupgradeneeded = () => request.result.createObjectStore(STORE);
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

// FileSystemDirectoryHandle is structured-cloneable, so IndexedDB persists it across
// reloads. Any storage failure is non-fatal: the picker just won't pre-open at the
// last folder.
async function loadHandle(projectId) {
  try {
    const db = await openDb();
    return await new Promise((resolve, reject) => {
      const tx = db.transaction(STORE, "readonly");
      const request = tx.objectStore(STORE).get(projectId);
      request.onsuccess = () => resolve(request.result || null);
      request.onerror = () => reject(request.error);
    });
  } catch {
    return null;
  }
}

async function saveHandle(projectId, handle) {
  try {
    const db = await openDb();
    await new Promise((resolve, reject) => {
      const tx = db.transaction(STORE, "readwrite");
      tx.objectStore(STORE).put(handle, projectId);
      tx.oncomplete = () => resolve();
      tx.onerror = () => reject(tx.error);
    });
  } catch {
    // Non-fatal: the next export simply won't start at the last folder.
  }
}

// The remembered folder name (info only) for the panel's destination hint line.
export async function lastDestinationName(projectId) {
  const handle = await loadHandle(projectId);
  return handle ? handle.name : null;
}

// Open the picker (starting at the last folder) and remember the new choice.
// Rejects with an AbortError when the lead cancels — the caller treats that as
// "cancel the export", not a fallback.
export async function pickDestination(projectId) {
  const options = { mode: "readwrite", id: "canvas-export" };
  const remembered = await loadHandle(projectId);
  if (remembered) options.startIn = remembered;
  const handle = await window.showDirectoryPicker(options);
  await saveHandle(projectId, handle);
  return handle;
}

const downloadRoute = (projectId, stamp, file) =>
  `/api/canvas/projects/${projectId}/export/${encodeURIComponent(stamp)}/${encodeURIComponent(file)}`;

// Write each exported file into the picked directory, fetching the bytes from the
// confined server download route.
export async function writeFilesToDir(dirHandle, projectId, stamp, files) {
  for (const file of files) {
    const response = await fetch(downloadRoute(projectId, stamp, file));
    if (!response.ok) throw new Error(`could not fetch ${file}`);
    const bytes = await response.arrayBuffer();
    const fileHandle = await dirHandle.getFileHandle(file, { create: true });
    const writable = await fileHandle.createWritable();
    await writable.write(bytes);
    await writable.close();
  }
}

// Fallback when the File System Access API is unavailable: trigger a real browser
// download per file (multiple files = one download each). No fake zip.
export function downloadFiles(projectId, stamp, files) {
  for (const file of files) {
    const anchor = document.createElement("a");
    anchor.href = downloadRoute(projectId, stamp, file);
    anchor.download = file;
    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
  }
}
