// Report rendering, kept apart from the fetch so a fixture can prove the table.

// Poki's retention bar for a session: the average alone hides a long tail, so
// the share of sessions past three minutes is judged next to it.
export const PLAY_BAR_SECONDS = 180;
export const PLAY_BAR_SHARE = 0.25;

function pct(value) {
  return `${(value * 100).toFixed(1)}%`;
}

function cell(value, width) {
  return String(value).padStart(width);
}

// Drop-off belongs to the step between levels: the share of players who started
// this level and never started the next one.
function dropOff(levels, index) {
  const current = levels[index];
  const next = levels[index + 1];
  if (!next || !current.start) return "-";
  return pct(Math.max(1 - next.start / current.start, 0));
}

export function renderReport(report, scope = {}) {
  const lines = [];
  const title = [scope.game || "?", `build=${scope.build || "all"}`, `platform=${scope.platform || "all"}`];
  lines.push(title.join("  "));
  lines.push(
    `sessions ${report.sessions}  players ${report.players}  `
    + `avg play ${Number(report.play_avg).toFixed(1)}s  over ${PLAY_BAR_SECONDS}s ${pct(report.play_over_180_share)}`,
  );

  const buckets = report.play_buckets || {};
  lines.push(
    "play buckets  "
    + ["0-60", "60-180", "180-600", "600+"].map((name) => `${name} ${buckets[name] || 0}`).join("  "),
  );

  const levels = Array.isArray(report.levels) ? report.levels : [];
  lines.push("");
  lines.push(["lvl".padStart(4), cell("start", 8), cell("done", 8), cell("fail", 8), cell("avg s", 8), cell("drop", 8)].join(""));
  for (let index = 0; index < levels.length; index += 1) {
    const level = levels[index];
    lines.push([
      cell(level.level, 4),
      cell(level.start, 8),
      cell(level.complete, 8),
      cell(level.fail, 8),
      cell(Number(level.sec_avg).toFixed(1), 8),
      cell(dropOff(levels, index), 8),
    ].join(""));
  }

  lines.push("");
  lines.push(verdict(report));
  return lines.join("\n");
}

export function verdict(report) {
  const avgOk = Number(report.play_avg) > PLAY_BAR_SECONDS;
  const shareOk = Number(report.play_over_180_share) >= PLAY_BAR_SHARE;
  const mark = avgOk && shareOk ? "PASS" : "BELOW BAR";
  return `${mark}: avg ${Number(report.play_avg).toFixed(1)}s (bar > ${PLAY_BAR_SECONDS}s), `
    + `${pct(report.play_over_180_share)} of sessions over ${PLAY_BAR_SECONDS}s (bar ${pct(PLAY_BAR_SHARE)})`;
}
