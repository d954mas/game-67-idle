export function normalizeStageView(value) {
  return value === "edit" ? "edit" : "review";
}

export function normalizeWorkspaceMode(value) {
  return value === "slice9" ? "slice9" : "regions";
}

export function resolveStageView(value = {}) {
  const workspaceMode = normalizeWorkspaceMode(value.workspaceMode);
  const stageView = normalizeStageView(value.stageView);
  const showReviewStage = workspaceMode === "regions" && stageView === "review";
  return {
    workspaceMode,
    stageView,
    showReviewStage,
    showCanvasStage: !showReviewStage,
  };
}
