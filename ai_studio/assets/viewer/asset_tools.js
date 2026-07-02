import {
  clampRect,
  fitViewport,
  handlePoints,
  hitRectHandle,
  imageToScreenPoint,
  moveRect,
  resizeRectFromHandle,
  screenToImagePoint,
  zoomViewportAt,
} from "./asset_tools_viewport.mjs";
import {
  createRegionHistory,
  historyCanRedo,
  historyCanUndo,
  historyPush,
  historyRedo,
  historyUndo,
} from "./asset_tools_history.mjs";
import { applyAlphaPreviewMatte, applyGenerationAlphaDiagnostic, applyPolygonPreviewMask } from "./asset_tools_alpha_preview.mjs";
import { fitRegionOverlayLabel, regionOverlayLabel } from "./asset_tools_region_label.mjs";
import { buildNineSliceDraws, clampInsets, clampNineSliceSize } from "./asset_tools_slice9.mjs";
import { normalizeStageView, resolveStageView } from "./asset_tools_stage_view.mjs";

const $ = (id) => document.getElementById(id);
const alphaModes = new Set(["key_matte", "generation"]);
const slice9InputIds = ["slice9Left", "slice9Right", "slice9Top", "slice9Bottom", "slice9PreviewW", "slice9PreviewH"];
const defaultKeyColor = "#ff00ff";

const state = {
  sourceCanvas: document.createElement("canvas"),
  sourcePath: "",
  imagePath: "",
  sourceName: "",
  keyColor: defaultKeyColor,
  normalizeReport: null,
  regions: [],
  selectedId: null,
  workspaceMode: "regions",
  stageView: "review",
  editMode: "select",
  interaction: null,
  draftRect: null,
  polygonDraft: [],
  polygonHover: null,
  reviewTimer: 0,
  reviewReady: false,
  viewport: { scale: 1, offsetX: 0, offsetY: 0 },
  canvasSize: { width: 1, height: 1 },
  autoFit: true,
  spaceDown: false,
  regionHistory: createRegionHistory({ regions: [], selectedId: null }),
  slice9: { sourceMode: "image", left: 0, right: 0, top: 0, bottom: 0, targetWidth: 128, targetHeight: 128 },
};

const sourceContext = state.sourceCanvas.getContext("2d", { willReadFrequently: true });
const canvas = $("regionCanvas");
const context = canvas.getContext("2d");
const canvasFrame = $("canvasFrame");

function setStatus(text) {
  $("assetToolStatus").textContent = text;
}

function safeAssetBaseName(value, fallback = "asset") {
  const cleaned = String(value || "")
    .trim()
    .replace(/\.[^.]+$/, "")
    .replace(/[^a-zA-Z0-9_-]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .slice(0, 80);
  return cleaned || fallback;
}

function hasImage() {
  return state.sourceCanvas.width > 0 && state.sourceCanvas.height > 0 && Boolean(state.imagePath);
}

function imageBounds() {
  return { width: Math.max(1, state.sourceCanvas.width), height: Math.max(1, state.sourceCanvas.height) };
}

function numberValue(id, fallback) {
  const value = Number.parseInt($(id).value, 10);
  return Number.isFinite(value) ? value : fallback;
}

function boundedNumberValue(id, fallback, min, max) {
  const value = numberValue(id, fallback);
  return Math.max(min, Math.min(max, value));
}

function detectionOptions() {
  return {
    backgroundMode: $("backgroundMode").value,
    keyTolerance: numberValue("keyTolerance", 32),
    minArea: numberValue("minArea", 256),
    padding: numberValue("padding", 8),
    mergeDistance: numberValue("mergeDistance", 0),
    rowTolerance: numberValue("rowTolerance", 32),
  };
}

function postJson(path, body) {
  return fetch(path, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  }).then(async (response) => {
    const data = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(data.error || `${path} returned ${response.status}`);
    return data;
  });
}

function triggerBrowserDownload(url, fileName) {
  const link = document.createElement("a");
  link.href = `${url}${url.includes("?") ? "&" : "?"}t=${Date.now()}`;
  link.download = fileName || "";
  document.body.append(link);
  link.click();
  link.remove();
}

function readFileAsDataUrl(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result);
    reader.onerror = () => reject(reader.error || new Error("file read failed"));
    reader.readAsDataURL(file);
  });
}

function loadCanvasFromUrl(url) {
  return new Promise((resolve, reject) => {
    const image = new Image();
    image.onload = () => {
      state.sourceCanvas.width = image.naturalWidth;
      state.sourceCanvas.height = image.naturalHeight;
      sourceContext.clearRect(0, 0, image.naturalWidth, image.naturalHeight);
      sourceContext.drawImage(image, 0, 0);
      resolve();
    };
    image.onerror = () => reject(new Error("image load failed"));
    image.src = `${url}${url.includes("?") ? "&" : "?"}t=${Date.now()}`;
  });
}

function loadCanvasFromFile(file) {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(file);
    const image = new Image();
    image.onload = () => {
      URL.revokeObjectURL(url);
      state.sourceCanvas.width = image.naturalWidth;
      state.sourceCanvas.height = image.naturalHeight;
      sourceContext.clearRect(0, 0, image.naturalWidth, image.naturalHeight);
      sourceContext.drawImage(image, 0, 0);
      resolve();
    };
    image.onerror = () => {
      URL.revokeObjectURL(url);
      reject(new Error("image load failed"));
    };
    image.src = url;
  });
}

function rectFromArray(value) {
  return {
    x: Math.max(0, Math.round(Number(value?.[0]) || 0)),
    y: Math.max(0, Math.round(Number(value?.[1]) || 0)),
    width: Math.max(1, Math.round(Number(value?.[2]) || 1)),
    height: Math.max(1, Math.round(Number(value?.[3]) || 1)),
  };
}

function rectToArray(rect) {
  return [rect.x, rect.y, rect.width, rect.height];
}

function sanitizeRect(rect) {
  return clampRect(rect, imageBounds());
}

function cleanRegionName(value) {
  return String(value || "").trim().slice(0, 80);
}

function regionDisplayName(region) {
  return cleanRegionName(region?.name) || region?.id || "region";
}

function normalizeAlphaPolicy(value) {
  const raw = value && typeof value === "object" ? value : {};
  let mode = String(raw.mode || "key_matte").trim().toLowerCase();
  if (["dual_plate", "generated", "regenerate"].includes(mode)) mode = "generation";
  if (!alphaModes.has(mode)) mode = "key_matte";
  return {
    ...raw,
    mode,
  };
}

function regionAlphaMode(region) {
  return normalizeAlphaPolicy(region?.alpha).mode;
}

function alphaModeLabel(mode) {
  return mode === "generation" ? "generation" : "key matte";
}

function roundImagePoint(point) {
  return {
    x: Math.round(Math.max(0, Math.min(state.sourceCanvas.width, point.x))),
    y: Math.round(Math.max(0, Math.min(state.sourceCanvas.height, point.y))),
  };
}

function pointToArray(point) {
  return [point.x, point.y];
}

function normalizePolygonPoints(value) {
  if (!Array.isArray(value)) return [];
  return value
    .map((point) => {
      if (!Array.isArray(point) || point.length !== 2) return null;
      return roundImagePoint({ x: Number(point[0]) || 0, y: Number(point[1]) || 0 });
    })
    .filter(Boolean)
    .map(pointToArray);
}

function rectFromPolygon(points) {
  const normalized = normalizePolygonPoints(points);
  const xs = normalized.map((point) => point[0]);
  const ys = normalized.map((point) => point[1]);
  const x = Math.floor(Math.min(...xs));
  const y = Math.floor(Math.min(...ys));
  const right = Math.ceil(Math.max(...xs));
  const bottom = Math.ceil(Math.max(...ys));
  return sanitizeRect({
    x,
    y,
    width: Math.max(1, right - x),
    height: Math.max(1, bottom - y),
  });
}

function transformPolygon(points, oldRect, newRect) {
  const normalized = normalizePolygonPoints(points);
  if (normalized.length < 3) return [];
  const scaleX = newRect.width / Math.max(1, oldRect.width);
  const scaleY = newRect.height / Math.max(1, oldRect.height);
  return normalizePolygonPoints(
    normalized.map((point) => [
      newRect.x + (point[0] - oldRect.x) * scaleX,
      newRect.y + (point[1] - oldRect.y) * scaleY,
    ]),
  );
}

function renumberRegions(regions) {
  return regions.map((region, index) => {
    const rect = sanitizeRect(rectFromArray(region.rect));
    const content = sanitizeRect(rectFromArray(region.content_bbox || region.rect));
    const polygon = normalizePolygonPoints(region.polygon);
    const normalized = {
      ...region,
      id: `region_${String(index + 1).padStart(3, "0")}`,
      name: cleanRegionName(region.name),
      alpha: normalizeAlphaPolicy(region.alpha),
      rect: rectToArray(rect),
      content_bbox: rectToArray(content),
      area_px: Number(region.area_px) || rect.width * rect.height,
    };
    if (!normalized.name) delete normalized.name;
    if (polygon.length >= 3) {
      normalized.polygon = polygon;
    } else {
      delete normalized.polygon;
    }
    return normalized;
  });
}

function selectedRegion() {
  return state.regions.find((region) => region.id === state.selectedId) || null;
}

function regionSnapshot() {
  return {
    regions: state.regions,
    selectedId: state.selectedId,
  };
}

function resetRegionHistory() {
  state.regionHistory = createRegionHistory(regionSnapshot());
}

function commitRegionHistory() {
  state.regionHistory = historyPush(state.regionHistory, regionSnapshot());
}

function restoreRegionSnapshot(snapshot, label) {
  state.regions = snapshot.regions;
  state.selectedId = snapshot.selectedId;
  state.polygonDraft = [];
  state.polygonHover = null;
  setStatus(label);
  renderAll(true);
}

function undoRegions() {
  if (!historyCanUndo(state.regionHistory)) return;
  const result = historyUndo(state.regionHistory);
  state.regionHistory = result.history;
  restoreRegionSnapshot(result.snapshot, "Undo regions");
}

function redoRegions() {
  if (!historyCanRedo(state.regionHistory)) return;
  const result = historyRedo(state.regionHistory);
  state.regionHistory = result.history;
  restoreRegionSnapshot(result.snapshot, "Redo regions");
}

function updateRegionRect(regionId, rect) {
  const clamped = sanitizeRect(rect);
  state.regions = state.regions.map((region) => {
    if (region.id !== regionId) return region;
    const originalRect = rectFromArray(region.rect);
    const polygon = transformPolygon(region.polygon, originalRect, clamped);
    const updated = {
      ...region,
      rect: rectToArray(clamped),
      content_bbox: rectToArray(clamped),
      area_px: clamped.width * clamped.height,
      source: region.source || "manual",
    };
    if (polygon.length >= 3) {
      updated.polygon = polygon;
    } else {
      delete updated.polygon;
    }
    return updated;
  });
}

function syncCanvasSize() {
  const bounds = canvasFrame.getBoundingClientRect();
  const width = Math.max(1, Math.floor(bounds.width));
  const height = Math.max(1, Math.floor(bounds.height));
  const ratio = window.devicePixelRatio || 1;
  if (canvas.width !== Math.floor(width * ratio) || canvas.height !== Math.floor(height * ratio)) {
    canvas.width = Math.floor(width * ratio);
    canvas.height = Math.floor(height * ratio);
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;
  }
  state.canvasSize = { width, height };
  context.setTransform(ratio, 0, 0, ratio, 0, 0);
  context.imageSmoothingEnabled = false;
}

function fitToImage() {
  syncCanvasSize();
  if (!hasImage()) return;
  state.viewport = fitViewport({
    imageWidth: state.sourceCanvas.width,
    imageHeight: state.sourceCanvas.height,
    frameWidth: state.canvasSize.width,
    frameHeight: state.canvasSize.height,
    padding: 28,
  });
  state.autoFit = true;
  renderAll(false);
}

function setActualSize() {
  syncCanvasSize();
  if (!hasImage()) return;
  state.viewport = {
    scale: 1,
    offsetX: (state.canvasSize.width - state.sourceCanvas.width) / 2,
    offsetY: (state.canvasSize.height - state.sourceCanvas.height) / 2,
  };
  state.autoFit = false;
  renderAll(false);
}

function zoomBy(factor, screenPoint = null) {
  if (!hasImage()) return;
  syncCanvasSize();
  const point = screenPoint || { x: state.canvasSize.width / 2, y: state.canvasSize.height / 2 };
  state.viewport = zoomViewportAt(state.viewport, factor, point);
  state.autoFit = false;
  renderAll(false);
}

function framePoint(event) {
  const bounds = canvas.getBoundingClientRect();
  return {
    x: event.clientX - bounds.left,
    y: event.clientY - bounds.top,
  };
}

function imagePointFromEvent(event) {
  const point = screenToImagePoint(framePoint(event), state.viewport);
  return {
    x: Math.max(0, Math.min(state.sourceCanvas.width, point.x)),
    y: Math.max(0, Math.min(state.sourceCanvas.height, point.y)),
  };
}

function rectFromPoints(a, b) {
  return sanitizeRect({
    x: Math.min(a.x, b.x),
    y: Math.min(a.y, b.y),
    width: Math.abs(a.x - b.x),
    height: Math.abs(a.y - b.y),
  });
}

function hitRegion(point) {
  const tolerance = Math.max(3, 8 / state.viewport.scale);
  for (let index = state.regions.length - 1; index >= 0; index -= 1) {
    const region = state.regions[index];
    const rect = rectFromArray(region.rect);
    const handle = hitRectHandle(point, rect, tolerance);
    if (handle) return { region, handle };
  }
  return null;
}

function hideRegionContextMenu() {
  $("regionContextMenu").classList.add("hidden");
}

function showRegionContextMenu(region, event) {
  if (!region) return;
  event.preventDefault();
  state.selectedId = region.id;
  if (state.workspaceMode === "slice9" && state.slice9.sourceMode === "region") {
    normalizeSlice9State();
  }
  renderAll(false);

  const menu = $("regionContextMenu");
  menu.style.left = "0px";
  menu.style.top = "0px";
  menu.classList.remove("hidden");
  const bounds = menu.getBoundingClientRect();
  const left = Math.max(8, Math.min(event.clientX, window.innerWidth - bounds.width - 8));
  const top = Math.max(8, Math.min(event.clientY, window.innerHeight - bounds.height - 8));
  menu.style.left = `${left}px`;
  menu.style.top = `${top}px`;
}

function screenRect(rect) {
  const start = imageToScreenPoint({ x: rect.x, y: rect.y }, state.viewport);
  const end = imageToScreenPoint({ x: rect.x + rect.width, y: rect.y + rect.height }, state.viewport);
  return { x: start.x, y: start.y, width: end.x - start.x, height: end.y - start.y };
}

function tracePolygon(points) {
  const normalized = normalizePolygonPoints(points);
  if (normalized.length < 3) return false;
  const first = imageToScreenPoint({ x: normalized[0][0], y: normalized[0][1] }, state.viewport);
  context.beginPath();
  context.moveTo(first.x, first.y);
  for (const point of normalized.slice(1)) {
    const screenPoint = imageToScreenPoint({ x: point[0], y: point[1] }, state.viewport);
    context.lineTo(screenPoint.x, screenPoint.y);
  }
  context.closePath();
  return true;
}

function drawRegionOverlay(region, index) {
  const rect = rectFromArray(region.rect);
  const display = screenRect(rect);
  const selected = region.id === state.selectedId;
  const polygon = normalizePolygonPoints(region.polygon);
  context.save();
  context.lineWidth = selected ? 2.5 : 1.75;
  context.strokeStyle = selected ? "#77a7ff" : "#3fc7ba";
  context.fillStyle = selected ? "rgba(119, 167, 255, 0.17)" : "rgba(63, 199, 186, 0.11)";
  if (polygon.length >= 3 && tracePolygon(polygon)) {
    context.fill();
    context.stroke();
    context.setLineDash([5, 5]);
    context.lineWidth = 1;
    context.strokeStyle = selected ? "rgba(119, 167, 255, 0.65)" : "rgba(63, 199, 186, 0.5)";
    context.strokeRect(display.x + 0.5, display.y + 0.5, Math.max(1, display.width - 1), Math.max(1, display.height - 1));
    context.setLineDash([]);
  } else {
    context.fillRect(display.x, display.y, display.width, display.height);
    context.strokeRect(display.x + 0.5, display.y + 0.5, Math.max(1, display.width - 1), Math.max(1, display.height - 1));
  }

  context.font = "700 12px Segoe UI, sans-serif";
  const namedLabel = Boolean(cleanRegionName(region.name));
  const availableBadgeWidth = Math.max(24, state.canvasSize.width - display.x - 8);
  const preferredBadgeWidth = namedLabel ? 180 : Math.max(24, display.width);
  const maxBadgeWidth = Math.min(preferredBadgeWidth, availableBadgeWidth);
  const label = fitRegionOverlayLabel(regionOverlayLabel(region, index), maxBadgeWidth - 10, context.measureText.bind(context));
  const labelWidth = Math.max(22, Math.min(maxBadgeWidth, context.measureText(label).width + 10));
  context.fillStyle = selected ? "#77a7ff" : "#3fc7ba";
  context.fillRect(display.x, display.y, labelWidth, 20);
  context.fillStyle = "#071014";
  context.fillText(label, display.x + 6, display.y + 14);

  if (selected) {
    context.fillStyle = "#f8fbff";
    context.strokeStyle = "#1c2430";
    context.lineWidth = 1;
    for (const [, handlePoint] of handlePoints(rect)) {
      const point = imageToScreenPoint(handlePoint, state.viewport);
      context.fillRect(point.x - 4, point.y - 4, 8, 8);
      context.strokeRect(point.x - 4.5, point.y - 4.5, 9, 9);
    }
    if (polygon.length >= 3) {
      context.fillStyle = "#d7a14a";
      context.strokeStyle = "#1c2430";
      for (const point of polygon) {
        const screenPoint = imageToScreenPoint({ x: point[0], y: point[1] }, state.viewport);
        context.beginPath();
        context.arc(screenPoint.x, screenPoint.y, 4, 0, Math.PI * 2);
        context.fill();
        context.stroke();
      }
    }
  }
  context.restore();
}

function drawDraftRect() {
  if (!state.draftRect) return;
  const display = screenRect(state.draftRect);
  context.save();
  context.setLineDash([7, 5]);
  context.lineWidth = 2;
  context.strokeStyle = "#d7a14a";
  context.fillStyle = "rgba(215, 161, 74, 0.14)";
  context.fillRect(display.x, display.y, display.width, display.height);
  context.strokeRect(display.x + 0.5, display.y + 0.5, Math.max(1, display.width - 1), Math.max(1, display.height - 1));
  context.restore();
}

function drawDraftPolygon() {
  if (state.polygonDraft.length === 0) return;
  const points = state.polygonHover ? [...state.polygonDraft, pointToArray(state.polygonHover)] : state.polygonDraft;
  context.save();
  context.setLineDash([7, 5]);
  context.lineWidth = 2;
  context.strokeStyle = "#d7a14a";
  context.fillStyle = "rgba(215, 161, 74, 0.14)";
  if (points.length >= 2) {
    const first = imageToScreenPoint({ x: points[0][0], y: points[0][1] }, state.viewport);
    context.beginPath();
    context.moveTo(first.x, first.y);
    for (const point of points.slice(1)) {
      const screenPoint = imageToScreenPoint({ x: point[0], y: point[1] }, state.viewport);
      context.lineTo(screenPoint.x, screenPoint.y);
    }
    if (state.polygonDraft.length >= 3) context.closePath();
    context.stroke();
    if (state.polygonDraft.length >= 3) context.fill();
  }
  context.setLineDash([]);
  context.fillStyle = "#f8fbff";
  context.strokeStyle = "#1c2430";
  for (const point of state.polygonDraft) {
    const screenPoint = imageToScreenPoint({ x: point[0], y: point[1] }, state.viewport);
    context.beginPath();
    context.arc(screenPoint.x, screenPoint.y, 4, 0, Math.PI * 2);
    context.fill();
    context.stroke();
  }
  context.restore();
}

function slice9SourceRect(sourceMode = state.slice9.sourceMode) {
  const bounds = imageBounds();
  if (sourceMode === "region") {
    const region = selectedRegion();
    if (region) return sanitizeRect(rectFromArray(region.rect));
  }
  return { x: 0, y: 0, width: bounds.width, height: bounds.height };
}

function normalizeSlice9SourceMode() {
  if (state.slice9.sourceMode === "region" && !selectedRegion()) {
    state.slice9.sourceMode = "image";
  }
}

function currentSlice9Insets() {
  const source = slice9SourceRect();
  return clampInsets(state.slice9, source);
}

function defaultSlice9State(sourceMode = state.slice9.sourceMode || "image") {
  const source = slice9SourceRect(sourceMode);
  const edge = Math.max(1, Math.round(Math.min(source.width, source.height) * 0.18));
  const insets = clampInsets({ left: edge, right: edge, top: edge, bottom: edge }, source);
  const target = clampNineSliceSize({
    targetWidth: Math.min(1024, Math.max(source.width * 2, insets.left + insets.right + 1)),
    targetHeight: Math.min(1024, Math.max(source.height * 2, insets.top + insets.bottom + 1)),
    insets,
  });
  return { sourceMode, ...insets, targetWidth: target.width, targetHeight: target.height };
}

function resetSlice9State(sourceMode = "image") {
  state.slice9 = defaultSlice9State(sourceMode);
  syncSlice9Inputs();
}

function normalizeSlice9State() {
  normalizeSlice9SourceMode();
  const insets = currentSlice9Insets();
  const target = clampNineSliceSize({
    targetWidth: state.slice9.targetWidth,
    targetHeight: state.slice9.targetHeight,
    insets,
  });
  state.slice9 = { ...state.slice9, ...insets, targetWidth: target.width, targetHeight: target.height };
  syncSlice9Inputs();
}

function syncSlice9Inputs() {
  if (!$("slice9Left")) return;
  $("slice9Left").value = String(state.slice9.left);
  $("slice9Right").value = String(state.slice9.right);
  $("slice9Top").value = String(state.slice9.top);
  $("slice9Bottom").value = String(state.slice9.bottom);
  $("slice9PreviewW").value = String(state.slice9.targetWidth);
  $("slice9PreviewH").value = String(state.slice9.targetHeight);
}

function syncSlice9SourceControls() {
  if (!$("slice9WholeSource")) return;
  normalizeSlice9SourceMode();
  const region = selectedRegion();
  $("slice9WholeSource").classList.toggle("is-active", state.slice9.sourceMode === "image");
  $("slice9RegionSource").classList.toggle("is-active", state.slice9.sourceMode === "region");
  $("slice9RegionSource").disabled = !region;
  $("slice9SourceMeta").textContent =
    state.slice9.sourceMode === "region" && region
      ? `${regionDisplayName(region)} - ${slice9SourceRect("region").width} x ${slice9SourceRect("region").height}`
      : `${state.sourceCanvas.width || 0} x ${state.sourceCanvas.height || 0}`;
}

function slice9InputFocused() {
  return slice9InputIds.includes(document.activeElement?.id);
}

function drawSlice9Overlay() {
  if (state.workspaceMode !== "slice9" || !hasImage()) return;
  normalizeSlice9SourceMode();
  const source = slice9SourceRect();
  const insets = currentSlice9Insets();
  const xs = [source.x + insets.left, source.x + source.width - insets.right].filter(
    (x) => x > source.x && x < source.x + source.width,
  );
  const ys = [source.y + insets.top, source.y + source.height - insets.bottom].filter(
    (y) => y > source.y && y < source.y + source.height,
  );
  const topLeft = imageToScreenPoint({ x: source.x, y: source.y }, state.viewport);
  const bottomRight = imageToScreenPoint({ x: source.x + source.width, y: source.y + source.height }, state.viewport);

  context.save();
  context.lineWidth = 2;
  context.strokeStyle = "#f6c65b";
  context.setLineDash([8, 6]);
  for (const x of xs) {
    const screen = imageToScreenPoint({ x, y: 0 }, state.viewport);
    context.beginPath();
    context.moveTo(screen.x, topLeft.y);
    context.lineTo(screen.x, bottomRight.y);
    context.stroke();
  }
  for (const y of ys) {
    const screen = imageToScreenPoint({ x: 0, y }, state.viewport);
    context.beginPath();
    context.moveTo(topLeft.x, screen.y);
    context.lineTo(bottomRight.x, screen.y);
    context.stroke();
  }
  context.setLineDash([]);
  context.strokeStyle = "#77a7ff";
  context.strokeRect(topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
  context.restore();
}

function renderCanvas() {
  syncCanvasSize();
  context.clearRect(0, 0, state.canvasSize.width, state.canvasSize.height);
  if (!state.sourceCanvas.width) {
    canvas.classList.add("hidden");
    $("emptyState").classList.remove("hidden");
    return;
  }
  $("emptyState").classList.add("hidden");
  canvas.classList.remove("hidden");
  context.drawImage(
    state.sourceCanvas,
    state.viewport.offsetX,
    state.viewport.offsetY,
    state.sourceCanvas.width * state.viewport.scale,
    state.sourceCanvas.height * state.viewport.scale,
  );
  if (state.workspaceMode === "slice9") {
    if (state.slice9.sourceMode === "region") {
      state.regions.forEach(drawRegionOverlay);
    }
    drawSlice9Overlay();
  } else {
    state.regions.forEach(drawRegionOverlay);
    drawDraftRect();
    drawDraftPolygon();
  }
  updateCanvasCursor();
}

function stageState() {
  return resolveStageView({
    workspaceMode: state.workspaceMode,
    stageView: state.stageView,
  });
}

function updateStageVisibility() {
  const stage = stageState();
  $("canvasFrame").classList.toggle("hidden", !stage.showCanvasStage);
  $("stageReviewFrame").classList.toggle("hidden", !stage.showReviewStage);
  return stage;
}

function drawCheckerboard(targetContext, width, height, cellSize = 12) {
  targetContext.fillStyle = "#151a1f";
  targetContext.fillRect(0, 0, width, height);
  targetContext.fillStyle = "#20262b";
  for (let y = 0; y < height; y += cellSize) {
    for (let x = 0; x < width; x += cellSize) {
      if (((x / cellSize) + (y / cellSize)) % 2 === 0) {
        targetContext.fillRect(x, y, cellSize, cellSize);
      }
    }
  }
}

function renderSlice9Preview() {
  const preview = $("slice9PreviewCanvas");
  if (!preview) return;
  const previewContext = preview.getContext("2d");
  syncSlice9SourceControls();
  const enabled = hasImage();
  for (const id of slice9InputIds) {
    $(id).disabled = !enabled;
  }
  if (!enabled) {
    preview.width = 1;
    preview.height = 1;
    previewContext.clearRect(0, 0, 1, 1);
    $("slice9Meta").textContent = "empty";
    $("slice9Empty").classList.remove("hidden");
    preview.classList.add("hidden");
    return;
  }

  const source = slice9SourceRect();
  const insets = currentSlice9Insets();
  const target = clampNineSliceSize({
    targetWidth: state.slice9.targetWidth,
    targetHeight: state.slice9.targetHeight,
    insets,
  });
  const draws = buildNineSliceDraws({
    sourceX: source.x,
    sourceY: source.y,
    sourceWidth: source.width,
    sourceHeight: source.height,
    targetWidth: target.width,
    targetHeight: target.height,
    insets,
  });
  preview.width = target.width;
  preview.height = target.height;
  drawCheckerboard(previewContext, preview.width, preview.height);
  for (const draw of draws) {
    previewContext.drawImage(
      state.sourceCanvas,
      draw.source.x,
      draw.source.y,
      draw.source.width,
      draw.source.height,
      draw.destination.x,
      draw.destination.y,
      draw.destination.width,
      draw.destination.height,
    );
  }
  previewContext.save();
  previewContext.strokeStyle = "#f6c65b";
  previewContext.lineWidth = 1;
  previewContext.setLineDash([6, 4]);
  previewContext.beginPath();
  previewContext.moveTo(insets.left, 0);
  previewContext.lineTo(insets.left, preview.height);
  previewContext.moveTo(preview.width - insets.right, 0);
  previewContext.lineTo(preview.width - insets.right, preview.height);
  previewContext.moveTo(0, insets.top);
  previewContext.lineTo(preview.width, insets.top);
  previewContext.moveTo(0, preview.height - insets.bottom);
  previewContext.lineTo(preview.width, preview.height - insets.bottom);
  previewContext.stroke();
  previewContext.restore();
  $("slice9Meta").textContent = `${preview.width} x ${preview.height}`;
  $("slice9Empty").classList.add("hidden");
  preview.classList.remove("hidden");
}

function renderRegionList() {
  const list = $("regionList");
  $("regionCount").textContent = String(state.regions.length);
  if (state.regions.length === 0) {
    list.innerHTML = '<div class="empty-list">No regions.</div>';
    return;
  }
  list.innerHTML = "";
  for (const region of state.regions) {
    const rect = rectFromArray(region.rect);
    const button = document.createElement("button");
    button.type = "button";
    button.className = "region-row";
    button.classList.toggle("is-selected", region.id === state.selectedId);
    const regionText = document.createElement("span");
    const title = document.createElement("strong");
    title.textContent = regionDisplayName(region);
    const meta = document.createElement("span");
    meta.textContent = `${region.id} - ${rect.x}, ${rect.y}, ${rect.width} x ${rect.height} - ${alphaModeLabel(regionAlphaMode(region))}`;
    regionText.append(title, meta);
    const source = document.createElement("span");
    source.className = "region-source";
    source.textContent = region.source || "auto";
    button.append(regionText, source);
    button.addEventListener("click", () => {
      state.selectedId = region.id;
      focusRegion(region);
      renderAll(false);
    });
    button.addEventListener("contextmenu", (event) => showRegionContextMenu(region, event));
    list.append(button);
  }
}

function renderInspector() {
  const region = selectedRegion();
  const fields = $("inspectorFields");
  const empty = $("inspectorEmpty");
  if (!region) {
    fields.classList.add("hidden");
    empty.classList.remove("hidden");
    $("selectedName").value = "";
    $("selectedAlphaMode").value = "key_matte";
    $("selectedMeta").textContent = "";
    return;
  }
  const rect = rectFromArray(region.rect);
  fields.classList.remove("hidden");
  empty.classList.add("hidden");
  $("selectedName").value = region.name || "";
  $("selectedAlphaMode").value = regionAlphaMode(region);
  $("selectedX").value = String(rect.x);
  $("selectedY").value = String(rect.y);
  $("selectedW").value = String(rect.width);
  $("selectedH").value = String(rect.height);
  $("selectedMeta").textContent = `${region.source || "auto"} - ${region.id} - ${alphaModeLabel(regionAlphaMode(region))}`;
}

function renderSelectedRegionPreview() {
  const preview = $("selectedRegionPreviewCanvas");
  if (!preview) return;
  const previewContext = preview.getContext("2d", { willReadFrequently: true });
  const region = selectedRegion();
  const mode = $("selectedPreviewMode").value;
  if (!hasImage() || !region) {
    preview.width = 1;
    preview.height = 1;
    previewContext.clearRect(0, 0, 1, 1);
    $("selectedPreviewMeta").textContent = "empty";
    return;
  }

  const rect = sanitizeRect(rectFromArray(region.rect));
  preview.width = rect.width;
  preview.height = rect.height;
  drawCheckerboard(previewContext, preview.width, preview.height, 8);
  if (mode === "alpha" && regionAlphaMode(region) === "key_matte") {
    const imageData = sourceContext.getImageData(rect.x, rect.y, rect.width, rect.height);
    const stats = applyAlphaPreviewMatte(imageData, {
      keyColor: state.keyColor,
      rect,
      polygon: region.polygon,
    });
    previewContext.putImageData(imageData, 0, 0);
    const fixes = stats.despilledPixels ? `, despill ${stats.despilledPixels}` : "";
    $("selectedPreviewMeta").textContent = `${rect.width} x ${rect.height} - key matte ${state.keyColor}${fixes}`;
    return;
  }

  if (mode === "alpha" && regionAlphaMode(region) === "generation") {
    const imageData = sourceContext.getImageData(rect.x, rect.y, rect.width, rect.height);
    const stats = applyGenerationAlphaDiagnostic(imageData, {
      keyColor: state.keyColor,
      rect,
      polygon: region.polygon,
    });
    previewContext.putImageData(imageData, 0, 0);
    $("selectedPreviewMeta").textContent = `${rect.width} x ${rect.height} - generation check, ${stats.diagnosticPixels} edge px`;
    return;
  }

  const imageData = sourceContext.getImageData(rect.x, rect.y, rect.width, rect.height);
  const stats = applyPolygonPreviewMask(imageData, {
    rect,
    polygon: region.polygon,
  });
  previewContext.putImageData(imageData, 0, 0);
  const mask = stats.polygonMaskedPixels ? ", polygon mask" : "";
  $("selectedPreviewMeta").textContent = `${rect.width} x ${rect.height} - source${mask}`;
}

function setStepState(step, stateName) {
  const node = document.querySelector(`.pipeline-step[data-step="${step}"]`);
  if (!node) return;
  node.classList.toggle("is-current", stateName === "current");
  node.classList.toggle("is-done", stateName === "done");
}

function updatePipeline() {
  const imageReady = hasImage();
  const regionCount = state.regions.length;
  const generationAlphaCount = state.regions.filter((region) => regionAlphaMode(region) === "generation").length;
  $("sourceStepStatus").textContent = imageReady ? "loaded" : "empty";
  if (!imageReady) {
    $("backgroundStepStatus").textContent = "empty";
  } else if (state.normalizeReport?.mode === "passthrough_no_background") {
    $("backgroundStepStatus").textContent = "skipped";
  } else if (state.normalizeReport) {
    $("backgroundStepStatus").textContent = `normalized ${state.normalizeReport.key_color || ""}`.trim();
  } else {
    $("backgroundStepStatus").textContent =
      $("backgroundMode").value === "whole_image" ? "will skip" : "runs on detect";
  }
  $("regionsStepStatus").textContent = regionCount ? `${regionCount} regions` : "not detected";
  $("alphaStepStatus").textContent = regionCount ? (generationAlphaCount ? `${generationAlphaCount} generation` : "key matte") : "pending";
  $("reviewStepStatus").textContent = state.reviewReady ? "ready" : regionCount ? "building" : "pending";
  $("exportStepStatus").textContent = regionCount ? "ready" : "locked";

  for (const step of ["source", "background", "regions", "alpha", "review", "export"]) {
    setStepState(step, "");
  }
  setStepState("source", imageReady ? "done" : "current");
  if (imageReady && !regionCount) setStepState("regions", "current");
  if (state.normalizeReport) setStepState("background", "done");
  if (regionCount) {
    setStepState("regions", "done");
    setStepState("alpha", generationAlphaCount ? "current" : "done");
    setStepState("review", state.reviewReady ? "done" : "current");
  }
}

function showReview(url, count) {
  const image = $("reviewImage");
  const empty = $("reviewEmpty");
  const stageImage = $("stageReviewImage");
  const stageEmpty = $("stageReviewEmpty");
  state.reviewReady = Boolean(url);
  if (!url) {
    for (const node of [image, stageImage]) {
      node.hidden = true;
      node.removeAttribute("src");
    }
    empty.hidden = false;
    stageEmpty.hidden = false;
    stageEmpty.textContent = hasImage() && state.regions.length ? "Building review sheet..." : "Review sheet appears after regions are ready.";
    $("reviewMeta").textContent = "empty";
    updatePipeline();
    return;
  }
  const cacheBustUrl = `${url}${url.includes("?") ? "&" : "?"}t=${Date.now()}`;
  for (const node of [image, stageImage]) {
    node.hidden = false;
    node.src = cacheBustUrl;
  }
  empty.hidden = true;
  stageEmpty.hidden = true;
  $("reviewMeta").textContent = `${count} regions`;
  updatePipeline();
}

function scheduleReview() {
  clearTimeout(state.reviewTimer);
  state.reviewReady = false;
  if (!hasImage() || state.regions.length === 0) {
    showReview(null, 0);
    return;
  }
  $("stageReviewEmpty").textContent = "Building review sheet...";
  updatePipeline();
  state.reviewTimer = setTimeout(async () => {
    try {
      const result = await postJson("/api/asset-tools/raster2d/review", {
        imagePath: state.imagePath,
        regions: state.regions,
        prefix: $("prefix").value,
      });
      showReview(result.reviewSheetUrl, state.regions.length);
    } catch (error) {
      setStatus(error.message);
    }
  }, 350);
}

function updateButtons() {
  const imageReady = hasImage();
  const hasRegions = state.regions.length > 0;
  const regionsMode = state.workspaceMode === "regions";
  const stage = stageState();
  const editStage = stage.showCanvasStage;
  $("detectRegions").disabled = !imageReady;
  $("downloadZip").disabled = !imageReady || !hasRegions;
  $("downloadSelectedRegion").disabled = !imageReady || !selectedRegion();
  $("deleteRegion").disabled = !regionsMode || !selectedRegion();
  $("clearRegions").disabled = !regionsMode || !hasRegions;
  $("undoRegions").disabled = !regionsMode || !historyCanUndo(state.regionHistory);
  $("redoRegions").disabled = !regionsMode || !historyCanRedo(state.regionHistory);
  $("regionsWorkspaceMode").classList.toggle("is-active", regionsMode);
  $("slice9WorkspaceMode").classList.toggle("is-active", state.workspaceMode === "slice9");
  $("reviewStageMode").disabled = !regionsMode;
  $("editStageMode").disabled = !regionsMode;
  $("reviewStageMode").classList.toggle("is-active", regionsMode && state.stageView === "review");
  $("editStageMode").classList.toggle("is-active", regionsMode && state.stageView === "edit");
  $("selectMode").disabled = !regionsMode || !editStage;
  $("rectMode").disabled = !regionsMode || !editStage;
  $("polygonMode").disabled = !regionsMode || !editStage;
  $("panMode").disabled = !editStage;
  $("fitView").disabled = !editStage || !imageReady;
  $("zoomOut").disabled = !editStage || !imageReady;
  $("zoomIn").disabled = !editStage || !imageReady;
  $("actualSize").disabled = !editStage || !imageReady;
  $("selectMode").classList.toggle("is-active", state.editMode === "select");
  $("rectMode").classList.toggle("is-active", state.editMode === "rect");
  $("polygonMode").classList.toggle("is-active", state.editMode === "polygon");
  $("panMode").classList.toggle("is-active", state.editMode === "pan");
  $("toleranceValue").textContent = $("keyTolerance").value;
  $("zoomValue").textContent = stage.showReviewStage ? "Review" : `${Math.round(state.viewport.scale * 100)}%`;
  updatePipeline();
}

function renderAll(updateReview = true) {
  const stage = updateStageVisibility();
  if (stage.showCanvasStage) renderCanvas();
  renderRegionList();
  renderInspector();
  renderSelectedRegionPreview();
  renderSlice9Preview();
  $("canvasMeta").textContent = state.sourceCanvas.width ? `${state.sourceCanvas.width} x ${state.sourceCanvas.height}` : "0 x 0";
  $("regionMeta").textContent =
    state.workspaceMode === "slice9"
      ? `slice 9 ${state.slice9.targetWidth} x ${state.slice9.targetHeight}`
      : `${state.regions.length} region${state.regions.length === 1 ? "" : "s"}`;
  updateButtons();
  if (updateReview) scheduleReview();
}

function focusRegion(region) {
  if (!region || !hasImage()) return;
  const rect = sanitizeRect(rectFromArray(region.rect));
  const availableWidth = Math.max(1, state.canvasSize.width - 96);
  const availableHeight = Math.max(1, state.canvasSize.height - 96);
  const scale = Math.max(0.05, Math.min(12, Math.min(availableWidth / rect.width, availableHeight / rect.height)));
  state.viewport = {
    scale,
    offsetX: state.canvasSize.width / 2 - (rect.x + rect.width / 2) * scale,
    offsetY: state.canvasSize.height / 2 - (rect.y + rect.height / 2) * scale,
  };
  state.autoFit = false;
}

async function loadImageFile(file) {
  if (!file) return;
  setStatus("Uploading source...");
  const [dataUrl] = await Promise.all([readFileAsDataUrl(file), loadCanvasFromFile(file)]);
  const uploaded = await postJson("/api/asset-tools/raster2d/upload", {
    fileName: file.name,
    dataUrl,
  });
  state.sourcePath = uploaded.sourcePath;
  state.imagePath = uploaded.sourcePath;
  state.sourceName = file.name;
  state.keyColor = defaultKeyColor;
  state.normalizeReport = null;
  state.regions = [];
  state.selectedId = null;
  state.draftRect = null;
  state.polygonDraft = [];
  state.polygonHover = null;
  state.reviewReady = false;
  resetRegionHistory();
  $("prefix").value = safeAssetBaseName(file.name);
  $("imageMeta").textContent = `${file.name} - ${state.sourceCanvas.width} x ${state.sourceCanvas.height}`;
  showReview(null, 0);
  resetSlice9State();
  fitToImage();
  setStatus(`Uploaded ${file.name}`);
  renderAll(false);
}

async function runDetectRegions() {
  if (!hasImage()) return;
  setStatus($("backgroundMode").value === "whole_image" ? "Creating whole-image region..." : "Running Python pipeline: background -> regions...");
  const result = await postJson("/api/asset-tools/raster2d/detect", {
    sourcePath: state.sourcePath,
    options: detectionOptions(),
  });
  state.imagePath = result.normalizedPath;
  state.keyColor = result.normalizeReport?.key_color || defaultKeyColor;
  state.normalizeReport = result.normalizeReport || null;
  state.regions = renumberRegions(result.regions.regions || []);
  state.selectedId = state.regions[0]?.id || null;
  state.draftRect = null;
  state.polygonDraft = [];
  state.polygonHover = null;
  state.reviewReady = false;
  commitRegionHistory();
  await loadCanvasFromUrl(result.normalizedUrl);
  normalizeSlice9State();
  setStatus(`Detected ${state.regions.length} region${state.regions.length === 1 ? "" : "s"}`);
  renderAll(true);
}

function deleteSelectedRegion() {
  if (!selectedRegion()) return;
  state.regions = renumberRegions(state.regions.filter((region) => region.id !== state.selectedId));
  state.selectedId = state.regions[0]?.id || null;
  commitRegionHistory();
  setStatus("Deleted selected region");
  renderAll(true);
}

function clearRegions() {
  state.regions = [];
  state.selectedId = null;
  commitRegionHistory();
  setStatus("Cleared regions");
  renderAll(true);
}

async function downloadZip() {
  if (!hasImage() || state.regions.length === 0) return;
  setStatus("Running Python export...");
  const result = await postJson("/api/asset-tools/raster2d/export", {
    imagePath: state.imagePath,
    regions: state.regions,
    prefix: $("prefix").value,
    includeReviewSheet: $("includeReviewSheet").checked,
  });
  if (result.reviewSheetUrl) showReview(result.reviewSheetUrl, state.regions.length);
  if (result.zipUrl) {
    window.location.href = result.zipUrl;
    setStatus(`Exported ${state.regions.length} slice${state.regions.length === 1 ? "" : "s"}`);
  }
}

async function downloadSelectedRegion() {
  const region = selectedRegion();
  if (!hasImage() || !region) return;
  hideRegionContextMenu();
  setStatus(`Exporting ${regionDisplayName(region)}...`);
  const result = await postJson("/api/asset-tools/raster2d/export-one", {
    imagePath: state.imagePath,
    prefix: $("prefix").value,
    region,
  });
  if (!result.sliceUrl) throw new Error("single-region export did not produce a PNG");
  triggerBrowserDownload(result.sliceUrl, result.fileName || `${regionDisplayName(region)}.png`);
  setStatus(`Saved ${result.fileName || regionDisplayName(region)}`);
}

function setEditMode(mode) {
  state.editMode = mode;
  state.interaction = null;
  state.draftRect = null;
  if (mode !== "polygon") {
    state.polygonDraft = [];
    state.polygonHover = null;
  }
  renderAll(false);
}

function addPolygonPoint(point) {
  state.polygonDraft = [...state.polygonDraft, pointToArray(roundImagePoint(point))];
  state.polygonHover = roundImagePoint(point);
  setStatus(`${state.polygonDraft.length} polygon point${state.polygonDraft.length === 1 ? "" : "s"}`);
  renderAll(false);
}

function finishPolygonDraft() {
  if (state.polygonDraft.length < 3) return;
  const polygon = normalizePolygonPoints(state.polygonDraft);
  if (polygon.length < 3) return;
  const rect = rectFromPolygon(polygon);
  state.regions = renumberRegions([
    ...state.regions,
    {
      id: "manual_poly",
      rect: rectToArray(rect),
      content_bbox: rectToArray(rect),
      polygon,
      area_px: rect.width * rect.height,
      source: "manual_poly",
    },
  ]);
  state.selectedId = state.regions.at(-1)?.id || null;
  state.polygonDraft = [];
  state.polygonHover = null;
  commitRegionHistory();
  setStatus("Added polygon region");
  renderAll(true);
}

function cancelPolygonDraft() {
  if (state.polygonDraft.length === 0) return false;
  state.polygonDraft = [];
  state.polygonHover = null;
  setStatus("Canceled polygon region");
  renderAll(false);
  return true;
}

function removeLastPolygonPoint() {
  if (state.polygonDraft.length === 0) return false;
  state.polygonDraft = state.polygonDraft.slice(0, -1);
  state.polygonHover = null;
  setStatus(`${state.polygonDraft.length} polygon point${state.polygonDraft.length === 1 ? "" : "s"}`);
  renderAll(false);
  return true;
}

function updateSelectedFromInspector() {
  const region = selectedRegion();
  if (!region) return;
  updateRegionRect(region.id, {
    x: numberValue("selectedX", 0),
    y: numberValue("selectedY", 0),
    width: numberValue("selectedW", 1),
    height: numberValue("selectedH", 1),
  });
  commitRegionHistory();
  renderAll(true);
}

function updateSelectedName() {
  const region = selectedRegion();
  if (!region) return;
  const name = cleanRegionName($("selectedName").value);
  state.regions = state.regions.map((item) => {
    if (item.id !== region.id) return item;
    const updated = { ...item, name };
    if (!updated.name) delete updated.name;
    return updated;
  });
  commitRegionHistory();
  setStatus(name ? `Named ${region.id} as ${name}` : `Cleared name for ${region.id}`);
  renderAll(true);
}

function updateSelectedAlphaMode() {
  const region = selectedRegion();
  if (!region) return;
  const alpha = normalizeAlphaPolicy({ mode: $("selectedAlphaMode").value });
  state.regions = state.regions.map((item) => {
    if (item.id !== region.id) return item;
    return { ...item, alpha };
  });
  commitRegionHistory();
  setStatus(`Set ${region.id} alpha to ${alphaModeLabel(alpha.mode)}`);
  renderAll(true);
}

function showTab(name) {
  const selected = ["regions", "review", "slice9"].includes(name) ? name : "regions";
  for (const panel of ["regions", "review", "slice9"]) {
    const active = panel === selected;
    $(`${panel}Tab`).classList.toggle("is-active", active);
    $(`${panel}Tab`).setAttribute("aria-selected", String(active));
    $(`${panel}Panel`).classList.toggle("hidden", !active);
  }
}

function setWorkspaceMode(mode, tab = mode === "slice9" ? "slice9" : "regions") {
  state.workspaceMode = mode === "slice9" ? "slice9" : "regions";
  state.interaction = null;
  state.draftRect = null;
  if (state.workspaceMode !== "regions") {
    state.polygonDraft = [];
    state.polygonHover = null;
  }
  showTab(tab);
  if (stageState().showCanvasStage && state.autoFit && hasImage()) {
    updateStageVisibility();
    fitToImage();
    return;
  }
  renderAll(false);
}

function setStageView(view) {
  state.stageView = normalizeStageView(view);
  state.interaction = null;
  state.draftRect = null;
  if (state.stageView === "review") {
    state.polygonDraft = [];
    state.polygonHover = null;
  }
  if (stageState().showCanvasStage && state.autoFit && hasImage()) {
    updateStageVisibility();
    fitToImage();
    return;
  }
  renderAll(false);
}

function setSlice9SourceMode(sourceMode) {
  if (sourceMode === "region" && !selectedRegion()) {
    setStatus("Select a region before using Slice 9 region source");
    return;
  }
  state.slice9 = defaultSlice9State(sourceMode === "region" ? "region" : "image");
  setStatus(sourceMode === "region" ? "Slice 9 source: selected region" : "Slice 9 source: whole image");
  renderAll(false);
}

function updateSlice9FromInputs(commit = false) {
  if (!hasImage()) return;
  const source = slice9SourceRect();
  const insets = clampInsets({
    left: boundedNumberValue("slice9Left", state.slice9.left, 0, 4096),
    right: boundedNumberValue("slice9Right", state.slice9.right, 0, 4096),
    top: boundedNumberValue("slice9Top", state.slice9.top, 0, 4096),
    bottom: boundedNumberValue("slice9Bottom", state.slice9.bottom, 0, 4096),
  }, source);
  const target = clampNineSliceSize({
    targetWidth: boundedNumberValue("slice9PreviewW", state.slice9.targetWidth, 1, 4096),
    targetHeight: boundedNumberValue("slice9PreviewH", state.slice9.targetHeight, 1, 4096),
    insets,
  });
  state.slice9 = { ...state.slice9, ...insets, targetWidth: target.width, targetHeight: target.height };
  if (commit || !slice9InputFocused()) syncSlice9Inputs();
  renderAll(false);
}

function updateCanvasCursor() {
  const regionsMode = state.workspaceMode === "regions";
  canvas.classList.toggle("is-rect", regionsMode && state.editMode === "rect");
  canvas.classList.toggle("is-polygon", regionsMode && state.editMode === "polygon");
  canvas.classList.toggle("is-pan", state.editMode === "pan" || state.spaceDown);
  canvas.classList.toggle("is-panning", state.interaction?.type === "pan");
  if (state.interaction) return;
  if ((state.editMode === "select" || !regionsMode) && hasImage()) {
    canvas.style.cursor = "default";
  } else {
    canvas.style.cursor = "";
  }
}

function startPan(event) {
  state.interaction = {
    type: "pan",
    pointerId: event.pointerId,
    startScreen: framePoint(event),
    startViewport: { ...state.viewport },
  };
  canvas.setPointerCapture(event.pointerId);
  canvas.classList.add("is-panning");
}

function handlePointerDown(event) {
  if (!hasImage()) return;
  if (event.button === 2) return;
  event.preventDefault();
  if (event.button === 1 || state.editMode === "pan" || state.spaceDown) {
    startPan(event);
    return;
  }
  const imagePoint = imagePointFromEvent(event);
  if (state.workspaceMode !== "regions") {
    if (state.workspaceMode === "slice9" && state.slice9.sourceMode === "region") {
      const hit = hitRegion(imagePoint);
      if (hit?.region) {
        state.selectedId = hit.region.id;
        normalizeSlice9State();
        setStatus(`Slice 9 source: ${regionDisplayName(hit.region)}`);
        renderAll(false);
      }
    }
    return;
  }

  if (state.editMode === "rect") {
    state.interaction = {
      type: "create",
      pointerId: event.pointerId,
      startImage: imagePoint,
    };
    state.draftRect = { x: imagePoint.x, y: imagePoint.y, width: 1, height: 1 };
    canvas.setPointerCapture(event.pointerId);
    renderAll(false);
    return;
  }

  if (state.editMode === "polygon") {
    if (event.detail > 1) {
      finishPolygonDraft();
      return;
    }
    addPolygonPoint(imagePoint);
    return;
  }

  const hit = hitRegion(imagePoint);
  state.selectedId = hit?.region.id || null;
  if (hit?.handle === "body") {
    state.interaction = {
      type: "move",
      pointerId: event.pointerId,
      startImage: imagePoint,
      regionId: hit.region.id,
      originalRect: rectFromArray(hit.region.rect),
    };
    canvas.setPointerCapture(event.pointerId);
  } else if (hit?.handle) {
    state.interaction = {
      type: "resize",
      pointerId: event.pointerId,
      startImage: imagePoint,
      regionId: hit.region.id,
      handle: hit.handle,
      originalRect: rectFromArray(hit.region.rect),
    };
    canvas.setPointerCapture(event.pointerId);
  }
  renderAll(false);
}

function handlePointerMove(event) {
  if (!hasImage()) return;
  const imagePoint = imagePointFromEvent(event);
  $("cursorMeta").textContent = `x ${Math.round(imagePoint.x)}, y ${Math.round(imagePoint.y)}`;
  if (state.workspaceMode === "regions" && state.editMode === "polygon" && state.polygonDraft.length > 0 && !state.interaction) {
    state.polygonHover = roundImagePoint(imagePoint);
    renderCanvas();
    return;
  }
  const interaction = state.interaction;
  if (!interaction) return;
  event.preventDefault();

  if (interaction.type === "pan") {
    const screen = framePoint(event);
    state.viewport = {
      ...state.viewport,
      offsetX: interaction.startViewport.offsetX + screen.x - interaction.startScreen.x,
      offsetY: interaction.startViewport.offsetY + screen.y - interaction.startScreen.y,
    };
    state.autoFit = false;
    renderAll(false);
    return;
  }

  if (interaction.type === "create") {
    state.draftRect = rectFromPoints(interaction.startImage, imagePoint);
    renderCanvas();
    return;
  }

  if (interaction.type === "move") {
    updateRegionRect(
      interaction.regionId,
      moveRect(
        interaction.originalRect,
        imagePoint.x - interaction.startImage.x,
        imagePoint.y - interaction.startImage.y,
        imageBounds(),
      ),
    );
    renderAll(false);
    return;
  }

  if (interaction.type === "resize") {
    updateRegionRect(
      interaction.regionId,
      resizeRectFromHandle(interaction.originalRect, interaction.handle, imagePoint, imageBounds()),
    );
    renderAll(false);
  }
}

function handlePointerUp(event) {
  const interaction = state.interaction;
  if (!interaction) return;
  if (canvas.hasPointerCapture(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
  state.interaction = null;
  canvas.classList.remove("is-panning");

  if (interaction.type === "create" && state.draftRect && state.draftRect.width >= 3 && state.draftRect.height >= 3) {
    const clamped = sanitizeRect(state.draftRect);
    state.regions = renumberRegions([
      ...state.regions,
      {
        id: "manual",
        rect: rectToArray(clamped),
        content_bbox: rectToArray(clamped),
        area_px: clamped.width * clamped.height,
        source: "manual",
      },
    ]);
    state.selectedId = state.regions.at(-1)?.id || null;
    commitRegionHistory();
    setStatus("Added manual region");
  }

  state.draftRect = null;
  if (interaction.type === "move" || interaction.type === "resize") {
    commitRegionHistory();
  }
  renderAll(interaction.type !== "pan");
}

function nudgeSelected(dx, dy) {
  const region = selectedRegion();
  if (!region) return;
  updateRegionRect(region.id, moveRect(rectFromArray(region.rect), dx, dy, imageBounds()));
  commitRegionHistory();
  renderAll(true);
}

$("imageInput").addEventListener("change", (event) => {
  loadImageFile(event.target.files?.[0]).catch((error) => setStatus(error.message));
});

const dropZone = $("dropZone");
dropZone.addEventListener("dragover", (event) => {
  event.preventDefault();
  dropZone.classList.add("is-over");
});
dropZone.addEventListener("dragleave", () => dropZone.classList.remove("is-over"));
dropZone.addEventListener("drop", (event) => {
  event.preventDefault();
  dropZone.classList.remove("is-over");
  loadImageFile(event.dataTransfer.files?.[0]).catch((error) => setStatus(error.message));
});

$("detectRegions").addEventListener("click", () => runDetectRegions().catch((error) => setStatus(error.message)));
$("selectMode").addEventListener("click", () => setEditMode("select"));
$("rectMode").addEventListener("click", () => setEditMode("rect"));
$("polygonMode").addEventListener("click", () => setEditMode("polygon"));
$("panMode").addEventListener("click", () => setEditMode("pan"));
$("fitView").addEventListener("click", fitToImage);
$("actualSize").addEventListener("click", setActualSize);
$("zoomIn").addEventListener("click", () => zoomBy(1.2));
$("zoomOut").addEventListener("click", () => zoomBy(1 / 1.2));
$("deleteRegion").addEventListener("click", deleteSelectedRegion);
$("clearRegions").addEventListener("click", clearRegions);
$("undoRegions").addEventListener("click", undoRegions);
$("redoRegions").addEventListener("click", redoRegions);
$("downloadZip").addEventListener("click", () => downloadZip().catch((error) => setStatus(error.message)));
$("downloadSelectedRegion").addEventListener("click", () => downloadSelectedRegion().catch((error) => setStatus(error.message)));
$("contextDownloadRegion").addEventListener("click", () => downloadSelectedRegion().catch((error) => setStatus(error.message)));
$("regionsWorkspaceMode").addEventListener("click", () => setWorkspaceMode("regions", "regions"));
$("slice9WorkspaceMode").addEventListener("click", () => setWorkspaceMode("slice9", "slice9"));
$("reviewStageMode").addEventListener("click", () => setStageView("review"));
$("editStageMode").addEventListener("click", () => setStageView("edit"));
$("regionsTab").addEventListener("click", () => setWorkspaceMode("regions", "regions"));
$("reviewTab").addEventListener("click", () => setWorkspaceMode("regions", "review"));
$("slice9Tab").addEventListener("click", () => setWorkspaceMode("slice9", "slice9"));
$("slice9WholeSource").addEventListener("click", () => setSlice9SourceMode("image"));
$("slice9RegionSource").addEventListener("click", () => setSlice9SourceMode("region"));
$("keyTolerance").addEventListener("input", updateButtons);

for (const id of ["prefix", "backgroundMode", "minArea", "padding", "mergeDistance", "rowTolerance", "includeReviewSheet"]) {
  $(id).addEventListener("input", updateButtons);
}

for (const id of ["selectedX", "selectedY", "selectedW", "selectedH"]) {
  $(id).addEventListener("change", updateSelectedFromInspector);
}
$("selectedName").addEventListener("change", updateSelectedName);
$("selectedAlphaMode").addEventListener("change", updateSelectedAlphaMode);
$("selectedPreviewMode").addEventListener("change", () => renderAll(false));
$("fitSelectedRegion").addEventListener("click", () => {
  const region = selectedRegion();
  if (!region) return;
  focusRegion(region);
  renderAll(false);
});

for (const id of slice9InputIds) {
  $(id).addEventListener("input", () => updateSlice9FromInputs(false));
  $(id).addEventListener("change", () => updateSlice9FromInputs(true));
}

canvas.addEventListener("pointerdown", handlePointerDown);
canvas.addEventListener("pointermove", handlePointerMove);
canvas.addEventListener("pointerup", handlePointerUp);
canvas.addEventListener("pointercancel", handlePointerUp);
canvas.addEventListener("contextmenu", (event) => {
  if (!hasImage()) return;
  const hit = hitRegion(imagePointFromEvent(event));
  if (!hit?.region) {
    hideRegionContextMenu();
    return;
  }
  showRegionContextMenu(hit.region, event);
});
canvas.addEventListener("dblclick", (event) => {
  if (state.editMode !== "polygon") return;
  event.preventDefault();
  finishPolygonDraft();
});
canvas.addEventListener("wheel", (event) => {
  if (!hasImage()) return;
  event.preventDefault();
  zoomBy(event.deltaY < 0 ? 1.12 : 1 / 1.12, framePoint(event));
}, { passive: false });

$("selectedRegionPreviewCanvas").addEventListener("contextmenu", (event) => {
  const region = selectedRegion();
  if (!region) return;
  showRegionContextMenu(region, event);
});

document.addEventListener("click", (event) => {
  if (!$("regionContextMenu").contains(event.target)) {
    hideRegionContextMenu();
  }
});

window.addEventListener("blur", hideRegionContextMenu);

window.addEventListener("resize", () => {
  if (state.autoFit) {
    fitToImage();
  } else {
    renderAll(false);
  }
});

window.addEventListener("keydown", (event) => {
  const tagName = document.activeElement?.tagName;
  const editingInput = tagName === "INPUT" || tagName === "TEXTAREA" || tagName === "SELECT";
  const isUndo = (event.ctrlKey || event.metaKey) && !event.shiftKey && event.key.toLowerCase() === "z";
  const isRedo =
    ((event.ctrlKey || event.metaKey) && event.shiftKey && event.key.toLowerCase() === "z") ||
    ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "y");
  if (state.workspaceMode !== "regions") {
    if (event.code === "Space" && !editingInput) {
      state.spaceDown = true;
      updateCanvasCursor();
      return;
    }
    if (!editingInput && event.key === "Escape") {
      event.preventDefault();
      setWorkspaceMode("regions", "regions");
    }
    return;
  }
  if (!editingInput && isUndo) {
    event.preventDefault();
    if (removeLastPolygonPoint()) return;
    undoRegions();
    return;
  }
  if (!editingInput && isRedo) {
    event.preventDefault();
    redoRegions();
    return;
  }
  if (event.code === "Space" && !editingInput) {
    state.spaceDown = true;
    updateCanvasCursor();
  }
  if (editingInput) return;
  if (event.key === "Enter" && state.editMode === "polygon") {
    event.preventDefault();
    finishPolygonDraft();
    return;
  }
  if (event.key === "Delete" || event.key === "Backspace") {
    event.preventDefault();
    if (removeLastPolygonPoint()) return;
    deleteSelectedRegion();
    return;
  }
  const step = event.shiftKey ? 10 : 1;
  if (event.key === "ArrowLeft") {
    event.preventDefault();
    nudgeSelected(-step, 0);
  } else if (event.key === "ArrowRight") {
    event.preventDefault();
    nudgeSelected(step, 0);
  } else if (event.key === "ArrowUp") {
    event.preventDefault();
    nudgeSelected(0, -step);
  } else if (event.key === "ArrowDown") {
    event.preventDefault();
    nudgeSelected(0, step);
  } else if (event.key === "Escape") {
    if (cancelPolygonDraft()) return;
    setEditMode("select");
  }
});

window.addEventListener("keyup", (event) => {
  if (event.code === "Space") {
    state.spaceDown = false;
    updateCanvasCursor();
  }
});

renderAll(false);
