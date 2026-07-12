import { setStatus } from "../app.js";

export function readOnly(label, value) {
  const row = document.createElement("div");
  row.className = "insp-ro";
  const span = document.createElement("span");
  span.className = "insp-label";
  span.textContent = label;
  const val = document.createElement("span");
  val.className = "insp-value";
  val.textContent = value;
  row.append(span, val);
  return row;
}

// ---- collapsible section -----------------------------------------------------

const COLLAPSE_KEY = "canvas.inspector.collapsed";

function loadCollapsed() {
  try {
    return JSON.parse(localStorage.getItem(COLLAPSE_KEY) || "{}") || {};
  } catch {
    return {};
  }
}

function isCollapsed(key) {
  return loadCollapsed()[key] === true;
}

function setCollapsed(key, collapsed) {
  const map = loadCollapsed();
  if (collapsed) map[key] = true;
  else delete map[key];
  try {
    localStorage.setItem(COLLAPSE_KEY, JSON.stringify(map));
  } catch {
    // Private mode / disabled storage: collapse still works this session, just no persist.
  }
}

// A titled collapsible section. `key` persists the collapsed state per section
// (shared across selections, not per project). `badge` is an optional trailing
// header node (e.g. a count). Returns the body element to append content to.
export function collapsible(root, key, title, badge) {
  const wrap = document.createElement("section");
  wrap.className = "insp-group";
  if (isCollapsed(key)) wrap.classList.add("collapsed");

  const head = document.createElement("button");
  head.type = "button";
  head.className = "insp-group-head";
  const chevron = document.createElement("span");
  chevron.className = "insp-chevron";
  chevron.textContent = "▾"; // ▾
  const label = document.createElement("span");
  label.className = "insp-group-title";
  label.textContent = title;
  head.append(chevron, label);
  if (badge) head.appendChild(badge);
  head.addEventListener("click", () => {
    const collapsed = !wrap.classList.contains("collapsed");
    wrap.classList.toggle("collapsed", collapsed);
    setCollapsed(key, collapsed);
  });

  const body = document.createElement("div");
  body.className = "insp-group-body";
  wrap.append(head, body);
  root.appendChild(wrap);
  return body;
}

export function smallBtn(label, onClick) {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "insp-btn-small";
  button.textContent = label;
  button.addEventListener("click", onClick);
  return button;
}

// ---- prompt modal (T0250) -----------------------------------------------------
//
// A full-viewport modal for reading/editing a long prompt string (lead: "промпт тяжело
// вот так читать" — the 3-row Recipe textarea is too small to read a real prompt).
// Reused for BOTH the Recipe card's LIVE prompt (editable — Save commits through the
// caller's `onSave`, e.g. patchRecipeAction, so it is still just one journal entry) and a
// minted element's FROZEN prompt_snapshot in the Generation section (read-only — pass
// `{ readOnly: true }`; `onSave` is never called). Escape / a click on the dimmed
// overlay / Cancel all close without committing; Ctrl+Enter is a Save shortcut in
// editable mode. Positioned above every other floating layer in this file (preset menu /
// toasts are z-index:40, the context menu is z-index:50/51, the layer-drag-ghost is
// z-index:60) so it can always be opened on top of them.
export function openPromptModal(title, initialValue, onSave, { readOnly: viewOnly = false } = {}) {
  const overlay = document.createElement("div");
  overlay.className = "prompt-modal-overlay";

  const panel = document.createElement("div");
  panel.className = "prompt-modal-panel";

  const head = document.createElement("div");
  head.className = "prompt-modal-head";
  head.textContent = title;

  const textarea = document.createElement("textarea");
  textarea.className = "prompt-modal-textarea";
  textarea.value = initialValue == null ? "" : String(initialValue);
  // readOnly, NOT disabled: a disabled textarea refuses focus (so the frozen prompt
  // couldn't be selected/copied, and Escape would land on a non-typing target and fall
  // through to the canvas handler below).
  textarea.readOnly = viewOnly;

  const footer = document.createElement("div");
  footer.className = "prompt-modal-footer";

  const close = () => {
    document.removeEventListener("keydown", onKeydown, true);
    overlay.remove();
  };
  const save = () => {
    if (viewOnly) return;
    // Match the inline textarea's change-event semantics: an unedited Save/Ctrl+Enter
    // is a plain close, never a no-op journal entry. A save whose onSave THROWS (T0260's
    // Animation JSON editor validates JSON.parse + the spec client-side) keeps the modal
    // OPEN and toasts the reason, so the lead fixes the text instead of losing it — every
    // pre-existing caller's onSave never throws, so their close-on-save is unchanged.
    if (textarea.value !== (initialValue == null ? "" : String(initialValue))) {
      try {
        onSave(textarea.value);
      } catch (error) {
        setStatus(error.message, true);
        return;
      }
    }
    close();
  };

  if (viewOnly) {
    footer.appendChild(smallBtn("Close", close));
  } else {
    footer.append(smallBtn("Cancel", close), (() => {
      const btn = smallBtn("Save", save);
      btn.classList.add("primary");
      return btn;
    })());
  }

  // stopPropagation: this capture-phase handler runs before the canvas's window-level
  // keydown (canvas.js) — without it, Escape would ALSO unwind canvas selection/scope
  // behind the closing modal.
  const onKeydown = (event) => {
    if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      close();
    } else if (event.key === "Enter" && event.ctrlKey) {
      event.preventDefault();
      event.stopPropagation();
      save();
    }
  };
  document.addEventListener("keydown", onKeydown, true);
  overlay.addEventListener("mousedown", (event) => {
    if (event.target === overlay) close();
  });

  panel.append(head, textarea, footer);
  overlay.appendChild(panel);
  document.body.appendChild(overlay);
  textarea.focus();
  textarea.setSelectionRange(textarea.value.length, textarea.value.length);
  return overlay;
}
