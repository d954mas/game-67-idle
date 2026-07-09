// Home screen: a full-page grid of project cards plus a "+ New project" card
// that creates a project instantly (random default title, Figma-style — no
// name prompt) and opens it straight into the workspace. Cards show a cover
// thumbnail, title, image count, and updated date; hovering reveals a delete
// button with a two-step in-place confirm (no browser confirm()). Renaming
// lives only in the workspace top bar. Pure rendering/input over the shared API.
import { api, coverUrl, el, hooks, loadProjects, rememberHomeStore, setStatus, state } from "./app.js";
import { ALL_STORES_ID, STUDIO_STORE_ID, projectStoreId } from "./store_scope.js";

function formatDate(iso) {
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return "";
  return date.toLocaleDateString(undefined, { year: "numeric", month: "short", day: "numeric" });
}

function coverNode(project) {
  const cover = document.createElement("div");
  cover.className = "card-cover";
  const url = coverUrl(project);
  if (url) {
    const img = document.createElement("img");
    img.src = url;
    img.alt = "";
    cover.appendChild(img);
  } else {
    cover.classList.add("empty");
    cover.textContent = "No images";
  }
  return cover;
}

function storeById(storeId) {
  return state.stores.find((store) => store.storeId === storeId) || null;
}

function storeLabel(storeId) {
  const store = storeById(storeId);
  return store ? store.label : storeId;
}

function visibleProjects() {
  if (state.homeStoreId === ALL_STORES_ID) return state.projects;
  return state.projects.filter((project) => projectStoreId(project) === state.homeStoreId);
}

function createStoreId() {
  return state.homeStoreId === ALL_STORES_ID ? STUDIO_STORE_ID : state.homeStoreId;
}

async function deleteProjectFromHome(project) {
  try {
    await api("DELETE", `/projects/${project.id}`, undefined, { storeId: projectStoreId(project) });
    await loadProjects();
    render();
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Card actions: only Delete, gated by a lean two-step in-place confirm (no
// browser confirm()). First click swaps the button row for a "Delete project?"
// prompt with Delete/Cancel; it reverts on Cancel, Escape, or a click anywhere
// outside the actions row. Trash-move is not undoable from the UI, so — unlike
// element delete on the canvas, which is journaled and needs no confirmation —
// this is the one place a confirm step earns its keep.
function projectActions(project) {
  const actions = document.createElement("div");
  actions.className = "card-actions";

  function onOutside(event) {
    if (!actions.contains(event.target)) showDefault();
  }
  function onEscape(event) {
    if (event.key === "Escape") showDefault();
  }
  function stopWatching() {
    document.removeEventListener("mousedown", onOutside, true);
    document.removeEventListener("keydown", onEscape, true);
  }

  function showDefault() {
    stopWatching();
    const del = document.createElement("button");
    del.type = "button";
    del.className = "card-action danger";
    del.title = "Delete (moves to .trash)";
    del.textContent = "Delete";
    del.addEventListener("click", (event) => {
      event.stopPropagation();
      showConfirm();
    });
    actions.replaceChildren(del);
  }

  function showConfirm() {
    const label = document.createElement("span");
    label.className = "card-confirm-label";
    label.textContent = "Delete project?";
    const confirmBtn = document.createElement("button");
    confirmBtn.type = "button";
    confirmBtn.className = "card-action danger";
    confirmBtn.textContent = "Delete";
    confirmBtn.addEventListener("click", (event) => {
      event.stopPropagation();
      showDefault();
      deleteProjectFromHome(project);
    });
    const cancelBtn = document.createElement("button");
    cancelBtn.type = "button";
    cancelBtn.className = "card-action";
    cancelBtn.textContent = "Cancel";
    cancelBtn.addEventListener("click", (event) => {
      event.stopPropagation();
      showDefault();
    });
    actions.replaceChildren(label, confirmBtn, cancelBtn);
    document.addEventListener("mousedown", onOutside, true);
    document.addEventListener("keydown", onEscape, true);
  }

  showDefault();
  return actions;
}

function projectCard(project) {
  const card = document.createElement("div");
  card.className = "project-card";

  card.appendChild(coverNode(project));

  const body = document.createElement("div");
  body.className = "card-body";

  const title = document.createElement("div");
  title.className = "card-title";
  title.textContent = project.title;
  body.appendChild(title);

  const meta = document.createElement("div");
  meta.className = "card-meta";
  const count = (project.elements || []).length;
  meta.textContent = `${count} image${count === 1 ? "" : "s"} · ${formatDate(project.updated)}`;
  const store = state.homeStoreId === ALL_STORES_ID ? `${storeLabel(projectStoreId(project))} / ` : "";
  meta.textContent = `${store}${count} image${count === 1 ? "" : "s"} / ${formatDate(project.updated)}`;
  body.appendChild(meta);

  card.appendChild(body);
  card.appendChild(projectActions(project));

  card.addEventListener("click", () => hooks.openProject(project.id, { storeId: project.storeId }));
  return card;
}

// Instant create (Figma-style): no name prompt. The op layer generates a
// random default title; the project opens straight into the workspace, where
// the top-bar title is the one place to rename it.
async function createProject() {
  try {
    const storeId = createStoreId();
    state.storeId = storeId;
    const { project } = await api("POST", "/projects", undefined, { storeId });
    await loadProjects();
    hooks.openProject(project.id, { storeId: project.storeId });
  } catch (error) {
    setStatus(error.message, true);
  }
}

function newProjectCard() {
  const card = document.createElement("div");
  card.className = "project-card new-card";
  const plus = document.createElement("div");
  plus.className = "new-plus";
  plus.textContent = "+";
  const label = document.createElement("div");
  label.className = "new-label";
  label.textContent = state.homeStoreId === ALL_STORES_ID ? "New project in AI Studio" : `New project in ${storeLabel(state.homeStoreId)}`;
  card.append(plus, label);
  card.addEventListener("click", () => createProject());
  return card;
}

function renderStoreSelector() {
  const select = el("home-store-select");
  if (!select) return;
  const options = [{ storeId: ALL_STORES_ID, label: "All stores" }, ...state.stores];
  select.replaceChildren(...options.map((store) => {
    const option = document.createElement("option");
    option.value = store.storeId;
    option.textContent = store.label;
    return option;
  }));
  select.value = options.some((store) => store.storeId === state.homeStoreId) ? state.homeStoreId : ALL_STORES_ID;
  select.onchange = () => {
    state.homeStoreId = select.value || ALL_STORES_ID;
    rememberHomeStore(state.homeStoreId);
    render();
  };
}

export function render() {
  const grid = el("home-grid");
  if (!grid) return;
  renderStoreSelector();
  grid.replaceChildren();
  grid.appendChild(newProjectCard());
  for (const project of visibleProjects()) grid.appendChild(projectCard(project));
}

export function initHome() {
  hooks.renderHome = render;
}
