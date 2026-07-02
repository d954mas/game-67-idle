function cleanRegionOverlayName(value) {
  return String(value || "").trim().slice(0, 80);
}

export function regionOverlayLabel(region, index) {
  return cleanRegionOverlayName(region?.name) || String(index + 1);
}

export function fitRegionOverlayLabel(label, maxWidth, measureText) {
  const text = String(label || "");
  const width = Math.max(0, Number(maxWidth) || 0);
  if (!text || width <= 0) return "";
  if (measureText(text).width <= width) return text;

  const suffix = "...";
  let base = text;
  while (base.length > 0 && measureText(`${base}${suffix}`).width > width) {
    base = base.slice(0, -1);
  }

  if (base) return `${base}${suffix}`;
  return measureText(suffix).width <= width ? suffix : "";
}
