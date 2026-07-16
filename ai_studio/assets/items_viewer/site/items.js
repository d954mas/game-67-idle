// Read-only Items Viewer page. Bare ESM, no framework or build step.
const state = {
  catalogs: [],
  selectedId: null,
  view: null,
  // T0316: the view.icons.page_data_uri, decoded ONCE per loadCatalog (before
  // render()) so every card's canvas crop reads from the same already-decoded
  // <img> instead of each card re-decoding the same base64 PNG.
  iconsImage: null,
};

const els = {
  select: document.getElementById("catalogSelect"),
  status: document.getElementById("statusLine"),
  topBanner: document.getElementById("topBanner"),
  summaryPanel: document.getElementById("summaryPanel"),
  removedSection: document.getElementById("removedSection"),
  cardGrid: document.getElementById("cardGrid"),
};

function make(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

// Snapshot fields carry stable machine names; humanize them at this UI boundary.
function humanize(key) {
  const text = String(key || "").replace(/_/g, " ");
  return text ? text.charAt(0).toUpperCase() + text.slice(1) : text;
}

function field(label, value) {
  const row = make("div", "iv-field");
  row.append(make("strong", "iv-field-label", label));
  const box = make("span", "iv-field-value");
  if (value instanceof Node) box.append(value);
  else box.textContent = value === undefined || value === null || value === "" ? "—" : value;
  row.append(box);
  return row;
}

function setStatus(text, isError = false) {
  els.status.textContent = text;
  els.status.classList.toggle("is-error", Boolean(isError));
}

// Icons: view.icons.regions maps an item icon id ("icons/gold")
// straight to a pixel rect on the ALREADY-DECODED atlas debug-PNG page (decoded
// once in loadCatalog, before render() — see state.iconsImage). No gallery
// search, no icon-link write layer (phase 2) — this reads exactly what the
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

// The Snapshot summary is bounded. Only nested capability params need a generic
// object renderer.
function renderRawValue(value) {
  if (value === undefined || value === null) return "—";
  if (Array.isArray(value)) return value.length ? value.map(String).join(", ") : "—";
  if (typeof value === "boolean") return value ? "true" : "false";
  if (typeof value === "object") {
    const nested = make("div", "iv-nested");
    for (const [key, v] of Object.entries(value)) nested.append(field(humanize(key), renderRawValue(v)));
    return nested;
  }
  return String(value);
}

const CORE_FIELDS = ["created", "tags", "base_value", "stack", "authoring_mode"];
const BLOCK_FIELDS = {
  equip: ["slot"],
  use: ["effect_id", "params"],
  currency: ["hud", "cap"],
};

function renderBlock(blockName, blockValue) {
  const box = make("div", "iv-block");
  box.append(make("span", "iv-chip iv-chip-block", humanize(blockName)));
  const rows = make("div", "iv-block-rows");
  for (const key of BLOCK_FIELDS[blockName]) {
    rows.append(field(humanize(key), renderRawValue(blockValue[key])));
  }
  box.append(rows);
  return box;
}

function renderCard(item, view, issues) {
  const card = make("article", "iv-card");

  const top = make("div", "iv-card-top");
  top.append(renderIconSlot(item, view));
  const titleBox = make("div", "iv-card-title");
  titleBox.append(make("h3", "", item.name || item.id || "—"));
  titleBox.append(make("span", "iv-card-id", item.id || "—"));
  top.append(titleBox);
  card.append(top);

  const chips = make("div", "iv-card-chips");
  const kindEntry = view.item_kinds.find((k) => k.id === item.kind);
  chips.append(make("span", "iv-chip iv-chip-kind", (kindEntry && kindEntry.label) || item.kind || "—"));
  const lockStatus = view.lock.status_by_id[item.id] || "draft";
  chips.append(make("span", `iv-chip iv-chip-lock iv-chip-lock-${lockStatus}`, lockStatus));
  card.append(chips);

  const rows = make("div", "iv-card-rows");
  for (const key of CORE_FIELDS) rows.append(field(humanize(key), renderRawValue(item[key])));
  card.append(rows);

  for (const blockName of Object.keys(BLOCK_FIELDS)) {
    if (!item[blockName]) continue;
    card.append(renderBlock(blockName, item[blockName]));
  }

  if (issues.length) {
    const issuesBox = make("div", "iv-card-issues");
    for (const issue of issues) {
      issuesBox.append(make("p", `iv-issue iv-issue-${issue.severity}`, `[${issue.rule}] ${issue.msg}`));
    }
    card.append(issuesBox);
  }

  return card;
}

// Tag validate.errors/warnings with a severity so the same list can be filtered once
// for card/summary/removed placement (spec §4 "Per-item issues": red/yellow on the
// card).
function taggedIssues(view) {
  const errors = (view.validate.errors || []).map((issue) => ({ ...issue, severity: "error" }));
  const warnings = (view.validate.warnings || []).map((issue) => ({ ...issue, severity: "warning" }));
  return [...errors, ...warnings];
}

// Issue routing rule (spec §4, load-bearing — mirrors ops.mjs's routeIssues(), which
// this module cannot import: it pulls in node:child_process/node:fs and there is no
// build step to strip that for a browser bundle). An issue attaches to a card IFF its
// `id` is present in items[]; a catalog-level rule (id:null) goes to the Summary
// panel; a rule keyed on a DELETED id (removed-without-reaction and neighbors) has no
// card and goes to the Removed/lock section.
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

function renderCards(view, cardIssuesById) {
  const root = els.cardGrid;
  root.replaceChildren();
  if (!view.items.length) {
    root.append(make("div", "iv-empty", view.meta.hasItems ? "No items in this catalog." : "Items not connected for this game."));
    return;
  }
  for (const item of view.items) {
    root.append(renderCard(item, view, cardIssuesById.get(item.id) || []));
  }
}

function render(view) {
  renderTopBanner(view);

  const itemIds = new Set(view.items.map((item) => item.id));
  const allIssues = taggedIssues(view);
  const catalogIssues = allIssues.filter((issue) => issue.id == null);
  const homelessIssues = allIssues.filter((issue) => issue.id != null && !itemIds.has(issue.id));
  const cardIssuesById = new Map();
  for (const issue of allIssues) {
    if (issue.id == null || !itemIds.has(issue.id)) continue;
    if (!cardIssuesById.has(issue.id)) cardIssuesById.set(issue.id, []);
    cardIssuesById.get(issue.id).push(issue);
  }

  renderSummary(view, catalogIssues);
  renderRemovedSection(view, homelessIssues);
  renderCards(view, cardIssuesById);
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
  setStatus("Loading…");
  try {
    const response = await fetch(`/api/items-viewer/catalog?id=${encodeURIComponent(id)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`status ${response.status}`);
    const view = await response.json();
    state.view = view;
    // One decode for the whole page, BEFORE render() (spec §5) — every card's
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
