"use strict";

const state = {
  board: null,        // payload from /api/board
  view: "board",
  filterProject: "",
  filterEpic: "",
  search: "",
  showClosed: false,
  editing: null,      // { kind, id } or { kind, id: null } for create
  dragging: false,
  lastPayload: "",
};

const els = {
  board: document.getElementById("board"),
  overview: document.getElementById("overview"),
  boardStatus: document.getElementById("boardStatus"),
  viewTabs: Array.from(document.querySelectorAll(".view-tab")),
  search: document.getElementById("search"),
  projectFilter: document.getElementById("projectFilter"),
  epicFilter: document.getElementById("epicFilter"),
  showClosed: document.getElementById("showClosed"),
  newTask: document.getElementById("newTask"),
  newEpic: document.getElementById("newEpic"),
  newProject: document.getElementById("newProject"),
  editor: document.getElementById("editor"),
  editorForm: document.getElementById("editorForm"),
  editorId: document.getElementById("editorId"),
  editorMeta: document.getElementById("editorMeta"),
  editorCancel: document.getElementById("editorCancel"),
  fTitle: document.getElementById("fTitle"),
  fStatus: document.getElementById("fStatus"),
  fPriority: document.getElementById("fPriority"),
  fProject: document.getElementById("fProject"),
  fEpic: document.getElementById("fEpic"),
  fProjectKind: document.getElementById("fProjectKind"),
  fTarget: document.getElementById("fTarget"),
  fTags: document.getElementById("fTags"),
  fBody: document.getElementById("fBody"),
  bodyPreview: document.getElementById("bodyPreview"),
  projectFieldWrap: document.getElementById("projectFieldWrap"),
  epicFieldWrap: document.getElementById("epicFieldWrap"),
  projectKindFieldWrap: document.getElementById("projectKindFieldWrap"),
  targetFieldWrap: document.getElementById("targetFieldWrap"),
};

async function api(path, options) {
  const res = await fetch(path, options);
  const data = await res.json();
  if (!res.ok) {
    alert(data.error || `Request failed: ${res.status}`);
    const err = new Error(data.error || res.status);
    err.status = res.status;
    throw err;
  }
  return data;
}

async function refresh(force = false) {
  const data = await api("/api/board");
  const payload = JSON.stringify(data);
  if (!force && payload === state.lastPayload) {
    return;
  }
  state.lastPayload = payload;
  state.board = data;
  render();
}

// --- rendering ---------------------------------------------------------------

const ACTIVE_COLUMNS = ["backlog", "todo", "doing", "review"];
const ALL_COLUMNS = ["idea", "backlog", "todo", "doing", "review", "done", "dropped"];
const SECONDARY_STATUSES = new Set(["idea", "done", "dropped"]);

function projectForTask(task) {
  if (task.fields.project) return task.fields.project;
  const epic = findDoc("epic", task.fields.epic);
  return epic ? (epic.fields.project || "") : "";
}

function visibleTasks() {
  const q = state.search.trim().toLowerCase();
  return state.board.tasks.filter((t) => {
    const f = t.fields;
    const project = projectForTask(t);
    if (!state.showClosed && SECONDARY_STATUSES.has(f.status)) return false;
    if (state.filterProject && project !== state.filterProject) return false;
    if (state.filterEpic && f.epic !== state.filterEpic) return false;
    if (q) {
      const hay = `${f.id} ${f.title} ${(f.tags || []).join(" ")} ${project} ${f.epic || ""}`.toLowerCase();
      if (!hay.includes(q)) return false;
    }
    return true;
  });
}

function visibleProjects() {
  const q = state.search.trim().toLowerCase();
  return state.board.projects.filter((p) => {
    const f = p.fields;
    if (!state.showClosed && SECONDARY_STATUSES.has(f.status)) return false;
    if (state.filterProject && f.id !== state.filterProject) return false;
    if (q) {
      const hay = `${f.id} ${f.title} ${f.kind || ""} ${f.target || ""} ${(f.tags || []).join(" ")}`.toLowerCase();
      if (!hay.includes(q)) return false;
    }
    return true;
  });
}

function visibleEpics() {
  const q = state.search.trim().toLowerCase();
  return state.board.epics.filter((e) => {
    const f = e.fields;
    if (!state.showClosed && SECONDARY_STATUSES.has(f.status)) return false;
    if (state.filterProject && f.project !== state.filterProject) return false;
    if (state.filterEpic && f.id !== state.filterEpic) return false;
    if (q) {
      const hay = `${f.id} ${f.title} ${f.project || ""} ${(f.tags || []).join(" ")}`.toLowerCase();
      if (!hay.includes(q)) return false;
    }
    return true;
  });
}

function taskCountsByProject() {
  const counts = new Map();
  for (const task of state.board.tasks) {
    const project = projectForTask(task) || "";
    counts.set(project, (counts.get(project) || 0) + 1);
  }
  return counts;
}

function epicCountsByProject() {
  const counts = new Map();
  for (const epic of state.board.epics) {
    const project = epic.fields.project || "";
    counts.set(project, (counts.get(project) || 0) + 1);
  }
  return counts;
}

function render() {
  renderViewTabs();
  renderProjectSelects();
  renderEpicSelects();
  renderOverview();
  renderMain();
}

function renderViewTabs() {
  for (const tab of els.viewTabs) {
    const active = tab.dataset.view === state.view;
    tab.classList.toggle("is-active", active);
    tab.setAttribute("aria-selected", active ? "true" : "false");
  }
}

function renderProjectSelects() {
  const current = state.filterProject;
  const options = ['<option value="">All projects</option>'];
  for (const p of selectableProjects(current)) {
    const suffix = p.fields.kind ? ` (${p.fields.kind})` : "";
    options.push(`<option value="${p.fields.id}">${p.fields.id} ${escapeHtml(p.fields.title)}${escapeHtml(suffix)}</option>`);
  }
  els.projectFilter.innerHTML = options.join("");
  els.projectFilter.value = current;
}

function renderEpicSelects() {
  const current = state.filterEpic;
  const options = ['<option value="">All epics</option>'];
  for (const e of selectableEpics(current, state.filterProject)) {
    options.push(`<option value="${e.fields.id}">${e.fields.id} ${escapeHtml(e.fields.title)}</option>`);
  }
  els.epicFilter.innerHTML = options.join("");
  els.epicFilter.value = current;
}

function renderOverview() {
  const taskProjectCounts = taskCountsByProject();
  const epicProjectCounts = epicCountsByProject();
  const activeTasks = state.board.tasks.filter((task) => !SECONDARY_STATUSES.has(task.fields.status)).length;
  const activeEpics = state.board.epics.filter((epic) => epic.fields.status === "active").length;
  const activeProjects = state.board.projects.filter((project) => project.fields.status === "active").length;
  els.boardStatus.textContent = `${activeProjects} projects / ${activeEpics} active epics / ${activeTasks} active tasks`;

  const projects = visibleProjects();
  const projectCards = projects.map((project) => {
    const f = project.fields;
    const selected = state.filterProject === f.id;
    return `
      <button type="button" class="project-card ${selected ? "is-selected" : ""}" data-project="${escapeHtml(f.id)}">
        <span class="project-card-top">
          <span class="project-id">${escapeHtml(f.id)}</span>
          <span class="status-chip status-${escapeHtml(f.status)}">${escapeHtml(f.status)}</span>
        </span>
        <span class="project-title">${escapeHtml(f.title)}</span>
        <span class="project-meta">${escapeHtml(f.kind || "other")} &middot; ${escapeHtml(f.target || "no target")}</span>
        <span class="project-stats">
          <span>${epicProjectCounts.get(f.id) || 0} epics</span>
          <span>${taskProjectCounts.get(f.id) || 0} tasks</span>
        </span>
      </button>
    `;
  }).join("");

  els.overview.innerHTML = `
    <div class="overview-metrics" aria-label="Taskboard metrics">
      <div class="metric"><span class="metric-value">${activeProjects}</span><span class="metric-label">Active projects</span></div>
      <div class="metric"><span class="metric-value">${activeEpics}</span><span class="metric-label">Active epics</span></div>
      <div class="metric"><span class="metric-value">${activeTasks}</span><span class="metric-label">Active tasks</span></div>
    </div>
    <div class="project-strip">
      ${projectCards || `<div class="empty-inline">No projects match the current filters.</div>`}
    </div>
  `;

  els.overview.querySelectorAll(".project-card").forEach((card) => {
    card.addEventListener("click", () => {
      const projectId = card.dataset.project || "";
      state.filterProject = state.filterProject === projectId ? "" : projectId;
      if (state.filterEpic) {
        const epic = findDoc("epic", state.filterEpic);
        if (epic && state.filterProject && epic.fields.project !== state.filterProject) {
          state.filterEpic = "";
        }
      }
      render();
    });
  });
}

function selectableProjects(currentProjectId = "") {
  return state.board.projects.filter((p) => state.showClosed || p.fields.status === "active" || p.fields.id === currentProjectId);
}

function selectableEpics(currentEpicId = "", projectId = "") {
  return state.board.epics.filter((e) => {
    const statusOk = state.showClosed || e.fields.status === "active" || e.fields.id === currentEpicId;
    const projectOk = !projectId || e.fields.project === projectId || e.fields.id === currentEpicId;
    return statusOk && projectOk;
  });
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

function renderBodyPreview() {
  els.bodyPreview.innerHTML = TaskboardMarkdown.renderMarkdown(els.fBody.value);
}

function renderMain() {
  if (state.view === "projects") {
    renderProjectsView();
    return;
  }
  if (state.view === "epics") {
    renderEpicsView();
    return;
  }
  renderBoard();
}

function renderBoard() {
  els.board.innerHTML = "";
  const tasks = visibleTasks();
  const columns = state.showClosed ? ALL_COLUMNS : ACTIVE_COLUMNS;
  els.board.className = "board board-kanban";
  for (const status of columns) {
    const column = document.createElement("section");
    column.className = "column";
    column.dataset.status = status;
    const inColumn = tasks
      .filter((t) => t.fields.status === status)
      .sort((a, b) => String(a.fields.priority).localeCompare(String(b.fields.priority)) ||
        String(a.fields.id).localeCompare(String(b.fields.id)));
    column.innerHTML = `<h2>${status} <span class="count">${inColumn.length}</span></h2>`;
    const cards = document.createElement("div");
    cards.className = "cards";
    for (const task of inColumn) {
      cards.appendChild(renderCard(task));
    }
    if (inColumn.length === 0) {
      cards.appendChild(renderColumnEmpty(status));
    }
    column.appendChild(cards);
    wireDropTarget(column, status);
    els.board.appendChild(column);
  }
}

function renderProjectsView() {
  els.board.className = "board board-grid";
  const projects = visibleProjects();
  if (projects.length === 0) {
    els.board.innerHTML = `<div class="empty-state">No projects match the current filters.</div>`;
    return;
  }
  const epicCounts = epicCountsByProject();
  const taskCounts = taskCountsByProject();
  els.board.innerHTML = projects.map((project) => {
    const f = project.fields;
    return `
      <article class="work-card project-work-card" data-kind="project" data-id="${escapeHtml(f.id)}">
        <div class="work-card-head">
          <span class="project-id">${escapeHtml(f.id)}</span>
          <span class="status-chip status-${escapeHtml(f.status)}">${escapeHtml(f.status)}</span>
        </div>
        <h2>${escapeHtml(f.title)}</h2>
        <p>${escapeHtml(f.kind || "other")} &middot; ${escapeHtml(f.target || "no target")}</p>
        <div class="work-card-stats">
          <span>${epicCounts.get(f.id) || 0} epics</span>
          <span>${taskCounts.get(f.id) || 0} tasks</span>
          <span>${escapeHtml(f.priority || "P2")}</span>
        </div>
      </article>
    `;
  }).join("");
  wireWorkCards();
}

function renderEpicsView() {
  els.board.className = "board board-grid";
  const epics = visibleEpics();
  if (epics.length === 0) {
    els.board.innerHTML = `<div class="empty-state">No epics match the current filters.</div>`;
    return;
  }
  const taskCounts = new Map();
  for (const task of state.board.tasks) {
    const epic = task.fields.epic || "";
    taskCounts.set(epic, (taskCounts.get(epic) || 0) + 1);
  }
  els.board.innerHTML = epics.map((epic) => {
    const f = epic.fields;
    const project = findDoc("project", f.project);
    return `
      <article class="work-card epic-work-card" data-kind="epic" data-id="${escapeHtml(f.id)}">
        <div class="work-card-head">
          <span class="project-id">${escapeHtml(f.id)}</span>
          <span class="status-chip status-${escapeHtml(f.status)}">${escapeHtml(f.status)}</span>
        </div>
        <h2>${escapeHtml(f.title)}</h2>
        <p>${escapeHtml(project ? project.fields.title : (f.project || "No project"))}</p>
        <div class="work-card-stats">
          <span>${taskCounts.get(f.id) || 0} tasks</span>
          <span>${escapeHtml(f.priority || "P2")}</span>
          <span>${escapeHtml(f.project || "no-project")}</span>
        </div>
      </article>
    `;
  }).join("");
  wireWorkCards();
}

function renderColumnEmpty(status) {
  const item = document.createElement("div");
  item.className = "column-empty";
  item.textContent = status === "backlog" ? "No scoped backlog tasks." : `No ${status} tasks.`;
  return item;
}

function wireWorkCards() {
  els.board.querySelectorAll(".work-card").forEach((card) => {
    card.addEventListener("click", () => openEditor(card.dataset.kind, card.dataset.id).catch(() => {}));
  });
}

function renderCard(task) {
  const f = task.fields;
  const project = projectForTask(task);
  const card = document.createElement("article");
  card.className = `card prio-${f.priority} status-${f.status}`;
  card.draggable = true;
  card.dataset.id = f.id;
  const title = document.createElement("div");
  title.className = "title";
  title.textContent = f.title;
  const meta = document.createElement("div");
  meta.className = "meta";
  const pills = [f.id, f.priority];
  if (project) pills.push(project);
  if (f.epic) pills.push(f.epic);
  for (const tag of f.tags || []) pills.push("#" + tag);
  meta.innerHTML = pills.map((p) => `<span class="pill">${escapeHtml(p)}</span>`).join("");
  card.append(title, meta);
  card.addEventListener("click", () => openEditor("task", f.id).catch(() => {}));
  card.addEventListener("dragstart", (ev) => {
    state.dragging = true;
    ev.dataTransfer.setData("text/plain", f.id);
    ev.dataTransfer.effectAllowed = "move";
  });
  card.addEventListener("dragend", () => { state.dragging = false; });
  return card;
}

function wireDropTarget(column, status) {
  column.addEventListener("dragover", (ev) => {
    ev.preventDefault();
    column.classList.add("drag-over");
  });
  column.addEventListener("dragleave", () => column.classList.remove("drag-over"));
  column.addEventListener("drop", async (ev) => {
    ev.preventDefault();
    column.classList.remove("drag-over");
    const id = ev.dataTransfer.getData("text/plain");
    if (!id) return;
    const doc = findDoc("task", id);
    try {
      await api(`/api/tasks/${id}`, {
        method: "PATCH",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ fields: { status }, rev: doc ? doc.rev : undefined }),
      });
    } catch (err) {
      if (err.status !== 409) throw err;
    }
    await refresh(true);
  });
}

// --- editor -------------------------------------------------------------------

function fillSelect(select, values, current) {
  select.innerHTML = values.map((v) => `<option value="${v}">${v}</option>`).join("");
  select.value = current;
}

function findDoc(kind, id) {
  const pool = kind === "task" ? state.board.tasks : (kind === "epic" ? state.board.epics : state.board.projects);
  return pool.find((d) => d.fields.id === id) || null;
}

async function loadFullDoc(kind, id) {
  if (!id) return null;
  const collection = kind === "task" ? "tasks" : (kind === "project" ? "projects" : "epics");
  const existing = findDoc(kind, id);
  if (existing && existing.body !== undefined) {
    return existing;
  }
  const doc = await api(`/api/${collection}/${id}`);
  const pool = kind === "task" ? state.board.tasks : (kind === "epic" ? state.board.epics : state.board.projects);
  const index = pool.findIndex((item) => item.fields.id === id);
  if (index >= 0) {
    pool[index] = doc;
  }
  return doc;
}

async function openEditor(kind, id) {
  const isTask = kind === "task";
  const isProject = kind === "project";
  const doc = id ? await loadFullDoc(kind, id) : null;
  state.editing = { kind, id, rev: doc ? doc.rev : undefined };
  els.editorId.textContent = id || (isTask ? "new task" : (isProject ? "new project" : "new epic"));
  els.fTitle.value = doc ? doc.fields.title : "";
  const statuses = isTask ? state.board.taskStatuses : (isProject ? state.board.projectStatuses : state.board.epicStatuses);
  fillSelect(els.fStatus, statuses,
    doc ? doc.fields.status : (isTask ? "idea" : "active"));
  fillSelect(els.fPriority, state.board.priorities, doc ? (doc.fields.priority || "P2") : "P2");
  const currentProject = doc ? (doc.fields.project || (isTask ? projectForTask(doc) : "")) : state.filterProject;
  els.projectFieldWrap.style.display = isProject ? "none" : "";
  if (!isProject) {
    const options = ['<option value="">(none)</option>'];
    for (const p of selectableProjects(currentProject)) {
      options.push(`<option value="${p.fields.id}">${p.fields.id} ${escapeHtml(p.fields.title)}</option>`);
    }
    els.fProject.innerHTML = options.join("");
    els.fProject.value = currentProject;
  }
  els.epicFieldWrap.style.display = isTask ? "" : "none";
  if (isTask) {
    renderEditorEpicOptions(doc ? doc.fields.epic : state.filterEpic);
    els.fEpic.value = doc ? (doc.fields.epic || "") : state.filterEpic;
  }
  els.projectKindFieldWrap.style.display = isProject ? "" : "none";
  els.targetFieldWrap.style.display = isProject ? "" : "none";
  if (isProject) {
    fillSelect(els.fProjectKind, state.board.projectKinds, doc ? (doc.fields.kind || "other") : "other");
    els.fTarget.value = doc ? (doc.fields.target || "") : "";
  }
  els.fTags.value = doc ? (doc.fields.tags || []).join(", ") : "";
  els.fBody.value = doc ? doc.body : "";
  renderBodyPreview();
  els.editorMeta.textContent = doc ? `created ${doc.fields.created} / updated ${doc.fields.updated}` : "";
  els.editor.showModal();
  els.fTitle.focus();
}

function renderEditorEpicOptions(currentEpicId = "") {
  const epics = selectableEpics(currentEpicId, els.fProject.value);
  const options = ['<option value="">(none)</option>'];
  for (const e of epics) {
    options.push(`<option value="${e.fields.id}">${e.fields.id} ${escapeHtml(e.fields.title)}</option>`);
  }
  els.fEpic.innerHTML = options.join("");
  els.fEpic.value = epics.some((e) => e.fields.id === currentEpicId) ? currentEpicId : "";
}

async function saveEditor() {
  const { kind, id, rev } = state.editing;
  const isTask = kind === "task";
  const isProject = kind === "project";
  const fields = {
    title: els.fTitle.value.trim(),
    status: els.fStatus.value,
    priority: els.fPriority.value,
    tags: els.fTags.value.split(",").map((s) => s.trim()).filter(Boolean),
  };
  if (!isProject) {
    fields.project = els.fProject.value;
  }
  if (isTask) {
    fields.epic = els.fEpic.value;
  }
  if (isProject) {
    fields.kind = els.fProjectKind.value;
    fields.target = els.fTarget.value.trim();
  }
  const body = els.fBody.value;
  const collection = isTask ? "tasks" : (isProject ? "projects" : "epics");
  if (id) {
    await api(`/api/${collection}/${id}`, {
      method: "PATCH",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ fields, body, rev }),
    });
  } else {
    await api(`/api/${collection}`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ...fields, body: body || undefined }),
    });
  }
  state.editing = null;
  await refresh(true);
}

// --- wiring -------------------------------------------------------------------

els.viewTabs.forEach((tab) => {
  tab.addEventListener("click", () => {
    state.view = tab.dataset.view || "board";
    render();
  });
});
els.search.addEventListener("input", () => { state.search = els.search.value; render(); });
els.projectFilter.addEventListener("change", () => {
  state.filterProject = els.projectFilter.value;
  if (state.filterEpic) {
    const epic = findDoc("epic", state.filterEpic);
    if (epic && state.filterProject && epic.fields.project !== state.filterProject) {
      state.filterEpic = "";
    }
  }
  render();
});
els.epicFilter.addEventListener("change", () => { state.filterEpic = els.epicFilter.value; render(); });
els.showClosed.addEventListener("change", () => { state.showClosed = els.showClosed.checked; render(); });
els.newTask.addEventListener("click", () => openEditor("task", null).catch(() => {}));
els.newEpic.addEventListener("click", () => openEditor("epic", null).catch(() => {}));
els.newProject.addEventListener("click", () => openEditor("project", null).catch(() => {}));
els.fProject.addEventListener("change", () => {
  if (state.editing && state.editing.kind === "task") {
    renderEditorEpicOptions("");
  }
});
els.fEpic.addEventListener("change", () => {
  const epic = findDoc("epic", els.fEpic.value);
  if (epic && epic.fields.project) {
    els.fProject.value = epic.fields.project;
    renderEditorEpicOptions(epic.fields.id);
  }
});
els.fBody.addEventListener("input", renderBodyPreview);
els.editorCancel.addEventListener("click", () => { state.editing = null; els.editor.close(); });
els.editorForm.addEventListener("submit", (ev) => {
  ev.preventDefault();
  saveEditor().then(() => els.editor.close()).catch(() => {});
});
els.editor.addEventListener("close", () => { state.editing = null; });

setInterval(() => {
  if (!state.editing && !state.dragging) {
    refresh().catch(() => {});
  }
}, 4000);

refresh(true).catch((err) => {
  els.board.innerHTML = `<div class="empty-hint">Failed to load board: ${escapeHtml(err.message)}</div>`;
});
