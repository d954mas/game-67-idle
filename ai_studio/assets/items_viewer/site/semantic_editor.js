import { make } from "./dom.js";

const EDIT_OPERATIONS = new Set(["level-set", "curve-set", "override-set"]);

export function editableOperations(detail) {
  if (!(detail?.fields || []).length) return [];
  const levels = detail?.item?.item?.levels || [];
  const provenance = new Set(levels.flatMap((row) => Object.values(row.provenance || {})));
  const operations = [];
  if (provenance.has("table")) operations.push("level-set");
  if (detail?.item?.item?.values?.authoring_mode === "columns") operations.push("curve-set");
  if (provenance.has("override")) operations.push("override-set");
  return operations;
}

export function editableFields(detail, operation) {
  const fields = (detail?.fields || []).map((field) => field.member);
  const provenance = detail?.item?.item?.levels || [];
  const expected = {
    "level-set": "table",
    "curve-set": "columns",
    "override-set": "override",
  }[operation];
  if (!expected) return [];
  return fields.filter((field) => provenance.some((row) => row.provenance?.[field] === expected));
}

export function buildEditPatch({ operation, item, field, level, parameter, value, expectedSourceHash }) {
  if (!EDIT_OPERATIONS.has(operation)) throw new Error("Unsupported edit operation.");
  const numericValue = Number(value);
  if (!Number.isSafeInteger(numericValue)) throw new Error("Value must be a safe integer.");
  const patch = {
    schema: "items.cli.patch.v1",
    operation,
    item,
    field,
    value: numericValue,
    expected_source_hash: expectedSourceHash,
  };
  if (operation === "curve-set") {
    patch.parameter = parameter;
  } else {
    const numericLevel = Number(level);
    if (!Number.isSafeInteger(numericLevel) || numericLevel < 1) throw new Error("Level must be a positive integer.");
    patch.level = numericLevel;
  }
  return patch;
}

function control(labelText, input) {
  const label = make("label", "iv-edit-control");
  label.append(make("span", "iv-field-label", labelText));
  label.append(input);
  return label;
}

function replaceOptions(select, values, selected) {
  select.replaceChildren();
  for (const value of values) {
    const option = make("option", "", value);
    option.value = value;
    option.selected = String(value) === String(selected);
    select.append(option);
  }
}

function selectFor(values, selected) {
  const select = make("select", "iv-edit-input");
  replaceOptions(select, values, selected);
  return select;
}

function formatDiffValue(value) {
  return typeof value === "string" ? value : JSON.stringify(value);
}

function renderPreview(preview) {
  const box = make("div", "iv-preview-result");
  const sourceDiff = preview.source_diff;
  if (sourceDiff) {
    box.append(make("p", "iv-preview-source", `${sourceDiff.old_value} → ${sourceDiff.new_value}`));
    box.append(make("code", "iv-source-path", `${sourceDiff.file}:${sourceDiff.line}`));
  }
  const changes = preview.semantic_diff?.changes || [];
  if (changes.length) {
    const list = make("ul", "iv-diff-list");
    for (const change of changes) {
      const before = Object.hasOwn(change, "before") ? formatDiffValue(change.before) : "∅";
      const after = Object.hasOwn(change, "after") ? formatDiffValue(change.after) : "∅";
      list.append(make("li", "", `${change.path || "/"}: ${before} → ${after}`));
    }
    box.append(list);
  }
  return box;
}

export function renderSemanticEditor(model) {
  const root = make("section", "iv-detail-section iv-semantic-editor");
  root.append(make("h3", "", "What-if and semantic edit"));
  root.append(make("p", "iv-ephemeral-note", "Ephemeral preview only — never a build input until Apply succeeds."));

  const operations = editableOperations(model.detail);
  if (!operations.length) {
    root.append(make("p", "iv-muted", "This source shape is not safely editable here. Use the checked source location or Edit with agent."));
    return root;
  }

  const draft = model.draft || {};
  const levels = (model.detail.item?.item?.levels || []).map((row) => row.level);
  const operation = selectFor(operations, draft.operation || operations[0]);
  const initialFields = editableFields(model.detail, operation.value);
  const field = selectFor(initialFields, draft.field || initialFields[0]);
  const level = selectFor(levels, draft.level || levels[0]);
  const parameter = selectFor(["start", "step"], draft.parameter || "step");
  const value = make("input", "iv-edit-input");
  value.type = "number";
  value.step = "1";
  value.required = true;
  value.value = draft.value ?? "";

  const operationControl = control("Operation", operation);
  const fieldControl = control("Field", field);
  const levelControl = control("Level", level);
  const parameterControl = control("Curve parameter", parameter);
  const valueControl = control("Integer value", value);
  const form = make("div", "iv-edit-form");
  form.append(operationControl, fieldControl, levelControl, parameterControl, valueControl);
  root.append(form);

  const syncShape = () => {
    const curve = operation.value === "curve-set";
    replaceOptions(field, editableFields(model.detail, operation.value), field.value);
    levelControl.classList.toggle("is-hidden", curve);
    parameterControl.classList.toggle("is-hidden", !curve);
  };
  operation.addEventListener("change", syncShape);
  syncShape();

  const actions = make("div", "iv-edit-actions");
  const previewButton = make("button", "iv-action-button", model.busy ? "Working…" : "Preview what-if");
  previewButton.type = "button";
  previewButton.disabled = model.busy;
  previewButton.addEventListener("click", () => {
    try {
      model.onPreview(buildEditPatch({
        operation: operation.value,
        item: model.itemId,
        field: field.value,
        level: level.value,
        parameter: parameter.value,
        value: value.value,
        expectedSourceHash: model.sourceHash,
      }));
    } catch (error) {
      model.onInputError(error.message);
    }
  });
  actions.append(previewButton);

  if (model.preview) {
    const apply = make("button", "iv-action-button iv-action-primary", "Apply reviewed patch");
    apply.type = "button";
    apply.disabled = model.busy;
    apply.addEventListener("click", model.onApply);
    actions.append(apply);
  }
  if (model.undoCount > 0) {
    const undo = make("button", "iv-action-button", `Undo (${model.undoCount})`);
    undo.type = "button";
    undo.disabled = model.busy;
    undo.addEventListener("click", model.onUndo);
    actions.append(undo);
  }
  root.append(actions);

  if (model.message) root.append(make("p", model.error ? "iv-edit-message is-error" : "iv-edit-message", model.message));
  if (model.preview) root.append(renderPreview(model.preview));
  return root;
}
