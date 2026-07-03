// Canvas 9-slice math: the ONE shared, pure, dependency-free contract both clients
// use so a slice9 element renders the SAME on the browser canvas and in the PIL
// export (the fonts.mjs/tree.mjs pattern — imported by ops.mjs in node AND served
// to the site over /ai_studio/, see site/workspace.js). The exact Python twin is
// tools/slice9.py; mirror the clamp formula line-for-line — parity is the contract.
//
// v1 model (T0233 design; lead ask: «слайс9 картинкой, чтобы проверить а работает
// ли слайс9» — corners stay fixed, edges stretch one axis, center stretches both,
// when the element box is resized). `element.slice9 = {left, top, right, bottom,
// scale?}`:
//   - left/top/right/bottom: insets in SOURCE PIXELS (integers >= 0). Absent field
//     on the element = today's single-drawImage/resize behavior everywhere
//     (additive, zero migration). Image elements only (enforced by the setSlice9
//     op, which has element-type context this pure module does not).
//   - scale (T0233 scope addition, lead: «важно чтобы я мог скейлить края, иногда
//     мне нужно больше или меньше»): an optional multiplier > 0 (default 1, capped
//     at MAX_SLICE9_SCALE) applied to the DESTINATION corner/edge band size only —
//     it never touches the SOURCE crop (the pixels sampled from the image are
//     always exactly `left`/`top`/`right`/`bottom` px). scale > 1 paints fatter
//     corners/edges; scale < 1 paints thinner ones. Stored ONLY when != 1 (mirrors
//     the rotation:0/flipH:false "absent = default" convention elsewhere in this
//     schema), so a scale-less slice9 element is byte-identical to pre-scale saves.

// Engine precedent (nt_sprite_renderer.c:697): `sl + sr < source_w && st + sb <
// source_h` — adopted as the set-time loud invariant (validateSlice9 below).
const MAX_SLICE9_SCALE = 16;

// Validate + normalize a slice9 insets object against the element's SOURCE pixel
// size. Throws loudly on: a non-object, a negative/non-integer/non-finite inset, a
// corner pair that would consume (or exceed) the whole source axis, or a `scale`
// outside (0, MAX_SLICE9_SCALE]. Returns a FRESH {left, top, right, bottom[, scale]}
// object (scale present only when != 1) — never shares references with the input.
// Used by the setSlice9 op (the loud gate at SET time, from either client).
export function validateSlice9(insets, sourceW, sourceH) {
  if (!insets || typeof insets !== "object" || Array.isArray(insets)) {
    throw new Error(`slice9 insets must be an object {left,top,right,bottom}, got ${JSON.stringify(insets)}`);
  }
  const srcW = Number(sourceW);
  const srcH = Number(sourceH);
  if (!(srcW > 0) || !(srcH > 0)) {
    throw new Error(`slice9 requires a positive source size, got ${srcW}x${srcH}`);
  }
  const inset = (field) => {
    const raw = insets[field];
    const num = Number(raw);
    if (!Number.isFinite(num) || num < 0 || Math.round(num) !== num) {
      throw new Error(`slice9 ${field} must be a non-negative integer, got ${JSON.stringify(raw)}`);
    }
    return num;
  };
  const left = inset("left");
  const top = inset("top");
  const right = inset("right");
  const bottom = inset("bottom");
  if (left + right >= srcW) {
    throw new Error(`slice9 left+right (${left + right}) must be < source width ${srcW}`);
  }
  if (top + bottom >= srcH) {
    throw new Error(`slice9 top+bottom (${top + bottom}) must be < source height ${srcH}`);
  }
  const out = { left, top, right, bottom };
  if (insets.scale !== undefined && insets.scale !== null) {
    const scale = Number(insets.scale);
    if (!Number.isFinite(scale) || !(scale > 0) || scale > MAX_SLICE9_SCALE) {
      throw new Error(`slice9 scale must be a finite number in (0, ${MAX_SLICE9_SCALE}], got ${JSON.stringify(insets.scale)}`);
    }
    if (scale !== 1) out.scale = scale;
  }
  return out;
}

// Compute the <=9 patches for a slice-9 box:
//   - `sxs`/`sys` are the FIXED SOURCE bands, derived only from srcW/srcH + the raw
//     (un-scaled) insets — `scale` NEVER touches source pixels.
//   - `dxs`/`dys` are the DESTINATION bands: each corner/edge inset is first
//     multiplied by `scale` (default 1), THEN proportionally clamped so corners
//     never overlap when the box is smaller than the (scaled) corner sum on that
//     axis — standard CSS border-image behavior, deterministic, float. Identical
//     clamp shape as scale=1, just operating on the scaled inset.
// Zero-area patches (a squished-to-0 band) are dropped. dst coords are ELEMENT-
// LOCAL world units in [0..dstW] x [0..dstH]; each consumer (canvas ctx / PIL) maps
// them to its own device space. Mirror: tools/slice9.py's slice9_patches — keep the
// formula identical, parity is the contract.
export function slice9Patches(insets, srcW, srcH, dstW, dstH) {
  const left = Number(insets.left) || 0;
  const top = Number(insets.top) || 0;
  const right = Number(insets.right) || 0;
  const bottom = Number(insets.bottom) || 0;
  const rawScale = Number(insets.scale);
  const scale = Number.isFinite(rawScale) && rawScale > 0 ? rawScale : 1;

  const w = Number(srcW);
  const h = Number(srcH);
  const dw0 = Math.max(0, Number(dstW));
  const dh0 = Math.max(0, Number(dstH));

  // Source bands (fixed; middle > 0 is guaranteed by validateSlice9's invariant).
  const sxs = [0, left, w - right, w];
  const sys = [0, top, h - bottom, h];

  // Destination insets: scale first, THEN the proportional clamp.
  let dL = left * scale;
  let dR = right * scale;
  if (dL + dR > dw0 && dL + dR > 0) {
    const f = dw0 / (dL + dR);
    dL *= f;
    dR *= f;
  }
  let dT = top * scale;
  let dB = bottom * scale;
  if (dT + dB > dh0 && dT + dB > 0) {
    const f = dh0 / (dT + dB);
    dT *= f;
    dB *= f;
  }
  const dxs = [0, dL, dw0 - dR, dw0];
  const dys = [0, dT, dh0 - dB, dh0];

  const patches = [];
  for (let c = 0; c < 3; c += 1) {
    for (let r = 0; r < 3; r += 1) {
      const sw = sxs[c + 1] - sxs[c];
      const sh = sys[r + 1] - sys[r];
      const dw = dxs[c + 1] - dxs[c];
      const dh = dys[r + 1] - dys[r];
      if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) continue; // skip an empty band
      patches.push({ sx: sxs[c], sy: sys[r], sw, sh, dx: dxs[c], dy: dys[r], dw, dh });
    }
  }
  return patches;
}
