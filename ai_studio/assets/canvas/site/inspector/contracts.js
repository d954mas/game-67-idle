const ASPECT_RATIO_EPSILON = 0.01;

export function defaultAspectLock(box) {
  const sw = Number(box && box.source_w);
  const sh = Number(box && box.source_h);
  if (!(sw > 0) || !(sh > 0)) return true;
  const w = Number(box && box.w);
  const h = Number(box && box.h);
  if (!(w > 0) || !(h > 0)) return true;
  const sourceRatio = sw / sh;
  return Math.abs(w / h - sourceRatio) <= sourceRatio * ASPECT_RATIO_EPSILON;
}

export function linkedDimension(currentW, currentH, editedKey, newValue) {
  const w = Number(currentW);
  const h = Number(currentH);
  if (!(w > 0) || !(h > 0)) return null;
  const value = Number(newValue);
  if (!Number.isFinite(value) || !(value > 0)) return null;
  if (editedKey === "w") return Math.round(value * (h / w));
  if (editedKey === "h") return Math.round(value * (w / h));
  return null;
}

export function normalizeSmartQuotes(text) {
  return String(text ?? "").replace(/[“”„«»]/g, '"').replace(/[‘’‚]/g, "'");
}

export const PACK_AXES_SKELETON = `{
  "material": ["stone", "wood"],
  "grade": ["rusty", "plain", "gilded"]
}`;

export function parseAxesJson(rawText) {
  const text = normalizeSmartQuotes(rawText);
  let value;
  try { value = JSON.parse(text); }
  catch (error) {
    const lineCol = /line (\d+) column (\d+)/i.exec(error.message);
    if (lineCol) throw new Error(`Invalid JSON at line ${lineCol[1]}, column ${lineCol[2]}: ${error.message}`);
    const positionOnly = /position (\d+)/.exec(error.message);
    if (positionOnly) {
      const offset = Number(positionOnly[1]);
      let line = 1;
      let col = 1;
      for (let i = 0; i < offset && i < text.length; i += 1) {
        if (text[i] === "\n") { line += 1; col = 1; } else col += 1;
      }
      throw new Error(`Invalid JSON at line ${line}, column ${col}: ${error.message}`);
    }
    throw new Error(`Invalid JSON: ${error.message}`);
  }
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error(`Axes must be a JSON object of axisName -> array of values, e.g.:\n${PACK_AXES_SKELETON}`);
  }
  return value;
}

export function estimatePackSheetCount(pack) {
  if (!pack || typeof pack !== "object") return 0;
  const axes = pack.axes && typeof pack.axes === "object" ? pack.axes : {};
  let count = 1;
  for (const [name, values] of Object.entries(axes)) {
    if (name === pack.vary) continue;
    count *= Array.isArray(values) ? values.length : 0;
  }
  return count;
}
