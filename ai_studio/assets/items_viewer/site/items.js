// Items Workbench page. Bare ESM, no framework or build step.
import { field, make } from "./dom.js";
import { renderItemDetail } from "./item_detail.js";

const state = {
  catalogs: [],
  selectedId: null,
  selectedItemId: null,
  view: null,
  detail: null,
  chart: null,
  chartField: null,
  detailRequest: 0,
  issuesById: new Map(),
  editDraft: null,
  preview: null,
  previewEdit: null,
  editBusy: false,
  editMessage: "",
  editError: false,
  undoStack: [],
  // T0316: the view.icons.page_data_uri, decoded ONCE per loadCatalog (before
  // render()) so every row's canvas crop reads from the same already-decoded
  // <img> instead of each row re-decoding the same base64 PNG.
  iconsImage: null,
};

const els = {
  select: document.getElementById("catalogSelect"),
  status: document.getElementById("statusLine"),
  topBanner: document.getElementById("topBanner"),
  summaryPanel: document.getElementById("summaryPanel"),
  removedSection: document.getElementById("removedSection"),
  itemList: document.getElementById("itemList"),
  itemDetail: document.getElementById("itemDetail"),
};

function setStatus(text, isError = false) {
  els.status.textContent = text;
  els.status.classList.toggle("is-error", Boolean(isError));
}

// Icons: view.icons.regions maps an item icon id ("icons/gold")
// straight to a pixel rect on the ALREADY-DECODED atlas debug-PNG page (decoded
// once in loadCatalog, before render() — see state.iconsImage). No gallery
// search and no icon-link write layer — this reads exactly what the
// engine packed.
function resolveIcon(view, assetId) {
  return view.icons && view.icons.regions ? view.icons.regions[assetId] : undefined;
}

function renderIconSlot(item, view) {
  const box = make("div", "iv-icon-slot");
  const region = resolveIcon(view, item.icon);
  if (region && state.iconsImage) {
    const canvas = document.createElement("canvas");
    canvas.width = region.w;
    canvas.height = region.h;
    const ctx = canvas.getContext("2d");
    // Straight alpha: the debug-PNG page is copied BEFORE the builder's
    // premultiply step (nt_builder_atlas.c vs nt_builder_texture.c) — a bare
    // drawImage crop is correct; dividing RGB by alpha here would burn edges
    // that were never premultiplied in the first place (spec §5/§8).
    ctx.drawImage(state.iconsImage, region.x, region.y, region.w, region.h, 0, 0, region.w, region.h);
    box.append(canvas);
    return box;
  }
  box.classList.add("iv-icon-missing");
  box.append(make("span", "iv-icon-glyph", "?"));
  box.append(make("span", "iv-icon-caption", item.icon || "no icon"));
  if (view.icons && view.icons.reason) box.title = view.icons.reason;
  return box;
}


// Tag validate.errors/warnings with a severity so the same list can be filtered once
// for item/summary/removed placement.
function taggedIssues(view) {
  const errors = (view.validate.errors || []).map((issue) => ({ ...issue, severity: "error" }));
  const warnings = (view.validate.warnings || []).map((issue) => ({ ...issue, severity: "warning" }));
  return [...errors, ...warnings];
}

// Issue routing rule (spec §4, load-bearing — mirrors ops.mjs's routeIssues(), which
// this module cannot import: it pulls in node:child_process/node:fs and there is no
// build step to strip that for a browser bundle). An issue attaches to an item IFF its
// `id` is present in items[]; a catalog-level rule (id:null) goes to the Summary
// panel; a rule keyed on a deleted id has no current item and goes to the
// Removed/lock section.
function renderTopBanner(view) {
  els.topBanner.replaceChildren();
  els.topBanner.classList.add("is-hidden");
  els.topBanner.classList.remove("is-error");
  if (view.content_error) {
    els.topBanner.classList.remove("is-hidden");
    els.topBanner.classList.add("is-error");
    els.topBanner.textContent = `Catalog broken (${view.content_error.source}): ${view.content_error.stderr}`;
    return;
  }
  if (!view.meta.hasItems) {
    els.topBanner.classList.remove("is-hidden");
    els.topBanner.textContent = "Items not connected for this game.";
  }
}

function badgeClass(validate) {
  if (!validate.available) return "iv-badge-unknown";
  return validate.ok ? "iv-badge-ok" : "iv-badge-fail";
}

function badgeText(validate) {
  if (!validate.available) return `validate unavailable${validate.reason ? `: ${validate.reason}` : ""}`;
  return validate.ok ? "validate OK" : "validate FAILED";
}

function renderSummary(view, catalogIssues) {
  const root = els.summaryPanel;
  root.replaceChildren();

  const head = make("div", "iv-summary-head");
  head.append(make("h2", "", "Summary"));
  head.append(make("span", `iv-badge ${badgeClass(view.validate)}`, badgeText(view.validate)));
  root.append(head);

  const meta = make("div", "iv-summary-meta");
  meta.append(field("Namespace", view.namespace));
  meta.append(field("Errors", String(view.validate.errors.length)));
  meta.append(field("Warnings", String(view.validate.warnings.length)));
  root.append(meta);

  if (catalogIssues.length) {
    const box = make("div", "iv-catalog-issues");
    box.append(make("h3", "", `Catalog-level issues (${catalogIssues.length})`));
    for (const issue of catalogIssues) {
      box.append(make("p", `iv-issue iv-issue-${issue.severity}`, `[${issue.rule}] ${issue.msg}`));
    }
    root.append(box);
  }
}

function renderRemovedSection(view, homelessIssues) {
  const root = els.removedSection;
  root.replaceChildren();

  const removedIds = new Set(Object.keys(view.lock.removed || {}));
  for (const issue of homelessIssues) removedIds.add(issue.id);

  if (removedIds.size === 0) {
    root.classList.add("is-hidden");
    return;
  }
  root.classList.remove("is-hidden");
  root.append(make("h2", "", `Removed / lock issues (${removedIds.size})`));

  const list = make("div", "iv-removed-list");
  for (const id of [...removedIds].sort()) {
    const receipt = view.lock.removed[id];
    const row = make("article", "iv-removed-row");
    row.append(make("span", "iv-removed-id", id));
    if (receipt) {
      row.append(make("span", "iv-removed-version", `fragment_version ${receipt.fragment_version}`));
      if (receipt.note) row.append(make("p", "iv-removed-note", receipt.note));
    } else {
      row.append(make("span", "iv-removed-version iv-removed-noreceipt", "no removal receipt yet"));
    }
    for (const issue of homelessIssues.filter((entry) => entry.id === id)) {
      row.append(make("p", `iv-issue iv-issue-${issue.severity}`, `[${issue.rule}] ${issue.msg}`));
    }
    list.append(row);
  }
  root.append(list);
}

function renderItemTable(view, issuesById) {
  const root = els.itemList;
  root.replaceChildren();
  if (!view.items.length) {
    root.append(make("div", "iv-empty", view.meta.hasItems ? "No items in this catalog." : "Items not connected for this game."));
    return;
  }
  const table = make("table", "iv-table iv-master-table");
  const thead = make("thead", "");
  const heading = make("tr", "");
  for (const label of ["Item", "Kind", "Storage", "Release", "Issues"]) {
    heading.append(make("th", "", label));
  }
  thead.append(heading);
  table.append(thead);
  const body = make("tbody", "");
  for (const item of view.items) {
    const row = make("tr", item.id === state.selectedItemId ? "is-selected" : "");
    const itemCell = make("td", "");
    const button = make("button", "iv-item-button");
    button.type = "button";
    button.dataset.itemId = item.id;
    button.setAttribute("aria-label", `Inspect ${item.name || item.id} (${item.id})`);
    button.setAttribute("aria-current", item.id === state.selectedItemId ? "true" : "false");
    button.append(renderIconSlot(item, view));
    const title = make("span", "iv-item-title");
    title.append(make("strong", "", item.name || item.id));
    title.append(make("code", "", item.id));
    button.append(title);
    button.addEventListener("click", () => selectItem(item.id, { focus: true }));
    itemCell.append(button);
    row.append(itemCell);
    row.append(make("td", "", item.kind || "—"));
    row.append(make("td", "", item.runtime?.storage || "—"));
    const release = view.lock.status_by_id[item.id] || "draft";
    const releaseCell = make("td", "");
    releaseCell.append(make("span", `iv-chip iv-chip-lock iv-chip-lock-${release}`, release));
    row.append(releaseCell);
    row.append(make("td", "iv-diagnostic-count", String((issuesById.get(item.id) || []).length)));
    body.append(row);
  }
  table.append(body);
  root.append(table);
}

function detailModel(extra = {}) {
  const summary = state.view?.items.find((item) => item.id === state.selectedItemId);
  return {
    summary,
    detail: state.detail,
    chart: state.chart,
    chartField: state.chartField,
    release: state.view?.lock.status_by_id[state.selectedItemId] || "draft",
    issues: state.issuesById.get(state.selectedItemId) || [],
    onChartField: loadSelectedChart,
    editor: {
      draft: state.editDraft,
      preview: state.preview,
      busy: state.editBusy,
      message: state.editMessage,
      error: state.editError,
      undoCount: state.undoStack.length,
      onPreview: previewEdit,
      onApply: applyPreview,
      onUndo: undoLastEdit,
      onInputError: showEditInputError,
    },
    ...extra,
  };
}

function focusedUrl(endpoint, values) {
  const params = new URLSearchParams({ catalog: state.selectedId, ...values });
  return `/api/items-viewer/${endpoint}?${params}`;
}

function resetEditView() {
  state.editDraft = null;
  state.preview = null;
  state.previewEdit = null;
  state.editBusy = false;
  state.editMessage = "";
  state.editError = false;
}

function renderCurrentDetail() {
  renderItemDetail(els.itemDetail, detailModel());
}

function editFailureMessage(payload) {
  const error = payload?.error;
  if (typeof error === "string") return error;
  if (error?.message) return `${error.code ? `${error.code}: ` : ""}${error.message}`;
  return "Semantic edit failed.";
}

async function submitSemanticEdit(edit, apply) {
  const response = await fetch("/api/items-viewer/edit", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ catalog: state.selectedId, edit, apply }),
  });
  const payload = await response.json();
  return payload.ok ? payload : { ...payload, ok: false };
}

function showEditInputError(message) {
  state.editMessage = message;
  state.editError = true;
  renderCurrentDetail();
  els.itemDetail.querySelector(".iv-action-button")?.focus();
}

async function previewEdit(edit) {
  state.editDraft = edit;
  state.previewEdit = edit;
  state.preview = null;
  state.editBusy = true;
  state.editMessage = "Evaluating ephemeral preview…";
  state.editError = false;
  renderCurrentDetail();
  try {
    const payload = await submitSemanticEdit(edit, false);
    state.editBusy = false;
    if (!payload.ok) {
      state.editMessage = editFailureMessage(payload);
      state.editError = true;
    } else {
      state.preview = payload.result;
      state.editMessage = "Preview validated. Review the source and semantic diff before Apply.";
      state.editError = false;
    }
  } catch (error) {
    state.editBusy = false;
    state.editMessage = `Preview request failed: ${error.message}`;
    state.editError = true;
  }
  renderCurrentDetail();
  els.itemDetail.querySelector(state.preview ? ".iv-action-primary" : ".iv-action-button")?.focus();
}

async function applyPreview() {
  const edit = state.previewEdit;
  if (!edit || !state.preview) return;
  state.editBusy = true;
  state.editMessage = "Applying reviewed semantic patch…";
  state.editError = false;
  renderCurrentDetail();
  try {
    const payload = await submitSemanticEdit(edit, true);
    if (!payload.ok) {
      state.editBusy = false;
      state.editMessage = editFailureMessage(payload);
      state.editError = true;
      renderCurrentDetail();
      els.itemDetail.querySelector(".iv-action-primary")?.focus();
      return;
    }
    if (payload.result.applied) state.undoStack.push(payload.result.inverse_patch);
    await selectItem(edit.item, { focus: true });
    state.editMessage = payload.result.applied ? "Patch applied and full validation passed." : "Preview matched the current source; no bytes changed.";
    renderCurrentDetail();
  } catch (error) {
    state.editBusy = false;
    state.editMessage = `Apply request failed: ${error.message}`;
    state.editError = true;
    renderCurrentDetail();
  }
}

async function undoLastEdit() {
  const inverse = state.undoStack.at(-1);
  if (!inverse) return;
  state.editBusy = true;
  state.editMessage = "Replaying returned inverse patch…";
  state.editError = false;
  renderCurrentDetail();
  try {
    const payload = await submitSemanticEdit(inverse, true);
    if (!payload.ok) {
      state.editBusy = false;
      state.editMessage = editFailureMessage(payload);
      state.editError = true;
      renderCurrentDetail();
      return;
    }
    state.undoStack.pop();
    await selectItem(inverse.item, { focus: true });
    state.editMessage = payload.result.applied ? "Undo applied from the stored inverse patch." : "Undo required no source change.";
    renderCurrentDetail();
  } catch (error) {
    state.editBusy = false;
    state.editMessage = `Undo request failed: ${error.message}`;
    state.editError = true;
    renderCurrentDetail();
  }
}

function focusSelectedItem() {
  const selected = [...els.itemList.querySelectorAll(".iv-item-button")]
    .find((button) => button.dataset.itemId === state.selectedItemId);
  selected?.focus();
}

function focusSeriesSelect() {
  els.itemDetail.querySelector(".iv-series-select")?.focus();
}

async function loadSelectedChart(field, options = {}) {
  const request = state.detailRequest;
  state.chartField = field;
  state.chart = null;
  renderItemDetail(els.itemDetail, detailModel());
  if (options.focus) focusSeriesSelect();
  try {
    const response = await fetch(focusedUrl("chart", { item: state.selectedItemId, field }), { cache: "no-store" });
    if (!response.ok) throw new Error(`status ${response.status}`);
    const chart = await response.json();
    if (request !== state.detailRequest) return;
    state.chart = chart;
    renderItemDetail(els.itemDetail, detailModel());
    if (options.focus) focusSeriesSelect();
  } catch (error) {
    if (request !== state.detailRequest) return;
    state.chart = { content_error: { stderr: `Could not load chart: ${error.message}` } };
    renderItemDetail(els.itemDetail, detailModel());
    if (options.focus) focusSeriesSelect();
  }
}

async function selectItem(itemId, options = {}) {
  state.selectedItemId = itemId;
  state.detail = null;
  state.chart = null;
  state.chartField = null;
  resetEditView();
  const request = ++state.detailRequest;
  renderItemTable(state.view, state.issuesById);
  if (options.focus) focusSelectedItem();
  renderItemDetail(els.itemDetail, detailModel({ loading: true, message: "Loading item detail…" }));
  try {
    const response = await fetch(focusedUrl("item", { item: itemId }), { cache: "no-store" });
    if (!response.ok) throw new Error(`status ${response.status}`);
    const detail = await response.json();
    if (request !== state.detailRequest) return;
    state.detail = detail;
    state.chartField = detail.fields?.[0]?.member || null;
    renderItemDetail(els.itemDetail, detailModel());
    if (state.chartField && !detail.content_error) await loadSelectedChart(state.chartField);
  } catch (error) {
    if (request !== state.detailRequest) return;
    state.detail = { content_error: { stderr: `Could not load item: ${error.message}` } };
    renderItemDetail(els.itemDetail, detailModel());
  }
}

function render(view) {
  renderTopBanner(view);

  const itemIds = new Set(view.items.map((item) => item.id));
  const allIssues = taggedIssues(view);
  const catalogIssues = allIssues.filter((issue) => issue.id == null);
  const homelessIssues = allIssues.filter((issue) => issue.id != null && !itemIds.has(issue.id));
  const itemIssuesById = new Map();
  for (const issue of allIssues) {
    if (issue.id == null || !itemIds.has(issue.id)) continue;
    if (!itemIssuesById.has(issue.id)) itemIssuesById.set(issue.id, []);
    itemIssuesById.get(issue.id).push(issue);
  }

  renderSummary(view, catalogIssues);
  renderRemovedSection(view, homelessIssues);
  state.issuesById = itemIssuesById;
  renderItemTable(view, itemIssuesById);
}

function renderDropdown() {
  els.select.replaceChildren();
  for (const catalog of state.catalogs) {
    const option = document.createElement("option");
    option.value = catalog.id;
    option.textContent = `${catalog.title} (${catalog.kind}${catalog.hasItems ? "" : ", no items"})`;
    els.select.append(option);
  }
}

// The page always issues /catalog for the selected id — no dropdown-flag
// short-circuit (spec §3/§4); a hasItems:false catalog is a valid, selectable, honest
// empty state.
async function loadCatalog(id) {
  state.selectedId = id;
  state.selectedItemId = null;
  state.detail = null;
  state.chart = null;
  state.chartField = null;
  state.undoStack = [];
  resetEditView();
  state.detailRequest += 1;
  setStatus("Loading…");
  try {
    const response = await fetch(`/api/items-viewer/catalog?id=${encodeURIComponent(id)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`status ${response.status}`);
    const view = await response.json();
    state.view = view;
    // One decode for the whole page, BEFORE render() (spec §5) — every row's
    // canvas crop below reads from this same decoded <img>, never re-fetches.
    state.iconsImage = null;
    if (view.icons && view.icons.page_data_uri) {
      const img = new Image();
      img.src = view.icons.page_data_uri;
      try {
        await img.decode();
        state.iconsImage = img;
      } catch {
        state.iconsImage = null; // decode failure degrades to the "?" placeholder, same as a missing region
      }
    }
    render(view);
    setStatus(`${view.items.length} item(s) in ${view.meta.title}`);
    if (view.items.length) await selectItem(view.items[0].id);
    else renderItemDetail(els.itemDetail, { message: "No item selected." });
  } catch (error) {
    setStatus(`Could not load catalog: ${error.message}`, true);
  }
}

async function loadCatalogs() {
  try {
    const response = await fetch("/api/items-viewer/catalogs", { cache: "no-store" });
    if (!response.ok) throw new Error(`status ${response.status}`);
    const data = await response.json();
    state.catalogs = data.catalogs || [];
    renderDropdown();
    if (!state.catalogs.length) {
      setStatus("No catalogs registered.", true);
      return;
    }
    const preferred = state.catalogs.find((catalog) => catalog.hasItems) || state.catalogs[0];
    els.select.value = preferred.id;
    await loadCatalog(preferred.id);
  } catch (error) {
    setStatus(`Could not load catalogs: ${error.message}`, true);
  }
}

els.select.addEventListener("change", () => loadCatalog(els.select.value));

loadCatalogs();
