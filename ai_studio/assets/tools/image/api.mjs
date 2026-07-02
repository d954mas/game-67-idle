// image asset tools: composed HTTP handler over the per-tool bridges
// (sources / regions / slice) and the shared _bridge plumbing.
//
// HARD CONTRACT: the public endpoint URLs are kept byte-identical to the frozen
// Asset Tools viewer surface -- /api/asset-tools/raster2d/{upload,detect,review,
// export,export-one} -- and the tmp path prefix stays tmp/ai_studio/assets/
// raster2d/. Internal symbol names (createImageAssetToolsApi, resolveImageTmpPath)
// changed with the decomposition; the URL strings did not.
import { join } from "node:path";

import { readJsonBody, safeResolve, writeJson } from "./_bridge/bridge.mjs";
import { uploadImageSource } from "./sources/api.mjs";
import { detectImageRegions } from "./regions/api.mjs";
import { exportImageRegion, exportImageRegions, reviewImageRegions } from "./slice/api.mjs";

export function resolveImageTmpPath(root, pathname) {
  if (!pathname.startsWith("/tmp/")) return null;
  return safeResolve(join(root, "tmp"), pathname.slice("/tmp/".length));
}

export function createImageAssetToolsApi(root) {
  return async function handleImageAssetToolsApi(req, res, url) {
    try {
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/upload") {
        writeJson(res, 200, await uploadImageSource(root, await readJsonBody(req)));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/detect") {
        writeJson(res, 200, await detectImageRegions(root, await readJsonBody(req)));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/review") {
        writeJson(res, 200, await reviewImageRegions(root, await readJsonBody(req)));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/export") {
        writeJson(res, 200, await exportImageRegions(root, await readJsonBody(req)));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/export-one") {
        writeJson(res, 200, await exportImageRegion(root, await readJsonBody(req)));
        return true;
      }
      return false;
    } catch (error) {
      writeJson(res, 400, { error: error.message });
      return true;
    }
  };
}
