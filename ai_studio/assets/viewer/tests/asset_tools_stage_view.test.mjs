import { strict as assert } from "node:assert";
import test from "node:test";
import { resolveStageView } from "../asset_tools_stage_view.mjs";

test("regions review view shows the review sheet as the main stage", () => {
  assert.deepEqual(resolveStageView({ workspaceMode: "regions", stageView: "review" }), {
    workspaceMode: "regions",
    stageView: "review",
    showReviewStage: true,
    showCanvasStage: false,
  });
});

test("regions edit view shows the canvas editor", () => {
  assert.deepEqual(resolveStageView({ workspaceMode: "regions", stageView: "edit" }), {
    workspaceMode: "regions",
    stageView: "edit",
    showReviewStage: false,
    showCanvasStage: true,
  });
});

test("slice 9 workspace keeps the canvas stage even when review is selected", () => {
  assert.deepEqual(resolveStageView({ workspaceMode: "slice9", stageView: "review" }), {
    workspaceMode: "slice9",
    stageView: "review",
    showReviewStage: false,
    showCanvasStage: true,
  });
});
