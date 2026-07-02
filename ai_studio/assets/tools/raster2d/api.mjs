// Raster 2D asset tools HTTP bridge.
//
// The generic plumbing (Python interpreter resolution, tmp-path confinement,
// session dirs, JSON/image-size helpers) lives in the shared image tools bridge;
// this file keeps only the raster2d pipeline logic and the public HTTP handlers.
// The endpoint URLs (/api/asset-tools/raster2d/*) and the tmp/ai_studio/assets/
// raster2d/ path prefix are a stable public contract for the frozen Asset Tools
// viewer and must not change here.
import { randomUUID } from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";

import {
  color,
  decodeDataUrl,
  ensureInsideTmp,
  extensionForUpload,
  intOption,
  readImageSize,
  readJson,
  readJsonBody,
  runPython,
  safeResolve,
  safeSlug,
  sessionDirForPath,
  tmpRoot,
  tmpUrl,
  workspaceRel,
  writeJson,
  writeJsonFile,
} from "../image/_bridge/bridge.mjs";

function sessionKeyColor(root, sessionDir) {
  const reportPath = join(sessionDir, "background", "normalize_report.json");
  if (!existsSync(reportPath)) return "";
  try {
    return color(readJson(reportPath).key_color, "");
  } catch {
    return "";
  }
}

export async function uploadRaster2dSource(root, body) {
  const { mime, buffer } = decodeDataUrl(body.dataUrl);
  if (!buffer.length) throw new Error("empty source image");
  const sessionId = `${new Date().toISOString().replace(/[:.]/g, "-")}-${randomUUID().slice(0, 8)}`;
  const outDir = join(tmpRoot(root), sessionId, "sources");
  mkdirSync(outDir, { recursive: true });
  const ext = extensionForUpload(body.fileName, mime);
  const sourceName = `${safeSlug(basename(String(body.fileName || "source")))}${ext}`;
  const sourcePath = join(outDir, sourceName);
  writeFileSync(sourcePath, buffer);
  return {
    sessionId,
    sourcePath: workspaceRel(root, sourcePath),
    sourceUrl: tmpUrl(root, sourcePath),
    fileName: sourceName,
  };
}

export async function detectRaster2dRegions(root, body) {
  const sourcePath = ensureInsideTmp(root, body.sourcePath);
  if (!existsSync(sourcePath)) throw new Error("source image not found");
  const sessionDir = sessionDirForPath(root, sourcePath);
  const backgroundDir = join(sessionDir, "background");
  const regionsDir = join(sessionDir, "regions");
  mkdirSync(backgroundDir, { recursive: true });
  mkdirSync(regionsDir, { recursive: true });

  const options = body.options || {};
  const explicitKeyColor = typeof options.keyColor === "string" ? color(options.keyColor, "") : "";
  const backgroundMode = String(options.backgroundMode || "auto").trim().toLowerCase();
  const wholeImageMode = ["whole_image", "none", "no_background"].includes(backgroundMode);
  const keyTolerance = intOption(options.keyTolerance ?? options.tolerance, 32, 255);
  const minArea = intOption(options.minArea, 256);
  const padding = intOption(options.padding, 8, 1024);
  const mergeDistance = intOption(options.mergeDistance, 0, 1024);
  const rowTolerance = intOption(options.rowTolerance, 32, 4096);
  const normalizedPath = join(backgroundDir, "normalized.png");
  const normalizeReportPath = join(backgroundDir, "normalize_report.json");
  const regionsPath = join(regionsDir, "regions.json");
  const overlayPath = join(regionsDir, "overlay.png");

  if (wholeImageMode) {
    copyFileSync(sourcePath, normalizedPath);
    const { width, height } = readImageSize(sourcePath);
    const normalizeReport = {
      schema: "ai_studio.raster2d.background_normalize.v1",
      mode: "passthrough_no_background",
      key_color: "#ff00ff",
      key_tolerance: keyTolerance,
      alpha_threshold: 0,
      image: { width, height },
      background_pixels: 0,
      changed_pixels: 0,
      source: workspaceRel(root, sourcePath),
      output: workspaceRel(root, normalizedPath),
    };
    writeJsonFile(normalizeReportPath, normalizeReport);
    const regions = {
      schema: "ai_studio.raster2d.detected_regions.v1",
      version: 1,
      source: workspaceRel(root, normalizedPath),
      mode: "whole_image",
      image: { width, height },
      options: {
        background_mode: "whole_image",
        key_color: "#ff00ff",
        key_tolerance: 0,
        min_area: minArea,
        padding: 0,
        merge_distance: 0,
        row_tolerance: rowTolerance,
      },
      count: 1,
      regions: [
        {
          id: "region_001",
          rect: [0, 0, width, height],
          content_bbox: [0, 0, width, height],
          area_px: width * height,
          source: "whole_image",
        },
      ],
    };
    writeJsonFile(regionsPath, regions);
    return {
      sessionId: basename(sessionDir),
      sourcePath: workspaceRel(root, sourcePath),
      normalizedPath: workspaceRel(root, normalizedPath),
      normalizedUrl: tmpUrl(root, normalizedPath),
      normalizeReportPath: workspaceRel(root, normalizeReportPath),
      regionsPath: workspaceRel(root, regionsPath),
      overlayUrl: "",
      normalizeReport,
      regions,
    };
  }

  const normalizeArgs = [
    "ai_studio/assets/tools/image/bg_fix/normalize_background.py",
    "--source", sourcePath,
    "--output", normalizedPath,
    "--mode", "auto",
    "--key-tolerance", String(keyTolerance),
    "--json-output", normalizeReportPath,
  ];
  if (explicitKeyColor) {
    normalizeArgs.push("--key-color", explicitKeyColor);
  }
  await runPython(root, normalizeArgs);
  const normalizeReport = readJson(normalizeReportPath);
  const keyColor = color(normalizeReport.key_color);
  await runPython(root, [
    "ai_studio/assets/tools/image/regions/detect_regions.py",
    "--source", normalizedPath,
    "--key-color", keyColor,
    "--key-tolerance", "0",
    "--min-area", String(minArea),
    "--padding", String(padding),
    "--merge-distance", String(mergeDistance),
    "--row-tolerance", String(rowTolerance),
    "--json-output", regionsPath,
    "--overlay-output", overlayPath,
  ]);

  return {
    sessionId: basename(sessionDir),
    sourcePath: workspaceRel(root, sourcePath),
    normalizedPath: workspaceRel(root, normalizedPath),
    normalizedUrl: tmpUrl(root, normalizedPath),
    normalizeReportPath: workspaceRel(root, normalizeReportPath),
    regionsPath: workspaceRel(root, regionsPath),
    overlayUrl: tmpUrl(root, overlayPath),
    normalizeReport,
    regions: readJson(regionsPath),
  };
}

function reviewedRegionsPayload(body) {
  const regions = Array.isArray(body.regions) ? body.regions : [];
  return {
    schema: "ai_studio.raster2d.region_review.v1",
    source: body.sourcePath || body.normalizedPath || "",
    updated_at: new Date().toISOString(),
    region_count: regions.length,
    regions,
  };
}

async function runSlice(root, body, { mode }) {
  const imagePath = ensureInsideTmp(root, body.imagePath || body.normalizedPath || body.sourcePath);
  if (!existsSync(imagePath)) throw new Error("image for slicing not found");
  const sessionDir = sessionDirForPath(root, imagePath);
  const outDirName = mode === "export" ? "export" : mode === "single" ? "single" : "review";
  const outDir = join(sessionDir, outDirName);
  mkdirSync(outDir, { recursive: true });
  const regionsPath = join(outDir, "regions.reviewed.json");
  writeFileSync(regionsPath, JSON.stringify(reviewedRegionsPayload({ ...body, sourcePath: workspaceRel(root, imagePath) }), null, 2) + "\n", "utf8");

  const prefix = safeSlug(body.prefix || basename(imagePath), "asset");
  const wantsReviewSheet = mode === "review" || (mode === "export" && body.includeReviewSheet !== false);
  const reviewSheetPath = wantsReviewSheet ? join(outDir, "review_sheet.png") : null;
  const manifestPath = join(outDir, "manifest.json");
  const zipPath = mode === "export" ? join(outDir, `${prefix}_regions.zip`) : null;
  const keyColor = sessionKeyColor(root, sessionDir);
  const args = [
    "ai_studio/assets/tools/image/slice/slice_regions.py",
    "--source", imagePath,
    "--regions", regionsPath,
    "--output-dir", outDir,
    "--prefix", prefix,
    "--manifest-output", manifestPath,
  ];
  if (reviewSheetPath) {
    args.push("--review-sheet", reviewSheetPath);
  }
  if (zipPath) {
    args.push("--zip-output", zipPath);
  }
  if (keyColor) {
    args.push("--key-color", keyColor);
  }
  await runPython(root, args);

  return {
    sessionId: basename(sessionDir),
    reviewedRegionsPath: workspaceRel(root, regionsPath),
    manifestPath: workspaceRel(root, manifestPath),
    manifest: readJson(manifestPath),
    reviewSheetPath: reviewSheetPath && existsSync(reviewSheetPath) ? workspaceRel(root, reviewSheetPath) : null,
    reviewSheetUrl: reviewSheetPath && existsSync(reviewSheetPath) ? tmpUrl(root, reviewSheetPath) : null,
    zipPath: zipPath && existsSync(zipPath) ? workspaceRel(root, zipPath) : null,
    zipUrl: zipPath && existsSync(zipPath) ? tmpUrl(root, zipPath) : null,
  };
}

function directSliceResult(root, result) {
  const slice = result.manifest?.slices?.[0];
  if (!slice?.path) return result;
  const slicePath = safeResolve(root, slice.path);
  if (!slicePath || !existsSync(slicePath)) return result;
  return {
    ...result,
    slice,
    fileName: slice.file || basename(slicePath),
    slicePath: workspaceRel(root, slicePath),
    sliceUrl: tmpUrl(root, slicePath),
  };
}

export async function reviewRaster2dRegions(root, body) {
  return runSlice(root, body, { mode: "review" });
}

export async function exportRaster2dRegions(root, body) {
  return runSlice(root, body, { mode: "export" });
}

export async function exportRaster2dRegion(root, body) {
  const region = body.region || (Array.isArray(body.regions) ? body.regions[0] : null);
  if (!region) throw new Error("missing region for single export");
  const result = await runSlice(root, { ...body, regions: [region], includeReviewSheet: false }, { mode: "single" });
  return directSliceResult(root, result);
}

export function resolveRaster2dTmpPath(root, pathname) {
  if (!pathname.startsWith("/tmp/")) return null;
  return safeResolve(join(root, "tmp"), pathname.slice("/tmp/".length));
}

export function createRaster2dAssetToolsApi(root) {
  return async function handleRaster2dAssetToolsApi(req, res, url) {
    try {
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/upload") {
        writeJson(res, 200, await uploadRaster2dSource(root, await readJsonBody(req)));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/detect") {
        writeJson(res, 200, await detectRaster2dRegions(root, await readJsonBody(req)));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/review") {
        writeJson(res, 200, await reviewRaster2dRegions(root, await readJsonBody(req)));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/export") {
        writeJson(res, 200, await exportRaster2dRegions(root, await readJsonBody(req)));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-tools/raster2d/export-one") {
        writeJson(res, 200, await exportRaster2dRegion(root, await readJsonBody(req)));
        return true;
      }
      return false;
    } catch (error) {
      writeJson(res, 400, { error: error.message });
      return true;
    }
  };
}
