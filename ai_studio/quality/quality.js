const state = {
  catalog: null,
  activeGroup: "all",
};

const groupColors = {
  player_clarity: "#6b93f2",
  art: "#e56a6a",
  gdd: "#b48cff",
  game_design: "#6fbf73",
  technical: "#28b8a5",
  assets: "#d3933e",
};

const groupCount = document.getElementById("groupCount");
const checkCount = document.getElementById("checkCount");
const groupTabs = document.getElementById("groupTabs");
const groupsRoot = document.getElementById("qualityGroups");
const statusRoot = document.getElementById("qualityStatus");

function setText(node, value) {
  if (node) node.textContent = value;
}

function make(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function evidenceTags(check) {
  const text = `${check.evidence} ${check.whatItChecks} ${check.useWhen}`.toLowerCase();
  const tags = [];
  const add = (tag, pattern) => {
    if (pattern.test(text) && !tags.includes(tag)) tags.push(tag);
  };
  add("screenshot", /screenshot|visual|viewport|screen/);
  add("runtime", /runtime|launch|smoke|scenario|playable/);
  add("command", /command|test|build|parser|schema|log/);
  add("source", /source|provenance|origin|license|manifest/);
  add("design", /design|gdd|table|handoff|decision/);
  add("asset", /asset|material|texture|viewer|render/);
  return tags.slice(0, 4);
}

function groupColor(group) {
  return groupColors[group.slug] || "#d3933e";
}

function field(label, value) {
  const root = make("div", "quality-field");
  root.append(make("strong", "", label));
  root.append(make("p", "", value || "Not specified."));
  return root;
}

async function copyText(text, button) {
  try {
    const textarea = document.createElement("textarea");
    textarea.value = text;
    textarea.setAttribute("readonly", "");
    textarea.style.position = "fixed";
    textarea.style.left = "-9999px";
    document.body.append(textarea);
    textarea.select();
    const copied = document.execCommand("copy");
    textarea.remove();
    if (!copied) throw new Error("copy command returned false");
    const previous = button.textContent;
    button.textContent = "Copied";
    window.setTimeout(() => {
      button.textContent = previous;
    }, 1100);
  } catch {
    button.textContent = "Copy failed";
  }
}

function renderTabs(catalog) {
  groupTabs.replaceChildren();
  const tabs = [
    {
      slug: "all",
      title: "All Checks",
      prefix: "ALL",
      checks: Array.from({ length: catalog.totalChecks }),
      color: "#93a2b7",
    },
    ...catalog.groups,
  ];

  for (const group of tabs) {
    const button = make("button", "group-tab", group.prefix || group.title);
    button.type = "button";
    button.setAttribute("role", "tab");
    button.setAttribute("aria-selected", String(group.slug === state.activeGroup));
    button.title = group.title;
    button.dataset.group = group.slug;
    button.style.setProperty("--group-color", group.color || groupColor(group));

    const title = make("span", "group-tab-title", group.title.replace(" Rules", ""));
    const meta = make("span", "group-tab-meta", `${group.checks.length} checks`);
    button.textContent = "";
    button.append(make("span", "group-tab-prefix", group.prefix), title, meta);
    groupTabs.append(button);
  }
}

function renderCheck(group, check) {
  const card = make("article", "quality-card");
  card.style.setProperty("--group-color", groupColor(group));

  const top = make("div", "quality-card-top");
  const title = make("div", "quality-card-title");
  title.append(make("span", "quality-id", check.id));
  title.append(make("h3", "", check.name));

  const actions = make("div", "quality-actions");
  const copyId = make("button", "copy-button", "Copy ID");
  copyId.type = "button";
  copyId.dataset.copyText = check.copyText;
  actions.append(copyId);
  top.append(title, actions);

  const body = make("div", "quality-card-body");
  body.append(
    field("Description", check.description),
    field("Checks", check.whatItChecks),
    field("Use When", check.useWhen),
  );

  const chips = make("div", "quality-evidence-tags");
  for (const tag of evidenceTags(check)) {
    chips.append(make("span", "quality-chip", tag));
  }

  const details = make("details", "quality-details");
  const detailsSummary = make("summary", "", "Evidence and source");
  const fields = make("div", "quality-fields");
  fields.append(
    field("Evidence", check.evidence),
    field("Not Enough", check.notEnough),
  );

  const path = make("a", "quality-path", check.path);
  path.href = `/${check.path}`;
  path.target = "_blank";
  path.rel = "noreferrer";
  details.append(detailsSummary, fields, path);

  card.append(top, body, chips, details);
  return card;
}

function renderGroup(group, checks) {
  const section = make("article", "quality-group");
  section.style.setProperty("--group-color", groupColor(group));
  const head = make("div", "quality-group-head");
  const main = make("div", "");
  const title = make("div", "quality-group-title");
  title.append(make("span", "quality-prefix", group.prefix));
  title.append(make("h2", "", group.title));
  main.append(title, make("p", "", group.description));
  head.append(main, make("span", "quality-check-count", `${checks.length} checks`));

  const grid = make("div", "quality-card-grid");
  for (const check of checks) {
    grid.append(renderCheck(group, check));
  }

  section.append(head, grid);
  return section;
}

function render() {
  const catalog = state.catalog;
  if (!catalog) return;

  renderTabs(catalog);
  groupsRoot.replaceChildren();

  let visibleChecks = 0;
  const active = catalog.groups.find((group) => group.slug === state.activeGroup);
  const groups = state.activeGroup === "all" || !active ? catalog.groups : [active];
  for (const group of groups) {
    const checks = group.checks;
    if (!checks.length) continue;
    visibleChecks += checks.length;
    groupsRoot.append(renderGroup(group, checks));
  }

  if (visibleChecks === 0) {
    groupsRoot.append(make("div", "quality-empty", "No quality checks match the current filters."));
  }

  statusRoot.textContent = state.activeGroup === "all" || !active
    ? `${visibleChecks} checks across all groups`
    : `${visibleChecks} checks in ${active.title.replace(" Rules", "")}`;
  statusRoot.classList.remove("is-error");
}

async function load() {
  try {
    const response = await fetch("/api/quality-checks", { cache: "no-store" });
    if (!response.ok) throw new Error(`status ${response.status}`);
    state.catalog = await response.json();
    state.activeGroup = "all";
    setText(groupCount, state.catalog.totalGroups);
    setText(checkCount, state.catalog.totalChecks);
    render();
  } catch (error) {
    setText(groupCount, "offline");
    setText(checkCount, "offline");
    statusRoot.textContent = `Could not load quality checks: ${error.message}`;
    statusRoot.classList.add("is-error");
  }
}

groupTabs.addEventListener("click", (event) => {
  const button = event.target.closest("[data-group]");
  if (!button) return;
  state.activeGroup = button.dataset.group;
  render();
});

groupsRoot.addEventListener("click", (event) => {
  const button = event.target.closest("[data-copy-text]");
  if (!button) return;
  copyText(button.dataset.copyText, button);
});

load();
