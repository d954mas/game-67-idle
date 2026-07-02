export function parseKeyColor(value, fallback = [255, 0, 255]) {
  const match = String(value || "").trim().match(/^#?([0-9a-fA-F]{6})$/);
  if (!match) return fallback;
  const raw = match[1];
  return [
    Number.parseInt(raw.slice(0, 2), 16),
    Number.parseInt(raw.slice(2, 4), 16),
    Number.parseInt(raw.slice(4, 6), 16),
  ];
}

function colorDistance(data, index, keyColor) {
  return Math.max(
    Math.abs(data[index] - keyColor[0]),
    Math.abs(data[index + 1] - keyColor[1]),
    Math.abs(data[index + 2] - keyColor[2]),
  );
}

function isMagentaKey(keyColor) {
  return keyColor[0] > 220 && keyColor[2] > 220 && keyColor[1] < 80;
}

function isExactKeyPixel(data, index, keyColor, tolerance) {
  return data[index + 3] > 0 && colorDistance(data, index, keyColor) <= tolerance;
}

function isMagentaSpillLike(data, index) {
  const red = data[index];
  const green = data[index + 1];
  const blue = data[index + 2];
  return (
    data[index + 3] > 0 &&
    red > 70 &&
    blue > 70 &&
    green < 170 &&
    Math.min(red, blue) - green > 14 &&
    red + blue > green * 2 + 80
  );
}

function isBackgroundCandidate(data, index, keyColor, exactTolerance) {
  if (isExactKeyPixel(data, index, keyColor, exactTolerance)) return true;
  if (!isMagentaKey(keyColor) || !isMagentaSpillLike(data, index)) return false;
  return colorDistance(data, index, keyColor) <= 48;
}

function buildConnectedBackgroundMask(data, width, height, keyColor, exactTolerance) {
  const mask = new Uint8Array(width * height);
  const queue = [];
  const enqueue = (x, y) => {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    const offset = y * width + x;
    if (mask[offset]) return;
    if (!isBackgroundCandidate(data, offset * 4, keyColor, exactTolerance)) return;
    mask[offset] = 1;
    queue.push(offset);
  };

  for (let x = 0; x < width; x += 1) {
    enqueue(x, 0);
    enqueue(x, height - 1);
  }
  for (let y = 1; y < height - 1; y += 1) {
    enqueue(0, y);
    enqueue(width - 1, y);
  }

  for (let cursor = 0; cursor < queue.length; cursor += 1) {
    const offset = queue[cursor];
    const x = offset % width;
    const y = Math.floor(offset / width);
    enqueue(x - 1, y);
    enqueue(x + 1, y);
    enqueue(x, y - 1);
    enqueue(x, y + 1);
  }

  return mask;
}

function hasTransparentNeighbor(data, backgroundMask, width, height, x, y, radius = 2) {
  for (let yy = Math.max(0, y - radius); yy <= Math.min(height - 1, y + radius); yy += 1) {
    for (let xx = Math.max(0, x - radius); xx <= Math.min(width - 1, x + radius); xx += 1) {
      if (xx === x && yy === y) continue;
      const offset = yy * width + xx;
      if (backgroundMask[offset] || data[offset * 4 + 3] === 0) return true;
    }
  }
  return false;
}

function despillMagentaPixel(data, index) {
  const red = data[index];
  const green = data[index + 1];
  const blue = data[index + 2];
  const neutral = (green + Math.min(red, blue)) * 0.5;
  const limit = Math.max(green, neutral);
  const nextRed = Math.min(red, limit);
  const nextBlue = Math.min(blue, limit);
  data[index] = Math.round(nextRed);
  data[index + 2] = Math.round(nextBlue);
  return nextRed !== red || nextBlue !== blue;
}

function pointInPolygon(x, y, polygon) {
  let inside = false;
  for (let current = 0, previous = polygon.length - 1; current < polygon.length; previous = current, current += 1) {
    const [x1, y1] = polygon[current];
    const [x2, y2] = polygon[previous];
    const intersects = y1 > y !== y2 > y && x < ((x2 - x1) * (y - y1)) / (y2 - y1 || 1) + x1;
    if (intersects) inside = !inside;
  }
  return inside;
}

function applyPolygonMask(data, width, height, polygon, rect) {
  if (!Array.isArray(polygon) || polygon.length < 3 || !rect) return 0;
  let masked = 0;
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      if (pointInPolygon(rect.x + x + 0.5, rect.y + y + 0.5, polygon)) continue;
      const index = (y * width + x) * 4;
      if (data[index + 3] > 0) masked += 1;
      data[index] = 0;
      data[index + 1] = 0;
      data[index + 2] = 0;
      data[index + 3] = 0;
    }
  }
  return masked;
}

export function applyPolygonPreviewMask(imageData, options = {}) {
  const polygonMaskedPixels = applyPolygonMask(imageData.data, imageData.width, imageData.height, options.polygon, options.rect);
  return { polygonMaskedPixels };
}

export function applyAlphaPreviewMatte(imageData, options = {}) {
  const data = imageData.data;
  const width = imageData.width;
  const height = imageData.height;
  const keyColor = Array.isArray(options.keyColor) ? options.keyColor : parseKeyColor(options.keyColor);
  const exactTolerance = Number.isFinite(options.exactTolerance) ? options.exactTolerance : 12;
  const foregroundTolerance = Number.isFinite(options.foregroundTolerance) ? options.foregroundTolerance : 88;
  const backgroundMask = buildConnectedBackgroundMask(data, width, height, keyColor, exactTolerance);
  const magentaKey = isMagentaKey(keyColor);
  let transparentPixels = 0;
  let despilledPixels = 0;

  for (let offset = 0; offset < width * height; offset += 1) {
    const index = offset * 4;
    if (backgroundMask[offset] || isExactKeyPixel(data, index, keyColor, exactTolerance)) {
      data[index] = 0;
      data[index + 1] = 0;
      data[index + 2] = 0;
      data[index + 3] = 0;
      transparentPixels += 1;
    }
  }

  if (magentaKey) {
    for (let y = 0; y < height; y += 1) {
      for (let x = 0; x < width; x += 1) {
        const offset = y * width + x;
        const index = offset * 4;
        if (data[index + 3] === 0 || !isMagentaSpillLike(data, index)) continue;
        if (!hasTransparentNeighbor(data, backgroundMask, width, height, x, y)) continue;
        if (despillMagentaPixel(data, index)) despilledPixels += 1;
        const distance = colorDistance(data, index, keyColor);
        if (distance < foregroundTolerance) {
          const alpha = Math.round(Math.max(0, Math.min(1, (distance - exactTolerance) / (foregroundTolerance - exactTolerance))) * 255);
          data[index + 3] = Math.min(data[index + 3], alpha);
        }
      }
    }
  }

  const { polygonMaskedPixels } = applyPolygonPreviewMask(imageData, options);
  return { transparentPixels, despilledPixels, polygonMaskedPixels };
}

function copyPixel(target, source, index) {
  target[index] = source[index];
  target[index + 1] = source[index + 1];
  target[index + 2] = source[index + 2];
  target[index + 3] = source[index + 3];
}

function tintPixel(data, index, tint = [246, 198, 91], amount = 0.42) {
  data[index] = Math.round(data[index] * (1 - amount) + tint[0] * amount);
  data[index + 1] = Math.round(data[index + 1] * (1 - amount) + tint[1] * amount);
  data[index + 2] = Math.round(data[index + 2] * (1 - amount) + tint[2] * amount);
  data[index + 3] = Math.max(data[index + 3], 210);
}

export function applyGenerationAlphaDiagnostic(imageData, options = {}) {
  const width = imageData.width;
  const height = imageData.height;
  const keyColor = Array.isArray(options.keyColor) ? options.keyColor : parseKeyColor(options.keyColor);
  const exactTolerance = Number.isFinite(options.exactTolerance) ? options.exactTolerance : 12;
  const original = new Uint8ClampedArray(imageData.data);
  const matte = {
    width,
    height,
    data: new Uint8ClampedArray(original),
  };
  const stats = applyAlphaPreviewMatte(matte, { ...options, keyColor, exactTolerance });
  let diagnosticPixels = 0;

  for (let offset = 0; offset < width * height; offset += 1) {
    const index = offset * 4;
    copyPixel(imageData.data, matte.data, index);
    const exactKey = isExactKeyPixel(original, index, keyColor, exactTolerance);
    const changedAlpha = matte.data[index + 3] !== original[index + 3];
    const changedRgb =
      matte.data[index] !== original[index] ||
      matte.data[index + 1] !== original[index + 1] ||
      matte.data[index + 2] !== original[index + 2];
    if (exactKey || matte.data[index + 3] === 0 || (!changedAlpha && !changedRgb)) continue;
    tintPixel(imageData.data, index);
    diagnosticPixels += 1;
  }

  return { ...stats, diagnosticPixels };
}
