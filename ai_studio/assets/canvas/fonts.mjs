// Canvas text fonts: the ONE shared, pure, dependency-free contract both clients
// use so a text element renders the SAME on the browser canvas and in the PIL
// export. Like tree.mjs, this file is imported by ops.mjs in node AND by the site
// statically over the /ai_studio/ route, so it must never touch node OR browser
// globals — it only transforms a manifest object + a style object.
//
// The font FILES + the fonts.json manifest live under site/fonts/ (so the page can
// fetch them relative to canvas.html and build @font-face). ops.mjs reads the same
// manifest from disk and resolves each entry to an absolute .ttf path for
// render_group.py; PIL loads that exact file. Parity = same manifest, same files.
//
// v1 text model (see T0222 design): AUTO-WIDTH ONLY (explicit \n newlines, no
// auto-wrap), solid fill, OUTLINE + HARD offset shadow (blur stored but always 0),
// align left/center/right, unitless lineHeight. Rich text / gradient / wrap / curve
// are out of scope.

// Manifest location, relative to the repo root (node) — the page fetches the sibling
// URL "fonts/fonts.json" relative to canvas.html.
export const FONTS_MANIFEST_REPO_PATH = "ai_studio/assets/canvas/site/fonts/fonts.json";
export const FONTS_DIR_REPO_PATH = "ai_studio/assets/canvas/site/fonts";

// The default text style. content defaults to "Text"; a fresh element merges any
// partial style over this. shadow is null (off) by default; when set it is a hard
// offset {dx,dy,blur,color} whose blur is stored but always rendered as 0 in v1.
export const DEFAULT_TEXT_STYLE = Object.freeze({
  fontFamily: "Inter",
  fontWeight: 400,
  fontStyle: "normal",
  fontSize: 24,
  lineHeight: 1.2,
  align: "left",
  color: "#ffffff",
  stroke: Object.freeze({ width: 2, color: "#000000" }),
  shadow: null,
  autoResize: "width",
});

export const TEXT_ALIGNS = new Set(["left", "center", "right"]);
export const TEXT_FONT_STYLES = new Set(["normal", "italic"]);
const MAX_FONT_SIZE = 2000;

export function defaultTextStyle() {
  return {
    ...DEFAULT_TEXT_STYLE,
    stroke: { ...DEFAULT_TEXT_STYLE.stroke },
    shadow: null,
  };
}

function isHexColor(value) {
  return typeof value === "string" && /^#[0-9a-fA-F]{6}$/.test(value.trim());
}

function normHex(value, field) {
  if (!isHexColor(value)) throw new Error(`${field} must be #rrggbb, got ${JSON.stringify(value)}`);
  return String(value).trim().toLowerCase();
}

function finiteNumber(value, field) {
  const n = Number(value);
  if (!Number.isFinite(n)) throw new Error(`${field} must be a finite number, got ${JSON.stringify(value)}`);
  return n;
}

// The families in the manifest, in manifest order (the picker's family list).
export function familyNames(manifest) {
  return (manifest && Array.isArray(manifest.families) ? manifest.families : []).map((f) => f.family);
}

function findFamily(manifest, family) {
  const families = manifest && Array.isArray(manifest.families) ? manifest.families : [];
  return families.find((f) => f.family === family) || null;
}

// The available weights for a family (sorted ascending) — the weight picker list.
export function weightsForFamily(manifest, family) {
  const entry = findFamily(manifest, family);
  if (!entry) return [];
  return [...new Set(entry.fonts.filter((f) => (f.style || "normal") === "normal").map((f) => Number(f.weight)))].sort(
    (a, b) => a - b,
  );
}

// Resolve a (family, weight, style) triple to its manifest font entry (with the
// relative .ttf `file`). A loud error names what is missing — this is the single
// gate that makes "unknown fontFamily/weight vs fonts.json" a clear failure in BOTH
// clients and at render time.
export function resolveFontEntry(manifest, { family, weight, style } = {}) {
  const fam = findFamily(manifest, family);
  if (!fam) {
    throw new Error(`unknown font family ${JSON.stringify(family)} (available: ${familyNames(manifest).join(", ") || "none"})`);
  }
  const wantStyle = style === "italic" ? "italic" : "normal";
  const wantWeight = Number(weight);
  const entry = fam.fonts.find((f) => Number(f.weight) === wantWeight && (f.style || "normal") === wantStyle);
  if (!entry) {
    const have = fam.fonts.map((f) => `${f.weight}/${f.style || "normal"}`).join(", ");
    throw new Error(`font ${JSON.stringify(family)} has no ${wantWeight}/${wantStyle} instance (available: ${have})`);
  }
  return entry;
}

// Validate + normalize a stroke sub-object over a base stroke (partial patches keep
// the untouched field). Returns { width, color }; width is clamped to >= 0.
function mergeStroke(base, patch) {
  const out = { width: Number(base.width) || 0, color: base.color || "#000000" };
  if (patch === undefined) return out;
  if (patch === null || typeof patch !== "object") throw new Error(`stroke must be an object {width,color}, got ${JSON.stringify(patch)}`);
  if (patch.width !== undefined) {
    const w = finiteNumber(patch.width, "stroke.width");
    if (w < 0) throw new Error(`stroke.width must be >= 0, got ${w}`);
    out.width = w;
  }
  if (patch.color !== undefined) out.color = normHex(patch.color, "stroke.color");
  return out;
}

// Validate + normalize the optional shadow: null (off) or {dx,dy,blur,color}. blur is
// stored (default 0) but v1 rendering always treats it as a HARD offset (blur 0).
function mergeShadow(base, patch) {
  if (patch === undefined) {
    if (base == null) return null;
    return { dx: Number(base.dx) || 0, dy: Number(base.dy) || 0, blur: Number(base.blur) || 0, color: base.color || "#000000" };
  }
  if (patch === null) return null;
  if (typeof patch !== "object") throw new Error(`shadow must be null or {dx,dy,blur,color}, got ${JSON.stringify(patch)}`);
  const src = base && typeof base === "object" ? base : {};
  const out = {
    dx: Number(src.dx) || 0,
    dy: Number(src.dy) || 0,
    blur: Number(src.blur) || 0,
    color: src.color || "#000000",
  };
  if (patch.dx !== undefined) out.dx = finiteNumber(patch.dx, "shadow.dx");
  if (patch.dy !== undefined) out.dy = finiteNumber(patch.dy, "shadow.dy");
  if (patch.blur !== undefined) {
    const b = finiteNumber(patch.blur, "shadow.blur");
    if (b < 0) throw new Error(`shadow.blur must be >= 0, got ${b}`);
    out.blur = b;
  }
  if (patch.color !== undefined) out.color = normHex(patch.color, "shadow.color");
  return out;
}

// Merge a (possibly partial) style patch over a base style and validate the RESULT
// against the manifest — the one normalizer both addText (base = defaults) and
// patchElement (base = the element's current style) call. Throws loudly on an unknown
// family/weight, a bad align/color, a non-finite size, or a malformed stroke/shadow.
// Returns a fresh, fully-populated normalized style object (never shares references).
export function mergeTextStyle(base, patch = {}, manifest) {
  if (patch === null || typeof patch !== "object" || Array.isArray(patch)) {
    throw new Error(`text style must be an object, got ${JSON.stringify(patch)}`);
  }
  const src = base && typeof base === "object" ? base : DEFAULT_TEXT_STYLE;
  const out = {
    fontFamily: patch.fontFamily !== undefined ? String(patch.fontFamily) : String(src.fontFamily || DEFAULT_TEXT_STYLE.fontFamily),
    fontWeight: Number(patch.fontWeight !== undefined ? patch.fontWeight : src.fontWeight ?? DEFAULT_TEXT_STYLE.fontWeight),
    fontStyle: patch.fontStyle !== undefined ? String(patch.fontStyle) : String(src.fontStyle || "normal"),
    fontSize: finiteNumber(patch.fontSize !== undefined ? patch.fontSize : src.fontSize ?? DEFAULT_TEXT_STYLE.fontSize, "fontSize"),
    lineHeight: finiteNumber(patch.lineHeight !== undefined ? patch.lineHeight : src.lineHeight ?? DEFAULT_TEXT_STYLE.lineHeight, "lineHeight"),
    align: patch.align !== undefined ? String(patch.align) : String(src.align || "left"),
    color: normHex(patch.color !== undefined ? patch.color : src.color ?? DEFAULT_TEXT_STYLE.color, "color"),
    stroke: mergeStroke(src.stroke || DEFAULT_TEXT_STYLE.stroke, patch.stroke),
    shadow: mergeShadow(src.shadow ?? null, patch.shadow),
    autoResize: "width",
  };
  if (!TEXT_FONT_STYLES.has(out.fontStyle)) throw new Error(`fontStyle must be normal|italic, got ${JSON.stringify(out.fontStyle)}`);
  if (!TEXT_ALIGNS.has(out.align)) throw new Error(`align must be left|center|right, got ${JSON.stringify(out.align)}`);
  if (!(out.fontSize > 0) || out.fontSize > MAX_FONT_SIZE) throw new Error(`fontSize must be in (0, ${MAX_FONT_SIZE}], got ${out.fontSize}`);
  if (!(out.lineHeight > 0)) throw new Error(`lineHeight must be > 0, got ${out.lineHeight}`);
  // The single gate: the (family, weight, style) must exist in the manifest.
  resolveFontEntry(manifest, { family: out.fontFamily, weight: out.fontWeight, style: out.fontStyle });
  return out;
}

// Split content into rendered lines on explicit newlines only (no auto-wrap in v1).
// Always at least one (empty) line so an empty text element still has a measurable box.
export function splitTextLines(content) {
  const text = content == null ? "" : String(content);
  const lines = text.split("\n");
  return lines.length ? lines : [""];
}

// The first non-empty content line (trimmed) — the default layer name for a text
// element. Falls back to "Text" for empty content.
export function firstTextLine(content) {
  for (const line of splitTextLines(content)) {
    const trimmed = line.trim();
    if (trimmed) return trimmed;
  }
  return "";
}

// The canvas 2D `ctx.font` string for a style at a given rendered pixel size (the
// page passes size * viewport.scale; PIL uses round(size * scale) separately). Kept
// here so the font string is built ONE way.
export function canvasFontString(style, pixelSize) {
  const size = pixelSize == null ? Number(style.fontSize) || 24 : pixelSize;
  const italic = style.fontStyle === "italic" ? "italic " : "";
  const weight = Number(style.fontWeight) || 400;
  return `${italic}${weight} ${size}px "${style.fontFamily}", sans-serif`;
}

// A ROUGH offline w/h estimate for a text element, used only when a box cannot be
// measured with a real font (the CLI / server-side addText). The page re-measures
// precisely on open and the renderer re-measures at export, so this is bookkeeping
// only (selection/marquee) and never load-bearing for pixels.
export function nominalTextBox(content, style) {
  const size = Number(style.fontSize) || 24;
  const lineHeight = Number(style.lineHeight) || 1.2;
  const lines = splitTextLines(content);
  const maxChars = lines.reduce((max, line) => Math.max(max, line.length), 0);
  const w = Math.max(1, Math.round(maxChars * size * 0.6));
  const h = Math.max(1, Math.round(lines.length * size * lineHeight));
  return { w, h };
}

// ---- note cards (T0268) ------------------------------------------------------
//
// A NOTE element (`type:"note"`) is a Miro/FigJam sticky card: plain text on a colored
// box. It reuses this shared font contract for its style (a SUBSET of the text style —
// no stroke/shadow/autoResize/italic), but differs from text in three deliberate ways:
// the box is FULLY user-fixed (both w AND h are set/resized, never auto), the text is
// WORD-WRAPPED to the box (browser-side greedy wrap in site/fonts.js — notes never reach
// a PNG so there is NO PIL-parity wrap and NO nominal-box math here), and overflow CLIPS
// at the padded box. A note is a work annotation, NOT render content: renderGroup /
// exportProject skip it and exportElements refuses it (see ops.mjs).

// The padded inset (world px) between the note box edge and its wrapped text, on every
// side. Shared so the page's paint/edit/wrap and the inspector agree on ONE inner width.
export const NOTE_PADDING = 12;

// A fresh note's default box (world px). The op passes this when no explicit w/h is
// given; both are user-resizable afterward (fully fixed box, T0268 lead decision).
export const DEFAULT_NOTE_SIZE = Object.freeze({ w: 220, h: 180 });

// The default note style: the font subset only (no stroke/shadow/autoResize — those are
// text-only). A fresh note merges any partial style over this; content defaults to "".
export const DEFAULT_NOTE_STYLE = Object.freeze({
  fontFamily: "Inter",
  fontWeight: 400,
  fontSize: 18,
  lineHeight: 1.35,
  align: "left",
  color: "#1a1a1a",
});

// Sticky-note background presets (the inspector's swatch row). Arbitrary #rrggbb is also
// allowed via the custom color input; the stored shape is {type:"color", color} either way.
export const NOTE_BACKGROUND_PRESETS = Object.freeze([
  Object.freeze({ name: "Yellow", color: "#fff9b1" }),
  Object.freeze({ name: "Green", color: "#d5f6b1" }),
  Object.freeze({ name: "Pink", color: "#ffd6e0" }),
  Object.freeze({ name: "Blue", color: "#cfe6ff" }),
  Object.freeze({ name: "Gray", color: "#e6e6e6" }),
]);

// The default fill a fresh note is created with (the first preset).
export const DEFAULT_NOTE_BACKGROUND = Object.freeze({ type: "color", color: "#fff9b1" });

export function defaultNoteStyle() {
  return { ...DEFAULT_NOTE_STYLE };
}

// Merge a (possibly partial) note style patch over a base and validate the RESULT against
// the manifest — the note twin of mergeTextStyle, but for the font SUBSET only (no
// stroke/shadow/italic). Both addNote (base = defaults) and patchElement (base = the
// note's current style) call it. Throws loudly on an unknown family/weight, a bad
// align/color, or a non-finite size. Returns a fresh normalized style object.
export function mergeNoteStyle(base, patch = {}, manifest) {
  if (patch === null || typeof patch !== "object" || Array.isArray(patch)) {
    throw new Error(`note style must be an object, got ${JSON.stringify(patch)}`);
  }
  const src = base && typeof base === "object" ? base : DEFAULT_NOTE_STYLE;
  const out = {
    fontFamily: patch.fontFamily !== undefined ? String(patch.fontFamily) : String(src.fontFamily || DEFAULT_NOTE_STYLE.fontFamily),
    fontWeight: Number(patch.fontWeight !== undefined ? patch.fontWeight : src.fontWeight ?? DEFAULT_NOTE_STYLE.fontWeight),
    fontSize: finiteNumber(patch.fontSize !== undefined ? patch.fontSize : src.fontSize ?? DEFAULT_NOTE_STYLE.fontSize, "fontSize"),
    lineHeight: finiteNumber(patch.lineHeight !== undefined ? patch.lineHeight : src.lineHeight ?? DEFAULT_NOTE_STYLE.lineHeight, "lineHeight"),
    align: patch.align !== undefined ? String(patch.align) : String(src.align || "left"),
    color: normHex(patch.color !== undefined ? patch.color : src.color ?? DEFAULT_NOTE_STYLE.color, "color"),
  };
  if (!TEXT_ALIGNS.has(out.align)) throw new Error(`align must be left|center|right, got ${JSON.stringify(out.align)}`);
  if (!(out.fontSize > 0) || out.fontSize > MAX_FONT_SIZE) throw new Error(`fontSize must be in (0, ${MAX_FONT_SIZE}], got ${out.fontSize}`);
  if (!(out.lineHeight > 0)) throw new Error(`lineHeight must be > 0, got ${out.lineHeight}`);
  // The single gate: the (family, weight, normal) must exist in the manifest.
  resolveFontEntry(manifest, { family: out.fontFamily, weight: out.fontWeight, style: "normal" });
  return out;
}
