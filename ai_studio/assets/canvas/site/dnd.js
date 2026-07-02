// Drag & drop from the OS and clipboard paste. Dragging image files over the
// stage shows a drop highlight; dropping adds each image at the drop point
// (screen -> world), offsetting multiples. Ctrl/Cmd+V pastes a clipboard image at
// the viewport center. Pure input wiring over the shared actions.
import { el, state } from "./app.js";
import { addImageFiles, pasteImageBlob } from "./actions.js";
import { screenToImagePoint } from "./viewport.mjs";

function dropWorldPoint(event) {
  const canvas = el("canvas");
  const rect = canvas.getBoundingClientRect();
  return screenToImagePoint({ x: event.clientX - rect.left, y: event.clientY - rect.top }, state.viewport);
}

function hasFiles(event) {
  return Boolean(event.dataTransfer) && [...event.dataTransfer.types].includes("Files");
}

export function initDnd() {
  const stage = el("stage");
  const overlay = el("drop-overlay");

  stage.addEventListener("dragover", (event) => {
    if (!hasFiles(event) || !state.project) return;
    event.preventDefault();
    event.dataTransfer.dropEffect = "copy";
    overlay.classList.remove("hidden");
  });
  stage.addEventListener("dragleave", (event) => {
    if (event.target === stage || !stage.contains(event.relatedTarget)) overlay.classList.add("hidden");
  });
  stage.addEventListener("drop", (event) => {
    overlay.classList.add("hidden");
    if (!hasFiles(event) || !state.project) return;
    event.preventDefault();
    const files = [...event.dataTransfer.files].filter((file) => file.type.startsWith("image/"));
    if (files.length) addImageFiles(files, dropWorldPoint(event));
  });

  // Clipboard paste (Ctrl/Cmd+V): drop the first pasted image at viewport center.
  window.addEventListener("paste", (event) => {
    if (!state.project) return;
    const items = event.clipboardData && event.clipboardData.items;
    if (!items) return;
    for (const item of items) {
      if (item.kind === "file" && item.type.startsWith("image/")) {
        event.preventDefault();
        pasteImageBlob(item.getAsFile());
        return;
      }
    }
  });
}
