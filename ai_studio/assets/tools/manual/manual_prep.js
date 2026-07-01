import {
  buildStoredZip,
  createSliceRects,
  cropPlan,
  safeAssetBaseName,
  sliceFileName,
  toHexColor,
  transformPixels,
  trimTransparentRect,
} from "./image_ops.mjs";

const root = document.getElementById("manualPrepRoot");
const statusNode = document.getElementById("prepStatus");

const state = {
  sourceCanvas: document.createElement("canvas"),
  processedCanvas: document.createElement("canvas"),
  sourceName: "",
  sliceMode: "grid",
  renderQueued: false,
  pickKey: false,
  lastRects: [],
};

root.innerHTML = `
  <div class="prep-layout">
    <aside class="prep-controls" aria-label="Asset prep controls">
      <section class="control-section">
        <h2>Source</h2>
        <label id="dropZone" class="drop-zone">
          <input id="imageInput" type="file" accept="image/png,image/jpeg,image/webp,image/gif">
          <span>
            <span class="drop-main">Load image</span>
            <span class="drop-sub">PNG, JPG, WEBP, GIF</span>
          </span>
        </label>
        <div id="imageMeta" class="meta-line">No file selected</div>
      </section>

      <section class="control-section">
        <h2>Output</h2>
        <div class="field-grid">
          <div class="field-row wide">
            <label for="prefix">Name prefix</label>
            <input id="prefix" type="text" value="asset" autocomplete="off">
          </div>
          <label class="check-row wide">
            <input id="trimTiles" type="checkbox">
            <span>Trim transparent edges</span>
          </label>
        </div>
      </section>

      <section class="control-section">
        <h2>Slice</h2>
        <div class="segmented" role="group" aria-label="Slice mode">
          <button type="button" class="is-active" data-slice-mode="grid">Grid</button>
          <button type="button" data-slice-mode="tile">Tile size</button>
        </div>

        <div id="gridPanel" class="mode-panel">
          <div class="field-grid">
            <div class="field-row">
              <label for="columns">Columns</label>
              <input id="columns" type="number" min="1" max="256" value="2">
            </div>
            <div class="field-row">
              <label for="rows">Rows</label>
              <input id="rows" type="number" min="1" max="256" value="2">
            </div>
          </div>
        </div>

        <div id="tilePanel" class="mode-panel" hidden>
          <div class="field-grid">
            <div class="field-row">
              <label for="tileWidth">Tile W</label>
              <input id="tileWidth" type="number" min="1" value="64">
            </div>
            <div class="field-row">
              <label for="tileHeight">Tile H</label>
              <input id="tileHeight" type="number" min="1" value="64">
            </div>
          </div>
        </div>

        <div class="field-grid slice-offset-grid">
          <div class="field-row">
            <label for="offsetX">Offset X</label>
            <input id="offsetX" type="number" min="0" value="0">
          </div>
          <div class="field-row">
            <label for="offsetY">Offset Y</label>
            <input id="offsetY" type="number" min="0" value="0">
          </div>
          <div class="field-row">
            <label for="gapX">Gap X</label>
            <input id="gapX" type="number" min="0" value="0">
          </div>
          <div class="field-row">
            <label for="gapY">Gap Y</label>
            <input id="gapY" type="number" min="0" value="0">
          </div>
        </div>
      </section>

      <section class="control-section">
        <h2>Alpha</h2>
        <div class="field-row">
          <label for="alphaMode">Mode</label>
          <select id="alphaMode">
            <option value="keep">Keep alpha</option>
            <option value="discard">Make opaque</option>
            <option value="flatten">Flatten on color</option>
            <option value="threshold">Alpha threshold</option>
            <option value="key">Border key color</option>
          </select>
        </div>

        <div id="flattenControls" class="alpha-extra" hidden>
          <div class="field-row inline">
            <label for="backgroundColor">Background</label>
            <input id="backgroundColor" type="color" value="#0f1115">
          </div>
        </div>

        <div id="thresholdControls" class="alpha-extra" hidden>
          <div class="field-row">
            <label for="alphaThreshold">Threshold</label>
            <input id="alphaThreshold" type="range" min="0" max="255" value="128">
            <span id="thresholdValue" class="field-note">128</span>
          </div>
        </div>

        <div id="keyControls" class="alpha-extra" hidden>
          <div class="field-row inline">
            <label for="keyColor">Key color</label>
            <input id="keyColor" type="color" value="#ff00ff">
          </div>
          <div class="field-row">
            <label for="keyTolerance">Tolerance</label>
            <input id="keyTolerance" type="range" min="0" max="128" value="16">
            <span id="toleranceValue" class="field-note">16</span>
          </div>
          <button id="pickKey" type="button" class="ghost">Pick from image</button>
        </div>
      </section>

      <section class="control-section">
        <h2>Export</h2>
        <div class="export-stack">
          <button id="downloadImage" type="button" disabled>Processed PNG</button>
          <button id="downloadZip" type="button" disabled>Tiles ZIP</button>
          <button id="downloadPlan" type="button" class="ghost" disabled>Crop plan JSON</button>
        </div>
      </section>
    </aside>

    <section class="preview-area" aria-label="Image preview">
      <div class="canvas-frame">
        <div id="canvasStage" class="canvas-stage">
          <div id="emptyState" class="canvas-empty">Load an image to start.</div>
          <canvas id="previewCanvas" class="hidden"></canvas>
        </div>
      </div>
      <div class="preview-footer">
        <span id="previewMeta">0 x 0</span>
        <span id="sliceMeta">0 slices</span>
      </div>
    </section>

    <aside class="prep-tiles" aria-label="Prepared tile previews">
      <section class="tile-section">
        <div class="tile-toolbar">
          <h2>Tiles</h2>
          <span id="tileCount" class="tile-count">0</span>
        </div>
      </section>
      <div id="tileGrid" class="tile-grid">
        <div class="tile-empty">No slices yet.</div>
      </div>
    </aside>
  </div>
`;

const $ = (id) => document.getElementById(id);
const previewCanvas = $("previewCanvas");
const previewContext = previewCanvas.getContext("2d");
const sourceContext = state.sourceCanvas.getContext("2d", { willReadFrequently: true });
const processedContext = state.processedCanvas.getContext("2d", { willReadFrequently: true });

const controlIds = [
  "prefix", "trimTiles", "columns", "rows", "tileWidth", "tileHeight", "offsetX", "offsetY", "gapX", "gapY",
  "alphaMode", "backgroundColor", "alphaThreshold", "keyColor", "keyTolerance",
];

function setStatus(text) {
  statusNode.textContent = text;
}

function numberValue(id, fallback) {
  const value = Number.parseInt($(id).value, 10);
  return Number.isFinite(value) ? value : fallback;
}

function hasImage() {
  return state.sourceCanvas.width > 0 && state.sourceCanvas.height > 0;
}

function setExportsEnabled(enabled) {
  $("downloadImage").disabled = !enabled;
  $("downloadZip").disabled = !enabled;
  $("downloadPlan").disabled = !enabled;
}

function updateModePanels() {
  $("gridPanel").hidden = state.sliceMode !== "grid";
  $("tilePanel").hidden = state.sliceMode !== "tile";
  for (const button of document.querySelectorAll("[data-slice-mode]")) {
    button.classList.toggle("is-active", button.dataset.sliceMode === state.sliceMode);
  }

  const alphaMode = $("alphaMode").value;
  $("flattenControls").hidden = alphaMode !== "flatten";
  $("thresholdControls").hidden = alphaMode !== "threshold";
  $("keyControls").hidden = alphaMode !== "key";
  $("thresholdValue").textContent = $("alphaThreshold").value;
  $("toleranceValue").textContent = $("keyTolerance").value;
}

function sliceSettings() {
  return {
    mode: state.sliceMode,
    columns: numberValue("columns", 1),
    rows: numberValue("rows", 1),
    tileWidth: numberValue("tileWidth", 64),
    tileHeight: numberValue("tileHeight", 64),
    offsetX: numberValue("offsetX", 0),
    offsetY: numberValue("offsetY", 0),
    gapX: numberValue("gapX", 0),
    gapY: numberValue("gapY", 0),
  };
}

function alphaSettings() {
  const mode = $("alphaMode").value;
  if (mode === "flatten") return { mode, background: $("backgroundColor").value };
  if (mode === "threshold") return { mode, threshold: numberValue("alphaThreshold", 128) };
  if (mode === "key") return { mode, keyColor: $("keyColor").value, tolerance: numberValue("keyTolerance", 16) };
  return { mode };
}

function prepareProcessedCanvas() {
  const width = state.sourceCanvas.width;
  const height = state.sourceCanvas.height;
  const source = sourceContext.getImageData(0, 0, width, height);
  const transformed = transformPixels(source.data, width, height, alphaSettings());
  state.processedCanvas.width = width;
  state.processedCanvas.height = height;
  processedContext.putImageData(new ImageData(transformed.data, width, height), 0, 0);
  return transformed.changedPixels;
}

function drawPreview(rects) {
  const width = state.processedCanvas.width;
  const height = state.processedCanvas.height;
  previewCanvas.width = width;
  previewCanvas.height = height;
  previewContext.clearRect(0, 0, width, height);
  previewContext.drawImage(state.processedCanvas, 0, 0);

  previewContext.save();
  previewContext.strokeStyle = "rgba(125, 211, 252, 0.95)";
  previewContext.lineWidth = Math.max(1, Math.round(Math.min(width, height) / 512));
  previewContext.font = `${Math.max(10, Math.round(Math.min(width, height) / 42))}px Segoe UI, sans-serif`;
  previewContext.textBaseline = "top";
  rects.forEach((rect) => {
    previewContext.strokeRect(rect.x + 0.5, rect.y + 0.5, rect.width - 1, rect.height - 1);
    const label = String(rect.index + 1);
    const metrics = previewContext.measureText(label);
    previewContext.fillStyle = "rgba(10, 14, 20, 0.78)";
    previewContext.fillRect(rect.x + 3, rect.y + 3, metrics.width + 8, 18);
    previewContext.fillStyle = "#e7f6ff";
    previewContext.fillText(label, rect.x + 7, rect.y + 5);
  });
  previewContext.restore();
}

function tileCanvas(rect, trim = $("trimTiles").checked) {
  const base = document.createElement("canvas");
  base.width = Math.max(1, rect.width);
  base.height = Math.max(1, rect.height);
  const baseContext = base.getContext("2d", { willReadFrequently: true });
  baseContext.clearRect(0, 0, base.width, base.height);
  baseContext.drawImage(state.processedCanvas, rect.x, rect.y, rect.width, rect.height, 0, 0, rect.width, rect.height);
  if (!trim) return base;

  const data = baseContext.getImageData(0, 0, base.width, base.height);
  const bounds = trimTransparentRect(data.data, base.width, base.height, 0);
  const trimmed = document.createElement("canvas");
  trimmed.width = bounds.width;
  trimmed.height = bounds.height;
  trimmed.getContext("2d").drawImage(base, bounds.x, bounds.y, bounds.width, bounds.height, 0, 0, bounds.width, bounds.height);
  return trimmed;
}

function renderTiles(rects) {
  const tileGrid = $("tileGrid");
  tileGrid.innerHTML = "";
  if (!rects.length) {
    tileGrid.innerHTML = '<div class="tile-empty">No slices match the current settings.</div>';
    return;
  }

  for (const rect of rects.slice(0, 120)) {
    const card = document.createElement("article");
    card.className = "tile-card";

    const thumb = document.createElement("canvas");
    thumb.width = 96;
    thumb.height = 96;
    const thumbContext = thumb.getContext("2d");
    thumbContext.clearRect(0, 0, thumb.width, thumb.height);
    const scale = Math.min(thumb.width / rect.width, thumb.height / rect.height);
    const drawWidth = Math.max(1, Math.round(rect.width * scale));
    const drawHeight = Math.max(1, Math.round(rect.height * scale));
    const drawX = Math.floor((thumb.width - drawWidth) / 2);
    const drawY = Math.floor((thumb.height - drawHeight) / 2);
    thumbContext.drawImage(state.processedCanvas, rect.x, rect.y, rect.width, rect.height, drawX, drawY, drawWidth, drawHeight);

    const meta = document.createElement("div");
    meta.className = "tile-meta";
    const fileName = sliceFileName($("prefix").value, rect);
    meta.innerHTML = `
      <strong title="${fileName}">${fileName}</strong>
      <span>${rect.width}x${rect.height} at ${rect.x},${rect.y}</span>
      <button type="button" class="ghost">PNG</button>
    `;
    meta.querySelector("button").addEventListener("click", () => downloadTile(rect));

    card.append(thumb, meta);
    tileGrid.append(card);
  }

  if (rects.length > 120) {
    const more = document.createElement("div");
    more.className = "tile-empty";
    more.textContent = `${rects.length - 120} more slices will be included in exports.`;
    tileGrid.append(more);
  }
}

function render() {
  state.renderQueued = false;
  updateModePanels();

  if (!hasImage()) {
    $("emptyState").classList.remove("hidden");
    previewCanvas.classList.add("hidden");
    $("previewMeta").textContent = "0 x 0";
    $("sliceMeta").textContent = "0 slices";
    $("tileCount").textContent = "0";
    setExportsEnabled(false);
    return;
  }

  const changedPixels = prepareProcessedCanvas();
  const rects = createSliceRects(state.sourceCanvas.width, state.sourceCanvas.height, sliceSettings());
  state.lastRects = rects;
  drawPreview(rects);
  renderTiles(rects);

  $("emptyState").classList.add("hidden");
  previewCanvas.classList.remove("hidden");
  $("previewMeta").textContent = `${state.sourceCanvas.width} x ${state.sourceCanvas.height}`;
  $("sliceMeta").textContent = `${rects.length} slices`;
  $("tileCount").textContent = String(rects.length);
  setExportsEnabled(rects.length > 0);
  setStatus(`${state.sourceName}: ${rects.length} slices, ${changedPixels} alpha pixels changed`);
}

function scheduleRender() {
  if (state.renderQueued) return;
  state.renderQueued = true;
  requestAnimationFrame(render);
}

function drawSourceImage(image, fileName) {
  state.sourceName = fileName;
  state.sourceCanvas.width = image.naturalWidth;
  state.sourceCanvas.height = image.naturalHeight;
  sourceContext.clearRect(0, 0, state.sourceCanvas.width, state.sourceCanvas.height);
  sourceContext.drawImage(image, 0, 0);
  $("imageMeta").textContent = `${fileName} - ${image.naturalWidth} x ${image.naturalHeight}`;
  if ($("prefix").value === "asset") $("prefix").value = safeAssetBaseName(fileName);
  $("tileWidth").value = Math.max(1, Math.floor(image.naturalWidth / Math.max(1, numberValue("columns", 2))));
  $("tileHeight").value = Math.max(1, Math.floor(image.naturalHeight / Math.max(1, numberValue("rows", 2))));
  scheduleRender();
}

function loadFile(file) {
  if (!file || !file.type.startsWith("image/")) {
    setStatus("Selected file is not an image.");
    return;
  }
  const url = URL.createObjectURL(file);
  const image = new Image();
  image.onload = () => {
    drawSourceImage(image, file.name);
    URL.revokeObjectURL(url);
  };
  image.onerror = () => {
    setStatus("Could not read image.");
    URL.revokeObjectURL(url);
  };
  image.src = url;
}

function canvasToBlob(canvas) {
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => {
      if (blob) resolve(blob);
      else reject(new Error("Could not encode PNG."));
    }, "image/png");
  });
}

function downloadBlob(blob, name) {
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = name;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

async function downloadTile(rect) {
  const canvas = tileCanvas(rect);
  const blob = await canvasToBlob(canvas);
  downloadBlob(blob, sliceFileName($("prefix").value, rect));
}

async function downloadProcessedImage() {
  if (!hasImage()) return;
  const blob = await canvasToBlob(state.processedCanvas);
  downloadBlob(blob, `${safeAssetBaseName($("prefix").value)}_processed.png`);
}

async function downloadZip() {
  if (!hasImage() || !state.lastRects.length) return;
  const button = $("downloadZip");
  button.disabled = true;
  setStatus(`Preparing ${state.lastRects.length} PNG files...`);
  try {
    const entries = [];
    for (const rect of state.lastRects) {
      const canvas = tileCanvas(rect);
      const blob = await canvasToBlob(canvas);
      entries.push({ name: sliceFileName($("prefix").value, rect), data: await blob.arrayBuffer() });
    }
    const zip = buildStoredZip(entries);
    downloadBlob(new Blob([zip], { type: "application/zip" }), `${safeAssetBaseName($("prefix").value)}_tiles.zip`);
    setStatus(`Exported ${entries.length} tiles.`);
  } catch (error) {
    setStatus(error.message || String(error));
  } finally {
    button.disabled = false;
  }
}

function downloadPlan() {
  if (!hasImage()) return;
  const plan = cropPlan({
    sourceName: state.sourceName,
    width: state.sourceCanvas.width,
    height: state.sourceCanvas.height,
    alpha: alphaSettings(),
    slice: sliceSettings(),
    rects: state.lastRects,
    prefix: $("prefix").value,
  });
  const blob = new Blob([`${JSON.stringify(plan, null, 2)}\n`], { type: "application/json" });
  downloadBlob(blob, `${safeAssetBaseName($("prefix").value)}_crop_plan.json`);
}

function pickKeyColor(event) {
  if (!state.pickKey || !hasImage()) return;
  const bounds = previewCanvas.getBoundingClientRect();
  const x = Math.floor((event.clientX - bounds.left) * (state.sourceCanvas.width / bounds.width));
  const y = Math.floor((event.clientY - bounds.top) * (state.sourceCanvas.height / bounds.height));
  const safeX = Math.max(0, Math.min(state.sourceCanvas.width - 1, x));
  const safeY = Math.max(0, Math.min(state.sourceCanvas.height - 1, y));
  const pixel = sourceContext.getImageData(safeX, safeY, 1, 1).data;
  $("keyColor").value = toHexColor(pixel[0], pixel[1], pixel[2]);
  $("alphaMode").value = "key";
  state.pickKey = false;
  $("pickKey").classList.remove("warn");
  scheduleRender();
}

for (const id of controlIds) {
  $(id).addEventListener("input", scheduleRender);
  $(id).addEventListener("change", scheduleRender);
}

for (const button of document.querySelectorAll("[data-slice-mode]")) {
  button.addEventListener("click", () => {
    state.sliceMode = button.dataset.sliceMode;
    scheduleRender();
  });
}

$("imageInput").addEventListener("change", (event) => loadFile(event.target.files[0]));
$("downloadImage").addEventListener("click", downloadProcessedImage);
$("downloadZip").addEventListener("click", downloadZip);
$("downloadPlan").addEventListener("click", downloadPlan);
$("pickKey").addEventListener("click", () => {
  state.pickKey = !state.pickKey;
  $("pickKey").classList.toggle("warn", state.pickKey);
});
previewCanvas.addEventListener("click", pickKeyColor);

const dropZone = $("dropZone");
for (const eventName of ["dragenter", "dragover"]) {
  dropZone.addEventListener(eventName, (event) => {
    event.preventDefault();
    dropZone.classList.add("is-over");
  });
}
for (const eventName of ["dragleave", "drop"]) {
  dropZone.addEventListener(eventName, (event) => {
    event.preventDefault();
    dropZone.classList.remove("is-over");
  });
}
dropZone.addEventListener("drop", (event) => loadFile(event.dataTransfer.files[0]));

updateModePanels();
render();
