import { performance } from "node:perf_hooks";
import { isDeepStrictEqual } from "node:util";

import { normalizeAssetStatus } from "../asset_status.mjs";
import { resolveAcceptedGameStyleLock, resolveAutomaticGameStyleLock } from "../../style_lock/generation_origin.mjs";
import { runAssetQualityGate, thresholdsFromStyleLock } from "../../tools/image/quality_gate/api.mjs";
import { addFile, getProject, imageSize, resolveProjectFile, updateProject, withProjectLock } from "../store.mjs";
import { commitMutation, refuseIfHeadMoved } from "./core.mjs";

function technicalGateImage(project, elementId) {
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} asset technical gate is image-only`);
  return element;
}

const COUNT_METRICS = [
  "visible_px",
  "transparent_px",
  "edge_sample_px",
  "spill_edge_px",
  "halo_edge_px",
  "alpha_transition_sample_px",
  "alpha_noise_px",
];
const RATIO_METRICS = [
  "spill_edge_ratio",
  "halo_edge_ratio",
  "alpha_noise_ratio",
  "empty_margin_ratio",
  "aspect_relative_error",
];

function validBox(value, { nullable = true } = {}) {
  if (value === null) return nullable;
  return Array.isArray(value) && value.length === 4 && value.every((item) => Number.isInteger(item) && item >= 0);
}

function validMetrics(metrics, { requireContent = false } = {}) {
  if (!metrics || typeof metrics !== "object" || Array.isArray(metrics)) return false;
  if (!Array.isArray(metrics.size) || metrics.size.length !== 2 || !metrics.size.every((item) => Number.isInteger(item) && item > 0)) return false;
  if (!validBox(metrics.content_bbox, { nullable: !requireContent })) return false;
  if (!COUNT_METRICS.every((key) => Number.isInteger(metrics[key]) && metrics[key] >= 0)) return false;
  return RATIO_METRICS.every((key) => Number.isFinite(metrics[key]) && metrics[key] >= 0);
}

function validateReport(report, { thresholds, keyColor }) {
  const pass = report?.verdict === "pass";
  const problemsValid = Array.isArray(report?.problems) && report.problems.every((problem) => (
    problem && typeof problem === "object" && !Array.isArray(problem) && typeof problem.code === "string" && problem.code.length > 0
  ));
  if (
    !report ||
    report.schema !== "game.asset_technical_gate" ||
    report.version !== 1 ||
    !["pass", "fail"].includes(report.verdict) ||
    !problemsValid ||
    (report.verdict === "pass" && report.problems.length !== 0) ||
    (report.verdict === "fail" && report.problems.length === 0) ||
    !validBox(report.problem_bbox) ||
    (pass && report.problem_bbox !== null) ||
    (pass && !isDeepStrictEqual(report.thresholds, thresholds)) ||
    (pass && report.key_color !== keyColor) ||
    (pass && !validMetrics(report.metrics, { requireContent: true })) ||
    (!pass && report.thresholds !== undefined && !isDeepStrictEqual(report.thresholds, thresholds)) ||
    (!pass && report.key_color !== undefined && report.key_color !== keyColor) ||
    (!pass && report.metrics !== undefined && !validMetrics(report.metrics))
  ) {
    throw new Error("asset technical gate returned an invalid report");
  }
  return report;
}

async function prepareTechnicalGate(root, project, source, dependencies, defaultResolver) {
  const resolveStyleLock = dependencies.resolveStyleLock || defaultResolver;
  const resolved = resolveStyleLock(root, project);
  if (!resolved) return null;
  const runGate = dependencies.runGate || ((args) => runAssetQualityGate(root, args));
  const { lock } = resolved;
  const thresholds = thresholdsFromStyleLock(lock);
  const keyColor = lock.bg_rule?.mode === "chroma" ? String(lock.bg_rule.key_color).toUpperCase() : null;
  const gateResult = await runGate({ root, ...source, keyColor, thresholds });
  const report = validateReport(gateResult?.report, { thresholds, keyColor });
  if (report.verdict === "fail" && !Buffer.isBuffer(gateResult?.thumbnailBytes)) {
    throw new Error("asset technical gate returned FAIL without a problem thumbnail");
  }
  if (report.verdict === "fail") imageSize(gateResult.thumbnailBytes);
  return {
    lock,
    thresholds,
    keyColor,
    report,
    thumbnailBytes: report.verdict === "fail" && Buffer.isBuffer(gateResult?.thumbnailBytes)
      ? gateResult.thumbnailBytes
      : null,
    checkedAt: new Date().toISOString(),
  };
}

export function applyPreparedTechnicalGate(root, projectId, element, prepared) {
  const currentStatus = element.assetStatus == null ? null : normalizeAssetStatus(element.assetStatus);
  const nextStatus = prepared.report.verdict === "pass" && currentStatus === "accepted"
    ? "accepted"
    : prepared.report.verdict === "pass" ? "checked" : "quarantine";
  const thumbnail = prepared.thumbnailBytes
    ? addFile(root, projectId, { bytes: prepared.thumbnailBytes, name: `${element.name || element.id}-technical-gate.png` })
    : null;
  const evidence = {
    schema: prepared.report.schema,
    version: prepared.report.version,
    verdict: prepared.report.verdict,
    checked_at: prepared.checkedAt,
    style_lock_id: prepared.lock.id,
    source_ref: element.src,
    key_color: prepared.keyColor,
    thresholds: prepared.thresholds,
    metrics: prepared.report.metrics ?? null,
    problems: Array.isArray(prepared.report.problems) ? prepared.report.problems : [],
    problem_bbox: prepared.report.problem_bbox ?? null,
    problem_thumbnail: thumbnail?.src ?? null,
  };
  return {
    status: nextStatus,
    element: {
      ...element,
      assetStatus: nextStatus,
      meta: { ...(element.meta || {}), technical_gate: evidence },
    },
  };
}

export function prepareAutomaticTechnicalGate(root, project, sourceBytes, dependencies = {}) {
  if (!Buffer.isBuffer(sourceBytes)) throw new Error("automatic asset technical gate requires sourceBytes");
  return prepareTechnicalGate(root, project, { sourceBytes }, dependencies, resolveAutomaticGameStyleLock);
}

async function runAssetTechnicalGateImpl(root, { projectId, elementId } = {}, dependencies = {}) {
  if (!projectId) throw new Error("runAssetTechnicalGate requires projectId");
  if (!elementId) throw new Error("runAssetTechnicalGate requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const source = technicalGateImage(before, elementId);
  const sourcePath = resolveProjectFile(root, projectId, source.src);
  const prepared = await prepareTechnicalGate(root, before, { sourcePath }, dependencies, resolveAcceptedGameStyleLock);
  const report = prepared.report;

  return withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("runAssetTechnicalGate", before, current);
    const currentElement = technicalGateImage(current, elementId);
    const applied = applyPreparedTechnicalGate(root, projectId, currentElement, prepared);
    const nextStatus = applied.status;
    const after = updateProject(root, projectId, {
      elements: (current.elements || []).map((item) => item.id === elementId ? applied.element : item),
    });
    const project = commitMutation(root, projectId, {
      op: "runAssetTechnicalGate",
      args_summary: { elementId, verdict: report.verdict, status: nextStatus },
      before,
      after,
      startedAt,
    });
    return {
      project,
      element: (project.elements || []).find((item) => item.id === elementId),
      status: nextStatus,
      report,
    };
  });
}

export function runAssetTechnicalGate(root, args = {}) {
  return runAssetTechnicalGateImpl(root, args);
}

// Test-only seam kept out of ops.mjs: public/direct callers cannot replace either the
// accepted-lock resolver or the evaluator that authorizes an upward status transition.
export function __runAssetTechnicalGateForTest(root, args = {}, dependencies = {}) {
  return runAssetTechnicalGateImpl(root, args, dependencies);
}
