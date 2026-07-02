// Export destination delivery — the page's "where does the saved file land" layer.
//
// This is delivery-to-disk only (an allowed page concern, like rendering/input): the
// shared export op already produced the bytes server-side (a single image over the
// download route, or one STORE-mode .zip over the export-zip route). Here we hand a
// Blob to the lead's chosen location via a SAVE-FILE dialog with an editable name.
//
// Behavior (Figma-style, T0229 — replaces the T0206 directory picker that Chrome
// refused for Downloads/системные папки):
//   - showSaveFilePicker with suggestedName = "<element/screen name>.<ext>" (single) or
//     "<project/selection>.zip" (multiple) — the lead can rename in the dialog.
//   - User ABORT (Cancel in the dialog) = a quiet cancel (the caller shows an info toast:
//     "Отмена в диалоге = отмена экспорта"); nothing is written.
//   - Any OTHER picker/write failure is LOUD (thrown → error toast); there is NO silent
//     download fallback on an error.
//   - The plain browser-download fallback runs ONLY when showSaveFilePicker is absent in
//     this browser (never as a cancel/error path), so files are never silently dropped.
//
// No node/business logic lives here; it is 100% browser plumbing.

// True when this browser has the File System Access save-file picker.
export function supportsSaveDialog() {
  return typeof window !== "undefined" && typeof window.showSaveFilePicker === "function";
}

// Trigger a real browser download of a Blob under `name` (the FSA-absent fallback).
function triggerDownload(blob, name) {
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = name;
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  // Give the download a beat to start before releasing the object URL.
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}

// Save a Blob to disk via the save-file dialog. `suggestedName` seeds the (editable)
// file name; `types` is an optional showSaveFilePicker accept-filter array. Returns:
//   { saved: true,  name }           — written (FSA) or downloaded (fallback)
//   { saved: false, canceled: true } — the lead aborted the dialog (quiet cancel)
// Throws LOUDLY on any non-abort failure (no silent fallback).
export async function saveBlobToFile(blob, suggestedName, types) {
  if (!supportsSaveDialog()) {
    // Only fallback path: the browser has no save picker, so download with the name.
    triggerDownload(blob, suggestedName);
    return { saved: true, name: suggestedName, method: "download" };
  }
  let handle;
  try {
    const options = { suggestedName };
    if (Array.isArray(types) && types.length) options.types = types;
    handle = await window.showSaveFilePicker(options);
  } catch (error) {
    if (error && error.name === "AbortError") return { saved: false, canceled: true };
    throw new Error(`could not open the save dialog: ${error.message}`); // LOUD, no fallback
  }
  try {
    const writable = await handle.createWritable();
    await writable.write(blob);
    await writable.close();
  } catch (error) {
    throw new Error(`could not save “${handle.name || suggestedName}”: ${error.message}`);
  }
  return { saved: true, name: handle.name || suggestedName, method: "fsa" };
}
