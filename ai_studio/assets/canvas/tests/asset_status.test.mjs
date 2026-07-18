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
import { thresholdsFromStyleLock } from "../../tools/image/quality_gate/api.mjs";
import { __runAssetTechnicalGateForTest } from "../ops/technical_gate.mjs";
import { __decideAssetStyleForTest } from "../ops/style_decision.mjs";
import { __runAssetStyleVerdictForTest } from "../ops/style_verdict.mjs";
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
  assert.deepEqual(historyEntryLabel("runAssetStyleVerdict", { verdict: "revise" }), {
    label: "Check asset style",
    summary: "revise",
  });
  assert.deepEqual(historyEntryLabel("decideAssetStyle", { decision: "accept" }), {
    label: "Decide asset style",
    summary: "accept",
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

function styleVerdictFixture(t) {
  tempProjects(t);
  const project = createProject(REPO_ROOT, {
    title: "Style verdict",
    ownership: { kind: "game", gameId: "test-game" },
  });
  const target = addImage(REPO_ROOT, project.id, { name: "target.png", bytes: solidPng(32, 32, [180, 110, 40]) }).element;
  const world = addImage(REPO_ROOT, project.id, { name: "world.png", bytes: solidPng(32, 32, [80, 120, 180]) }).element;
  const gui = addImage(REPO_ROOT, project.id, { name: "gui.png", bytes: solidPng(32, 32, [120, 80, 180]) }).element;
  const lock = {
    ...TECHNICAL_LOCK,
    prompt_preamble: "Chunky painterly dark fantasy with readable silhouettes.",
    negative_prompt: "No photorealism, thin silhouettes, text, or watermarks.",
    exemplar_refs: [
      { ref: `canvas://${project.id}/element/${world.id}`, origin: "owned", domain: "world" },
      { ref: `canvas://${project.id}/element/${gui.id}`, origin: "owned", domain: "gui" },
    ],
  };
  const current = getProject(REPO_ROOT, project.id);
  updateProject(REPO_ROOT, project.id, {
    elements: current.elements.map((element) => element.id === target.id ? {
      ...element,
      assetStatus: "checked",
      meta: {
        ...element.meta,
        technical_gate: {
          schema: "game.asset_technical_gate",
          version: 1,
          verdict: "pass",
          style_lock_id: lock.id,
          source_ref: target.src,
          key_color: lock.bg_rule.key_color,
          thresholds: thresholdsFromStyleLock(lock),
        },
      },
    } : element),
  });
  return { projectId: project.id, target, world, gui, lock };
}

function styleReport(verdict = "accept") {
  return {
    schema: "game.asset_style_verdict",
    version: 1,
    verdict,
    summary: verdict === "accept" ? "Matches the locked direction." : "Needs a style correction.",
    strengths: ["Readable silhouette"],
    concerns: verdict === "accept" ? [] : ["Surface treatment drifts"],
  };
}

function styleDeps(lock, report, { duringRun, finalError, finalLock = lock } = {}) {
  const calls = [];
  let resolutions = 0;
  return {
    calls,
    dependencies: {
      resolveStyleLock() {
        const firstResolution = resolutions++ === 0;
        if (!firstResolution && finalError) throw finalError;
        const resolvedLock = firstResolution ? lock : finalLock;
        return { gameId: "test-game", lock: resolvedLock };
      },
      async runJudge(args) {
        calls.push(args);
        if (duringRun) duringRun();
        return report;
      },
    },
  };
}

async function judgedStyleFixture(t, verdict = "accept") {
  const fixture = styleVerdictFixture(t);
  await __runAssetStyleVerdictForTest(REPO_ROOT, {
    projectId: fixture.projectId,
    elementId: fixture.target.id,
  }, styleDeps(fixture.lock, styleReport(verdict)).dependencies);
  return fixture;
}

function decisionDeps(lock) {
  return {
    resolveStyleLock() {
      return { gameId: "test-game", lock };
    },
  };
}

test("decideAssetStyle keeps the model advisory and makes the explicit lead decision undoable", async (t) => {
  const fixture = await judgedStyleFixture(t, "reject");
  const beforeDecision = getProject(REPO_ROOT, fixture.projectId);

  const accepted = __decideAssetStyleForTest(REPO_ROOT, {
    projectId: fixture.projectId,
    elementId: fixture.target.id,
    decision: "accept",
    reason: "Lead accepts the intentional silhouette exception.",
  }, decisionDeps(fixture.lock));

  assert.equal(accepted.status, "accepted", "the explicit lead remains the backstop over a model reject");
  assert.equal(accepted.element.meta.style_verdict.verdict, "reject");
  assert.deepEqual(accepted.element.meta.style_decision, {
    schema: "game.asset_style_decision",
    version: 1,
    decision: "accept",
    decided_at: accepted.element.meta.style_decision.decided_at,
    reason: "Lead accepts the intentional silhouette exception.",
    style_lock_id: fixture.lock.id,
    source_ref: fixture.target.src,
    advisory_verdict: "reject",
    advisory_checked_at: accepted.element.meta.style_verdict.checked_at,
  });
  assert.match(accepted.element.meta.style_decision.decided_at, /^\d{4}-\d{2}-\d{2}T/);

  const undone = undoOp(REPO_ROOT, { projectId: fixture.projectId }).project;
  const undoneElement = undone.elements.find((item) => item.id === fixture.target.id);
  assert.equal(undone.history_seq, beforeDecision.history_seq);
  assert.equal(undoneElement.assetStatus, "checked");
  assert.equal(undoneElement.meta.style_decision, undefined);

  const revised = __decideAssetStyleForTest(REPO_ROOT, {
    projectId: fixture.projectId,
    elementId: fixture.target.id,
    decision: "revise",
    reason: "Tighten the surface treatment before acceptance.",
  }, decisionDeps(fixture.lock));
  assert.equal(revised.status, "quarantine");
  assert.equal(revised.element.meta.style_decision.decision, "revise");
  assert.equal(undoOp(REPO_ROOT, { projectId: fixture.projectId }).project.elements.find((item) => item.id === fixture.target.id).assetStatus, "checked");

  const rejected = __decideAssetStyleForTest(REPO_ROOT, {
    projectId: fixture.projectId,
    elementId: fixture.target.id,
    decision: "reject",
    reason: "The direction conflicts with the locked canon.",
  }, decisionDeps(fixture.lock));
  assert.equal(rejected.status, "quarantine");
  assert.equal(rejected.element.meta.style_decision.advisory_verdict, "reject");
});

test("decideAssetStyle fails closed on invalid decisions and stale verdict inputs", async (t) => {
  const fixture = await judgedStyleFixture(t, "accept");
  const args = { projectId: fixture.projectId, elementId: fixture.target.id };

  assert.throws(
    () => __decideAssetStyleForTest(REPO_ROOT, { ...args, decision: "approve", reason: "No." }, decisionDeps(fixture.lock)),
    /decision must be accept\|revise\|reject/,
  );
  assert.throws(
    () => __decideAssetStyleForTest(REPO_ROOT, { ...args, decision: "accept", reason: "" }, decisionDeps(fixture.lock)),
    /non-empty reason/,
  );
  assert.throws(
    () => __decideAssetStyleForTest(REPO_ROOT, { ...args, decision: "accept", reason: "x".repeat(1001) }, decisionDeps(fixture.lock)),
    /at most 1000 characters/,
  );
  assert.throws(
    () => __decideAssetStyleForTest(REPO_ROOT, { ...args, decision: "accept", reason: "Stale lock." }, decisionDeps({
      ...fixture.lock,
      negative_prompt: "Changed after the verdict.",
    })),
    /current style-verdict evidence/,
  );
  assert.throws(
    () => __decideAssetStyleForTest(REPO_ROOT, { ...args, decision: "accept", reason: "Stale technical contract." }, decisionDeps({
      ...fixture.lock,
      asset_size: { ...fixture.lock.asset_size, width: fixture.lock.asset_size.width + 1 },
    })),
    /current style-verdict evidence/,
  );

  const current = getProject(REPO_ROOT, fixture.projectId);
  updateProject(REPO_ROOT, fixture.projectId, {
    elements: current.elements.map((element) => element.id === fixture.world.id
      ? { ...element, src: fixture.gui.src }
      : element),
  });
  assert.throws(
    () => __decideAssetStyleForTest(REPO_ROOT, { ...args, decision: "accept", reason: "Stale exemplar." }, decisionDeps(fixture.lock)),
    /current style-verdict evidence/,
  );
});

test("runAssetStyleVerdict stores 3-way advisory evidence without promoting checked art", async (t) => {
  const fixture = styleVerdictFixture(t);
  const before = getProject(REPO_ROOT, fixture.projectId);
  const { calls, dependencies } = styleDeps(fixture.lock, styleReport("accept"));

  const result = await __runAssetStyleVerdictForTest(REPO_ROOT, {
    projectId: fixture.projectId,
    elementId: fixture.target.id,
  }, dependencies);

  assert.equal(result.status, "checked", "the advisory model cannot mint accepted state");
  assert.equal(result.report.verdict, "accept");
  assert.equal(calls.length, 1);
  assert.match(calls[0].targetPath, /files[\\/].+\.png$/);
  assert.equal(calls[0].exemplars.length, 2);
  assert.deepEqual(calls[0].exemplars.map((entry) => entry.domain), ["world", "gui"]);
  assert.deepEqual(calls[0].exemplars.map((entry) => entry.ref), fixture.lock.exemplar_refs.map((entry) => entry.ref));
  assert.equal(calls[0].doPrompt, fixture.lock.prompt_preamble);
  assert.equal(calls[0].dontPrompt, fixture.lock.negative_prompt);
  assert.equal(result.element.meta.style_verdict.style_lock_id, fixture.lock.id);
  assert.deepEqual(result.element.meta.style_verdict.style_lock_snapshot, fixture.lock);
  assert.equal(result.element.meta.style_verdict.source_ref, fixture.target.src);
  assert.equal(result.element.meta.style_verdict.verdict, "accept");
  assert.deepEqual(
    result.element.meta.style_verdict.exemplar_refs,
    fixture.lock.exemplar_refs.map((entry, index) => ({
      ...entry,
      source_ref: index === 0 ? fixture.world.src : fixture.gui.src,
    })),
  );
  assert.match(result.element.meta.style_verdict.checked_at, /^\d{4}-\d{2}-\d{2}T/);
  assert.equal(undoOp(REPO_ROOT, { projectId: fixture.projectId }).project.history_seq, before.history_seq);
  const redone = redoOp(REPO_ROOT, { projectId: fixture.projectId }).project.elements.find((item) => item.id === fixture.target.id);
  assert.equal(redone.assetStatus, "checked");
  assert.equal(redone.meta.style_verdict.verdict, "accept");
});

test("runAssetStyleVerdict accepts all advisory verdicts but refuses stale technical evidence, malformed reports, and moved heads", async (t) => {
  const fixture = styleVerdictFixture(t);
  for (const verdict of ["accept", "revise", "reject"]) {
    const result = await __runAssetStyleVerdictForTest(REPO_ROOT, {
      projectId: fixture.projectId,
      elementId: fixture.target.id,
    }, styleDeps(fixture.lock, styleReport(verdict)).dependencies);
    assert.equal(result.report.verdict, verdict);
    assert.equal(result.status, "checked");
  }

  const current = getProject(REPO_ROOT, fixture.projectId);
  updateProject(REPO_ROOT, fixture.projectId, {
    elements: current.elements.map((element) => element.id === fixture.target.id ? {
      ...element,
      meta: { ...element.meta, technical_gate: { ...element.meta.technical_gate, source_ref: "files/stale.png" } },
    } : element),
  });
  await assert.rejects(
    () => __runAssetStyleVerdictForTest(REPO_ROOT, {
      projectId: fixture.projectId,
      elementId: fixture.target.id,
    }, styleDeps(fixture.lock, styleReport()).dependencies),
    /current passing technical-gate evidence/,
  );

  const repaired = getProject(REPO_ROOT, fixture.projectId);
  updateProject(REPO_ROOT, fixture.projectId, {
    elements: repaired.elements.map((element) => element.id === fixture.target.id ? {
      ...element,
      meta: { ...element.meta, technical_gate: { ...element.meta.technical_gate, source_ref: fixture.target.src } },
    } : element),
  });
  const currentThresholds = getProject(REPO_ROOT, fixture.projectId);
  updateProject(REPO_ROOT, fixture.projectId, {
    elements: currentThresholds.elements.map((element) => element.id === fixture.target.id ? {
      ...element,
      meta: {
        ...element.meta,
        technical_gate: {
          ...element.meta.technical_gate,
          thresholds: { ...element.meta.technical_gate.thresholds, max_alpha_noise_ratio: 0.99 },
        },
      },
    } : element),
  });
  await assert.rejects(
    () => __runAssetStyleVerdictForTest(REPO_ROOT, {
      projectId: fixture.projectId,
      elementId: fixture.target.id,
    }, styleDeps(fixture.lock, styleReport()).dependencies),
    /current passing technical-gate evidence/,
  );
  const staleThresholds = getProject(REPO_ROOT, fixture.projectId);
  updateProject(REPO_ROOT, fixture.projectId, {
    elements: staleThresholds.elements.map((element) => element.id === fixture.target.id ? {
      ...element,
      meta: { ...element.meta, technical_gate: { ...element.meta.technical_gate, thresholds: thresholdsFromStyleLock(fixture.lock) } },
    } : element),
  });
  for (const report of [
    { ...styleReport(), verdict: "maybe" },
    { ...styleReport(), summary: "" },
    { ...styleReport(), summary: "x".repeat(1001) },
    { ...styleReport(), concerns: [42] },
    { ...styleReport(), concerns: ["x".repeat(501)] },
  ]) {
    await assert.rejects(
      () => __runAssetStyleVerdictForTest(REPO_ROOT, {
        projectId: fixture.projectId,
        elementId: fixture.target.id,
      }, styleDeps(fixture.lock, report).dependencies),
      /invalid style verdict report/,
    );
  }

  await assert.rejects(
    () => __runAssetStyleVerdictForTest(REPO_ROOT, {
      projectId: fixture.projectId,
      elementId: fixture.target.id,
    }, styleDeps(fixture.lock, styleReport(), {
      finalLock: { ...fixture.lock, negative_prompt: "Changed while judging." },
    }).dependencies),
    (error) => error?.code === "HEAD_CONFLICT" && /style lock changed/.test(error.message),
  );

  await assert.rejects(
    () => __runAssetStyleVerdictForTest(REPO_ROOT, {
      projectId: fixture.projectId,
      elementId: fixture.target.id,
    }, styleDeps(fixture.lock, styleReport(), {
      finalError: new Error("accepted style lock disappeared"),
    }).dependencies),
    (error) => error?.code === "HEAD_CONFLICT"
      && /style lock changed/.test(error.message)
      && /accepted style lock disappeared/.test(error.message),
  );

  await assert.rejects(
    () => __runAssetStyleVerdictForTest(REPO_ROOT, {
      projectId: fixture.projectId,
      elementId: fixture.target.id,
    }, styleDeps(fixture.lock, styleReport(), {
      duringRun() {
        const project = getProject(REPO_ROOT, fixture.projectId);
        updateProject(REPO_ROOT, fixture.projectId, {
          elements: project.elements.map((element) => element.id === fixture.world.id
            ? { ...element, src: fixture.gui.src }
            : element),
        });
      },
    }).dependencies),
    (error) => error?.code === "HEAD_CONFLICT" && /exemplars changed/.test(error.message),
  );

  const moved = styleDeps(fixture.lock, styleReport(), {
    duringRun() {
      setAssetStatus(REPO_ROOT, { projectId: fixture.projectId, elementId: fixture.target.id, status: "quarantine" });
    },
  });
  await assert.rejects(
    () => __runAssetStyleVerdictForTest(REPO_ROOT, {
      projectId: fixture.projectId,
      elementId: fixture.target.id,
    }, moved.dependencies),
    (error) => error?.code === "HEAD_CONFLICT",
  );
  const stored = getProject(REPO_ROOT, fixture.projectId).elements.find((item) => item.id === fixture.target.id);
  assert.equal(stored.assetStatus, "quarantine");
  assert.equal(stored.meta.style_verdict.verdict, "reject", "the stale accept verdict was never committed");
});

test("runAssetStyleVerdict normalizes an exemplar disappearing during the judge to a head conflict", async (t) => {
  const fixture = styleVerdictFixture(t);

  await assert.rejects(
    () => __runAssetStyleVerdictForTest(REPO_ROOT, {
      projectId: fixture.projectId,
      elementId: fixture.target.id,
    }, styleDeps(fixture.lock, styleReport(), {
      duringRun() {
        const project = getProject(REPO_ROOT, fixture.projectId);
        updateProject(REPO_ROOT, fixture.projectId, {
          elements: project.elements.filter((element) => element.id !== fixture.gui.id),
        });
      },
    }).dependencies),
    (error) => error?.code === "HEAD_CONFLICT" && /exemplars changed/.test(error.message),
  );
});
