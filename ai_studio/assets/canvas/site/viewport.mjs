// Canvas viewport math: pure pan/zoom/transform helpers shared by the site
// modules (workspace, actions, dnd, regions). Screen<->image point conversion,
// fit-to-frame centering, and zoom-at-cursor that keeps the point under the
// cursor stable. No DOM. Rect/polygon editing geometry is NOT here -- the region
// workbench owns that in regions.js. Moved into the canvas module from the
// retired asset_tools editor so the canvas owns its own viewport code.
const minScale = 0.05;
const maxScale = 12;

export function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

export function fitViewport({ imageWidth, imageHeight, frameWidth, frameHeight, padding = 0 }) {
  if (!imageWidth || !imageHeight || !frameWidth || !frameHeight) {
    return { scale: 1, offsetX: 0, offsetY: 0 };
  }
  const availableWidth = Math.max(1, frameWidth - padding * 2);
  const availableHeight = Math.max(1, frameHeight - padding * 2);
  const scale = clamp(Math.min(availableWidth / imageWidth, availableHeight / imageHeight), minScale, maxScale);
  return {
    scale,
    offsetX: (frameWidth - imageWidth * scale) / 2,
    offsetY: (frameHeight - imageHeight * scale) / 2,
  };
}

export function zoomViewportAt(viewport, factor, screenPoint) {
  const nextScale = clamp(viewport.scale * factor, minScale, maxScale);
  const imagePoint = screenToImagePoint(screenPoint, viewport);
  return {
    scale: nextScale,
    offsetX: screenPoint.x - imagePoint.x * nextScale,
    offsetY: screenPoint.y - imagePoint.y * nextScale,
  };
}

export function imageToScreenPoint(point, viewport) {
  return {
    x: point.x * viewport.scale + viewport.offsetX,
    y: point.y * viewport.scale + viewport.offsetY,
  };
}

export function screenToImagePoint(point, viewport) {
  return {
    x: (point.x - viewport.offsetX) / viewport.scale,
    y: (point.y - viewport.offsetY) / viewport.scale,
  };
}
