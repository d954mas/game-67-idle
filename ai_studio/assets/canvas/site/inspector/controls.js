import { focusStage } from "../app.js";

export function field(label, node) {
  const row = document.createElement("label");
  row.className = "insp-field";
  const span = document.createElement("span");
  span.className = "insp-label";
  span.textContent = label;
  row.appendChild(span);
  row.appendChild(node);
  return row;
}

export function textInput(value, onCommit) {
  const input = document.createElement("input");
  input.type = "text";
  input.className = "insp-input";
  input.value = value == null ? "" : String(value);
  const commit = () => {
    const next = input.value.trim();
    if (next === String(value == null ? "" : value)) return;
    if (!next) return;
    onCommit(next);
  };
  input.addEventListener("change", commit);
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault(); input.blur(); focusStage();
    } else if (event.key === "Escape") {
      event.preventDefault(); input.value = value == null ? "" : String(value); input.blur(); focusStage();
    }
  });
  return input;
}

export function textareaInput(value, onCommit) {
  const textarea = document.createElement("textarea");
  textarea.className = "insp-input";
  textarea.rows = 3;
  textarea.value = value == null ? "" : String(value);
  const commit = () => {
    const next = textarea.value;
    if (next === String(value == null ? "" : value)) return;
    onCommit(next);
  };
  textarea.addEventListener("change", commit);
  textarea.addEventListener("keydown", (event) => {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault(); textarea.blur(); focusStage();
    } else if (event.key === "Escape") {
      event.preventDefault(); textarea.value = value == null ? "" : String(value); textarea.blur(); focusStage();
    }
  });
  return textarea;
}

export function numberInput(value, onCommit, { step } = {}) {
  const input = document.createElement("input");
  input.type = "number";
  input.className = "insp-input num";
  input.value = Number(value) || 0;
  if (step) input.step = String(step);
  const commit = () => {
    const next = Number(input.value);
    if (Number.isFinite(next) && next !== Number(value)) onCommit(next);
  };
  input.addEventListener("change", commit);
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault(); input.blur(); focusStage();
    } else if (event.key === "Escape") {
      event.preventDefault(); input.value = Number(value) || 0; input.blur(); focusStage();
    }
  });
  return input;
}

export function selectInput(value, options, onCommit) {
  const select = document.createElement("select");
  select.className = "insp-input";
  for (const option of options) {
    const node = document.createElement("option");
    node.value = option;
    node.textContent = option;
    if (option === value) node.selected = true;
    select.appendChild(node);
  }
  select.addEventListener("change", () => onCommit(select.value));
  return select;
}

export function colorInput(value, onCommit) {
  const input = document.createElement("input");
  input.type = "color";
  input.className = "insp-color";
  input.value = /^#[0-9a-fA-F]{6}$/.test(String(value || "")) ? value : "#111111";
  input.addEventListener("change", () => onCommit(input.value));
  return input;
}
