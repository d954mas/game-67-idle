// Ephemeral cleanup-preview state. This domain never mutates the project store;
// callers provide the repaint callback that reflects a state transition.
export function createCleanupPreviewController(repaint) {
  let preview = null;
  let comparing = false;

  function getPreview() {
    return preview;
  }

  function isComparing() {
    return comparing;
  }

  function setPreview(next) {
    preview = next;
    comparing = false;
    repaint();
  }

  function clearPreview() {
    if (!preview) return;
    preview = null;
    comparing = false;
    repaint();
  }

  function setComparing(active) {
    if (!preview || comparing === active) return;
    comparing = active;
    repaint();
  }

  function loadBitmap(base64) {
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = () => reject(new Error("could not decode the cleanup preview image"));
      img.src = `data:image/png;base64,${base64}`;
    });
  }

  return { clearPreview, getPreview, isComparing, loadBitmap, setComparing, setPreview };
}
