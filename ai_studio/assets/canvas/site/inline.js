// Inline text editing helper. Swaps a label's text for a text input, commits on
// Enter/blur, cancels on Escape. Used for renaming projects, elements, and groups
// without a browser prompt(). Pure DOM/input; no API knowledge.

// Turn a container into an inline editor. `value` seeds the input; `onCommit` is
// called with the trimmed new value only when it changed and is non-empty.
export function inlineEdit(container, value, onCommit) {
  const input = document.createElement("input");
  input.type = "text";
  input.className = "inline-input";
  input.value = value || "";
  container.replaceChildren(input);
  input.focus();
  input.select();

  let done = false;
  const finish = (commit) => {
    if (done) return;
    done = true;
    const next = input.value.trim();
    input.removeEventListener("keydown", onKey);
    input.removeEventListener("blur", onBlur);
    if (commit && next && next !== value) onCommit(next);
    else if (typeof onCommit.cancel === "function") onCommit.cancel();
  };
  const onKey = (event) => {
    event.stopPropagation();
    if (event.key === "Enter") {
      event.preventDefault();
      finish(true);
    } else if (event.key === "Escape") {
      event.preventDefault();
      finish(false);
    }
  };
  const onBlur = () => finish(true);
  input.addEventListener("keydown", onKey);
  input.addEventListener("blur", onBlur);
  // Don't let a click inside the editor bubble to selection/canvas handlers.
  input.addEventListener("mousedown", (event) => event.stopPropagation());
  input.addEventListener("dblclick", (event) => event.stopPropagation());
  return input;
}

// Build a standalone <input> pre-wired to commit/cancel, for building a new-card
// editor that isn't replacing an existing label.
export function makeInlineInput({ value = "", placeholder = "", onCommit, onCancel } = {}) {
  const input = document.createElement("input");
  input.type = "text";
  input.className = "inline-input";
  input.value = value;
  input.placeholder = placeholder;
  let done = false;
  const finish = (commit) => {
    if (done) return;
    done = true;
    const next = input.value.trim();
    if (commit && next) onCommit?.(next);
    else onCancel?.();
  };
  input.addEventListener("keydown", (event) => {
    event.stopPropagation();
    if (event.key === "Enter") {
      event.preventDefault();
      finish(true);
    } else if (event.key === "Escape") {
      event.preventDefault();
      finish(false);
    }
  });
  input.addEventListener("blur", () => finish(true));
  return input;
}
