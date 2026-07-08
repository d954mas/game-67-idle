// Items Viewer page (T0316 phase 1) — read-only. Bare ESM, no framework, no build
// step: fetches the two /api/items-viewer/* endpoints and renders. See the module
// README ("Why the site can't import ops.mjs") for why the tiny "issue routing" and
// BOM-agnostic-JSON logic here is a deliberate, small duplication of ops.mjs's own
// pure helpers rather than a shared import.
const state = {
  catalogs: [],
  selectedId: null,
  view: null,
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

// Phase-1 label rule (spec §4): item_fields.schema.json fields carry ONLY
// {type, required} -- no labels. Humanize the key ("display_name" -> "Display name")
// until a future ui:{} namespace ships real labels; this is the single seam to swap.
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

// Icons — decision (spec §5): honest placeholder only in phase 1, no gallery search
// (100% miss rate today, no icon_asset_id -> file binding exists). Kept as a ONE
// function-wide seam: phase 2 wires this to `icon-link` output and nothing else in
// the page changes (it already renders "resolved img OR placeholder").
function resolveIcon(_assetId) {
  return null;
}

function renderIconSlot(item) {
  const box = make("div", "iv-icon-slot");
  const resolved = resolveIcon(item.icon_asset_id);
  if (resolved) {
    const img = document.createElement("img");
    img.src = resolved;
    img.alt = item.icon_asset_id || "";
    box.append(img);
    return box;
  }
  box.classList.add("iv-icon-missing");
  box.append(make("span", "iv-icon-glyph", "?"));
  box.append(make("span", "iv-icon-caption", item.icon_asset_id || "no icon"));
  return box;
}

// Per-type render rules (spec §4): object -> recurse into its own .fields; list<string>
// -> join; i64/string/bool -> scalar; enum -> the value itself. A schema key absent
// from the record renders as an em-dash (handled by field() above).
function renderTypedValue(type, value, spec) {
  if (value === undefined || value === null) return "—";
  if (type === "object") {
    if (spec && spec.fields) {
      const nested = make("div", "iv-nested");
      for (const [key, nestedSpec] of Object.entries(spec.fields)) {
        nested.append(field(humanize(key), renderTypedValue(nestedSpec.type, value[key], nestedSpec)));
      }
      return nested;
    }
    // An object-typed field with no declared sub-fields (e.g. blocks.use.params, an
    // open bag — item_fields.schema.json only says {"type":"object"}) -- render its
    // own keys directly instead of the useless default String(value) ("[object
    // Object]"), same degradation renderRawValue uses when the schema is unavailable.
    return renderRawValue(value);
  }
  if (type === "list<string>") return Array.isArray(value) && value.length ? value.join(", ") : "—";
  if (type === "bool") return value ? "true" : "false";
  return String(value);
}

// Degradation (spec §4): schema unavailable -> iterate the record's OWN keys, no type
// info to lean on.
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

function renderBlock(blockName, blockValue, blockSpec) {
  const box = make("div", "iv-block");
  box.append(make("span", "iv-chip iv-chip-block", humanize(blockName)));
  const rows = make("div", "iv-block-rows");
  if (blockSpec && blockSpec.fields) {
    for (const [key, spec] of Object.entries(blockSpec.fields)) {
      rows.append(field(humanize(key), renderTypedValue(spec.type, blockValue ? blockValue[key] : undefined, spec)));
    }
  } else if (blockValue && typeof blockValue === "object") {
    for (const [key, v] of Object.entries(blockValue)) rows.append(field(humanize(key), renderRawValue(v)));
  }
  box.append(rows);
  return box;
}

// Chrome keys are hand-rendered at fixed positions (icon/title/subtitle/kind chip),
// so they are excluded from the schema-iterated generic rows (spec §4). `blocks` is
// the item_json_record's own bookkeeping array, never a displayable field.
const CHROME_KEYS = new Set(["id", "display_name", "icon_asset_id", "kind", "blocks"]);
const BLOCK_KEYS = new Set(["equip", "use", "currency"]);

function renderCard(item, view, issues) {
  const card = make("article", "iv-card");

  const top = make("div", "iv-card-top");
  top.append(renderIconSlot(item));
  const titleBox = make("div", "iv-card-title");
  titleBox.append(make("h3", "", item.display_name || item.id || "—"));
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
  if (view.schema && view.schema.core) {
    for (const [key, spec] of Object.entries(view.schema.core)) {
      if (CHROME_KEYS.has(key)) continue;
      rows.append(field(humanize(key), renderTypedValue(spec.type, item[key], spec)));
    }
  } else {
    for (const key of Object.keys(item)) {
      if (CHROME_KEYS.has(key) || BLOCK_KEYS.has(key)) continue;
      rows.append(field(humanize(key), renderRawValue(item[key])));
    }
  }
  card.append(rows);

  for (const blockName of item.blocks || []) {
    const blockSpec = view.schema && view.schema.blocks ? view.schema.blocks[blockName] : null;
    card.append(renderBlock(blockName, item[blockName], blockSpec));
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

  const containersBox = make("div", "iv-containers");
  containersBox.append(make("h3", "", `Containers (${view.containers.length})`));
  if (view.containers.length) {
    const table = make("table", "iv-table");
    const thead = make("thead");
    const headRow = make("tr");
    for (const key of ["id", "capacity", "accept_policy", "hidden"]) headRow.append(make("th", "", humanize(key)));
    thead.append(headRow);
    table.append(thead);
    const tbody = make("tbody");
    for (const container of view.containers) {
      const row = make("tr");
      row.append(make("td", "", container.id));
      row.append(make("td", "", container.capacity ? String(container.capacity) : "unlimited"));
      row.append(make("td", "", container.accept_policy));
      row.append(make("td", "", container.hidden ? "yes" : "no"));
      tbody.append(row);
    }
    table.append(tbody);
    containersBox.append(table);
  }
  root.append(containersBox);

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
