#!/usr/bin/env node
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fail } from "../lib/cli.mjs";
import { readJson } from "../lib/json.mjs";

function usage() {
  console.error(`usage:
  node tools/product_gate/responsive_layout_audit.mjs --ui-tree <tree.json> --surface desktop|portrait --primary <id> --button <id>... [options]

Options:
  --width <px>               viewport width; defaults to root.w
  --height <px>              viewport height; defaults to root.h
  --min-touch <px>           minimum button width/height; default 44
  --portrait-primary-ratio <n>  minimum primary width / viewport width; default 0.78
  --output <path>            markdown report path
  --json-output <path>       JSON report path`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = { buttons: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--button") values.buttons.push(argv[++index]);
    else if (arg.startsWith("--")) {
      const key = arg.slice(2);
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values[key] = value;
      index += 1;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return values;
}

function numberArg(values, key, fallback) {
  if (values[key] === undefined) return fallback;
  const parsed = Number(values[key]);
  if (!Number.isFinite(parsed)) fail(`--${key} must be a number`);
  return parsed;
}

function bounds(node) {
  const source = node.bounds && typeof node.bounds === "object" ? node.bounds : node;
  return {
    x: Number(source.x),
    y: Number(source.y),
    w: Number(source.w),
    h: Number(source.h),
  };
}

function right(box) {
  return box.x + box.w;
}

function bottom(box) {
  return box.y + box.h;
}

function overlapArea(a, b) {
  const x = Math.max(0, Math.min(right(a), right(b)) - Math.max(a.x, b.x));
  const y = Math.max(0, Math.min(bottom(a), bottom(b)) - Math.max(a.y, b.y));
  return x * y;
}

function renderMarkdown(report) {
  const lines = [
    "---",
    "type: ResponsiveLayoutAudit",
    `surface: ${report.surface}`,
    `verdict: ${report.verdict}`,
    "---",
    "",
    "# Responsive Layout Audit",
    "",
    `Verdict: **${report.verdict.toUpperCase()}**`,
    "",
    `Surface: ${report.surface}`,
    `Viewport: ${report.viewport.width}x${report.viewport.height}`,
    `Buttons checked: ${report.buttons.length}`,
    `Problems: ${report.problems.length}`,
    "",
  ];
  if (report.problems.length > 0) {
    lines.push("## Problems", "");
    for (const problem of report.problems) lines.push(`- ${problem}`);
    lines.push("");
  }
  lines.push("## Button Bounds", "");
  for (const item of report.buttons) {
    lines.push(`- \`${item.id}\`: x=${item.bounds.x}, y=${item.bounds.y}, w=${item.bounds.w}, h=${item.bounds.h}, enabled=${item.enabled}`);
  }
  lines.push("");
  return lines.join("\n");
}

function validate(values) {
  if (values.help) usage();
  if (!values["ui-tree"]) usage();
  if (!values.surface) usage();
  if (!["desktop", "portrait"].includes(values.surface)) fail("--surface must be desktop or portrait");
  if (!values.primary) usage();
  if (!values.buttons.includes(values.primary)) values.buttons.unshift(values.primary);
}

const values = parseArgs(process.argv.slice(2));
validate(values);

const treePath = resolve(values["ui-tree"]);
if (!existsSync(treePath)) fail(`ui tree does not exist: ${values["ui-tree"]}`);
const tree = readJson(treePath);
if (!Array.isArray(tree)) fail("ui tree must be an array");
const byId = new Map(tree.map((node) => [node.id, node]));
const root = byId.get("root") || tree[0] || {};
const rootBounds = bounds(root);
const viewport = {
  width: numberArg(values, "width", rootBounds.w),
  height: numberArg(values, "height", rootBounds.h),
};
const minTouch = numberArg(values, "min-touch", 44);
const portraitPrimaryRatio = numberArg(values, "portrait-primary-ratio", 0.78);
const problems = [];
const checked = [];

for (const id of values.buttons) {
  const node = byId.get(id);
  if (!node) {
    problems.push(`missing button node: ${id}`);
    continue;
  }
  const box = bounds(node);
  const item = { id, role: node.role || "", enabled: node.enabled !== false, visible: node.visible !== false, bounds: box };
  checked.push(item);
  if (!item.visible) problems.push(`${id} is not visible`);
  if (![box.x, box.y, box.w, box.h].every(Number.isFinite)) problems.push(`${id} has invalid bounds`);
  if (box.w < minTouch) problems.push(`${id} width ${box.w}px < min touch ${minTouch}px`);
  if (box.h < minTouch) problems.push(`${id} height ${box.h}px < min touch ${minTouch}px`);
  if (box.x < -0.5 || box.y < -0.5 || right(box) > viewport.width + 0.5 || bottom(box) > viewport.height + 0.5) {
    problems.push(`${id} bounds leave viewport`);
  }
}

for (let a = 0; a < checked.length; a += 1) {
  for (let b = a + 1; b < checked.length; b += 1) {
    const area = overlapArea(checked[a].bounds, checked[b].bounds);
    if (area > 1) problems.push(`${checked[a].id} overlaps ${checked[b].id} by ${area.toFixed(1)}px`);
  }
}

const primary = checked.find((item) => item.id === values.primary);
if (primary && values.surface === "portrait") {
  const minPrimaryWidth = viewport.width * portraitPrimaryRatio;
  if (primary.bounds.w < minPrimaryWidth) {
    problems.push(`${primary.id} portrait width ${primary.bounds.w}px < ${portraitPrimaryRatio} viewport ratio (${minPrimaryWidth.toFixed(1)}px)`);
  }
  for (const item of checked) {
    if (item.id !== primary.id && item.bounds.y < bottom(primary.bounds) - 1) {
      problems.push(`${item.id} should sit below portrait primary action ${primary.id}`);
    }
  }
}

const report = {
  schema: "game.responsive_layout_audit",
  version: 1,
  surface: values.surface,
  ui_tree: values["ui-tree"].replaceAll("\\", "/"),
  viewport,
  primary: values.primary,
  buttons: checked,
  problems,
  verdict: problems.length > 0 ? "fail" : "pass",
};

if (values["json-output"]) {
  const out = resolve(values["json-output"]);
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, `${JSON.stringify(report, null, 2)}\n`, "utf8");
}
if (values.output) {
  const out = resolve(values.output);
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, renderMarkdown(report), "utf8");
}

console.log(`${report.verdict}: checked ${checked.length} responsive UI node(s)`);
for (const problem of problems) console.log(`problem: ${problem}`);
process.exit(problems.length > 0 ? 1 : 0);
