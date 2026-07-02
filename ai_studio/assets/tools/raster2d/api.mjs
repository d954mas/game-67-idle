// SEAM SHIM (temporary). The raster2d asset-tools API was decomposed into the
// per-tool bridges under ai_studio/assets/tools/image/*. The studio shell now
// wires the composed handler from ../image/api.mjs directly.
//
// This file survives ONLY because the canvas still imports detectRaster2dRegions
// and uploadRaster2dSource from here (ai_studio/assets/canvas/ops.mjs:46-49).
// Canvas migrates to ../image/{regions,sources}/api.mjs in increment 6, and then
// this file dies. Do not add new logic here -- re-exports only.
export { uploadImageSource as uploadRaster2dSource } from "../image/sources/api.mjs";
export { detectImageRegions as detectRaster2dRegions } from "../image/regions/api.mjs";
export {
  exportImageRegion as exportRaster2dRegion,
  exportImageRegions as exportRaster2dRegions,
  reviewImageRegions as reviewRaster2dRegions,
} from "../image/slice/api.mjs";
export {
  createImageAssetToolsApi as createRaster2dAssetToolsApi,
  resolveImageTmpPath as resolveRaster2dTmpPath,
} from "../image/api.mjs";
