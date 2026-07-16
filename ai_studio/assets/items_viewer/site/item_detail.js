import { field, humanize, make, renderValue } from "./dom.js";

export function formatCost(value) {
  if (!value || typeof value !== "object") return "—";
  if (value.__studio_kind === "free") return "Free";
  if (value.__studio_kind === "cost") return `${value.count} × ${value.item?.id || "?"}`;
  if (value.__studio_kind === "costs" && Array.isArray(value.entries)) {
    return value.entries.map(formatCost).join(" + ");
  }
  return JSON.stringify(value);
}

function section(title, className = "") {
  const root = make("section", `iv-detail-section ${className}`.trim());
  root.append(make("h3", "", title));
  return root;
}

function renderChart(chart, fieldMeta) {
  const root = make("div", "iv-chart");
  if (!chart || chart.content_error) {
    root.append(make("p", "iv-muted", chart?.content_error?.stderr || "Select a numeric series."));
    return root;
  }
  const points = chart.points || [];
  if (!points.length) {
    root.append(make("p", "iv-muted", "No chart points."));
    return root;
  }
  const width = 560;
  const height = 180;
  const inset = 18;
  const values = points.map((point) => point.value);
  const min = Math.min(...values);
  const max = Math.max(...values);
  const levelMin = points[0].level;
  const levelMax = points[points.length - 1].level;
  const x = (level) => inset + ((level - levelMin) / Math.max(1, levelMax - levelMin)) * (width - inset * 2);
  const y = (value) => height - inset - ((value - min) / Math.max(1, max - min)) * (height - inset * 2);
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
  svg.setAttribute("role", "img");
  svg.setAttribute("aria-label", `${fieldMeta?.member || chart.field?.schema?.member || "Value"} by level`);
  const line = document.createElementNS(svg.namespaceURI, "polyline");
  line.setAttribute("points", points.map((point) => `${x(point.level)},${y(point.value)}`).join(" "));
  line.setAttribute("class", "iv-chart-line");
  svg.append(line);
  for (const point of points) {
    const dot = document.createElementNS(svg.namespaceURI, "circle");
    dot.setAttribute("cx", x(point.level));
    dot.setAttribute("cy", y(point.value));
    dot.setAttribute("r", "3.5");
    dot.setAttribute("class", "iv-chart-point");
    svg.append(dot);
  }
  root.append(svg);
  root.append(make("p", "iv-chart-caption", `Levels ${levelMin}–${levelMax} · values ${min}–${max} · ${chart.downsampling?.method || "none"}`));
  return root;
}

function renderLevels(detail) {
  const root = section("Level grid", "iv-levels");
  const levels = detail.item?.item?.levels || [];
  if (!levels.length) {
    root.append(make("p", "iv-muted", "This definition has no level rows."));
    return root;
  }
  const table = make("table", "iv-table iv-level-table");
  const head = make("tr", "");
  head.append(make("th", "", "Level"));
  for (const fieldMeta of detail.fields || []) head.append(make("th", "", fieldMeta.member));
  head.append(make("th", "", "Cost to reach"));
  head.append(make("th", "", "Provenance"));
  const thead = make("thead", "");
  thead.append(head);
  table.append(thead);
  const body = make("tbody", "");
  for (const row of levels) {
    const tr = make("tr", "");
    tr.append(make("td", "iv-level-number", String(row.level)));
    for (const fieldMeta of detail.fields || []) {
      tr.append(make("td", "iv-level-value", String(row.values?.[fieldMeta.member] ?? "—")));
    }
    tr.append(make("td", "iv-level-cost", formatCost(row.values?.cost_to_reach)));
    tr.append(make("td", "iv-level-provenance", [...new Set(Object.values(row.provenance || {}))].join(", ") || "—"));
    body.append(tr);
  }
  table.append(body);
  root.append(table);
  return root;
}

export function renderItemDetail(root, model) {
  root.replaceChildren();
  if (!model || model.loading) {
    root.append(make("div", "iv-detail-empty", model?.message || "Select an item."));
    return;
  }
  if (model.detail?.content_error) {
    root.append(make("div", "iv-detail-empty iv-detail-error", model.detail.content_error.stderr));
    return;
  }

  const { summary, detail } = model;
  const head = make("header", "iv-detail-head");
  const title = make("div", "");
  title.append(make("h2", "", summary.name || summary.id));
  title.append(make("code", "iv-detail-id", summary.id));
  head.append(title);
  head.append(make("span", `iv-chip iv-chip-lock iv-chip-lock-${model.release}`, model.release));
  root.append(head);

  const facts = section("Definition", "iv-detail-facts");
  const factsGrid = make("div", "iv-detail-grid");
  factsGrid.append(field("Kind", summary.kind));
  factsGrid.append(field("Storage", detail.item?.runtime?.storage));
  factsGrid.append(field("Stack", detail.item?.item?.values?.stack));
  factsGrid.append(field("Authoring", detail.item?.item?.values?.authoring_mode));
  factsGrid.append(field("Base value", summary.base_value));
  factsGrid.append(field("Acquire", formatCost(detail.item?.item?.values?.acquire?.cost)));
  facts.append(factsGrid);
  for (const blockName of ["equip", "use", "currency"]) {
    if (!summary[blockName]) continue;
    const block = make("div", "iv-detail-block");
    block.append(make("span", "iv-chip iv-chip-block", humanize(blockName)));
    block.append(renderValue(summary[blockName]));
    facts.append(block);
  }
  root.append(facts);

  root.append(renderLevels(detail));

  const chartSection = section("Selected series", "iv-detail-chart");
  if ((detail.fields || []).length) {
    const select = make("select", "iv-series-select");
    select.setAttribute("aria-label", "Chart series");
    for (const fieldMeta of detail.fields) {
      const option = make("option", "", fieldMeta.member);
      option.value = fieldMeta.member;
      option.selected = fieldMeta.member === model.chartField;
      select.append(option);
    }
    select.addEventListener("change", () => model.onChartField(select.value, { focus: true }));
    chartSection.append(select);
    chartSection.append(renderChart(model.chart, detail.fields.find((entry) => entry.member === model.chartField)));
  } else {
    chartSection.append(make("p", "iv-muted", "No generated numeric level fields for this item."));
  }
  root.append(chartSection);

  const dependencies = section("Dependencies");
  const depGrid = make("div", "iv-detail-grid");
  depGrid.append(field("Inputs", (detail.dependencies?.inputs || []).join(", ") || "—"));
  depGrid.append(field("Dependents", (detail.dependencies?.dependents || []).join(", ") || "—"));
  dependencies.append(depGrid);
  root.append(dependencies);

  const source = section("Source");
  const definition = detail.source?.definition || {};
  const sourceLocation = `${definition.file || "—"}:${definition.line || 1}`;
  const sourceHead = make("div", "iv-source-head");
  sourceHead.append(make("code", "iv-source-path", sourceLocation));
  const copySource = make("button", "iv-source-copy", "Copy location");
  copySource.type = "button";
  copySource.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(sourceLocation);
      copySource.textContent = "Copied";
    } catch {
      copySource.textContent = "Copy failed";
    }
  });
  sourceHead.append(copySource);
  source.append(sourceHead);
  source.append(make("pre", "iv-source-snippet", definition.snippet || ""));
  source.append(field("Source hash", detail.source?.source_hash));
  root.append(source);

  if (model.issues?.length) {
    const diagnostics = section(`Diagnostics (${model.issues.length})`);
    for (const issue of model.issues) {
      diagnostics.append(make("p", `iv-issue iv-issue-${issue.severity}`, `[${issue.rule}] ${issue.msg}`));
    }
    root.append(diagnostics);
  }
}
