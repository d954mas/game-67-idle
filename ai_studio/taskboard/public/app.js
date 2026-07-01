"use strict";

const state = {
  board: null,        // payload from /api/board
  view: "board",
  filterProjects: [],
  filterEpics: [],
  statusFilters: {
    task: ["backlog", "todo", "doing", "review"],
    project: ["active"],
    epic: ["active"],
  },
  search: "",
  editing: null,      // { kind, id } or { kind, id: null } for create
  dragging: false,
  panning: null,
  lastPayload: "",
};

const els = {
  board: document.getElementById("board"),
  viewTabs: Array.from(document.querySelectorAll(".view-tab")),
  search: document.getElementById("search"),
  projectFilterWrap: document.getElementById("projectFilterWrap"),
  projectFilterButton: document.getElementById("projectFilterButton"),
  projectFilterMenu: document.getElementById("projectFilterMenu"),
  epicFilterWrap: document.getElementById("epicFilterWrap"),
  epicFilterButton: document.getElementById("epicFilterButton"),
  epicFilterMenu: document.getElementById("epicFilterMenu"),
  statusFilterWrap: document.getElementById("statusFilterWrap"),
  statusFilterButton: document.getElementById("statusFilterButton"),
  statusFilterMenu: document.getElementById("statusFilterMenu"),
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
const ALL_COLUMNS = ["idea", "backlog", "todo", "doing", "review", "done"];
const ACTIVE_ENTITY_STATUSES = ["active"];

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
    if (!state.statusFilters.task.includes(f.status)) return false;
    if (state.filterProjects.length > 0 && !state.filterProjects.includes(project)) return false;
    if (state.filterEpics.length > 0 && !state.filterEpics.includes(f.epic)) return false;
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
    if (!state.statusFilters.project.includes(f.status)) return false;
    if (state.filterProjects.length > 0 && !state.filterProjects.includes(f.id)) return false;
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
    if (!state.statusFilters.epic.includes(f.status)) return false;
    if (state.filterProjects.length > 0 && !state.filterProjects.includes(f.project)) return false;
    if (state.filterEpics.length > 0 && !state.filterEpics.includes(f.id)) return false;
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
  renderProjectFilter();
  renderEpicFilter();
  renderStatusFilter();
  renderMain();
}

function renderViewTabs() {
  for (const tab of els.viewTabs) {
    const active = tab.dataset.view === state.view;
    tab.classList.toggle("is-active", active);
    tab.setAttribute("aria-selected", active ? "true" : "false");
  }
}

function selectedLabel(kind, selectedIds, docs) {
  if (selectedIds.length === 0) return `${kind}: all`;
  if (selectedIds.length === 1) {
    const doc = docs.find((item) => item.fields.id === selectedIds[0]);
    return `${kind}: ${doc ? doc.fields.title : selectedIds[0]}`;
  }
  return `${kind}: ${selectedIds.length} selected`;
}

function renderMultiFilter(menu, allLabel, docs, selectedIds) {
  const rows = [
    `<label class="filter-option filter-option-all">
      <input type="checkbox" value="" ${selectedIds.length === 0 ? "checked" : ""}>
      <span>${escapeHtml(allLabel)}</span>
    </label>`,
  ];
  for (const doc of docs) {
    const f = doc.fields;
    rows.push(`
      <label class="filter-option">
        <input type="checkbox" value="${escapeHtml(f.id)}" ${selectedIds.includes(f.id) ? "checked" : ""}>
        <span class="filter-option-main">${escapeHtml(f.id)} ${escapeHtml(f.title)}</span>
        <span class="filter-option-meta">${escapeHtml(f.status)}${f.project ? ` &middot; ${escapeHtml(f.project)}` : ""}</span>
      </label>
    `);
  }
  menu.innerHTML = rows.join("");
}

function renderProjectFilter() {
  const projects = selectableProjects(state.filterProjects);
  els.projectFilterButton.textContent = selectedLabel("Projects", state.filterProjects, projects);
  renderMultiFilter(els.projectFilterMenu, "All projects", projects, state.filterProjects);
}

function renderEpicFilter() {
  const epics = selectableFilterEpics();
  els.epicFilterButton.textContent = selectedLabel("Epics", state.filterEpics, epics);
  renderMultiFilter(els.epicFilterMenu, "All epics", epics, state.filterEpics);
}

function statusFilterConfig() {
  if (state.view === "projects") {
    return {
      key: "project",
      statuses: state.board.projectStatuses,
      defaults: ACTIVE_ENTITY_STATUSES,
      defaultLabel: "active",
    };
  }
  if (state.view === "epics") {
    return {
      key: "epic",
      statuses: state.board.epicStatuses,
      defaults: ACTIVE_ENTITY_STATUSES,
      defaultLabel: "active",
    };
  }
  return {
    key: "task",
    statuses: state.board.taskStatuses,
    defaults: ACTIVE_COLUMNS,
    defaultLabel: "working",
  };
}

function sameItems(a, b) {
  return a.length === b.length && a.every((item) => b.includes(item));
}

function statusLabel(selected, statuses, defaults, defaultLabel) {
  if (selected.length === 0) return "Statuses: none";
  if (sameItems(selected, defaults)) return `Statuses: ${defaultLabel}`;
  if (sameItems(selected, statuses)) return "Statuses: all";
  if (selected.length === 1) return `Statuses: ${selected[0]}`;
  return `Statuses: ${selected.length} selected`;
}

function renderStatusFilter() {
  const config = statusFilterConfig();
  const selected = state.statusFilters[config.key];
  els.statusFilterButton.textContent = statusLabel(selected, config.statuses, config.defaults, config.defaultLabel);
  const rows = [
    `<label class="filter-option filter-option-all">
      <input type="checkbox" value="__all" ${sameItems(selected, config.statuses) ? "checked" : ""}>
      <span>All statuses</span>
    </label>`,
  ];
  for (const status of config.statuses) {
    rows.push(`
    <label class="filter-option">
      <input type="checkbox" value="${escapeHtml(status)}" ${selected.includes(status) ? "checked" : ""}>
      <span class="filter-option-main">${escapeHtml(status)}</span>
    </label>
  `);
  }
  els.statusFilterMenu.innerHTML = rows.join("");
}

function selectableProjects(currentProjectIds = []) {
  const ids = Array.isArray(currentProjectIds) ? currentProjectIds : [currentProjectIds].filter(Boolean);
  return state.board.projects.filter((p) => state.statusFilters.project.includes(p.fields.status) || ids.includes(p.fields.id));
}

function selectableEpics(currentEpicId = "", projectId = "") {
  return state.board.epics.filter((e) => {
    const statusOk = state.statusFilters.epic.includes(e.fields.status) || e.fields.id === currentEpicId;
    const projectOk = !projectId || e.fields.project === projectId || e.fields.id === currentEpicId;
    return statusOk && projectOk;
  });
}

function selectableFilterEpics() {
  return state.board.epics.filter((e) => {
    const selected = state.filterEpics.includes(e.fields.id);
    const statusOk = state.statusFilters.epic.includes(e.fields.status) || selected;
    const projectOk = state.filterProjects.length === 0 || state.filterProjects.includes(e.fields.project) || selected;
    return statusOk && projectOk;
  });
}

function selectedProjectForCreate() {
  return state.filterProjects.length === 1 ? state.filterProjects[0] : "";
}

function selectedEpicForCreate() {
  return state.filterEpics.length === 1 ? state.filterEpics[0] : "";
}

function syncEpicFiltersWithProjects() {
  if (state.filterProjects.length === 0) return;
  state.filterEpics = state.filterEpics.filter((epicId) => {
    const epic = findDoc("epic", epicId);
    return epic && state.filterProjects.includes(epic.fields.project);
  });
}

function updateSelectedIds(selectedIds, id, checked) {
  if (checked && !selectedIds.includes(id)) selectedIds.push(id);
  if (!checked) {
    const index = selectedIds.indexOf(id);
    if (index >= 0) selectedIds.splice(index, 1);
  }
}

function closeFilterMenus(exceptWrap = null) {
  for (const [wrap, button, menu] of [
    [els.projectFilterWrap, els.projectFilterButton, els.projectFilterMenu],
    [els.epicFilterWrap, els.epicFilterButton, els.epicFilterMenu],
    [els.statusFilterWrap, els.statusFilterButton, els.statusFilterMenu],
  ]) {
    if (wrap === exceptWrap) continue;
    menu.hidden = true;
    button.setAttribute("aria-expanded", "false");
  }
}

function toggleFilterMenu(wrap, button, menu) {
  const nextOpen = menu.hidden;
  closeFilterMenus(nextOpen ? wrap : null);
  menu.hidden = !nextOpen;
  button.setAttribute("aria-expanded", nextOpen ? "true" : "false");
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
  const columns = ALL_COLUMNS.filter((status) => state.statusFilters.task.includes(status));
  els.board.className = "board board-kanban";
  if (columns.length === 0) {
    els.board.innerHTML = `<div class="empty-state">No statuses selected.</div>`;
    return;
  }
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

function normalizedWheelDelta(ev) {
  const scale = ev.deltaMode === 1 ? 32 : (ev.deltaMode === 2 ? els.board.clientWidth : 1);
  return { x: ev.deltaX * scale, y: ev.deltaY * scale };
}

function hasHorizontalBoardOverflow() {
  return els.board.classList.contains("board-kanban") && els.board.scrollWidth > els.board.clientWidth + 1;
}

function canScrollVertically(element, deltaY) {
  if (!element || deltaY === 0) return false;
  const maxScrollTop = element.scrollHeight - element.clientHeight;
  if (maxScrollTop <= 1) return false;
  return deltaY < 0 ? element.scrollTop > 0 : element.scrollTop < maxScrollTop - 1;
}

function closestElement(target, selector) {
  return target && typeof target.closest === "function" ? target.closest(selector) : null;
}

function onBoardWheel(ev) {
  if (!hasHorizontalBoardOverflow()) return;

  const delta = normalizedWheelDelta(ev);
  const verticalScroller = closestElement(ev.target, ".cards");
  if (verticalScroller && canScrollVertically(verticalScroller, delta.y)) return;

  const horizontalDelta = Math.abs(delta.x) > Math.abs(delta.y) ? delta.x : delta.y;
  if (horizontalDelta === 0) return;

  const maxScrollLeft = els.board.scrollWidth - els.board.clientWidth;
  const nextScrollLeft = Math.max(0, Math.min(maxScrollLeft, els.board.scrollLeft + horizontalDelta));
  if (nextScrollLeft === els.board.scrollLeft) return;

  ev.preventDefault();
  els.board.scrollLeft = nextScrollLeft;
}

function shouldSkipBoardPan(target) {
  return Boolean(closestElement(target, "a, button, input, select, textarea, label, dialog, .card, .work-card"));
}

function beginBoardPan(ev) {
  if (ev.button !== 0 || ev.pointerType === "touch" || !hasHorizontalBoardOverflow() || shouldSkipBoardPan(ev.target)) {
    return;
  }

  state.panning = {
    pointerId: ev.pointerId,
    startX: ev.clientX,
    scrollLeft: els.board.scrollLeft,
  };
  els.board.classList.add("is-panning");
  els.board.setPointerCapture(ev.pointerId);
}

function moveBoardPan(ev) {
  if (!state.panning || state.panning.pointerId !== ev.pointerId) return;
  ev.preventDefault();
  els.board.scrollLeft = state.panning.scrollLeft - (ev.clientX - state.panning.startX);
}

function endBoardPan(ev) {
  if (!state.panning || state.panning.pointerId !== ev.pointerId) return;
  try {
    els.board.releasePointerCapture(ev.pointerId);
  } catch {
    // Pointer capture can already be gone if the browser cancelled the pointer.
  }
  state.panning = null;
  els.board.classList.remove("is-panning");
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
  const currentProject = doc ? (doc.fields.project || (isTask ? projectForTask(doc) : "")) : selectedProjectForCreate();
  els.projectFieldWrap.style.display = isProject ? "none" : "";
  if (!isProject) {
    const options = ['<option value="">(none)</option>'];
    for (const p of selectableProjects([currentProject])) {
      options.push(`<option value="${p.fields.id}">${p.fields.id} ${escapeHtml(p.fields.title)}</option>`);
    }
    els.fProject.innerHTML = options.join("");
    els.fProject.value = currentProject;
  }
  els.epicFieldWrap.style.display = isTask ? "" : "none";
  if (isTask) {
    const currentEpic = doc ? (doc.fields.epic || "") : selectedEpicForCreate();
    renderEditorEpicOptions(currentEpic);
    els.fEpic.value = currentEpic;
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
els.projectFilterButton.addEventListener("click", () => {
  toggleFilterMenu(els.projectFilterWrap, els.projectFilterButton, els.projectFilterMenu);
});
els.epicFilterButton.addEventListener("click", () => {
  toggleFilterMenu(els.epicFilterWrap, els.epicFilterButton, els.epicFilterMenu);
});
els.statusFilterButton.addEventListener("click", () => {
  toggleFilterMenu(els.statusFilterWrap, els.statusFilterButton, els.statusFilterMenu);
});
els.projectFilterMenu.addEventListener("change", (ev) => {
  const input = ev.target;
  if (!input || input.type !== "checkbox") return;
  if (input.value === "") {
    state.filterProjects = [];
  } else {
    updateSelectedIds(state.filterProjects, input.value, input.checked);
  }
  syncEpicFiltersWithProjects();
  render();
});
els.epicFilterMenu.addEventListener("change", (ev) => {
  const input = ev.target;
  if (!input || input.type !== "checkbox") return;
  if (input.value === "") {
    state.filterEpics = [];
  } else {
    updateSelectedIds(state.filterEpics, input.value, input.checked);
  }
  render();
});
els.statusFilterMenu.addEventListener("change", (ev) => {
  const input = ev.target;
  if (!input || input.type !== "checkbox") return;
  const config = statusFilterConfig();
  if (input.value === "__all") {
    state.statusFilters[config.key] = input.checked ? [...config.statuses] : [...config.defaults];
    render();
    return;
  }
  updateSelectedIds(state.statusFilters[config.key], input.value, input.checked);
  render();
});
document.addEventListener("click", (ev) => {
  if (!closestElement(ev.target, ".multi-filter")) closeFilterMenus();
});
document.addEventListener("keydown", (ev) => {
  if (ev.key === "Escape") closeFilterMenus();
});
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
els.board.addEventListener("wheel", onBoardWheel, { passive: false });
els.board.addEventListener("pointerdown", beginBoardPan);
els.board.addEventListener("pointermove", moveBoardPan);
els.board.addEventListener("pointerup", endBoardPan);
els.board.addEventListener("pointercancel", endBoardPan);

setInterval(() => {
  if (!state.editing && !state.dragging && !state.panning) {
    refresh().catch(() => {});
  }
}, 4000);

refresh(true).catch((err) => {
  els.board.innerHTML = `<div class="empty-hint">Failed to load board: ${escapeHtml(err.message)}</div>`;
});
