// Browser-side rendering for the persisted non-destructive image-filter contract.
// The cache is content-addressed so flipbook frames cannot reuse stale element output.
const filterOffscreenCache = new Map();
const FILTER_OFFSCREEN_CACHE_MAX = 96;

export function clearImageFilterCache() {
  filterOffscreenCache.clear();
}

export function imageFilterCacheSize() {
  return filterOffscreenCache.size;
}

export function cssFilterFor(filters) {
  if (!filters) return "";
  const b = filters.brightness ?? 1;
  const s = filters.saturation ?? 1;
  const c = filters.contrast ?? 1;
  if (b === 1 && s === 1 && c === 1) return "";
  return `brightness(${b}) saturate(${s}) contrast(${c})`;
}

export function tintedOffscreenFor(img, filters) {
  const sig = JSON.stringify(filters);
  const key = `${img.src}\0${sig}`;
  const cached = filterOffscreenCache.get(key);
  if (cached) return cached;
  const offscreen = document.createElement("canvas");
  offscreen.width = Math.max(1, img.naturalWidth || 1);
  offscreen.height = Math.max(1, img.naturalHeight || 1);
  const octx = offscreen.getContext("2d");
  const cssFilter = cssFilterFor(filters);
  if (cssFilter) octx.filter = cssFilter;
  octx.drawImage(img, 0, 0);
  if (cssFilter) octx.filter = "none";
  const tint = filters.tint;
  if (tint && tint.strength > 0) {
    octx.globalCompositeOperation = "source-atop";
    octx.globalAlpha = tint.strength;
    octx.fillStyle = tint.color;
    octx.fillRect(0, 0, offscreen.width, offscreen.height);
    octx.globalCompositeOperation = "source-over";
    octx.globalAlpha = 1;
  }
  if (filterOffscreenCache.size >= FILTER_OFFSCREEN_CACHE_MAX) {
    filterOffscreenCache.delete(filterOffscreenCache.keys().next().value);
  }
  filterOffscreenCache.set(key, offscreen);
  return offscreen;
}
