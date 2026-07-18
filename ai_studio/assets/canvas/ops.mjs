// Stable public Canvas operation facade. Implementations live in domain modules
// under ops/; this file alone defines the supported API surface.
export { getProject, listProjects, recordOpFailure, resolveProjectFile, resolveProjectPath, setOpsActor, updateProject, withProjectLock } from "./ops/core.mjs";
export { createProject, deleteProject } from "./ops/project_lifecycle.mjs";
export { parseScaleSpec, resolveExportScale } from "./ops/export_scale.mjs";
export { addImage, addImageFromFile, addImages, addNote, addText, alignNodes, distributeNodes, getAssetStatus, moveNodes, patchElement, patchElements, patchProject, removeElement, removeElements, reorderElement, reorderNode, reorderNodes, setAssetStatus, setElementAnimation, setExportSettings, setRegions, setSlice9 } from "./ops/elements.mjs";
export { assignToGroup, createGroup, deleteGroup, fitGroup, patchGroup, patchGroups, reparentGroup, scaleGroup, ungroupGroup } from "./ops/groups.mjs";
export { animateElementFromText, createAnimCard, createRecipeCard, createStyleCard, expandRecipePrompt, extractFromElement, generateAnimFromCard, generateFromRecipe, packPreview, patchAnim, patchRecipe, patchStyle, promoteExtractedRecipe, promoteExtractedStyle } from "./ops/generation.mjs";
export { deleteNodes, duplicateNodes, historyEntryLabel, historyFlags, jumpHistory, listHistory, opsStats, pasteNodes, readHistory, redoOp, undoOp } from "./ops/history.mjs";
export { alphaCutout, alphaDualPlate, alphaDualPlateGenerate, bakeFilters, cleanupApply, cleanupPreview, detectRegions, exportElements, exportProject, hasBakeableFilters, isCorridorKeyGreenKey, isCorridorKeyMagentaKey, nameDetectedRegions, packSlice, renderGroup, sliceRegions, zipExport } from "./ops/image_pipeline.mjs";
export { runAssetTechnicalGate } from "./ops/technical_gate.mjs";
export { promoteAssetToGame } from "./ops/asset_promotion.mjs";
export { decideAssetStyle } from "./ops/style_decision.mjs";
export { runAssetStyleVerdict } from "./ops/style_verdict.mjs";
