function label(value) {
  return String(value || "")
    .replace(/^[^.]+\./, "")
    .replace(/_/g, " ")
    .replace(/\b\w/g, (letter) => letter.toUpperCase());
}

function costEntries(value) {
  if (value?.__studio_kind === "cost") return [value];
  if (value?.__studio_kind === "costs") return value.entries || [];
  return [];
}

export function progressionSeries(track, items) {
  const rows = track.levels?.rows || [];
  const itemsById = new Map(items.map((item) => [item.id, item]));
  const series = [];

  if (track.mode === "threshold") {
    series.push({
      id: "xp",
      kind: "threshold",
      label: "XP",
      values: rows.map((row) => Number.isFinite(row.xp_to_reach) ? row.xp_to_reach : null),
    });
  } else {
    const itemIds = new Set();
    for (const row of rows) {
      for (const entry of costEntries(row.cost_to_reach)) {
        if (entry.item?.id) itemIds.add(entry.item.id);
      }
    }
    for (const itemId of itemIds) {
      series.push({
        id: `cost:${itemId}`,
        kind: "cost",
        label: itemsById.get(itemId)?.name || label(itemId),
        values: rows.map((row) => {
          const entries = costEntries(row.cost_to_reach)
            .filter((entry) => entry.item?.id === itemId);
          return entries.length
            ? entries.reduce((total, entry) => total + entry.count, 0)
            : null;
        }),
      });
    }
  }

  const effectKeys = new Set();
  for (const row of rows) {
    for (const [key, value] of Object.entries(row)) {
      if (key !== "cost_to_reach" && key !== "xp_to_reach" && Number.isFinite(value)) {
        effectKeys.add(key);
      }
    }
  }
  for (const key of effectKeys) {
    series.push({
      id: `effect:${key}`,
      kind: "effect",
      label: label(key),
      values: rows.map((row) => Number.isFinite(row[key]) ? row[key] : null),
    });
  }

  return series.filter((entry) => entry.values.some(Number.isFinite));
}

export function normalizeSeries(values) {
  const finite = values.filter(Number.isFinite);
  const min = Math.min(...finite);
  const max = Math.max(...finite);
  const range = max - min;
  const lastIndex = Math.max(values.length - 1, 1);

  return values.map((value, index) => Number.isFinite(value) ? {
    level: index + 1,
    value,
    x: index / lastIndex,
    y: range ? (value - min) / range : 0.5,
  } : null);
}

export function seriesAtLevel(series, level) {
  const finite = series.values.filter(Number.isFinite);
  const selected = series.values[level - 1];
  return {
    first: finite[0],
    last: finite.at(-1),
    min: Math.min(...finite),
    max: Math.max(...finite),
    value: Number.isFinite(selected) ? selected : null,
  };
}

export function progressionRows(series) {
  return series[0].values.map((_, index) => ({
    level: index + 1,
    values: series.map((entry) => {
      const value = entry.values[index];
      return Number.isFinite(value) ? value : null;
    }),
  }));
}
