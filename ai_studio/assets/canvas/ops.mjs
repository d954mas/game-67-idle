// Canvas operation layer — the ONE surface both clients call.
//
// Product rule (tool parity): every canvas capability is exactly one operation
// here. The HTTP API adapter (api.mjs) and the agent CLI (cli.mjs) are thin
// clients that only marshal input/output; they hold no logic of their own. Tests
// exercise these functions directly, so the browser page and an agent always go
// through identical code.
//
// Most ops are thin wrappers over the store. detectRegions is the one bridged
// pipeline op: it reuses the existing raster2d tool functions unmodified to prove
// parity between the canvas layer and the established 2D asset pipeline.
import { randomUUID } from "node:crypto";
import {
  detectRaster2dRegions,
  uploadRaster2dSource,
} from "../tools/raster2d/api.mjs";
import {
  addImage,
  createProject,
  getProject,
  listProjects,
  patchElement,
  readElementBytes,
  removeElement,
  resolveProjectFile,
  updateProject,
} from "./store.mjs";

export {
  addImage,
  createProject,
  getProject,
  listProjects,
  patchElement,
  removeElement,
  resolveProjectFile,
  updateProject,
};

function mimeForExt(fileName) {
  const ext = String(fileName || "").toLowerCase().split(".").pop();
  return (
    { png: "image/png", jpg: "image/jpeg", jpeg: "image/jpeg", gif: "image/gif", webp: "image/webp" }[ext] ||
    "image/png"
  );
}

// detectRegions: read the element's stored image, run it through the existing
// raster2d upload + detect pipeline (imported unmodified from
// ai_studio/assets/tools/raster2d/api.mjs), then persist the detected regions on
// the element and record a tool_runs entry. `root` is the repo root: the store
// resolves the canvas projects root from it, and raster2d writes its session
// under <root>/tmp and runs Python with cwd=root.
export async function detectRegions(root, { projectId, elementId, params = {} } = {}) {
  if (!projectId) throw new Error("detectRegions requires projectId");
  if (!elementId) throw new Error("detectRegions requires elementId");
  const { buffer, fileName } = readElementBytes(root, projectId, elementId);

  // Reuse the raster2d op path exactly as the Asset Tools surface does: stage the
  // bytes as a session source, then detect on that source. No raster2d code is
  // modified; we only call its exported functions.
  const dataUrl = `data:${mimeForExt(fileName)};base64,${buffer.toString("base64")}`;
  const uploaded = await uploadRaster2dSource(root, { fileName, dataUrl });
  const detected = await detectRaster2dRegions(root, {
    sourcePath: uploaded.sourcePath,
    options: params || {},
  });
  const regions = Array.isArray(detected.regions && detected.regions.regions)
    ? detected.regions.regions
    : [];

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "detect_regions",
    elementId,
    at: new Date().toISOString(),
    params: params || {},
    result_summary: {
      region_count: regions.length,
      session_id: detected.sessionId,
      background_mode: (detected.regions && detected.regions.mode) || "",
    },
  };

  // Re-read to avoid clobbering any concurrent edits, then persist regions on the
  // element plus the tool_runs entry in one atomic project write.
  const project = getProject(root, projectId);
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  element.regions = regions;
  const toolRuns = [...(project.tool_runs || []), run];
  const saved = updateProject(root, projectId, { elements: project.elements, tool_runs: toolRuns });
  return { project: saved, element, run, regions };
}
