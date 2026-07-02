// image/slice bridge: apply reviewed regions to a source image and write PNG
// slices, a manifest, an optional review sheet, and an optional ZIP. Alpha is
// applied per region by slice_regions.py (key-matte default, no silent fallback).
// The key colour is recovered from the bg_fix normalize report the detect step
// wrote into the same session. Thin wrapper over the shared _bridge plumbing.
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";

import {
  color,
  ensureInsideTmp,
  readJson,
  runPython,
  safeResolve,
  safeSlug,
  sessionDirForPath,
  tmpUrl,
  workspaceRel,
} from "../_bridge/bridge.mjs";

function sessionKeyColor(root, sessionDir) {
  const reportPath = join(sessionDir, "background", "normalize_report.json");
  if (!existsSync(reportPath)) return "";
  try {
    return color(readJson(reportPath).key_color, "");
  } catch {
    return "";
  }
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

export async function reviewImageRegions(root, body) {
  return runSlice(root, body, { mode: "review" });
}

export async function exportImageRegions(root, body) {
  return runSlice(root, body, { mode: "export" });
}

export async function exportImageRegion(root, body) {
  const region = body.region || (Array.isArray(body.regions) ? body.regions[0] : null);
  if (!region) throw new Error("missing region for single export");
  const result = await runSlice(root, { ...body, regions: [region], includeReviewSheet: false }, { mode: "single" });
  return directSliceResult(root, result);
}
