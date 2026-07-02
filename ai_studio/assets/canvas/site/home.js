// Home screen: a full-page grid of project cards plus a "+ New project" card with
// an inline title input (no browser prompt). Cards show a cover thumbnail, title,
// image count, and updated date; hovering reveals inline rename and delete. Pure
// rendering/input over the shared API.
import { api, coverUrl, el, hooks, loadProjects, setStatus, state } from "./app.js";
import { inlineEdit, makeInlineInput } from "./inline.js";

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

async function renameProjectFromHome(project, nextTitle) {
  try {
    await api("PATCH", `/projects/${project.id}`, { title: nextTitle });
    await loadProjects();
    render();
  } catch (error) {
    setStatus(error.message, true);
  }
}

async function deleteProjectFromHome(project) {
  try {
    await api("DELETE", `/projects/${project.id}`);
    await loadProjects();
    render();
  } catch (error) {
    setStatus(error.message, true);
  }
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
  body.appendChild(meta);

  card.appendChild(body);

  const actions = document.createElement("div");
  actions.className = "card-actions";
  const rename = document.createElement("button");
  rename.type = "button";
  rename.className = "card-action";
  rename.title = "Rename";
  rename.textContent = "Rename";
  rename.addEventListener("click", (event) => {
    event.stopPropagation();
    inlineEdit(title, project.title, (next) => renameProjectFromHome(project, next));
  });
  const del = document.createElement("button");
  del.type = "button";
  del.className = "card-action danger";
  del.title = "Delete (moves to .trash)";
  del.textContent = "Delete";
  del.addEventListener("click", (event) => {
    event.stopPropagation();
    deleteProjectFromHome(project);
  });
  actions.append(rename, del);
  card.appendChild(actions);

  card.addEventListener("click", () => hooks.openProject(project.id));
  return card;
}

async function createProject(title) {
  try {
    const { project } = await api("POST", "/projects", { title });
    await loadProjects();
    hooks.openProject(project.id);
  } catch (error) {
    setStatus(error.message, true);
  }
}

function newProjectCard() {
  const card = document.createElement("div");
  card.className = "project-card new-card";

  const showInput = () => {
    card.replaceChildren();
    const input = makeInlineInput({
      value: "",
      placeholder: "Project title",
      onCommit: (title) => createProject(title),
      onCancel: () => render(),
    });
    card.appendChild(input);
    input.focus();
  };

  const plus = document.createElement("div");
  plus.className = "new-plus";
  plus.textContent = "+";
  const label = document.createElement("div");
  label.className = "new-label";
  label.textContent = "New project";
  card.append(plus, label);
  card.addEventListener("click", showInput);
  return card;
}

export function render() {
  const grid = el("home-grid");
  if (!grid) return;
  grid.replaceChildren();
  grid.appendChild(newProjectCard());
  for (const project of state.projects) grid.appendChild(projectCard(project));
}

export function initHome() {
  hooks.renderHome = render;
}
