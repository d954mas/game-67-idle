import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";
import { fileURLToPath } from "node:url";

import {
  addImage,
  addText,
  createProject,
  getAssetStatus,
  historyEntryLabel,
  readHistory,
  redoOp,
  setAssetStatus,
  undoOp,
  updateProject,
} from "../ops.mjs";
import { assetStatusBadge, assetStatusChipLayout } from "../asset_status.mjs";
import { solidPng } from "./png_fixture.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-asset-status-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
}

test("asset status is image-only, explicit, validated, and undoable", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Asset status" });
  const image = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: solidPng() }).element;
  const text = addText(REPO_ROOT, project.id, { content: "not art" }).element;

  assert.deepEqual(getAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id }), {
    projectId: project.id,
    elementId: image.id,
    status: null,
  });
  assert.throws(
    () => setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "checked" }),
    /promotion from untracked to checked requires gate evidence/,
  );

  const quarantined = setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "quarantine" });
  const historyBeforeNoop = readHistory(REPO_ROOT, { projectId: project.id });
  const repeated = setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "quarantine" });
  assert.equal(repeated.project.history_seq, quarantined.project.history_seq);
  assert.equal(readHistory(REPO_ROOT, { projectId: project.id }).entries.length, historyBeforeNoop.entries.length);

  assert.throws(
    () => setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "checked" }),
    /promotion from quarantine to checked requires gate evidence/,
  );
  assert.throws(
    () => setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "accepted" }),
    /promotion from quarantine to accepted requires gate evidence/,
  );

  updateProject(REPO_ROOT, project.id, {
    elements: quarantined.project.elements.map((item) => item.id === image.id ? { ...item, assetStatus: "accepted" } : item),
  });
  const downgraded = setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "checked" });
  assert.equal(downgraded.status, "checked");
  assert.equal(undoOp(REPO_ROOT, { projectId: project.id }).project.elements.find((item) => item.id === image.id).assetStatus, "accepted");
  assert.equal(redoOp(REPO_ROOT, { projectId: project.id }).project.elements.find((item) => item.id === image.id).assetStatus, "checked");

  assert.throws(
    () => setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "approved" }),
    /asset status must be quarantine\|checked\|accepted/,
  );
  assert.throws(
    () => setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: text.id, status: "quarantine" }),
    /asset status is image-only/,
  );
});

test("asset status badge uses text as well as stable semantic colors", () => {
  assert.equal(assetStatusBadge({ type: "image" }), null);
  assert.equal(assetStatusBadge({ type: "text", assetStatus: "accepted" }), null);
  assert.deepEqual(assetStatusBadge({ type: "image", assetStatus: "quarantine" }), {
    status: "quarantine",
    label: "quarantine",
    title: "Asset status: quarantine",
    fill: "#d7a14a",
    text: "#231a08",
  });
  assert.equal(assetStatusBadge({ type: "image", assetStatus: "checked" }).label, "checked");
  assert.equal(assetStatusBadge({ type: "image", assetStatus: "accepted" }).label, "accepted");
  assert.equal(assetStatusChipLayout({ label: "quarantine" }, { width: 12, height: 12, measureText: () => 50 }), null);
  assert.deepEqual(assetStatusChipLayout({ label: "quarantine" }, { width: 24, height: 24, measureText: () => 50 }), {
    label: "Q",
    x: 8,
    y: 0,
    width: 16,
    height: 16,
  });
  assert.deepEqual(assetStatusChipLayout({ label: "checked" }, { width: 100, height: 40, measureText: () => 42 }), {
    label: "checked",
    x: 48,
    y: 0,
    width: 52,
    height: 16,
  });
  assert.deepEqual(historyEntryLabel("setAssetStatus", { status: "accepted" }), {
    label: "Set asset status",
    summary: "accepted",
  });
});
