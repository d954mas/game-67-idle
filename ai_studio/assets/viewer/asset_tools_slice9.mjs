export function clampInsets(insets = {}, image = {}) {
  const width = positiveInteger(image.width, 1);
  const height = positiveInteger(image.height, 1);
  const [left, right] = clampInsetPair(nonNegativeInteger(insets.left), nonNegativeInteger(insets.right), width);
  const [top, bottom] = clampInsetPair(nonNegativeInteger(insets.top), nonNegativeInteger(insets.bottom), height);
  return { left, right, top, bottom };
}

export function clampNineSliceSize({ targetWidth, targetHeight, insets = {} }) {
  const left = nonNegativeInteger(insets.left);
  const right = nonNegativeInteger(insets.right);
  const top = nonNegativeInteger(insets.top);
  const bottom = nonNegativeInteger(insets.bottom);
  return {
    width: Math.max(positiveInteger(targetWidth, 1), left + right + 1),
    height: Math.max(positiveInteger(targetHeight, 1), top + bottom + 1),
  };
}

export function buildNineSliceDraws({ sourceX = 0, sourceY = 0, sourceWidth, sourceHeight, targetWidth, targetHeight, insets }) {
  const source = {
    x: nonNegativeInteger(sourceX),
    y: nonNegativeInteger(sourceY),
    width: positiveInteger(sourceWidth, 1),
    height: positiveInteger(sourceHeight, 1),
  };
  const safeInsets = clampInsets(insets, source);
  const target = clampNineSliceSize({ targetWidth, targetHeight, insets: safeInsets });
  const sourceXs = [
    source.x,
    source.x + safeInsets.left,
    source.x + source.width - safeInsets.right,
    source.x + source.width,
  ];
  const sourceYs = [
    source.y,
    source.y + safeInsets.top,
    source.y + source.height - safeInsets.bottom,
    source.y + source.height,
  ];
  const targetXs = [0, safeInsets.left, target.width - safeInsets.right, target.width];
  const targetYs = [0, safeInsets.top, target.height - safeInsets.bottom, target.height];
  const columns = ["left", "", "right"];
  const rows = ["top", "", "bottom"];
  const draws = [];

  for (let row = 0; row < 3; row += 1) {
    for (let column = 0; column < 3; column += 1) {
      const sourceRect = rectFromStops(sourceXs, sourceYs, column, row);
      const destinationRect = rectFromStops(targetXs, targetYs, column, row);
      if (!sourceRect.width || !sourceRect.height || !destinationRect.width || !destinationRect.height) continue;
      draws.push({
        key: patchKey(rows[row], columns[column]),
        source: sourceRect,
        destination: destinationRect,
      });
    }
  }

  return draws;
}

function clampInsetPair(near, far, size) {
  const maxTotal = Math.max(0, positiveInteger(size, 1) - 1);
  if (near + far <= maxTotal) return [near, far];
  if (maxTotal === 0) return [0, 0];
  const total = near + far;
  const clampedNear = Math.min(maxTotal, Math.round((near / total) * maxTotal));
  return [clampedNear, maxTotal - clampedNear];
}

function nonNegativeInteger(value) {
  const number = Number(value);
  return Number.isFinite(number) ? Math.max(0, Math.round(number)) : 0;
}

function positiveInteger(value, fallback) {
  const number = Number(value);
  return Number.isFinite(number) && number > 0 ? Math.round(number) : fallback;
}

function rectFromStops(xs, ys, column, row) {
  return {
    x: xs[column],
    y: ys[row],
    width: xs[column + 1] - xs[column],
    height: ys[row + 1] - ys[row],
  };
}

function patchKey(row, column) {
  if (!row && !column) return "center";
  if (!row) return column;
  if (!column) return row;
  return `${row}-${column}`;
}
