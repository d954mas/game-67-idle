const MAX_EXPORT_DIM = 16384;

export function parseScaleSpec(token) {
  const text = String(token == null ? "" : token).trim().toLowerCase();
  if (!text) throw new Error("export scale is required (e.g. 1x, 2x, 512w, 512h)");
  const mul = /^(\d+(?:\.\d+)?)x?$/.exec(text);
  if (mul) {
    const value = Number(mul[1]);
    if (!(value > 0)) throw new Error(`export scale must be > 0: ${JSON.stringify(token)}`);
    return { kind: "mul", value, token: `${mul[1]}x` };
  }
  const dim = /^(\d+(?:\.\d+)?)(w|h)$/.exec(text);
  if (dim) {
    const value = Number(dim[1]);
    if (!(value > 0)) throw new Error(`export scale pixels must be > 0: ${JSON.stringify(token)}`);
    return { kind: dim[2], value, token: text };
  }
  throw new Error(`invalid export scale ${JSON.stringify(token)} (use 0.5x/1x/2x, or 512w/512h)`);
}

export function resolveExportScale(token, srcW, srcH) {
  const spec = parseScaleSpec(token);
  const w0 = Math.max(1, Number(srcW) || 0);
  const h0 = Math.max(1, Number(srcH) || 0);
  let width;
  let height;
  if (spec.kind === "mul") {
    width = Math.max(1, Math.round(w0 * spec.value));
    height = Math.max(1, Math.round(h0 * spec.value));
  } else if (spec.kind === "w") {
    width = Math.max(1, Math.round(spec.value));
    height = Math.max(1, Math.round(h0 * (spec.value / w0)));
  } else {
    height = Math.max(1, Math.round(spec.value));
    width = Math.max(1, Math.round(w0 * (spec.value / h0)));
  }
  if (width > MAX_EXPORT_DIM || height > MAX_EXPORT_DIM) {
    throw new Error(`export scale ${JSON.stringify(token)} exceeds ${MAX_EXPORT_DIM}px (${width}x${height})`);
  }
  return { width, height };
}
