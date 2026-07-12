import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const canvasRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const studioRoot = resolve(canvasRoot, "..", "..");

const OPS_EXPORTS = ["addImage","addImageFromFile","addImages","addNote","addText","alignNodes","alphaCutout","alphaDualPlate","alphaDualPlateGenerate","animateElementFromText","assignToGroup","bakeFilters","cleanupApply","cleanupPreview","createAnimCard","createGroup","createProject","createRecipeCard","createStyleCard","deleteGroup","deleteNodes","deleteProject","detectRegions","distributeNodes","duplicateNodes","expandRecipePrompt","exportElements","exportProject","extractFromElement","fitGroup","generateAnimFromCard","generateFromRecipe","getProject","hasBakeableFilters","historyEntryLabel","historyFlags","isCorridorKeyGreenKey","isCorridorKeyMagentaKey","jumpHistory","listHistory","listProjects","migrateScreenFlags","moveNodes","nameDetectedRegions","opsStats","packPreview","packSlice","parseScaleSpec","pasteNodes","patchAnim","patchElement","patchElements","patchGroup","patchGroups","patchProject","patchRecipe","patchStyle","promoteExtractedRecipe","promoteExtractedStyle","readHistory","recordOpFailure","redoOp","removeElement","removeElements","renderGroup","reorderElement","reorderNode","reorderNodes","reparentGroup","resolveExportScale","resolveProjectFile","resolveProjectPath","scaleGroup","setElementAnimation","setExportSettings","setOpsActor","setRegions","setSlice9","sliceRegions","undoOp","ungroupGroup","updateProject","withProjectLock","zipExport"];
const INSPECTOR_EXPORTS = ["PACK_AXES_SKELETON","defaultAspectLock","estimatePackSheetCount","initInspector","linkedDimension","normalizeSmartQuotes","parseAxesJson","renderInspector"];
const WORKSPACE_EXPORTS = ["cancelPolygonDraft","clearCleanupPreview","finishPolygonDraft","fit","getCleanupPreview","initWorkspace","isAnimationPreviewing","loadCleanupBitmap","placeNoteAt","popPolygonVertex","render","requestRender","setCleanupPreview","setCleanupPreviewCompare","setRegionTool","setTool","startAnimationPreview","stopAnimationPreview","syncTopBar","toggleAnimationPreview","zoomTo"];

test("Canvas public facades preserve their baseline exports", async () => {
  const [ops, inspector, workspace] = await Promise.all([
    import("../ops.mjs"),
    import("../site/inspector.js"),
    import("../site/workspace.js"),
  ]);
  assert.deepEqual(Object.keys(ops).sort(), OPS_EXPORTS);
  assert.deepEqual(Object.keys(inspector).sort(), INSPECTOR_EXPORTS);
  assert.deepEqual(Object.keys(workspace).sort(), WORKSPACE_EXPORTS);
});

test("Canvas owns decomposed ops, UI internals, and Chat runtime", () => {
  for (const relative of [
    "ops/core.mjs",
    "ops/export_scale.mjs",
    "ops/project_lifecycle.mjs",
    "site/inspector/runtime.js",
    "site/inspector/contracts.js",
    "site/inspector/controls.js",
    "site/workspace/runtime.js",
    "site/workspace/animation_preview.js",
    "site/workspace/cleanup_preview.js",
    "site/workspace/image_filters.js",
    "chat/api.mjs",
    "chat/tests/api.test.mjs",
  ]) assert.equal(existsSync(resolve(canvasRoot, relative)), true, `missing ${relative}`);
  assert.equal(existsSync(resolve(studioRoot, "studio_shell", "chat")), false);
  assert.equal(existsSync(resolve(canvasRoot, "ops", "runtime.mjs")), false, "ops monolith must not remain");
  for (const domain of ["core", "elements", "groups", "generation", "history", "image_pipeline"]) {
    const source = readFileSync(resolve(canvasRoot, "ops", `${domain}.mjs`), "utf8");
    assert.match(source, /^export (?:async )?function /m, `${domain} must own implementation functions`);
    assert.doesNotMatch(source, /from ["']\.\/runtime\.mjs["']/);
    assert.ok(source.split("\n").length < 3000, `${domain} must stay below 3000 lines`);
  }
});

test("tracked Studio config is portable", () => {
  const configText = readFileSync(resolve(studioRoot, "studio.config.json"), "utf8");
  assert.doesNotMatch(configText, /[A-Za-z]:[\\/]|\/Users\/|\/home\//);
});
