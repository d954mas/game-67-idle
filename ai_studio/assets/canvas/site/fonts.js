// Browser font loading + text measurement for canvas text elements. This is the
// page side of the shared fonts.mjs contract: it fetches the SAME fonts.json the
// ops layer reads on disk, registers each static .ttf as a FontFace (so both the 2D
// canvas and the inline-edit textarea render with it), and gates the first text
// paint on document.fonts.ready — no FOUT mismatch between the canvas and the PIL
// export. All pure style/line logic lives in ../fonts.mjs (imported below), so the
// browser and the agent normalize a style identically.
import { canvasFontString, familyNames, splitTextLines, weightsForFamily } from "../fonts.mjs";

let manifest = null;
let ready = false;
let readyPromise = null;
let onReadyCb = null;

// A throwaway 2D context used only for text measurement (never painted). Measuring on
// a dedicated context keeps the main canvas' font/transform untouched.
let measureCtx = null;
function ctx2d() {
  if (!measureCtx) measureCtx = document.createElement("canvas").getContext("2d");
  return measureCtx;
}

export function getFontManifest() {
  return manifest;
}

export function areFontsReady() {
  return ready;
}

export function fontFamilies() {
  return familyNames(manifest);
}

export function fontWeights(family) {
  return weightsForFamily(manifest, family);
}

// Register + load every manifest font as a FontFace, then await document.fonts.ready.
// Resolves once (idempotent). On completion `ready` is true and onReady fires so the
// canvas repaints its text with the real glyphs.
export function loadCanvasFonts(onReady) {
  if (onReady) onReadyCb = onReady;
  if (readyPromise) return readyPromise;
  readyPromise = (async () => {
    try {
      const res = await fetch("fonts/fonts.json", { cache: "no-cache" });
      if (!res.ok) throw new Error(`fonts.json ${res.status}`);
      manifest = await res.json();
    } catch (error) {
      // Without the manifest text can't render; surface it once and leave ready=false.
      console.error("canvas: failed to load fonts manifest —", error && error.message);
      return;
    }
    const loads = [];
    for (const family of manifest.families || []) {
      for (const font of family.fonts || []) {
        try {
          const face = new FontFace(family.family, `url("fonts/${font.file}")`, {
            weight: String(font.weight),
            style: font.style || "normal",
          });
          document.fonts.add(face);
          loads.push(face.load().catch((error) => {
            console.error(`canvas: font ${family.family} ${font.weight} failed to load —`, error && error.message);
          }));
        } catch (error) {
          console.error("canvas: bad FontFace", family.family, font.weight, error && error.message);
        }
      }
    }
    await Promise.all(loads);
    try {
      await document.fonts.ready;
    } catch {
      // document.fonts.ready never rejects in practice; guard anyway.
    }
    ready = true;
    if (onReadyCb) onReadyCb();
  })();
  return readyPromise;
}

// Measure a text element's AUTO-WIDTH box in WORLD (unscaled) pixels: width = the
// widest line, height = lineCount * fontSize * lineHeight. Uses the real font (must be
// loaded); the returned {w,h} is bookkeeping for selection/marquee — both renderers
// re-measure for pixels. Rounded up so the selection box never clips the glyphs.
export function measureTextBox(content, style) {
  const ctx = ctx2d();
  ctx.font = canvasFontString(style, Number(style.fontSize) || 24);
  ctx.textBaseline = "top";
  const lines = splitTextLines(content);
  let maxW = 0;
  for (const line of lines) maxW = Math.max(maxW, ctx.measureText(line).width);
  const w = Math.max(1, Math.ceil(maxW));
  const h = Math.max(1, Math.ceil(lines.length * (Number(style.fontSize) || 24) * (Number(style.lineHeight) || 1.2)));
  return { w, h };
}

// Per-line measured widths (world px) for align offsets, plus the box width. Shared by
// the canvas painter so measurement happens ONE way.
export function measureTextLines(content, style) {
  const ctx = ctx2d();
  ctx.font = canvasFontString(style, Number(style.fontSize) || 24);
  const lines = splitTextLines(content);
  const widths = lines.map((line) => ctx.measureText(line).width);
  const boxW = widths.reduce((max, wv) => Math.max(max, wv), 0);
  return { lines, widths, boxW };
}

// Greedy word-wrap for a NOTE card (T0268). Wraps each explicit-\n paragraph to
// `innerWidth` (world px = the note box minus 2*NOTE_PADDING) using ctx.measureText, the
// SAME way the note painter draws. This is a BROWSER-DISPLAY concern only — a note is a
// canvas annotation that never reaches a PNG, so there is no PIL-parity wrap and no
// nominal-box math anywhere. A single word wider than innerWidth is hard-broken by
// characters so text never spills past the padded box horizontally. Returns an array of
// display lines (always at least one, possibly empty, so an empty note still measures).
export function wrapNoteLines(content, style, innerWidth) {
  const ctx = ctx2d();
  ctx.font = canvasFontString(style, Number(style.fontSize) || 24);
  const limit = Math.max(1, Number(innerWidth) || 1);
  const measure = (text) => ctx.measureText(text).width;

  // Split an over-long token into character-chunks that each fit `limit`; the LAST chunk is
  // returned as the "remainder" for the caller to keep filling (may still take more words).
  const breakToken = (token) => {
    const chunks = [];
    let piece = "";
    for (const ch of token) {
      const candidate = piece + ch;
      if (piece && measure(candidate) > limit) {
        chunks.push(piece);
        piece = ch;
      } else {
        piece = candidate;
      }
    }
    return { full: chunks, remainder: piece };
  };

  const out = [];
  for (const paragraph of splitTextLines(content)) {
    if (paragraph === "") {
      out.push("");
      continue;
    }
    let current = "";
    for (const word of paragraph.split(" ")) {
      const candidate = current ? `${current} ${word}` : word;
      if (measure(candidate) <= limit) {
        current = candidate;
        continue;
      }
      // Candidate overflows: flush the current line first (unless empty), then place the
      // word — hard-breaking it by characters if the word alone is wider than the box.
      if (current) out.push(current);
      if (measure(word) > limit) {
        const { full, remainder } = breakToken(word);
        for (const chunk of full) out.push(chunk);
        current = remainder;
      } else {
        current = word;
      }
    }
    out.push(current);
  }
  return out.length ? out : [""];
}
