export function make(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

export function humanize(key) {
  const text = String(key || "").replace(/_/g, " ");
  return text ? text.charAt(0).toUpperCase() + text.slice(1) : text;
}

export function field(label, value) {
  const row = make("div", "iv-field");
  row.append(make("strong", "iv-field-label", label));
  const box = make("span", "iv-field-value");
  if (value instanceof Node) box.append(value);
  else box.textContent = value === undefined || value === null || value === "" ? "—" : value;
  row.append(box);
  return row;
}

export function renderValue(value) {
  if (value === undefined || value === null) return "—";
  if (Array.isArray(value)) return value.length ? value.map(String).join(", ") : "—";
  if (typeof value === "boolean") return value ? "true" : "false";
  if (typeof value === "object") {
    const nested = make("div", "iv-nested");
    for (const [key, nestedValue] of Object.entries(value)) {
      nested.append(field(humanize(key), renderValue(nestedValue)));
    }
    return nested;
  }
  return String(value);
}
