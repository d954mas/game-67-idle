// History panel: a Photoshop-style hideable list of the journal, over the shared
// backend view. The panel is a THIN renderer — it never parses the journal; the op
// layer's listHistory returns fully-labeled rows { seq, label, summary, current,
// undone } (Base + the applied undo chain + the dimmed redo tail), and clicking a row
// calls the ONE jumpHistory op (the same op the CLI's history-jump drives — tool
// parity). Undo/redo/jump keep the undo chain coherent server-side; the panel is just
// the live feedback (like undo/redo, jump is intentionally toast-free).
//
// Panel visibility is VIEW-STATE only (never journaled/persisted into the project): it
// lives in localStorage like the layers-collapse pref, hidden by default. The panel
// renders through the shared refresh bus, so it stays live after every op.
import { api, el, hooks, state } from "./app.js";
import { jumpToHistory } from "./actions.js";

const OPEN_KEY = "canvas.historyOpen";

let open = false;
let entries = []; // last fetched listHistory rows (ordered oldest -> newest)
let lastSig = null; // entry-SET signature (excludes the head) so a head-only move re-highlights instead of rebuilding
let fetchToken = 0; // guards overlapping async list fetches

function panelEl() {
  return el("history-panel");
}

function loadOpen() {
  try {
    open = localStorage.getItem(OPEN_KEY) === "1";
  } catch {
    open = false; // private mode / disabled storage: default hidden
  }
}

function saveOpen() {
  try {
    localStorage.setItem(OPEN_KEY, open ? "1" : "0");
  } catch {
    // Private mode / disabled storage: the toggle still works this session.
  }
}

function syncToggle() {
  const button = el("history-toggle");
  if (button) {
    button.classList.toggle("active", open);
    button.setAttribute("aria-pressed", open ? "true" : "false");
  }
  const panel = panelEl();
  if (panel) panel.classList.toggle("hidden", !open);
}

function setOpen(next) {
  open = next;
  saveOpen();
  syncToggle();
  render(); // fetch + paint on open; on close syncToggle already hid it
}

// Toggle from the toolbar button OR the hotkey (canvas.js). Exported so the controller
// binds one key without duplicating the open/persist logic.
export function toggleHistoryPanel() {
  setOpen(!open);
}

// Re-apply the current/undone classes from a head seq over the CACHED ordered rows —
// the instant Photoshop dimming on undo/redo/jump, before the list re-fetch resolves.
// Rows at/above the current row are applied; rows below (more recent) are the dimmed
// redo tail. A head not yet in the cached rows (a brand-new mutation) yields no current
// row until the refetch rebuilds — that lands within one round-trip.
function applyCurrent(headSeq) {
  const panel = panelEl();
  if (!panel) return;
  const rows = [...panel.querySelectorAll(".history-row")];
  const currentIndex = rows.findIndex((row) => Number(row.dataset.seq) === Number(headSeq));
  rows.forEach((row, index) => {
    row.classList.toggle("current", index === currentIndex);
    row.classList.toggle("undone", currentIndex >= 0 && index > currentIndex);
  });
  const current = rows[currentIndex];
  if (current) current.scrollIntoView({ block: "nearest" });
}

function rebuild(headSeq) {
  const panel = panelEl();
  if (!panel) return;
  const list = panel.querySelector(".history-list");
  if (!list) return;
  list.replaceChildren();
  for (const entry of entries) {
    const row = document.createElement("button");
    row.type = "button";
    row.className = "history-row";
    row.dataset.seq = String(entry.seq);

    const dot = document.createElement("span");
    dot.className = "history-dot";
    dot.setAttribute("aria-hidden", "true");
    row.appendChild(dot);

    const label = document.createElement("span");
    label.className = "history-label";
    label.textContent = entry.label;
    row.appendChild(label);

    if (entry.summary) {
      const summary = document.createElement("span");
      summary.className = "history-summary";
      summary.textContent = entry.summary;
      summary.title = entry.summary;
      row.appendChild(summary);
    }

    // Clicking ANY row jumps the project to the state right after that entry (Base = seq 0
    // = empty). Clicking the current row is a cheap server no-op. One op = one HTTP call.
    row.addEventListener("click", () => jumpToHistory(entry.seq));
    list.appendChild(row);
  }
  applyCurrent(headSeq);
}

// Refresh the labeled list from the backend (GET is not journaled). Guarded against
// overlapping fetches; only rebuilds the DOM when the entry SET changed (a new mutation
// or compaction), otherwise just re-highlights — so Ctrl+Z spam never thrashes the DOM.
async function refreshList() {
  const project = state.project;
  if (!project) return;
  const token = (fetchToken += 1);
  let data;
  try {
    data = await api("GET", `/projects/${project.id}/history-list`);
  } catch {
    return; // transient failure: keep the last good list, never crash the page
  }
  if (token !== fetchToken || !open || state.project !== project) return; // superseded / closed / switched
  entries = data.entries || [];
  const sig = entries.map((entry) => `${entry.seq}:${entry.label}:${entry.summary}`).join("|");
  const hasRows = !!panelEl()?.querySelector(".history-row");
  if (sig === lastSig && hasRows) {
    applyCurrent(data.history_seq);
    return;
  }
  lastSig = sig;
  rebuild(data.history_seq);
}

// The refresh-bus hook: keep the toggle/panel in sync, and when open, re-highlight from
// the already-updated head (op responses carry history.seq) then refresh the list.
export function render() {
  syncToggle();
  if (!open || !state.project) return;
  applyCurrent(state.history?.seq); // instant highlight from the folded head
  void refreshList();
}

export function initHistory() {
  hooks.renderHistory = render;
  loadOpen();
  el("history-toggle")?.addEventListener("click", () => setOpen(!open));
  el("history-close")?.addEventListener("click", () => setOpen(false));
  syncToggle();
}
