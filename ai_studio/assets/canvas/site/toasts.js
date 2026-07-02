// Feedback layer — the Figma-like toast stack that replaced the bottom status bar.
//
// A fixed stack of toast cards sits at the bottom-right of the canvas working area
// (clear of the right inspector + its Export button, and of the top-center breadcrumb
// chips). Kinds:
//   success / info  — a transient confirmation; auto-hides ~3s (hover pauses the timer)
//   error           — persists until dismissed (×); carries the op name + message.
//                     Errors are NEVER silently swallowed (every red status became one)
//   pinned          — an export/render result; persists until dismissed, carries the
//                     "Exported N files…" line + download links (multiple results stack)
//   progress        — a long op in flight: a spinner + label, and while still QUEUED a ×
//                     that cancels it before it fires. Resolves in place into the result.
//
// Toasts never block input: the container is pointer-events:none, only the cards are
// interactive; keyboard focus is untouched. The stack caps at MAX_VISIBLE — when a new
// toast would exceed it, the oldest TRANSIENT (info/success) toast is dropped, so errors
// and results are never auto-evicted.
//
// This module is import-safe in node (no DOM touched at module load — the region-state
// test imports app.js which imports this file); every `document` access is lazy, inside
// a function that only runs in the browser.
import { LongOpQueue } from "./long_op_queue.mjs";

const AUTO_HIDE_MS = 3000;
const LEAVE_MS = 220; // fade-out transition
const MAX_VISIBLE = 5;

let container = null;
const live = []; // live toast handles, oldest first

function ensureContainer() {
  if (container) return container;
  container = document.createElement("div");
  container.id = "toast-stack";
  container.setAttribute("aria-live", "polite");
  document.body.appendChild(container);
  return container;
}

// ---- one toast card ----------------------------------------------------------

function makeSpinner() {
  const s = document.createElement("span");
  s.className = "toast-spinner";
  s.setAttribute("aria-hidden", "true");
  return s;
}

function makeIcon(kind) {
  const i = document.createElement("span");
  i.className = "toast-icon";
  i.setAttribute("aria-hidden", "true");
  i.textContent = kind === "success" ? "✓" : kind === "error" ? "!" : kind === "pinned" ? "⬇" : "•";
  return i;
}

function enforceCap() {
  const overflow = live.length - MAX_VISIBLE;
  if (overflow <= 0) return;
  // Drop the oldest TRANSIENT toasts (info/success); never auto-evict errors/pinned/progress.
  let dropped = 0;
  for (const toast of [...live]) {
    if (dropped >= overflow) break;
    if (toast.kind === "info" || toast.kind === "success") {
      dismiss(toast);
      dropped += 1;
    }
  }
}

function scheduleHide(toast) {
  clearTimeout(toast.timer);
  toast.timer = null;
  if (toast.sticky) return;
  toast.timer = setTimeout(() => dismiss(toast), AUTO_HIDE_MS);
}

function dismiss(toast) {
  if (toast.gone) return;
  toast.gone = true;
  clearTimeout(toast.timer);
  const index = live.indexOf(toast);
  if (index !== -1) live.splice(index, 1);
  toast.card.classList.add("toast-leaving");
  setTimeout(() => toast.card.remove(), LEAVE_MS);
}

function createToast() {
  ensureContainer();
  const card = document.createElement("div");
  card.className = "toast";
  container.appendChild(card);
  const toast = { card, kind: null, sticky: true, timer: null, paused: false, gone: false };
  // Hover pauses the auto-hide timer; leaving restarts it (only matters for transient toasts).
  card.addEventListener("mouseenter", () => {
    if (toast.timer) {
      clearTimeout(toast.timer);
      toast.timer = null;
      toast.paused = true;
    }
  });
  card.addEventListener("mouseleave", () => {
    if (toast.paused) {
      toast.paused = false;
      scheduleHide(toast);
    }
  });
  live.push(toast);
  enforceCap();
  return toast;
}

// (Re)render a toast's content + behavior in place. `spec`:
//   { kind, message, label?, links?, spinner?, sticky?, dismissable?, cancel? }
// `cancel` (a function) shows a × that runs it (used for a queued long op); `dismissable`
// shows a × that just hides the toast; `links` is [{ href, label }] for a pinned result.
function renderToast(toast, spec) {
  const { kind, message, label, links, spinner, sticky = true, dismissable = false, cancel = null } = spec;
  toast.kind = kind;
  toast.sticky = sticky;
  toast.card.className = `toast toast-${kind}`;
  toast.card.replaceChildren();

  if (spinner) toast.card.appendChild(makeSpinner());
  else toast.card.appendChild(makeIcon(kind));

  const body = document.createElement("div");
  body.className = "toast-body";
  if (label) {
    const lab = document.createElement("div");
    lab.className = "toast-label";
    lab.textContent = label;
    body.appendChild(lab);
  }
  const msg = document.createElement("div");
  msg.className = "toast-message";
  msg.textContent = message || "";
  body.appendChild(msg);

  if (Array.isArray(links) && links.length) {
    const row = document.createElement("div");
    row.className = "toast-links";
    for (const link of links) {
      const a = document.createElement("a");
      a.href = link.href;
      a.textContent = link.label;
      a.target = "_blank";
      a.rel = "noreferrer";
      a.className = "toast-dl";
      row.appendChild(a);
    }
    body.appendChild(row);
  }
  toast.card.appendChild(body);

  if (cancel || dismissable) {
    const close = document.createElement("button");
    close.type = "button";
    close.className = "toast-close";
    close.setAttribute("aria-label", cancel ? "Cancel" : "Dismiss");
    close.textContent = "×";
    close.addEventListener("click", () => (cancel ? cancel() : dismiss(toast)));
    toast.card.appendChild(close);
  }

  scheduleHide(toast);
  return toast;
}

// ---- public: one-shot toasts (the setStatus / setStatusLinks shim targets) ----

export function toastInfo(message) {
  return renderToast(createToast(), { kind: "info", message, sticky: false, dismissable: true });
}

export function toastSuccess(message) {
  return renderToast(createToast(), { kind: "success", message, sticky: false, dismissable: true });
}

// `label` is the op name (shown above the message) when known; a bare shim error has none.
export function toastError(message, label) {
  return renderToast(createToast(), { kind: "error", message, label, sticky: true, dismissable: true });
}

export function toastPinned(message, links = []) {
  return renderToast(createToast(), { kind: "pinned", message, links, sticky: true, dismissable: true });
}

// ---- public: long-op runner (limiter + progress→result toast + control disable) ----

const longOps = new LongOpQueue({ max: 2 });

// Turn a long-op's resolved value into a result toast on its progress card.
function transitionToResult(toast, res) {
  if (!res) {
    dismiss(toast); // nothing to show
    return;
  }
  if (res.kind === "pinned") {
    renderToast(toast, { kind: "pinned", message: res.message, links: res.links || [], sticky: true, dismissable: true });
  } else if (res.kind === "info") {
    renderToast(toast, { kind: "info", message: res.message, sticky: false, dismissable: true });
  } else {
    renderToast(toast, { kind: "success", message: res.message, sticky: false, dismissable: true });
  }
}

// Run a python-backed op through the limiter with feedback. `fn` is an async `() =>
// resultSpec` (see transitionToResult) — it may call applyMutation() itself; its return
// value drives the RESULT toast. `control` (optional) is the button/element that
// triggered it, disabled while queued+running. Returns a promise that resolves when the
// op settles or is cancelled (it never rejects — failures surface as an error toast).
export function runLongOp(label, fn, { control } = {}) {
  if (control) control.disabled = true;
  const toast = renderToast(createToast(), { kind: "progress", message: label, spinner: true, sticky: true });
  const reenable = () => {
    if (control) control.disabled = false;
  };
  const handle = {};
  return new Promise((resolve) => {
    handle.id = longOps.submit({
      label,
      run: fn,
      onStart: () => {
        // Running now (a slot was free / just freed): plain spinner + label, no cancel.
        renderToast(toast, { kind: "progress", message: label, spinner: true, sticky: true });
      },
      onQueue: (position) => {
        // Still waiting: show its place in line and a × that cancels it before it fires.
        renderToast(toast, {
          kind: "progress",
          message: `Queued: ${label} (#${position})`,
          spinner: true,
          sticky: true,
          cancel: () => {
            if (longOps.cancel(handle.id)) {
              dismiss(toast);
              reenable();
              resolve();
            }
          },
        });
      },
      onSettled: (err, res) => {
        reenable();
        if (err) {
          renderToast(toast, { kind: "error", label, message: err.message || String(err), sticky: true, dismissable: true });
        } else {
          transitionToResult(toast, res);
        }
        resolve();
      },
    });
  });
}
