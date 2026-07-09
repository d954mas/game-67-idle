// AI chat panel (T0242 increment 4): a collapsible right-side column that lets the lead
// type a Russian/English request and have an agent perform it on the LIVE project through
// the exact same ops layer the page itself uses — the chat NEVER mutates project.json
// directly, it only talks to /api/chat/ (chat/api.mjs spawns a codex-exec CLI driver;
// see chat/README.md for the parity law). This module is a thin renderer over that API,
// the same stance history_panel.js takes over listHistory/jumpHistory.
//
// State model (design doc tmp/design_T0242_chat_panel_2026-07-03.md, REVISIONS R1/R3):
//   - One chat CONVERSATION = one codex session, held server-side (chat/agent.mjs). This
//     module never sees the session id beyond what final/error echo back for logging.
//   - The per-project transcript.jsonl is PANEL DISPLAY ONLY (chat/context.mjs) — loaded on
//     open/project-switch via GET .../transcript, then EXTENDED locally (not re-fetched) as
//     SSE events stream in during a turn, so ephemeral turn-only data (the attached selection
//     refs, the denied-verb flags) can be shown even though the server never persists them.
//   - "Clear context" archives the transcript + mints a new session (server-side); the panel
//     just resets its local turn list and shows a note line.
//
// Panel visibility is VIEW-STATE only (localStorage, like the history palette), hidden by
// default. Unlike the history palette (a floating overlay inside #stage), this panel is a
// real flex sibling of #inspector in #ws-body (canvas.html) — "the inspector keeps the right
// side; the chat panel slides in as a sibling column" (design doc section 4) — so History,
// Inspector, and Chat can all be open together with no z-index fights.
//
// This module is NOT wired into app.js's shared refresh() bus (app.js is out of this
// increment's file ownership, and every trigger this panel needs — project open/switch,
// its own toggle, its own SSE stream — is already reachable directly from here or from
// canvas.js, which calls renderChat() at the same points it calls refresh()).
import { el, elementById, groupById, reloadProject, setStatus, state } from "./app.js";
import { canvasRefBase, canvasStoreHeaders, projectKey } from "./store_scope.js";

const OPEN_KEY = "canvas.chatOpen";
const CHAT_BASE = "/api/chat";

let open = false;
let turns = []; // display records: {role:"user"|"assistant"|"note", text, at, seqRange?, refs?, flags?, pending?, pendingMessage?, error?, cancelled?}
let loadedProjectKey = null; // which store-qualified project's transcript `turns` currently reflects
let sending = false;
let cancelledByUser = false; // distinguishes a Cancel-click error event from a genuine failure
let clearConfirmOpen = false;

function panelEl() {
  return el("chat-panel");
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
  const button = el("chat-toggle");
  if (button) {
    button.classList.toggle("active", open);
    button.setAttribute("aria-pressed", open ? "true" : "false");
  }
  const panel = panelEl();
  if (panel) panel.classList.toggle("hidden", !open);
}

// ---- transport: small JSON helper (transcript/cancel/clear) + raw SSE POST (message) -----

async function chatApi(method, path, body) {
  const res = await fetch(`${CHAT_BASE}${path}`, {
    method,
    headers: canvasStoreHeaders(state.storeId, body ? { "content-type": "application/json" } : {}),
    body: body ? JSON.stringify(body) : undefined,
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || res.statusText);
  return data;
}

// Parses one `event: <name>\ndata: <json>` block (chat/api.mjs's sseEvent format).
// Tolerates a torn/partial block (never throws) so a chunk boundary mid-event just waits
// for more bytes rather than crashing the reader loop.
function dispatchSseBlock(raw, handlers) {
  let eventName = "message";
  const dataLines = [];
  for (const line of raw.split("\n")) {
    if (line.startsWith("event:")) eventName = line.slice(6).trim();
    else if (line.startsWith("data:")) dataLines.push(line.slice(5).trim());
  }
  if (!dataLines.length) return;
  let data;
  try {
    data = JSON.parse(dataLines.join("\n"));
  } catch {
    return; // torn event — ignore, the stream is append-only text so this cannot recur
  }
  const handler = handlers[eventName];
  if (handler) handler(data);
}

// The repo has no EventSource-with-POST helper (EventSource is GET-only) — this reads the
// fetch Response body as a stream and splits it on the SSE blank-line block separator by
// hand. `handlers` keys are event names (progress / op-committed / final / error).
async function streamChatMessage(projectId, body, handlers) {
  const res = await fetch(`${CHAT_BASE}/projects/${encodeURIComponent(projectId)}/message`, {
    method: "POST",
    headers: canvasStoreHeaders(state.storeId, { "content-type": "application/json" }),
    body: JSON.stringify(body),
  });
  if (!res.body) throw new Error("chat: streaming is not supported by this browser/response");
  const reader = res.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    buffer += decoder.decode(value, { stream: true });
    let sep;
    while ((sep = buffer.indexOf("\n\n")) !== -1) {
      const block = buffer.slice(0, sep);
      buffer = buffer.slice(sep + 2);
      dispatchSseBlock(block, handlers);
    }
  }
}

// ---- ref formatting (mirrors context_menu.js's Copy ID / chat/context.mjs's formatSelectionRef byte-for-byte) --

function projectRefBase() {
  const project = state.project;
  return project ? canvasRefBase(project) : null;
}

function elementRefFor(id) {
  const base = projectRefBase();
  const element = elementById(id);
  if (!base || !element) return null;
  const ref = `${base.uri}/element/${element.id}`;
  return base.private ? ref : `${ref} — project "${base.title}", element "${element.name || element.id}"`;
}

function groupRefFor(id) {
  const base = projectRefBase();
  const group = groupById(id);
  if (!base || !group) return null;
  const ref = `${base.uri}/group/${group.id}`;
  return base.private ? ref : `${ref} — project "${base.title}", group "${group.name || "Group"}"`;
}

// Shortens a full ref string to just the "element/group "Name"" tail for a compact chip —
// the project title is already implied by the open project, no need to repeat it.
function refChipLabel(ref) {
  const match = ref.match(/,\s*(element|group|region)\s+"([^"]*)"/);
  return match ? `${match[1]} "${match[2]}"` : ref;
}

// The current selection as chat/context.mjs's formatSelectionRef target shape
// ({kind:"element"|"group", id}) — the exact payload buildChatContext resolves server-side.
function selectionTargets() {
  return [
    ...[...state.selectedIds].map((id) => ({ kind: "element", id })),
    ...[...state.selectedGroupIds].map((id) => ({ kind: "group", id })),
  ];
}

function selectionRefs() {
  return [...[...state.selectedIds].map(elementRefFor), ...[...state.selectedGroupIds].map(groupRefFor)].filter(Boolean);
}

// ---- rendering ----------------------------------------------------------------------

function renderTurn(turn) {
  if (turn.role === "note") {
    const note = document.createElement("div");
    note.className = "chat-note";
    note.textContent = turn.text;
    return note;
  }

  const row = document.createElement("div");
  row.className = `chat-turn chat-turn-${turn.role}`;
  if (turn.error) row.classList.add(turn.cancelled ? "chat-cancelled" : "chat-error");

  if (turn.refs && turn.refs.length) {
    const chips = document.createElement("div");
    chips.className = "chat-refs";
    for (const ref of turn.refs) {
      const chip = document.createElement("span");
      chip.className = "chat-ref-chip";
      chip.textContent = refChipLabel(ref);
      chip.title = ref;
      chips.appendChild(chip);
    }
    row.appendChild(chips);
  }

  const bubble = document.createElement("div");
  bubble.className = "chat-bubble";
  if (turn.pending) {
    bubble.classList.add("chat-pending");
    const spinner = document.createElement("span");
    spinner.className = "chat-spinner";
    spinner.setAttribute("aria-hidden", "true");
    bubble.appendChild(spinner);
    const label = document.createElement("span");
    label.textContent = turn.pendingMessage || "Working…";
    bubble.appendChild(label);
  } else {
    const text = document.createElement("div");
    text.className = "chat-text";
    text.textContent = turn.text;
    bubble.appendChild(text);
  }
  row.appendChild(bubble);

  if (turn.cancelled) {
    const note = document.createElement("div");
    note.className = "chat-cancel-note";
    note.textContent = "Cancelled — the codex session survives; send another message to continue.";
    row.appendChild(note);
  }

  if (turn.seqRange) {
    const chip = document.createElement("span");
    chip.className = "chat-ops-chip";
    chip.textContent = `ops ${turn.seqRange[0]}-${turn.seqRange[1]}`;
    chip.title = "Journal entries this reply created — see the History panel";
    row.appendChild(chip);
  }

  if (turn.flags && turn.flags.length) {
    for (const flag of turn.flags) {
      const warn = document.createElement("div");
      warn.className = "chat-flag";
      warn.textContent = `⚠ ${flag}`;
      row.appendChild(warn);
    }
  }

  return row;
}

function renderStream() {
  const streamEl = el("chat-stream");
  if (!streamEl) return;
  streamEl.replaceChildren();
  if (!state.project) {
    const empty = document.createElement("div");
    empty.className = "chat-empty";
    empty.textContent = "Open a project to chat with the AI agent.";
    streamEl.appendChild(empty);
  } else if (!turns.length) {
    const empty = document.createElement("div");
    empty.className = "chat-empty";
    empty.textContent = "No messages yet — ask the agent to act on the selection.";
    streamEl.appendChild(empty);
  } else {
    for (const turn of turns) streamEl.appendChild(renderTurn(turn));
    streamEl.scrollTop = streamEl.scrollHeight;
  }
  syncComposer();
}

function syncComposer() {
  const hasProject = Boolean(state.project);
  const input = el("chat-input");
  const sendBtn = el("chat-send");
  const cancelBtn = el("chat-cancel");
  const clearBtn = el("chat-clear");
  if (input) input.disabled = sending || !hasProject;
  if (sendBtn) sendBtn.disabled = sending || !hasProject;
  if (cancelBtn) cancelBtn.classList.toggle("hidden", !sending);
  if (clearBtn) clearBtn.disabled = !hasProject || sending;
}

// ---- clear-context inline confirm (home.js's two-step project-delete pattern) ------------

function setClearConfirmOpen(next) {
  clearConfirmOpen = next;
  const bar = el("chat-clear-confirm");
  if (bar) bar.classList.toggle("hidden", !clearConfirmOpen);
}

async function performClear() {
  const project = state.project;
  if (!project) return;
  try {
    await chatApi("POST", `/projects/${encodeURIComponent(project.id)}/clear`);
  } catch (error) {
    setStatus(error.message, true);
    return;
  }
  turns = [{ role: "note", text: "New conversation — previous context cleared (transcript archived).", at: new Date().toISOString() }];
  loadedProjectKey = projectKey(project);
  renderStream();
  setStatus("Chat context cleared.");
}

// ---- transcript load (open / project switch) ----------------------------------------

async function loadTranscript(project) {
  const projectId = project.id;
  const expectedKey = projectKey(project);
  let data;
  try {
    data = await chatApi("GET", `/projects/${encodeURIComponent(projectId)}/transcript`);
  } catch (error) {
    setStatus(error.message, true);
    return;
  }
  // A project switch mid-fetch (fast double-click) must not paint the wrong project's
  // transcript — only adopt the result if it is still the panel's current project.
  if (!state.project || projectKey(state.project) !== expectedKey) return;
  turns = (data.transcript || []).map((entry) => ({ ...entry }));
  loadedProjectKey = expectedKey;
  renderStream();
}

// ---- send / cancel -------------------------------------------------------------------

async function sendMessage() {
  const project = state.project;
  const input = el("chat-input");
  if (!project || !input || sending) return;
  const text = input.value.trim();
  if (!text) return;
  input.value = "";

  const refs = selectionRefs();
  const userTurn = { role: "user", text, at: new Date().toISOString(), refs };
  const assistantTurn = { role: "assistant", text: "", pending: true, pendingMessage: "Working…" };
  turns.push(userTurn, assistantTurn);
  sending = true;
  cancelledByUser = false;
  renderStream();

  try {
    await streamChatMessage(project.id, { text, selection: selectionTargets() }, {
      progress: (data) => {
        assistantTurn.pendingMessage = data.message || "Working…";
        renderStream();
      },
      "op-committed": () => {
        void reloadProject();
      },
      final: (data) => {
        assistantTurn.pending = false;
        assistantTurn.text = data.text;
        assistantTurn.seqRange = data.seqRange || null;
        assistantTurn.flags = data.flags || [];
        assistantTurn.at = new Date().toISOString();
        renderStream();
        // Belt-and-braces alongside the op-committed handler above (design doc: "on each
        // streamed op-committed event and on turn-end") — reloadProject() is a cheap GET,
        // a second call when both fire for the same turn is harmless.
        if (data.seqRange) void reloadProject();
      },
      error: (data) => {
        assistantTurn.pending = false;
        assistantTurn.error = true;
        assistantTurn.cancelled = cancelledByUser;
        assistantTurn.text = data.message;
        assistantTurn.at = new Date().toISOString();
        renderStream();
        if (!cancelledByUser) setStatus(data.message, true);
      },
    });
  } catch (error) {
    assistantTurn.pending = false;
    assistantTurn.error = true;
    assistantTurn.cancelled = cancelledByUser;
    assistantTurn.text = error.message;
    renderStream();
    if (!cancelledByUser) setStatus(error.message, true);
  } finally {
    sending = false;
    cancelledByUser = false;
    syncComposer();
  }
}

async function cancelTurn() {
  const project = state.project;
  if (!project || !sending) return;
  cancelledByUser = true; // the error SSE event this triggers renders as a cancel note, not a red error
  try {
    await chatApi("POST", `/projects/${encodeURIComponent(project.id)}/cancel`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

function onComposerKeydown(event) {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    void sendMessage();
  }
}

// ---- public surface -------------------------------------------------------------------

function setOpen(next) {
  open = next;
  saveOpen();
  syncToggle();
  renderChat();
}

export function toggleChatPanel() {
  setOpen(!open);
}

// canvas.js calls this at the same points it calls refresh()/renderHome(): after opening a
// project, after switching/closing one (showHome), and once at boot. Fetches the transcript
// only when the panel is open AND the loaded project changed — closed stays lazy, exactly
// like history_panel.js's own render().
export function renderChat() {
  syncToggle();
  const project = state.project;
  if (!project) {
    turns = [];
    loadedProjectKey = null;
    setClearConfirmOpen(false);
    renderStream();
    return;
  }
  if (!open) return; // load lazily on next open
  if (loadedProjectKey !== projectKey(project)) {
    setClearConfirmOpen(false);
    void loadTranscript(project);
    return; // loadTranscript() renders once it resolves
  }
  renderStream();
}

export function initChat() {
  loadOpen();
  el("chat-toggle")?.addEventListener("click", () => setOpen(!open));
  el("chat-close")?.addEventListener("click", () => setOpen(false));
  el("chat-send")?.addEventListener("click", () => void sendMessage());
  el("chat-cancel")?.addEventListener("click", () => void cancelTurn());
  el("chat-input")?.addEventListener("keydown", onComposerKeydown);
  el("chat-clear")?.addEventListener("click", () => setClearConfirmOpen(true));
  el("chat-clear-yes")?.addEventListener("click", () => {
    setClearConfirmOpen(false);
    void performClear();
  });
  el("chat-clear-no")?.addEventListener("click", () => setClearConfirmOpen(false));
  syncToggle();
  syncComposer();
}
