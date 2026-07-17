import { performance } from "node:perf_hooks";
import { isDeepStrictEqual } from "node:util";

import { normalizeAssetStatus } from "../asset_status.mjs";
import { resolveAcceptedGameStyleLock } from "../../style_lock/generation_origin.mjs";
import { runAssetQualityGate, thresholdsFromStyleLock } from "../../tools/image/quality_gate/api.mjs";
import { addFile, getProject, resolveProjectFile, updateProject, withProjectLock } from "../store.mjs";
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

async function runAssetTechnicalGateImpl(root, { projectId, elementId } = {}, dependencies = {}) {
  if (!projectId) throw new Error("runAssetTechnicalGate requires projectId");
  if (!elementId) throw new Error("runAssetTechnicalGate requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const source = technicalGateImage(before, elementId);
  const resolveStyleLock = dependencies.resolveStyleLock || resolveAcceptedGameStyleLock;
  const runGate = dependencies.runGate || ((args) => runAssetQualityGate(root, args));
  const { lock } = resolveStyleLock(root, before);
  const thresholds = thresholdsFromStyleLock(lock);
  const keyColor = lock.bg_rule?.mode === "chroma" ? String(lock.bg_rule.key_color).toUpperCase() : null;
  const sourcePath = resolveProjectFile(root, projectId, source.src);
  const gateResult = await runGate({ root, sourcePath, keyColor, thresholds });
  const report = validateReport(gateResult?.report, { thresholds, keyColor });
  const thumbnailBytes = report.verdict === "fail" && Buffer.isBuffer(gateResult?.thumbnailBytes)
    ? gateResult.thumbnailBytes
    : null;
  const checkedAt = new Date().toISOString();

  return withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("runAssetTechnicalGate", before, current);
    const currentElement = technicalGateImage(current, elementId);
    const currentStatus = currentElement.assetStatus == null ? null : normalizeAssetStatus(currentElement.assetStatus);
    const nextStatus = report.verdict === "pass" && currentStatus === "accepted"
      ? "accepted"
      : report.verdict === "pass" ? "checked" : "quarantine";
    const thumbnail = thumbnailBytes
      ? addFile(root, projectId, { bytes: thumbnailBytes, name: `${currentElement.name || elementId}-technical-gate.png` })
      : null;
    const evidence = {
      schema: report.schema,
      version: report.version,
      verdict: report.verdict,
      checked_at: checkedAt,
      style_lock_id: lock.id,
      source_ref: currentElement.src,
      key_color: keyColor,
      thresholds,
      metrics: report.metrics ?? null,
      problems: Array.isArray(report.problems) ? report.problems : [],
      problem_bbox: report.problem_bbox ?? null,
      problem_thumbnail: thumbnail?.src ?? null,
    };
    const after = updateProject(root, projectId, {
      elements: (current.elements || []).map((item) => item.id === elementId ? {
        ...item,
        assetStatus: nextStatus,
        meta: { ...(item.meta || {}), technical_gate: evidence },
      } : item),
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
