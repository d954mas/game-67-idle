import { sampleAnimation } from "../../animation.mjs";

// Owns the view-only preview clock, membership and rAF lifecycle. Project lookup,
// frame decoding and repaint stay at explicit browser-runtime boundaries.
export function createAnimationPreviewController({ getElements, getImageForSrc, repaint }) {
  const previewingElementIds = new Set();
  const elementPreviewStart = new Map();
  let previewClockT0 = 0;
  let previewRafId = 0;

  function isPreviewing(elementId) {
    return previewingElementIds.has(elementId);
  }

  function flipbookOnceFinished(element) {
    const fb = element.flipbook;
    if (!fb || (fb.play_mode || "loop") !== "once") return false;
    if (!Array.isArray(fb.frames)) return true;
    const count = fb.frames.filter((frame) => frame && frame.kept !== false && frame.src).length;
    if (count < 2) return true;
    const fps = Number(fb.fps) > 0 ? Number(fb.fps) : 12;
    const start = elementPreviewStart.get(element.id) ?? previewClockT0;
    const advance = Math.floor(((performance.now() - start) / 1000) * fps);
    return advance >= count;
  }

  function prune() {
    for (const id of previewingElementIds) {
      const element = getElements().find((item) => item.id === id);
      if (!element || (!element.animation && !element.flipbook) || flipbookOnceFinished(element)) {
        previewingElementIds.delete(id);
        elementPreviewStart.delete(id);
      }
    }
  }

  function loop() {
    previewRafId = 0;
    prune();
    if (!previewingElementIds.size) return;
    repaint();
    previewRafId = requestAnimationFrame(loop);
  }

  function start(elementId) {
    if (!elementId || previewingElementIds.has(elementId)) return;
    if (!previewingElementIds.size) previewClockT0 = performance.now();
    elementPreviewStart.set(elementId, performance.now());
    previewingElementIds.add(elementId);
    if (!previewRafId) previewRafId = requestAnimationFrame(loop);
    repaint();
  }

  function stop(elementId) {
    if (!previewingElementIds.delete(elementId)) return;
    elementPreviewStart.delete(elementId);
    if (!previewingElementIds.size && previewRafId) {
      cancelAnimationFrame(previewRafId);
      previewRafId = 0;
    }
    repaint();
  }

  function toggle(elementId) {
    if (previewingElementIds.has(elementId)) stop(elementId);
    else start(elementId);
  }

  function dispose() {
    if (previewRafId) cancelAnimationFrame(previewRafId);
    previewRafId = 0;
    previewingElementIds.clear();
    elementPreviewStart.clear();
  }

  function sampleFor(element) {
    if (!previewingElementIds.has(element.id) || !element.animation) return null;
    return sampleAnimation(element.animation, performance.now() - previewClockT0);
  }

  function flipbookFrameFor(element) {
    if (!previewingElementIds.has(element.id)) return null;
    const fb = element.flipbook;
    if (!fb || !Array.isArray(fb.frames)) return null;
    const kept = fb.frames.filter((frame) => frame && frame.kept !== false && frame.src);
    const count = kept.length;
    if (!count) return null;
    let index = 0;
    if (count >= 2) {
      const fps = Number(fb.fps) > 0 ? Number(fb.fps) : 12;
      const mode = fb.play_mode || "loop";
      if (mode === "once") {
        const start = elementPreviewStart.get(element.id) ?? previewClockT0;
        const advance = Math.floor(((performance.now() - start) / 1000) * fps);
        index = Math.min(advance, count - 1);
      } else if (mode === "pingpong") {
        const advance = Math.floor(((performance.now() - previewClockT0) / 1000) * fps);
        const period = 2 * (count - 1);
        const position = ((advance % period) + period) % period;
        index = position < count ? position : period - position;
      } else {
        const advance = Math.floor(((performance.now() - previewClockT0) / 1000) * fps);
        index = ((advance % count) + count) % count;
      }
    }
    const img = getImageForSrc(kept[index].src);
    return img.complete && img.naturalWidth ? img : null;
  }

  return { dispose, flipbookFrameFor, isPreviewing, sampleFor, start, stop, toggle };
}
