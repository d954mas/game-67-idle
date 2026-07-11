import assert from "node:assert/strict";
import test from "node:test";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = resolve(fileURLToPath(new URL("../../..", import.meta.url)));

function source(path) {
  return readFileSync(resolve(repoRoot, path), "utf8");
}

test("architecture map surface contains only the current ownership hierarchy", () => {
  const page = source("ai_studio/architecture_map/index.html");
  const shell = source("ai_studio/studio_shell/index.html");
  const homeCss = source("ai_studio/studio_shell/home.css");
  const readme = source("ai_studio/architecture_map/README.md");
  const combined = `${page}\n${shell}\n${homeCss}\n${readme}`;

  for (const forbidden of [
    "moduleEdges",
    "edgeData",
    "buildLegacyPayloadFromTree",
    "renderEdges",
    "drillArrow",
    "dependsOn",
    "usedBy",
    "dependency graph",
    "Local graph",
    "All contracts",
    "Inventory pressure",
    "queue-card",
    "graph-tree-node",
    "data-explorer-close",
    "data-toggle-description",
    "refactorGroups",
    "clearSavedLayout",
    "findExplorerParent",
    "syncSelectionFromExplorer",
    "type-pill graph",
    "type-pill.graph",
    "const search = { value:",
    "zoomLabel",
    "drillOpen",
    "setDrillOpen",
    "explorerNodeMatches",
    'role === "center"',
    'role !== "center"',
    '.viewport.drilling',
  ]) {
    assert.ok(!combined.includes(forbidden), `legacy-only token remains: ${forbidden}`);
  }

  for (const required of [
    "/api/architecture-tree",
    "studioTreeSource",
    "renderExplorer",
    "Hierarchy",
    "data-explorer-back",
    "data-explorer-card",
    "data-copy-path",
    "data-kind-toggle",
    "drillPositions",
    "saved.drill",
    "Иерархия",
    "Внутри уровня",
    "Назад",
    ".icon {",
    ".icon svg {",
    ".block { display: grid; gap: 7px; }",
    ".muted { color: var(--muted); font-size: 12px; }",
  ]) {
    assert.ok(page.includes(required), `current hierarchy token is missing: ${required}`);
  }

  for (const mojibake of ["Р", "Рќ", "Рџ"]) {
    assert.ok(!page.includes(mojibake), `mojibake token remains: ${mojibake}`);
  }

  assert.ok(shell.includes(">Tree<"), "Studio Shell labels the surface as a tree");
  assert.ok(!shell.includes(">Graph<"), "Studio Shell does not expose the legacy Graph label");
  assert.ok(shell.includes('class="type-pill tree"'), "Studio Shell uses the tree semantic class");
  assert.ok(homeCss.includes(".type-pill.tree"), "Studio Shell styles the tree semantic class");

  const inlineScripts = [...page.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/g)]
    .map((match) => match[1])
    .filter((script) => script.trim());
  assert.equal(inlineScripts.length, 1, "surface has one inline application script");
  assert.doesNotThrow(() => new Function(inlineScripts[0]), "inline application script syntax-compiles");

  const script = inlineScripts[0];
  for (const [producer, consumer, label] of [
    ['data-explorer-card="', 'closest("[data-explorer-card]")', "whole-card drill"],
    ['addEventListener("keydown"', "activateExplorerNode(card.dataset.explorerCard)", "keyboard drill"],
    ['data-explorer-back="1"', 'closest("[data-explorer-back]")', "back navigation"],
    ['data-kind-toggle="', 'closest("[data-kind-toggle]")', "type chip toggle"],
    ['addEventListener("pointerdown"', "drillDragging = {", "card drag start"],
    ["drillPositions.set(", "writeSavedLayout();", "drag position persistence"],
    ['addEventListener("wheel"', "setDrillZoomAt(", "wheel zoom"],
    ['data-copy-path="', 'closest("[data-copy-path]")', "copy path"],
  ]) {
    assert.ok(script.includes(producer), `${label} producer is present`);
    assert.ok(script.includes(consumer), `${label} consumer is present`);
  }

  const style = page.match(/<style>([\s\S]*?)<\/style>/)?.[1] || "";
  assert.equal((style.match(/{/g) || []).length, (style.match(/}/g) || []).length, "CSS braces are balanced");
  assert.ok(!/\n\s{6}box-shadow:[^\n]+;\n\s{6}z-index:\s*3;\n\s{4}}/.test(style), "CSS has no orphan center declarations");
});
