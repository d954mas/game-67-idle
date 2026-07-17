// Shared Canvas asset-review state. Node operations, the CLI, DOM layers, and the
// bitmap workspace all consume this module so labels and allowed values cannot drift.

export const ASSET_STATUSES = Object.freeze(["quarantine", "checked", "accepted"]);

const BADGES = Object.freeze({
  quarantine: Object.freeze({ fill: "#d7a14a", text: "#231a08" }),
  checked: Object.freeze({ fill: "#77a7ff", text: "#091a35" }),
  accepted: Object.freeze({ fill: "#65bd81", text: "#0a2413" }),
});

export function normalizeAssetStatus(value) {
  const status = String(value == null ? "" : value).trim();
  if (!ASSET_STATUSES.includes(status)) {
    throw new Error(`asset status must be ${ASSET_STATUSES.join("|")}, got ${JSON.stringify(value)}`);
  }
  return status;
}

export function assetStatusBadge(element) {
  if (!element || element.type !== "image" || !ASSET_STATUSES.includes(element.assetStatus)) return null;
  const status = element.assetStatus;
  return {
    status,
    label: status,
    title: `Asset status: ${status}`,
    ...BADGES[status],
  };
}

// Screen-space chip layout for the bitmap workspace. Full labels stay fixed-size and
// readable; small images compact to a letter, and sub-16px thumbnails rely on the DOM
// layers badge instead of letting canvas chrome overlap neighboring art.
export function assetStatusChipLayout(badge, { width, height, measureText }) {
  if (!badge || width < 16 || height < 16) return null;
  const fullWidth = Math.ceil(measureText(badge.label)) + 10;
  const compact = fullWidth > width;
  const chipWidth = compact ? 16 : fullWidth;
  return {
    label: compact ? badge.label.slice(0, 1).toUpperCase() : badge.label,
    x: width - chipWidth,
    y: 0,
    width: chipWidth,
    height: 16,
  };
}
