// Right inspector panel. Shows editable geometry + read-only detail for the
// current selection: a single element (name, X/Y/W/H, source size, provenance,
// regions, meta), a group/screen (name, X/Y/W/H, visible, member count, render
// controls), a multi-selection (count + export), or an empty state. Numeric edits
// PATCH through actions on change/Enter. Pure rendering/input.
import {
  el,
  elementById,
  groupById,
  hooks,
  memberElements,
  regionCount,
  selectedElements,
  state,
} from "./app.js";
import {
  exportElementIds,
  patchElementBox,
  patchGroupBox,
  renameElement,
  renameGroup,
  renderScreen,
  setGroupVisible,
} from "./actions.js";

function field(label, node) {
  const row = document.createElement("label");
  row.className = "insp-field";
  const span = document.createElement("span");
  span.className = "insp-label";
  span.textContent = label;
  row.appendChild(span);
  row.appendChild(node);
  return row;
}

function textInput(value, onCommit) {
  const input = document.createElement("input");
  input.type = "text";
  input.className = "insp-input";
  input.value = value == null ? "" : String(value);
  const commit = () => {
    const next = input.value.trim();
    if (next && next !== String(value)) onCommit(next);
  };
  input.addEventListener("change", commit);
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") input.blur();
  });
  return input;
}

function numberInput(value, onCommit) {
  const input = document.createElement("input");
  input.type = "number";
  input.className = "insp-input num";
  input.value = Number(value) || 0;
  const commit = () => {
    const next = Number(input.value);
    if (Number.isFinite(next) && next !== Number(value)) onCommit(next);
  };
  input.addEventListener("change", commit);
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") input.blur();
  });
  return input;
}

// A 2x2 grid of X/Y/W/H inputs bound to a patch function.
function boxGrid(box, onPatch) {
  const grid = document.createElement("div");
  grid.className = "insp-grid";
  const add = (key, label) => grid.appendChild(field(label, numberInput(box[key], (value) => onPatch({ [key]: value }))));
  add("x", "X");
  add("y", "Y");
  add("w", "W");
  add("h", "H");
  return grid;
}

function readOnly(label, value) {
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

function section(title) {
  const node = document.createElement("div");
  node.className = "insp-section";
  node.textContent = title;
  return node;
}

function renderElement(element, root) {
  root.appendChild(section("Element"));
  root.appendChild(field("Name", textInput(element.name, (next) => renameElement(element.id, next))));
  root.appendChild(boxGrid(element, (patch) => patchElementBox(element.id, patch)));
  root.appendChild(readOnly("Source", `${element.source_w || element.w} x ${element.source_h || element.h}`));
  root.appendChild(readOnly("Regions", String(regionCount(element))));

  if (element.meta && element.meta.parent) {
    root.appendChild(section("Provenance"));
    const parent = element.meta.parent;
    const parentEl = parent.elementId ? elementById(parent.elementId) : null;
    root.appendChild(readOnly("Parent sheet", parentEl ? parentEl.name : parent.sheetSrc || parent.elementId || "—"));
    root.appendChild(readOnly("Region", String(parent.regionId || "—")));
  }

  const metaKeys = Object.keys(element.meta || {}).filter((key) => key !== "parent");
  if (metaKeys.length) {
    root.appendChild(section("Meta"));
    for (const key of metaKeys) {
      root.appendChild(readOnly(key, typeof element.meta[key] === "object" ? JSON.stringify(element.meta[key]) : String(element.meta[key])));
    }
  }
}

function renderGroupInspector(group, root) {
  root.appendChild(section("Screen"));
  root.appendChild(field("Name", textInput(group.name, (next) => renameGroup(group.id, next))));
  root.appendChild(boxGrid(group, (patch) => patchGroupBox(group.id, patch)));

  const visRow = document.createElement("label");
  visRow.className = "insp-check";
  const check = document.createElement("input");
  check.type = "checkbox";
  check.checked = group.visible !== false;
  check.addEventListener("change", () => setGroupVisible(group.id, check.checked));
  const label = document.createElement("span");
  label.textContent = "Visible";
  visRow.append(check, label);
  root.appendChild(visRow);

  root.appendChild(readOnly("Members", String(memberElements(group.id).length)));

  root.appendChild(section("Render screen"));
  const controls = document.createElement("div");
  controls.className = "insp-render";

  const scale = document.createElement("select");
  scale.className = "insp-input";
  for (const value of [1, 2, 4]) {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = `${value}x`;
    scale.appendChild(option);
  }
  controls.appendChild(field("Scale", scale));

  const bgMode = document.createElement("select");
  bgMode.className = "insp-input";
  for (const [value, text] of [["transparent", "Transparent"], ["color", "Color"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = text;
    bgMode.appendChild(option);
  }
  const color = document.createElement("input");
  color.type = "color";
  color.value = "#1a1f2b";
  color.className = "insp-color";
  color.disabled = true;
  bgMode.addEventListener("change", () => {
    color.disabled = bgMode.value !== "color";
  });
  const bgRow = field("Background", bgMode);
  bgRow.appendChild(color);
  controls.appendChild(bgRow);

  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = "Render screen";
  button.addEventListener("click", () => {
    renderScreen(group.id, {
      scale: Number(scale.value),
      background: bgMode.value === "color" ? color.value : undefined,
    });
  });
  controls.appendChild(button);
  root.appendChild(controls);
}

function renderMulti(selected, root) {
  root.appendChild(section("Selection"));
  root.appendChild(readOnly("Selected", `${selected.length} elements`));
  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = "Export selected";
  button.addEventListener("click", () => exportElementIds(selected.map((element) => element.id)));
  root.appendChild(button);
}

export function renderInspector() {
  const root = el("inspector");
  if (!root) return;
  root.replaceChildren();

  const group = state.selectedGroupId ? groupById(state.selectedGroupId) : null;
  const selected = selectedElements();

  if (group) {
    renderGroupInspector(group, root);
  } else if (selected.length === 1) {
    renderElement(selected[0], root);
  } else if (selected.length > 1) {
    renderMulti(selected, root);
  } else {
    const empty = document.createElement("div");
    empty.className = "insp-nothing";
    empty.textContent = "Nothing selected";
    root.appendChild(empty);
  }
}

export function initInspector() {
  hooks.renderInspector = renderInspector;
}
