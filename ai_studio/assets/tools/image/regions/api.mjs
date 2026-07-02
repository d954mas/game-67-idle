// image/regions bridge: detect component regions in a source image. Chains the
// bg_fix background-normalization pre-step (unless whole-image passthrough) and
// the region detector, both run through the shared _bridge Python runner. Writes
// the per-session background/ and regions/ artifacts the slice step reads back.
import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { basename, join } from "node:path";

import {
  color,
  ensureInsideTmp,
  intOption,
  readImageSize,
  readJson,
  runPython,
  sessionDirForPath,
  tmpUrl,
  workspaceRel,
  writeJsonFile,
} from "../_bridge/bridge.mjs";

export async function detectImageRegions(root, body) {
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
