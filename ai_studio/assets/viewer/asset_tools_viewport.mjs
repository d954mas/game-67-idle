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

export function normalizeRect(rect) {
  const x1 = Math.min(rect.x, rect.x + rect.width);
  const y1 = Math.min(rect.y, rect.y + rect.height);
  const x2 = Math.max(rect.x, rect.x + rect.width);
  const y2 = Math.max(rect.y, rect.y + rect.height);
  return {
    x: Math.round(x1),
    y: Math.round(y1),
    width: Math.max(1, Math.round(x2 - x1)),
    height: Math.max(1, Math.round(y2 - y1)),
  };
}

export function clampRect(rect, image) {
  const normalized = normalizeRect(rect);
  const imageWidth = Math.max(1, image.width);
  const imageHeight = Math.max(1, image.height);
  const width = Math.min(normalized.width, imageWidth);
  const height = Math.min(normalized.height, imageHeight);
  return {
    x: clamp(normalized.x, 0, imageWidth - width),
    y: clamp(normalized.y, 0, imageHeight - height),
    width,
    height,
  };
}

export function moveRect(rect, dx, dy, image) {
  return clampRect({ ...rect, x: rect.x + dx, y: rect.y + dy }, image);
}

export function resizeRectFromHandle(rect, handle, point, image) {
  const clampedPoint = {
    x: clamp(point.x, 0, Math.max(1, image.width)),
    y: clamp(point.y, 0, Math.max(1, image.height)),
  };
  const right = rect.x + rect.width;
  const bottom = rect.y + rect.height;
  let x1 = rect.x;
  let y1 = rect.y;
  let x2 = right;
  let y2 = bottom;

  if (handle.includes("w")) x1 = clampedPoint.x;
  if (handle.includes("e")) x2 = clampedPoint.x;
  if (handle.includes("n")) y1 = clampedPoint.y;
  if (handle.includes("s")) y2 = clampedPoint.y;

  return clampRect({
    x: Math.min(x1, x2),
    y: Math.min(y1, y2),
    width: Math.abs(x2 - x1),
    height: Math.abs(y2 - y1),
  }, image);
}

export function handlePoints(rect) {
  const cx = rect.x + rect.width / 2;
  const cy = rect.y + rect.height / 2;
  const right = rect.x + rect.width;
  const bottom = rect.y + rect.height;
  return [
    ["nw", { x: rect.x, y: rect.y }],
    ["n", { x: cx, y: rect.y }],
    ["ne", { x: right, y: rect.y }],
    ["e", { x: right, y: cy }],
    ["se", { x: right, y: bottom }],
    ["s", { x: cx, y: bottom }],
    ["sw", { x: rect.x, y: bottom }],
    ["w", { x: rect.x, y: cy }],
  ];
}

export function hitRectHandle(point, rect, tolerance) {
  for (const [handle, handlePoint] of handlePoints(rect)) {
    if (Math.abs(point.x - handlePoint.x) <= tolerance && Math.abs(point.y - handlePoint.y) <= tolerance) {
      return handle;
    }
  }
  if (point.x >= rect.x && point.y >= rect.y && point.x <= rect.x + rect.width && point.y <= rect.y + rect.height) {
    return "body";
  }
  return "";
}
