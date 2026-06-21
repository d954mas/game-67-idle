#!/usr/bin/env node
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, relative, resolve } from "node:path";

function usage() {
  console.error(`usage: node tools/visual_invariant_guard.mjs [--root <dir>] [--json]

Fails active game work that uses product-visible debug visual paths without an
explicit debug-debt marker:
- handmade/pixel draw_text
- handmade bitmap/shape font tables or glyph helpers
- shape/debug renderers as final visuals
- Y-down game/UI layout conventions outside platform/input/devapi boundaries`);
  process.exit(2);
}

const args = process.argv.slice(2);
function takeString(name, fallback) {
  const index = args.indexOf(name);
  if (index === -1) return fallback;
  const value = args[index + 1];
  if (!value || value.startsWith("--")) usage();
  args.splice(index, 2);
  return value;
}

const root = resolve(takeString("--root", process.cwd()));
const json = args.includes("--json");
if (json) args.splice(args.indexOf("--json"), 1);
if (args.includes("--help") || args.includes("-h")) usage();
if (args.length > 0) usage();

function hasActiveConcept() {
  if (process.env.NT_FORCE_CONCEPT === "1") return true;
  if (process.env.NT_FORCE_CONCEPT === "0") return false;
  const statusPath = join(root, "tasks", "STATUS.md");
  if (!existsSync(statusPath)) return true;
  return !/no active game concept/i.test(readFileSync(statusPath, "utf8"));
}

function walkFiles(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const path = join(dir, name);
    const stat = statSync(path);
    if (stat.isDirectory()) {
      if (["build", "tmp", ".git"].includes(name)) continue;
      walkFiles(path, out);
    } else if (/\.(c|h|cpp|hpp|cc|m|mm|js|mjs|ts|tsx)$/.test(name)) {
      out.push(path);
    }
  }
  return out;
}

const debugDebtPattern = /debug[_ -]?only|debug debt|temporary debug|placeholder[_ -]?debug|iteration[_ -]?only/i;
const textPattern = /\b(draw_text|pixel_text|bitmap_text|shape_text)\b/i;
const manualFontPattern = /\b(glyph5|draw_text5|bitmap_font|shape_font|pixel_font|font5x7|glyph7|glyph_table)\b|(?:static\s+const\s+uint(?:8|16|32)_t\s+\w*(?:glyph|font)\w*\s*\[)/i;
const shapePattern = /\b(nt_shape_renderer_\w*|shape_renderer|debug_renderer)\b/i;
const yDownPattern = /\b(y[-_ ]?down|screen_y|top[- ]left)\b|(?:\b(?:height|h)\s*-\s*[^;\n]*(?:pointer|mouse|touch|cursor)?\.?y\b)/i;
const boundaryPathPattern = /(?:^|[\\/])(input|platform|devapi|window)(?:[\\/]|$)|(?:^|[\\/])game_devapi_ui\.(?:c|h)$/i;
const boundaryTextPattern = /boundary|platform|input|window|devapi|convert/i;

function nearbyText(lines, lineIndex) {
  return lines.slice(Math.max(0, lineIndex - 3), Math.min(lines.length, lineIndex + 2)).join("\n");
}

const activeConcept = hasActiveConcept();
const problems = [];

if (activeConcept) {
  const files = walkFiles(join(root, "src"));
  for (const file of files) {
    const rel = relative(root, file).replaceAll("\\", "/");
    const lines = readFileSync(file, "utf8").split(/\r?\n/);
    const isBoundaryFile = boundaryPathPattern.test(rel);
    const fileHasDebugDebt = debugDebtPattern.test(lines.slice(0, 12).join("\n"));
    lines.forEach((line, index) => {
      const context = nearbyText(lines, index);
      if (manualFontPattern.test(line)) {
        problems.push({ file: rel, line: index + 1, rule: "no-handmade-fonts", detail: "product/playable text must use generated font assets and the engine text renderer, not handmade glyph tables" });
      }
      if (textPattern.test(line) && !debugDebtPattern.test(context)) {
        problems.push({ file: rel, line: index + 1, rule: "engine-text-renderer", detail: "handmade draw_text needs explicit debug debt or engine text renderer" });
      }
      if (shapePattern.test(line) && !fileHasDebugDebt && !debugDebtPattern.test(context)) {
        problems.push({ file: rel, line: index + 1, rule: "debug-renderer", detail: "shape/debug renderer in active game needs explicit debug debt" });
      }
      if (yDownPattern.test(line) && !isBoundaryFile && !boundaryTextPattern.test(context)) {
        problems.push({ file: rel, line: index + 1, rule: "y-up-boundary", detail: "Y-down or top-left convention outside boundary file" });
      }
    });
  }
}

const report = {
  schema: "game.visual_invariant_guard",
  active_concept: activeConcept,
  status: problems.length > 0 ? "fail" : "pass",
  skipped: !activeConcept,
  problems,
};

if (json) {
  console.log(JSON.stringify(report, null, 2));
} else if (!activeConcept) {
  console.log("ok: visual invariant guard skipped (no active game concept)");
} else if (problems.length === 0) {
  console.log("ok: visual invariants clean");
} else {
  for (const problem of problems) {
    console.error(`${problem.file}:${problem.line}: ${problem.rule}: ${problem.detail}`);
  }
}

if (problems.length > 0) process.exit(1);
