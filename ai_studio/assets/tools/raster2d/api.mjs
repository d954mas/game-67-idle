import { execFile } from "node:child_process";
import { randomUUID } from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, extname, join, normalize, relative, resolve, sep } from "node:path";

const maxBodyBytes = 64 * 1024 * 1024;

function safeSlug(value, fallback = "asset") {
  return String(value || fallback)
    .trim()
    .replace(/\.[^.]+$/, "")
    .replace(/[^a-zA-Z0-9_.-]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .slice(0, 90) || fallback;
}

function safeResolve(base, relativePath) {
  const resolvedBase = resolve(base);
  const full = resolve(resolvedBase, normalize(relativePath));
  if (full !== resolvedBase && !full.startsWith(resolvedBase + sep)) return null;
  return full;
}

function tmpRoot(root) {
  return join(root, "tmp", "ai_studio", "assets", "raster2d");
}

function ensureInsideTmp(root, pathValue) {
  if (!pathValue) throw new Error("missing tmp path");
  const resolved = safeResolve(root, pathValue);
  if (!resolved) throw new Error("path must stay inside repository");
  const base = resolve(tmpRoot(root));
  if (resolved !== base && !resolved.startsWith(base + sep)) {
    throw new Error("asset tool files must stay under tmp/ai_studio/assets/raster2d");
  }
  return resolved;
}

function sessionDirForPath(root, absPath) {
  const base = resolve(tmpRoot(root));
  const rel = relative(base, absPath);
  const [session] = rel.split(sep);
  if (!session || session.startsWith("..")) throw new Error("invalid raster2d session path");
  return join(base, session);
}

function workspaceRel(root, absPath) {
  return relative(root, absPath).replaceAll("\\", "/");
}

function tmpUrl(root, absPath) {
  const relToTmp = relative(join(root, "tmp"), absPath).split(sep).map(encodeURIComponent).join("/");
  return `/tmp/${relToTmp}`;
}

function writeJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data, null, 2));
}

function readJsonBody(req) {
  return new Promise((resolveBody, rejectBody) => {
    let size = 0;
    let data = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => {
      size += Buffer.byteLength(chunk);
      if (size > maxBodyBytes) {
        rejectBody(new Error("request body too large"));
        req.destroy();
        return;
      }
      data += chunk;
    });
    req.on("end", () => {
      try {
        resolveBody(data ? JSON.parse(data) : {});
      } catch {
        rejectBody(new Error("invalid JSON body"));
      }
    });
    req.on("error", rejectBody);
  });
}

function pythonCandidates() {
  const candidates = [];
  for (const command of [process.env.AI_STUDIO_PYTHON, process.env.PYTHON]) {
    if (command && !candidates.some((candidate) => candidate.command === command)) {
      candidates.push({ command, args: [] });
    }
  }
  for (const command of ["C:\\Python312\\python.exe", "C:\\Python314\\python.exe"]) {
    if (existsSync(command) && !candidates.some((candidate) => candidate.command === command)) {
      candidates.push({ command, args: [] });
    }
  }
  candidates.push({ command: "py", args: ["-3.12"] });
  candidates.push({ command: "python", args: [] });
  return candidates;
}

function runPython(root, args) {
  return new Promise((resolveRun, rejectRun) => {
    const candidates = pythonCandidates();
    const failures = [];
    const tryCandidate = (index) => {
      const candidate = candidates[index];
      execFile(candidate.command, [...candidate.args, ...args], { cwd: root, windowsHide: true }, (error, stdout, stderr) => {
        if (!error) {
          resolveRun(stdout);
          return;
        }
        failures.push((stderr || stdout || error.message).trim());
        if (index + 1 < candidates.length) {
          tryCandidate(index + 1);
          return;
        }
        rejectRun(new Error(failures.filter(Boolean).at(-1) || error.message));
      });
    };
    tryCandidate(0);
  });
}

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function writeJsonFile(path, data) {
  writeFileSync(path, JSON.stringify(data, null, 2) + "\n", "utf8");
}

function readImageSize(path) {
  const buffer = readFileSync(path);
  if (
    buffer.length >= 24 &&
    buffer[0] === 0x89 &&
    buffer[1] === 0x50 &&
    buffer[2] === 0x4e &&
    buffer[3] === 0x47
  ) {
    return { width: buffer.readUInt32BE(16), height: buffer.readUInt32BE(20) };
  }
  if (buffer.length >= 10 && buffer.toString("ascii", 0, 3) === "GIF") {
    return { width: buffer.readUInt16LE(6), height: buffer.readUInt16LE(8) };
  }
  if (buffer.length >= 4 && buffer[0] === 0xff && buffer[1] === 0xd8) {
    let offset = 2;
    while (offset + 9 < buffer.length) {
      if (buffer[offset] !== 0xff) break;
      const marker = buffer[offset + 1];
      const length = buffer.readUInt16BE(offset + 2);
      if (length < 2) break;
      if (marker >= 0xc0 && marker <= 0xc3) {
        return { width: buffer.readUInt16BE(offset + 7), height: buffer.readUInt16BE(offset + 5) };
      }
      offset += 2 + length;
    }
  }
  throw new Error("whole image mode supports PNG, GIF, and JPEG dimensions");
}

function color(value, fallback = "#ff00ff") {
  const text = String(value || fallback).trim();
  return /^#[0-9a-fA-F]{6}$/.test(text) ? text : fallback;
}

function sessionKeyColor(root, sessionDir) {
  const reportPath = join(sessionDir, "background", "normalize_report.json");
  if (!existsSync(reportPath)) return "";
  try {
    return color(readJson(reportPath).key_color, "");
  } catch {
    return "";
  }
}

function intOption(value, fallback, max = 100000) {
  const number = Number.parseInt(String(value ?? fallback), 10);
  if (!Number.isFinite(number)) return fallback;
  return Math.max(0, Math.min(max, number));
}

function decodeDataUrl(dataUrl) {
  const match = /^data:([^;,]+)?;base64,(.+)$/s.exec(String(dataUrl || ""));
  if (!match) throw new Error("source image must be a base64 data URL");
  return {
    mime: match[1] || "application/octet-stream",
    buffer: Buffer.from(match[2], "base64"),
  };
}

function extensionForUpload(fileName, mime) {
  const ext = extname(String(fileName || "")).toLowerCase();
  if ([".png", ".jpg", ".jpeg", ".webp", ".gif"].includes(ext)) return ext;
  if (mime === "image/jpeg") return ".jpg";
  if (mime === "image/webp") return ".webp";
  if (mime === "image/gif") return ".gif";
  return ".png";
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
    "ai_studio/assets/tools/raster2d/background/normalize_background.py",
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
    "ai_studio/assets/tools/raster2d/regions/detect_regions.py",
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
    "ai_studio/assets/tools/raster2d/slicing/slice_regions.py",
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
