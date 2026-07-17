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
  getProject,
  getAssetStatus,
  historyEntryLabel,
  readHistory,
  redoOp,
  setAssetStatus,
  undoOp,
  updateProject,
} from "../ops.mjs";
import { assetStatusBadge, assetStatusChipLayout } from "../asset_status.mjs";
import { __runAssetTechnicalGateForTest } from "../ops/technical_gate.mjs";
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
  const image = addImage(REPO_ROOT, project.id, {
    name: "hero.png",
    bytes: solidPng(),
    assetStatus: "accepted",
  }).element;
  const text = addText(REPO_ROOT, project.id, { content: "not art" }).element;

  assert.equal(image.assetStatus, undefined, "ordinary imports cannot inject review state");

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
  assert.deepEqual(historyEntryLabel("runAssetTechnicalGate", { verdict: "pass" }), {
    label: "Check asset quality",
    summary: "pass",
  });
});

const TECHNICAL_LOCK = Object.freeze({
  id: "test-game-style-v1",
  bg_rule: { mode: "chroma", key_color: "#FF00FF" },
  asset_size: { width: 64, height: 32 },
  technical_gate: {
    max_spill_edge_ratio: 0.05,
    max_halo_edge_ratio: 0.04,
    max_alpha_noise_ratio: 0.03,
    max_empty_margin_ratio: 0.5,
    max_aspect_relative_error: 0.02,
  },
});
const TECHNICAL_THRESHOLDS = Object.freeze({
  max_spill_edge_ratio: 0.05,
  max_halo_edge_ratio: 0.04,
  max_alpha_noise_ratio: 0.03,
  max_empty_margin_ratio: 0.5,
  aspect_ratio: { width: 64, height: 32, max_relative_error: 0.02 },
});
const PASS_METRICS = Object.freeze({
  size: [64, 32],
  content_bbox: [2, 2, 60, 28],
  visible_px: 1200,
  transparent_px: 848,
  edge_sample_px: 100,
  spill_edge_px: 1,
  spill_edge_ratio: 0.01,
  halo_edge_px: 1,
  halo_edge_ratio: 0.01,
  alpha_transition_sample_px: 120,
  alpha_noise_px: 0,
  alpha_noise_ratio: 0,
  empty_margin_ratio: 0.18,
  aspect_relative_error: 0,
});

function passingReport(overrides = {}) {
  return {
    schema: "game.asset_technical_gate",
    version: 1,
    verdict: "pass",
    key_color: "#FF00FF",
    thresholds: structuredClone(TECHNICAL_THRESHOLDS),
    metrics: structuredClone(PASS_METRICS),
    problems: [],
    problem_bbox: null,
    ...overrides,
  };
}

function gateDeps(report, { thumbnailBytes, duringRun } = {}) {
  const calls = [];
  return {
    calls,
    dependencies: {
      resolveStyleLock() {
        return { gameId: "test-game", lock: TECHNICAL_LOCK };
      },
      async runGate(args) {
        calls.push(args);
        if (duringRun) duringRun();
        return { report, thumbnailBytes };
      },
    },
  };
}

test("runAssetTechnicalGate: PASS promotes quarantine to checked with frozen evidence in one undoable commit", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Technical pass" });
  const image = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: solidPng() }).element;
  setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "quarantine" });
  const before = getAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id });
  const { calls, dependencies } = gateDeps(passingReport());

  const result = await __runAssetTechnicalGateForTest(REPO_ROOT, {
    projectId: project.id,
    elementId: image.id,
  }, dependencies);

  assert.equal(result.status, "checked");
  assert.equal(result.report.verdict, "pass");
  assert.equal(calls.length, 1);
  assert.equal(calls[0].keyColor, "#FF00FF");
  assert.deepEqual(calls[0].thresholds, TECHNICAL_THRESHOLDS);
  assert.equal(result.element.meta.technical_gate.style_lock_id, TECHNICAL_LOCK.id);
  assert.equal(result.element.meta.technical_gate.source_ref, image.src);
  assert.equal(result.element.meta.technical_gate.verdict, "pass");
  assert.equal(result.element.meta.technical_gate.problem_thumbnail, null);
  assert.match(result.element.meta.technical_gate.checked_at, /^\d{4}-\d{2}-\d{2}T/);
  assert.equal(undoOp(REPO_ROOT, { projectId: project.id }).project.elements.find((item) => item.id === image.id).assetStatus, before.status);
  assert.equal(redoOp(REPO_ROOT, { projectId: project.id }).project.elements.find((item) => item.id === image.id).assetStatus, "checked");
});

test("runAssetTechnicalGate: FAIL downgrades accepted art to quarantine and stores the problem thumbnail", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Technical fail" });
  const image = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: solidPng() }).element;
  updateProject(REPO_ROOT, project.id, {
    elements: getProject(REPO_ROOT, project.id).elements.map((item) => item.id === image.id ? { ...item, assetStatus: "accepted" } : item),
  });
  const { dependencies } = gateDeps(passingReport({
    verdict: "fail",
    metrics: { ...PASS_METRICS, spill_edge_px: 25, spill_edge_ratio: 0.25 },
    problems: [{ code: "key_spill", metric: "spill_edge_ratio", value: 0.25, maximum: 0.05 }],
    problem_bbox: [1, 2, 3, 4],
  }), { thumbnailBytes: solidPng(8, 8, [255, 48, 48]) });

  const result = await __runAssetTechnicalGateForTest(REPO_ROOT, {
    projectId: project.id,
    elementId: image.id,
  }, dependencies);

  assert.equal(result.status, "quarantine");
  assert.match(result.element.meta.technical_gate.problem_thumbnail, /^files\//);
  assert.equal(result.element.meta.technical_gate.verdict, "fail");
});

test("runAssetTechnicalGate fails closed on malformed evaluator output and on a concurrent Canvas edit", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Technical refusal" });
  const image = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: solidPng() }).element;
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;

  const malformedReports = [
    { name: "bad verdict", report: { ...passingReport(), verdict: "maybe" } },
    { name: "missing metrics", report: (() => { const value = passingReport(); delete value.metrics; return value; })() },
    { name: "missing thresholds", report: (() => { const value = passingReport(); delete value.thresholds; return value; })() },
    { name: "mismatched thresholds", report: passingReport({ thresholds: { ...TECHNICAL_THRESHOLDS, max_spill_edge_ratio: 0.9 } }) },
    { name: "malformed bbox", report: passingReport({ problem_bbox: [1, 2, 3] }) },
  ];
  for (const fixture of malformedReports) {
    await assert.rejects(
      () => __runAssetTechnicalGateForTest(
        REPO_ROOT,
        { projectId: project.id, elementId: image.id },
        gateDeps(fixture.report).dependencies,
      ),
      /technical gate returned an invalid report/,
      fixture.name,
    );
  }
  await assert.rejects(
    () => __runAssetTechnicalGateForTest(
      REPO_ROOT,
      { projectId: project.id, elementId: image.id },
      gateDeps(passingReport({
        verdict: "fail",
        problems: [{ code: "key_spill" }],
        problem_bbox: [1, 2, 3, 4],
      })).dependencies,
    ),
    /problem thumbnail/,
    "a failed verdict without visual evidence fails closed",
  );
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore);

  const { dependencies } = gateDeps(passingReport(), {
    duringRun() {
      setAssetStatus(REPO_ROOT, { projectId: project.id, elementId: image.id, status: "quarantine" });
    },
  });
  await assert.rejects(
    () => __runAssetTechnicalGateForTest(REPO_ROOT, { projectId: project.id, elementId: image.id }, dependencies),
    (error) => error?.code === "HEAD_CONFLICT",
  );
  const stored = getProject(REPO_ROOT, project.id).elements.find((item) => item.id === image.id);
  assert.equal(stored.assetStatus, "quarantine", "the concurrent journaled edit survives");
  assert.equal(stored.meta?.technical_gate, undefined, "the stale gate result is never written");
});
