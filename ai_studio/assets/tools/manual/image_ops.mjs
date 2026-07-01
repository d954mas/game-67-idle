const DEFAULT_PREFIX = "asset";

function asPositiveInt(value, fallback = 1) {
  const number = Number.parseInt(String(value), 10);
  return Number.isFinite(number) && number > 0 ? number : fallback;
}

function asNonNegativeInt(value, fallback = 0) {
  const number = Number.parseInt(String(value), 10);
  return Number.isFinite(number) && number >= 0 ? number : fallback;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function safeAssetBaseName(value, fallback = DEFAULT_PREFIX) {
  const cleaned = String(value || "")
    .trim()
    .replace(/\.[^.]+$/, "")
    .replace(/[^a-zA-Z0-9_-]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .slice(0, 80);
  return cleaned || fallback;
}

function sliceFileName(prefix, rect, extension = "png") {
  const base = safeAssetBaseName(prefix);
  const index = String(rect.index + 1).padStart(3, "0");
  const row = String(rect.row + 1).padStart(2, "0");
  const column = String(rect.column + 1).padStart(2, "0");
  return `${base}_${index}_r${row}_c${column}.${extension.replace(/^\./, "") || "png"}`;
}

function createSliceRects(imageWidth, imageHeight, options = {}) {
  const width = asPositiveInt(imageWidth);
  const height = asPositiveInt(imageHeight);
  const mode = options.mode === "tile" ? "tile" : "grid";
  const offsetX = clamp(asNonNegativeInt(options.offsetX), 0, width - 1);
  const offsetY = clamp(asNonNegativeInt(options.offsetY), 0, height - 1);
  const gapX = asNonNegativeInt(options.gapX);
  const gapY = asNonNegativeInt(options.gapY);
  const rects = [];

  if (mode === "tile") {
    const tileWidth = asPositiveInt(options.tileWidth, width);
    const tileHeight = asPositiveInt(options.tileHeight, height);
    let row = 0;
    for (let y = offsetY; y + tileHeight <= height; y += tileHeight + gapY) {
      let column = 0;
      for (let x = offsetX; x + tileWidth <= width; x += tileWidth + gapX) {
        rects.push({ index: rects.length, row, column, x, y, width: tileWidth, height: tileHeight });
        column += 1;
      }
      row += 1;
    }
    return rects;
  }

  const columns = asPositiveInt(options.columns, 1);
  const rows = asPositiveInt(options.rows, 1);
  const availableWidth = width - offsetX - gapX * Math.max(0, columns - 1);
  const availableHeight = height - offsetY - gapY * Math.max(0, rows - 1);
  const cellWidth = Math.floor(availableWidth / columns);
  const cellHeight = Math.floor(availableHeight / rows);
  if (cellWidth <= 0 || cellHeight <= 0) return [];

  for (let row = 0; row < rows; row += 1) {
    for (let column = 0; column < columns; column += 1) {
      const x = offsetX + column * (cellWidth + gapX);
      const y = offsetY + row * (cellHeight + gapY);
      rects.push({ index: rects.length, row, column, x, y, width: cellWidth, height: cellHeight });
    }
  }
  return rects;
}

function parseHexColor(value, fallback = [255, 0, 255]) {
  const text = String(value || "").trim();
  const match = /^#?([0-9a-fA-F]{6})$/.exec(text);
  if (!match) return fallback.slice();
  const number = Number.parseInt(match[1], 16);
  return [(number >> 16) & 255, (number >> 8) & 255, number & 255];
}

function toHexColor(red, green, blue) {
  return `#${[red, green, blue].map((value) => clamp(Math.round(value), 0, 255).toString(16).padStart(2, "0")).join("")}`;
}

function isKeyPixel(data, index, key, tolerance) {
  return Math.max(
    Math.abs(data[index] - key[0]),
    Math.abs(data[index + 1] - key[1]),
    Math.abs(data[index + 2] - key[2]),
  ) <= tolerance;
}

function borderConnectedKeyMask(data, width, height, key, tolerance) {
  const mask = new Uint8Array(width * height);
  const queue = [];
  const enqueue = (x, y) => {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    const pixel = y * width + x;
    if (mask[pixel]) return;
    const offset = pixel * 4;
    if (!isKeyPixel(data, offset, key, tolerance)) return;
    mask[pixel] = 1;
    queue.push(pixel);
  };

  for (let x = 0; x < width; x += 1) {
    enqueue(x, 0);
    enqueue(x, height - 1);
  }
  for (let y = 1; y < height - 1; y += 1) {
    enqueue(0, y);
    enqueue(width - 1, y);
  }

  for (let head = 0; head < queue.length; head += 1) {
    const pixel = queue[head];
    const x = pixel % width;
    const y = Math.floor(pixel / width);
    enqueue(x + 1, y);
    enqueue(x - 1, y);
    enqueue(x, y + 1);
    enqueue(x, y - 1);
  }

  return mask;
}

function transformPixels(sourceData, width, height, options = {}) {
  const mode = options.mode || "keep";
  const data = new Uint8ClampedArray(sourceData);
  let changedPixels = 0;

  if (mode === "discard") {
    for (let index = 3; index < data.length; index += 4) {
      if (data[index] !== 255) changedPixels += 1;
      data[index] = 255;
    }
    return { data, changedPixels };
  }

  if (mode === "flatten") {
    const background = parseHexColor(options.background || "#000000", [0, 0, 0]);
    for (let index = 0; index < data.length; index += 4) {
      const alpha = data[index + 3] / 255;
      if (data[index + 3] !== 255) changedPixels += 1;
      data[index] = Math.round(data[index] * alpha + background[0] * (1 - alpha));
      data[index + 1] = Math.round(data[index + 1] * alpha + background[1] * (1 - alpha));
      data[index + 2] = Math.round(data[index + 2] * alpha + background[2] * (1 - alpha));
      data[index + 3] = 255;
    }
    return { data, changedPixels };
  }

  if (mode === "threshold") {
    const threshold = clamp(asNonNegativeInt(options.threshold, 128), 0, 255);
    for (let index = 3; index < data.length; index += 4) {
      const nextAlpha = data[index] >= threshold ? 255 : 0;
      if (data[index] !== nextAlpha) changedPixels += 1;
      data[index] = nextAlpha;
    }
    return { data, changedPixels };
  }

  if (mode === "key") {
    const key = parseHexColor(options.keyColor || "#ff00ff");
    const tolerance = clamp(asNonNegativeInt(options.tolerance, 16), 0, 255);
    const mask = borderConnectedKeyMask(data, width, height, key, tolerance);
    for (let pixel = 0; pixel < mask.length; pixel += 1) {
      if (!mask[pixel]) continue;
      const index = pixel * 4;
      if (data[index + 3] !== 0 || data[index] !== 0 || data[index + 1] !== 0 || data[index + 2] !== 0) {
        changedPixels += 1;
      }
      data[index] = 0;
      data[index + 1] = 0;
      data[index + 2] = 0;
      data[index + 3] = 0;
    }
    return { data, changedPixels };
  }

  return { data, changedPixels };
}

function trimTransparentRect(sourceData, width, height, padding = 0) {
  const pad = asNonNegativeInt(padding);
  let minX = width;
  let minY = height;
  let maxX = -1;
  let maxY = -1;
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      const alpha = sourceData[(y * width + x) * 4 + 3];
      if (alpha === 0) continue;
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
  }
  if (maxX < minX || maxY < minY) return { x: 0, y: 0, width: 1, height: 1 };
  const x = Math.max(0, minX - pad);
  const y = Math.max(0, minY - pad);
  const right = Math.min(width, maxX + 1 + pad);
  const bottom = Math.min(height, maxY + 1 + pad);
  return { x, y, width: right - x, height: bottom - y };
}

let crcTable = null;

function getCrcTable() {
  if (crcTable) return crcTable;
  crcTable = new Uint32Array(256);
  for (let n = 0; n < 256; n += 1) {
    let c = n;
    for (let k = 0; k < 8; k += 1) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    crcTable[n] = c >>> 0;
  }
  return crcTable;
}

function crc32(bytes) {
  const table = getCrcTable();
  let crc = 0xffffffff;
  for (const byte of bytes) crc = table[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

function dosDateTime(date = new Date()) {
  const year = Math.max(1980, date.getFullYear());
  const dosTime = (date.getHours() << 11) | (date.getMinutes() << 5) | Math.floor(date.getSeconds() / 2);
  const dosDate = ((year - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate();
  return { dosTime, dosDate };
}

function writeUint16(view, offset, value) {
  view.setUint16(offset, value, true);
}

function writeUint32(view, offset, value) {
  view.setUint32(offset, value >>> 0, true);
}

function uint8(value) {
  if (value instanceof Uint8Array) return value;
  if (value instanceof ArrayBuffer) return new Uint8Array(value);
  return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
}

function buildStoredZip(entries, date = new Date()) {
  const encoder = new TextEncoder();
  const normalized = entries.map((entry) => ({
    name: safeAssetBaseName(entry.name, "file").replace(/_png$/, ".png"),
    nameBytes: encoder.encode(String(entry.name || "file.bin").replaceAll("\\", "/")),
    data: uint8(entry.data),
  }));
  const { dosTime, dosDate } = dosDateTime(date);
  let localSize = 0;
  let centralSize = 0;
  for (const entry of normalized) {
    localSize += 30 + entry.nameBytes.length + entry.data.length;
    centralSize += 46 + entry.nameBytes.length;
  }
  const output = new Uint8Array(localSize + centralSize + 22);
  const view = new DataView(output.buffer);
  const centralRecords = [];
  let offset = 0;

  for (const entry of normalized) {
    const localOffset = offset;
    const checksum = crc32(entry.data);
    writeUint32(view, offset, 0x04034b50); offset += 4;
    writeUint16(view, offset, 20); offset += 2;
    writeUint16(view, offset, 0x0800); offset += 2;
    writeUint16(view, offset, 0); offset += 2;
    writeUint16(view, offset, dosTime); offset += 2;
    writeUint16(view, offset, dosDate); offset += 2;
    writeUint32(view, offset, checksum); offset += 4;
    writeUint32(view, offset, entry.data.length); offset += 4;
    writeUint32(view, offset, entry.data.length); offset += 4;
    writeUint16(view, offset, entry.nameBytes.length); offset += 2;
    writeUint16(view, offset, 0); offset += 2;
    output.set(entry.nameBytes, offset); offset += entry.nameBytes.length;
    output.set(entry.data, offset); offset += entry.data.length;
    centralRecords.push({ entry, checksum, localOffset });
  }

  const centralOffset = offset;
  for (const record of centralRecords) {
    const { entry, checksum, localOffset } = record;
    writeUint32(view, offset, 0x02014b50); offset += 4;
    writeUint16(view, offset, 20); offset += 2;
    writeUint16(view, offset, 20); offset += 2;
    writeUint16(view, offset, 0x0800); offset += 2;
    writeUint16(view, offset, 0); offset += 2;
    writeUint16(view, offset, dosTime); offset += 2;
    writeUint16(view, offset, dosDate); offset += 2;
    writeUint32(view, offset, checksum); offset += 4;
    writeUint32(view, offset, entry.data.length); offset += 4;
    writeUint32(view, offset, entry.data.length); offset += 4;
    writeUint16(view, offset, entry.nameBytes.length); offset += 2;
    writeUint16(view, offset, 0); offset += 2;
    writeUint16(view, offset, 0); offset += 2;
    writeUint16(view, offset, 0); offset += 2;
    writeUint16(view, offset, 0); offset += 2;
    writeUint32(view, offset, 0); offset += 4;
    writeUint32(view, offset, localOffset); offset += 4;
    output.set(entry.nameBytes, offset); offset += entry.nameBytes.length;
  }

  const centralEnd = offset;
  writeUint32(view, offset, 0x06054b50); offset += 4;
  writeUint16(view, offset, 0); offset += 2;
  writeUint16(view, offset, 0); offset += 2;
  writeUint16(view, offset, normalized.length); offset += 2;
  writeUint16(view, offset, normalized.length); offset += 2;
  writeUint32(view, offset, centralEnd - centralOffset); offset += 4;
  writeUint32(view, offset, centralOffset); offset += 4;
  writeUint16(view, offset, 0);
  return output;
}

function cropPlan({ sourceName, width, height, alpha, slice, rects, prefix }) {
  return {
    schema: "ai_studio.manual_asset_prep.v1",
    source: {
      name: sourceName || "",
      width,
      height,
    },
    alpha: alpha || { mode: "keep" },
    slice: slice || { mode: "grid" },
    crops: rects.map((rect) => ({
      id: safeAssetBaseName(sliceFileName(prefix, rect).replace(/\.png$/, "")),
      rect: [rect.x, rect.y, rect.width, rect.height],
      output: sliceFileName(prefix, rect),
    })),
  };
}

export {
  buildStoredZip,
  createSliceRects,
  cropPlan,
  crc32,
  parseHexColor,
  safeAssetBaseName,
  sliceFileName,
  toHexColor,
  transformPixels,
  trimTransparentRect,
};
