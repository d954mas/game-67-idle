import { performance } from "node:perf_hooks";
import { isDeepStrictEqual } from "node:util";

import { normalizeAssetStatus } from "../asset_status.mjs";
import { resolveAcceptedGameStyleLock } from "../../style_lock/generation_origin.mjs";
import { thresholdsFromStyleLock } from "../../tools/image/quality_gate/api.mjs";
import { getProject, updateProject } from "../store.mjs";
import { commitMutation } from "./core.mjs";

const DECISIONS = new Set(["accept", "revise", "reject"]);
const ADVISORY_VERDICTS = new Set(["accept", "revise", "reject"]);
const MAX_REASON_CHARS = 1000;

function imageElement(project, elementId) {
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} asset style decision is image-only`);
  return element;
}

function normalizeDecision(value) {
  const decision = typeof value === "string" ? value.trim() : "";
  if (!DECISIONS.has(decision)) {
    throw new Error(`asset style decision must be accept|revise|reject, got ${JSON.stringify(value)}`);
  }
  return decision;
}

function normalizeReason(value) {
  if (typeof value !== "string" || !value.trim()) {
    throw new Error("asset style decision requires a non-empty reason");
  }
  const reason = value.trim();
  if (reason.length > MAX_REASON_CHARS) {
    throw new Error(`asset style decision reason must be at most ${MAX_REASON_CHARS} characters`);
  }
  return reason;
}

function exemplarElementId(ref, projectId) {
  const match = /^canvas:\/\/(?:(?:game\/[a-z][a-z0-9-]*\/)?)([^/]+)\/element\/([A-Za-z0-9_-]+)$/.exec(ref || "");
  if (!match || match[1] !== projectId) return null;
  return match[2];
}

export function requireCurrentStyleEvidence(project, element, lock) {
  const status = element.assetStatus == null ? null : normalizeAssetStatus(element.assetStatus);
  const technical = element.meta?.technical_gate;
  const advisory = element.meta?.style_verdict;
  const expectedThresholds = lock ? thresholdsFromStyleLock(lock) : null;
  const expectedKeyColor = lock?.bg_rule?.mode === "chroma" ? String(lock.bg_rule.key_color).toUpperCase() : null;
  const baseValid = ["checked", "accepted"].includes(status)
    && technical?.schema === "game.asset_technical_gate"
    && technical?.version === 1
    && technical?.verdict === "pass"
    && technical?.source_ref === element.src
    && technical?.key_color === expectedKeyColor
    && isDeepStrictEqual(technical?.thresholds, expectedThresholds)
    && advisory?.schema === "game.asset_style_verdict"
    && advisory?.version === 1
    && ADVISORY_VERDICTS.has(advisory?.verdict)
    && typeof advisory?.checked_at === "string"
    && advisory.checked_at.length > 0
    && advisory?.source_ref === element.src
    && technical?.style_lock_id === advisory?.style_lock_id
    && advisory?.style_lock_id === lock?.id
    && isDeepStrictEqual(advisory?.style_lock_snapshot, lock)
    && advisory?.do_prompt === lock?.prompt_preamble
    && advisory?.dont_prompt === lock?.negative_prompt
    && Array.isArray(lock?.exemplar_refs)
    && Array.isArray(advisory?.exemplar_refs)
    && advisory.exemplar_refs.length === lock.exemplar_refs.length;
  if (!baseValid) {
    throw new Error("asset style decision requires current style-verdict evidence for this source, technical pass, and accepted style lock");
  }

  const exemplarsValid = lock.exemplar_refs.every((expected, index) => {
    const stored = advisory.exemplar_refs[index];
    const elementId = exemplarElementId(expected.ref, project.id);
    const exemplar = elementId ? (project.elements || []).find((item) => item.id === elementId) : null;
    return stored?.ref === expected.ref
      && stored?.origin === expected.origin
      && stored?.domain === expected.domain
      && typeof stored?.source_ref === "string"
      && exemplar?.type === "image"
      && exemplar?.src === stored.source_ref;
  });
  if (!exemplarsValid) {
    throw new Error("asset style decision requires current style-verdict evidence for every owned exemplar");
  }
  return { status, advisory };
}

export function requireAcceptedStyleDecision(project, element, lock) {
  const status = element.assetStatus == null ? null : normalizeAssetStatus(element.assetStatus);
  if (status !== "accepted") {
    throw new Error(`asset promotion requires accepted review state, got ${status || "untracked"}`);
  }
  const { advisory } = requireCurrentStyleEvidence(project, element, lock);
  const decision = element.meta?.style_decision;
  const reasonValid = typeof decision?.reason === "string"
    && decision.reason.trim().length > 0
    && decision.reason.length <= MAX_REASON_CHARS;
  if (
    decision?.schema !== "game.asset_style_decision" ||
    decision?.version !== 1 ||
    decision?.decision !== "accept" ||
    typeof decision?.decided_at !== "string" ||
    !decision.decided_at ||
    !reasonValid ||
    decision?.style_lock_id !== lock.id ||
    decision?.source_ref !== element.src ||
    decision?.advisory_verdict !== advisory.verdict ||
    decision?.advisory_checked_at !== advisory.checked_at
  ) {
    throw new Error("asset promotion requires an explicit current lead accept decision for this source and advisory verdict");
  }
  return { advisory, decision };
}

function decideAssetStyleImpl(root, { projectId, elementId, decision, reason } = {}, dependencies = {}) {
  if (!projectId) throw new Error("decideAssetStyle requires projectId");
  if (!elementId) throw new Error("decideAssetStyle requires elementId");
  const normalizedDecision = normalizeDecision(decision);
  const normalizedReason = normalizeReason(reason);
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const current = imageElement(before, elementId);
  const advisory = current.meta?.style_verdict;
  if (!advisory) {
    throw new Error("asset style decision requires current style-verdict evidence before a lead decision");
  }
  const resolveStyleLock = dependencies.resolveStyleLock || resolveAcceptedGameStyleLock;
  const resolved = resolveStyleLock(root, before);
  const { advisory: currentAdvisory } = requireCurrentStyleEvidence(before, current, resolved?.lock);
  const decidedAt = new Date().toISOString();
  const nextStatus = normalizedDecision === "accept" ? "accepted" : "quarantine";
  const evidence = {
    schema: "game.asset_style_decision",
    version: 1,
    decision: normalizedDecision,
    decided_at: decidedAt,
    reason: normalizedReason,
    style_lock_id: currentAdvisory.style_lock_id,
    source_ref: current.src,
    advisory_verdict: currentAdvisory.verdict,
    advisory_checked_at: currentAdvisory.checked_at,
  };
  const after = updateProject(root, projectId, {
    elements: (before.elements || []).map((item) => item.id === elementId ? {
      ...item,
      assetStatus: nextStatus,
      meta: { ...(item.meta || {}), style_decision: evidence },
    } : item),
  });
  const project = commitMutation(root, projectId, {
    op: "decideAssetStyle",
    args_summary: { elementId, decision: normalizedDecision, advisory_verdict: currentAdvisory.verdict },
    before,
    after,
    startedAt,
  });
  return {
    project,
    element: (project.elements || []).find((item) => item.id === elementId),
    status: nextStatus,
    decision: normalizedDecision,
  };
}

export function decideAssetStyle(root, args = {}) {
  return decideAssetStyleImpl(root, args);
}

// Test-only resolver seam: public callers cannot replace the accepted style-lock lookup.
export function __decideAssetStyleForTest(root, args = {}, dependencies = {}) {
  return decideAssetStyleImpl(root, args, dependencies);
}
