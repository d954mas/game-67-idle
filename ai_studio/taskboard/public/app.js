"use strict";

const state = {
  board: null,        // payload from /api/board
  filterEpic: "",
  search: "",
  showClosed: false,
  editing: null,      // { kind, id } or { kind, id: null } for create
  dragging: false,
  lastPayload: "",
};

const els = {
  board: document.getElementById("board"),
  search: document.getElementById("search"),
  epicFilter: document.getElementById("epicFilter"),
  showClosed: document.getElementById("showClosed"),
  newTask: document.getElementById("newTask"),
  newEpic: document.getElementById("newEpic"),
  editor: document.getElementById("editor"),
  editorForm: document.getElementById("editorForm"),
  editorId: document.getElementById("editorId"),
  editorMeta: document.getElementById("editorMeta"),
  editorCancel: document.getElementById("editorCancel"),
  fTitle: document.getElementById("fTitle"),
  fStatus: document.getElementById("fStatus"),
  fPriority: document.getElementById("fPriority"),
  fEpic: document.getElementById("fEpic"),
  fTags: document.getElementById("fTags"),
  fBody: document.getElementById("fBody"),
  bodyPreview: document.getElementById("bodyPreview"),
  epicFieldWrap: document.getElementById("epicFieldWrap"),
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

function visibleTasks() {
  const q = state.search.trim().toLowerCase();
  return state.board.tasks.filter((t) => {
    const f = t.fields;
    if (!state.showClosed && SECONDARY_STATUSES.has(f.status)) return false;
    if (state.filterEpic && f.epic !== state.filterEpic) return false;
    if (q) {
      const hay = `${f.id} ${f.title} ${(f.tags || []).join(" ")} ${f.epic || ""}`.toLowerCase();
      if (!hay.includes(q)) return false;
    }
    return true;
  });
}

function render() {
  renderEpicSelects();
  renderBoard();
}

function renderEpicSelects() {
  const current = state.filterEpic;
  const options = ['<option value="">All epics</option>'];
  for (const e of selectableEpics()) {
    options.push(`<option value="${e.fields.id}">${e.fields.id} ${escapeHtml(e.fields.title)}</option>`);
  }
  els.epicFilter.innerHTML = options.join("");
  els.epicFilter.value = current;
}

function selectableEpics(currentEpicId = "") {
  return state.board.epics.filter((e) => state.showClosed || e.fields.status === "active" || e.fields.id === currentEpicId);
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

function renderBodyPreview() {
  els.bodyPreview.innerHTML = TaskboardMarkdown.renderMarkdown(els.fBody.value);
}

function renderBoard() {
  els.board.innerHTML = "";
  const tasks = visibleTasks();
  const columns = state.showClosed ? ALL_COLUMNS : ACTIVE_COLUMNS;
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
    column.appendChild(cards);
    wireDropTarget(column, status);
    els.board.appendChild(column);
  }
}

function renderCard(task) {
  const f = task.fields;
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
  if (f.epic) pills.push(f.epic);
  for (const tag of f.tags || []) pills.push("#" + tag);
  meta.innerHTML = pills.map((p) => `<span class="pill">${escapeHtml(p)}</span>`).join("");
  card.append(title, meta);
  card.addEventListener("click", () => openEditor("task", f.id));
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
  const pool = kind === "task" ? state.board.tasks : state.board.epics;
  return pool.find((d) => d.fields.id === id) || null;
}

function openEditor(kind, id) {
  const isTask = kind === "task";
  const doc = id ? findDoc(kind, id) : null;
  state.editing = { kind, id, rev: doc ? doc.rev : undefined };
  els.editorId.textContent = id || (isTask ? "new task" : "new epic");
  els.fTitle.value = doc ? doc.fields.title : "";
  fillSelect(els.fStatus, isTask ? state.board.taskStatuses : state.board.epicStatuses,
    doc ? doc.fields.status : (isTask ? "idea" : "active"));
  fillSelect(els.fPriority, state.board.priorities, doc ? (doc.fields.priority || "P2") : "P2");
  els.epicFieldWrap.style.display = isTask ? "" : "none";
  if (isTask) {
    const options = ['<option value="">(none)</option>'];
    for (const e of selectableEpics(doc ? doc.fields.epic : state.filterEpic)) {
      options.push(`<option value="${e.fields.id}">${e.fields.id} ${escapeHtml(e.fields.title)}</option>`);
    }
    els.fEpic.innerHTML = options.join("");
    els.fEpic.value = doc ? (doc.fields.epic || "") : state.filterEpic;
  }
  els.fTags.value = doc ? (doc.fields.tags || []).join(", ") : "";
  els.fBody.value = doc ? doc.body : "";
  renderBodyPreview();
  els.editorMeta.textContent = doc ? `created ${doc.fields.created} · updated ${doc.fields.updated}` : "";
  els.editor.showModal();
  els.fTitle.focus();
}

async function saveEditor() {
  const { kind, id, rev } = state.editing;
  const isTask = kind === "task";
  const fields = {
    title: els.fTitle.value.trim(),
    status: els.fStatus.value,
    priority: els.fPriority.value,
    tags: els.fTags.value.split(",").map((s) => s.trim()).filter(Boolean),
  };
  if (isTask) {
    fields.epic = els.fEpic.value;
  }
  const body = els.fBody.value;
  if (id) {
    await api(`/api/${isTask ? "tasks" : "epics"}/${id}`, {
      method: "PATCH",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ fields, body, rev }),
    });
  } else {
    await api(`/api/${isTask ? "tasks" : "epics"}`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ...fields, body: body || undefined }),
    });
  }
  state.editing = null;
  await refresh(true);
}

// --- wiring -------------------------------------------------------------------

els.search.addEventListener("input", () => { state.search = els.search.value; renderBoard(); });
els.epicFilter.addEventListener("change", () => { state.filterEpic = els.epicFilter.value; render(); });
els.showClosed.addEventListener("change", () => { state.showClosed = els.showClosed.checked; render(); });
els.newTask.addEventListener("click", () => openEditor("task", null));
els.newEpic.addEventListener("click", () => openEditor("epic", null));
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
