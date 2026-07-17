import { performance } from "node:perf_hooks";
import { isDeepStrictEqual } from "node:util";

import { normalizeAssetStatus } from "../asset_status.mjs";
import { resolveAcceptedGameStyleLock } from "../../style_lock/generation_origin.mjs";
import { runStyleVerdict } from "../tools/style_verdict.mjs";
import { getProject, resolveProjectFile, updateProject, withProjectLock } from "../store.mjs";
import { commitMutation, refuseIfHeadMoved } from "./core.mjs";

const VERDICTS = new Set(["accept", "revise", "reject"]);
const REPORT_KEYS = ["concerns", "schema", "strengths", "summary", "verdict", "version"];
const MAX_REPORT_BYTES = 16 * 1024;
const MAX_SUMMARY_CHARS = 1000;
const MAX_OBSERVATION_CHARS = 500;

function imageElement(project, elementId, label = "asset style verdict") {
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} ${label} is image-only`);
  return element;
}

function nonEmptyString(value, maximum = Infinity) {
  return typeof value === "string" && value.trim().length > 0 && value.length <= maximum;
}

function stringList(value) {
  return Array.isArray(value) && value.length <= 8
    && value.every((item) => nonEmptyString(item, MAX_OBSERVATION_CHARS));
}

function validateReport(report) {
  const keys = report && typeof report === "object" && !Array.isArray(report)
    ? Object.keys(report).sort()
    : [];
  if (
    JSON.stringify(keys) !== JSON.stringify(REPORT_KEYS) ||
    report.schema !== "game.asset_style_verdict" ||
    report.version !== 1 ||
    !VERDICTS.has(report.verdict) ||
    !nonEmptyString(report.summary, MAX_SUMMARY_CHARS) ||
    !stringList(report.strengths) ||
    !stringList(report.concerns) ||
    Buffer.byteLength(JSON.stringify(report), "utf8") > MAX_REPORT_BYTES
  ) {
    throw new Error("asset style judge returned an invalid style verdict report");
  }
  return report;
}

function requireCurrentTechnicalPass(element, lock) {
  const status = element.assetStatus == null ? null : normalizeAssetStatus(element.assetStatus);
  const gate = element.meta?.technical_gate;
  if (
    !["checked", "accepted"].includes(status) ||
    gate?.schema !== "game.asset_technical_gate" ||
    gate?.version !== 1 ||
    gate?.verdict !== "pass" ||
    gate?.style_lock_id !== lock.id ||
    gate?.source_ref !== element.src
  ) {
    throw new Error("asset style verdict requires current passing technical-gate evidence for this source and style lock");
  }
  return status;
}

function exemplarElementId(ref, projectId) {
  const match = /^canvas:\/\/(?:(?:game\/[a-z][a-z0-9-]*\/)?)([^/]+)\/element\/([A-Za-z0-9_-]+)$/.exec(ref || "");
  if (!match || match[1] !== projectId) {
    throw new Error(`asset style verdict exemplar ref does not belong to Canvas project ${projectId}: ${ref}`);
  }
  return match[2];
}

function prepareInputs(root, project, target, lock) {
  const exemplars = lock.exemplar_refs.map((entry) => {
    const elementId = exemplarElementId(entry.ref, project.id);
    const element = imageElement(project, elementId, "style exemplar");
    return {
      ref: entry.ref,
      domain: entry.domain,
      sourceRef: element.src,
      path: resolveProjectFile(root, project.id, element.src),
    };
  });
  return {
    targetPath: resolveProjectFile(root, project.id, target.src),
    exemplars,
    doPrompt: lock.prompt_preamble,
    dontPrompt: lock.negative_prompt,
  };
}

function refuseExternalChange(message) {
  const error = new Error(`${message} — retry the operation`);
  error.code = "HEAD_CONFLICT";
  throw error;
}

async function runAssetStyleVerdictImpl(root, { projectId, elementId } = {}, dependencies = {}) {
  if (!projectId) throw new Error("runAssetStyleVerdict requires projectId");
  if (!elementId) throw new Error("runAssetStyleVerdict requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const target = imageElement(before, elementId);
  const resolveStyleLock = dependencies.resolveStyleLock || resolveAcceptedGameStyleLock;
  const resolved = resolveStyleLock(root, before);
  if (!resolved?.lock) throw new Error("asset style verdict requires an accepted game style lock");
  const lock = structuredClone(resolved.lock);
  const status = requireCurrentTechnicalPass(target, lock);
  const inputs = prepareInputs(root, before, target, lock);
  const runJudge = dependencies.runJudge || runStyleVerdict;
  const report = validateReport(await runJudge(inputs));
  const checkedAt = new Date().toISOString();

  return withProjectLock(root, projectId, () => {
    const current = getProject(root, projectId);
    refuseIfHeadMoved("runAssetStyleVerdict", before, current);
    let currentResolved;
    try {
      currentResolved = resolveStyleLock(root, current);
    } catch (error) {
      refuseExternalChange(`asset style lock changed while the vision verdict was running (${error.message})`);
    }
    if (!currentResolved?.lock || !isDeepStrictEqual(currentResolved.lock, lock)) {
      refuseExternalChange("asset style lock changed while the vision verdict was running");
    }
    const currentTarget = imageElement(current, elementId);
    requireCurrentTechnicalPass(currentTarget, lock);
    let currentInputs;
    try {
      currentInputs = prepareInputs(root, current, currentTarget, lock);
    } catch (error) {
      refuseExternalChange(`asset style exemplars changed while the vision verdict was running (${error.message})`);
    }
    const startedExemplars = inputs.exemplars.map(({ ref, domain, sourceRef }) => ({ ref, domain, sourceRef }));
    const currentExemplars = currentInputs.exemplars.map(({ ref, domain, sourceRef }) => ({ ref, domain, sourceRef }));
    if (!isDeepStrictEqual(currentExemplars, startedExemplars)) {
      refuseExternalChange("asset style exemplars changed while the vision verdict was running");
    }
    const evidence = {
      schema: report.schema,
      version: report.version,
      verdict: report.verdict,
      checked_at: checkedAt,
      style_lock_id: lock.id,
      source_ref: currentTarget.src,
      exemplar_refs: lock.exemplar_refs.map((entry, index) => ({
        ...structuredClone(entry),
        source_ref: inputs.exemplars[index].sourceRef,
      })),
      do_prompt: lock.prompt_preamble,
      dont_prompt: lock.negative_prompt,
      summary: report.summary,
      strengths: [...report.strengths],
      concerns: [...report.concerns],
    };
    const after = updateProject(root, projectId, {
      elements: (current.elements || []).map((item) => item.id === elementId ? {
        ...item,
        meta: { ...(item.meta || {}), style_verdict: evidence },
      } : item),
    });
    const project = commitMutation(root, projectId, {
      op: "runAssetStyleVerdict",
      args_summary: { elementId, verdict: report.verdict, status },
      before,
      after,
      startedAt,
    });
    return {
      project,
      element: (project.elements || []).find((item) => item.id === elementId),
      status,
      report,
    };
  });
}

export function runAssetStyleVerdict(root, args = {}) {
  return runAssetStyleVerdictImpl(root, args);
}

// Test-only dependency seam: public callers cannot inject a forged judge report.
export function __runAssetStyleVerdictForTest(root, args = {}, dependencies = {}) {
  return runAssetStyleVerdictImpl(root, args, dependencies);
}
