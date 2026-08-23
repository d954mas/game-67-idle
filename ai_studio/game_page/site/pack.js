// Pack inspector client: /game/<id>/pack?path=<rel> renders one parsed ntpack.
(function () {
  const match = /^\/game\/([^/]+)\/pack\/?$/.exec(decodeURIComponent(location.pathname));
  const gameId = match ? match[1] : "";
  const packPath = new URLSearchParams(location.search).get("path") || "";

  const byId = (id) => document.getElementById(id);

  function formatBytes(bytes) {
    if (bytes == null) return "";
    if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
    if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${bytes} B`;
  }

  function fail(message) {
    byId("packTitle").textContent = "Pack unavailable";
    byId("packSubtitle").textContent = message;
  }

  function cell(text) {
    const td = document.createElement("td");
    td.textContent = text;
    return td;
  }

  const TEXTURE_HEADER_SIZE = 28;
  let basisModulePromise = null;
  let loadedDump = null;
  const textureCanvasCache = new Map();

  function entryQuery(index) {
    return `game=${encodeURIComponent(gameId)}&path=${encodeURIComponent(packPath)}&index=${index}`;
  }

  async function fetchEntryDetail(index) {
    const response = await fetch(`/api/game-page/pack-entry?${entryQuery(index)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`entry detail failed: ${response.status}`);
    return response.json();
  }

  async function fetchEntryBytes(index) {
    const response = await fetch(`/api/game-page/pack-entry-data?${entryQuery(index)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`entry data failed: ${response.status}`);
    return new Uint8Array(await response.arrayBuffer());
  }

  function loadBasisModule() {
    if (basisModulePromise) return basisModulePromise;
    basisModulePromise = new Promise((resolveModule, reject) => {
      const script = document.createElement("script");
      script.src = "/ai_studio/game_page/vendor/basis/basis_transcoder.js";
      script.onload = () => {
        window.BASIS({
          locateFile: () => "/ai_studio/game_page/vendor/basis/basis_transcoder.wasm",
        }).then((module) => {
          module.initializeBasis();
          resolveModule(module);
        }, reject);
      };
      script.onerror = () => reject(new Error("basis transcoder failed to load"));
      document.head.append(script);
    });
    return basisModulePromise;
  }

  function drawRgba(width, height, rgba) {
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    canvas.className = "pack-preview-canvas";
    const context = canvas.getContext("2d");
    const image = context.createImageData(width, height);
    image.data.set(rgba);
    context.putImageData(image, 0, 0);
    return canvas;
  }

  function rawToRgba(bytes, width, height, format) {
    const channels = { 1: 4, 2: 3, 3: 2, 4: 1 }[format];
    if (!channels) return null;
    const pixels = width * height;
    if (bytes.length < pixels * channels) return null;
    const rgba = new Uint8ClampedArray(pixels * 4);
    for (let index = 0; index < pixels; index += 1) {
      const src = index * channels;
      const dst = index * 4;
      if (channels === 4) {
        rgba[dst] = bytes[src];
        rgba[dst + 1] = bytes[src + 1];
        rgba[dst + 2] = bytes[src + 2];
        rgba[dst + 3] = bytes[src + 3];
      } else if (channels === 3) {
        rgba[dst] = bytes[src];
        rgba[dst + 1] = bytes[src + 1];
        rgba[dst + 2] = bytes[src + 2];
        rgba[dst + 3] = 255;
      } else if (channels === 2) {
        rgba[dst] = bytes[src];
        rgba[dst + 1] = bytes[src + 1];
        rgba[dst + 2] = 0;
        rgba[dst + 3] = 255;
      } else {
        rgba[dst] = bytes[src];
        rgba[dst + 1] = bytes[src];
        rgba[dst + 2] = bytes[src];
        rgba[dst + 3] = 255;
      }
    }
    return rgba;
  }

  async function textureCanvas(index) {
    if (textureCanvasCache.has(index)) return textureCanvasCache.get(index);
    const bytes = await fetchEntryBytes(index);
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    if (bytes.length < TEXTURE_HEADER_SIZE || view.getUint32(0, true) !== 0x58455454) {
      throw new Error("entry does not carry a texture header");
    }
    const format = view.getUint16(6, true);
    const width = view.getUint32(8, true);
    const height = view.getUint32(12, true);
    const compression = view.getUint8(18);
    const payload = bytes.subarray(TEXTURE_HEADER_SIZE);
    let canvas;
    if (compression === 0) {
      const rgba = rawToRgba(payload, width, height, format);
      if (!rgba) throw new Error("raw pixel data is truncated");
      canvas = drawRgba(width, height, rgba);
    } else {
      const module = await loadBasisModule();
      const file = new module.BasisFile(payload);
      try {
        const RGBA32 = 13;
        if (!file.startTranscoding() || file.getNumImages() < 1) throw new Error("basis transcoding failed to start");
        const imageWidth = file.getImageWidth(0, 0);
        const imageHeight = file.getImageHeight(0, 0);
        const dst = new Uint8Array(file.getImageTranscodedSizeInBytes(0, 0, RGBA32));
        if (!file.transcodeImage(dst, 0, 0, RGBA32, 0, 0)) throw new Error("basis transcodeImage failed");
        canvas = drawRgba(imageWidth, imageHeight, new Uint8ClampedArray(dst.buffer, 0, imageWidth * imageHeight * 4));
      } finally {
        file.close();
        file.delete();
      }
    }
    textureCanvasCache.set(index, canvas);
    return canvas;
  }

  function previewNote(text) {
    const p = document.createElement("p");
    p.className = "game-placeholder";
    p.textContent = text;
    return p;
  }

  async function renderTexturePreview(container, index, detail) {
    const canvas = await textureCanvas(index);
    const info = detail.texture;
    container.append(
      previewNote(`${info.width}×${info.height} · format ${info.format} · mips ${info.mipCount} · ${info.compression ? "BASIS" : "RAW"}`),
      canvas,
    );
  }

  function findEntryByResourceId(resourceId) {
    return (loadedDump.entries || []).find((entry) => entry.resourceId === resourceId);
  }

  async function renderAtlasPreview(container, detail) {
    const atlas = detail.atlas;
    container.append(previewNote(
      `regions ${atlas.regionCount} · pages ${atlas.pageCount} · vertices ${atlas.totalVertexCount}`,
    ));
    for (let pageIndex = 0; pageIndex < atlas.pages.length; pageIndex += 1) {
      const page = atlas.pages[pageIndex];
      const pageEntry = findEntryByResourceId(page.resourceId);
      if (!pageEntry) {
        container.append(previewNote(`page ${pageIndex}: texture ${page.resourceId} is not in this pack`));
        continue;
      }
      const base = await textureCanvas(pageEntry.index);
      const canvas = document.createElement("canvas");
      canvas.width = base.width;
      canvas.height = base.height;
      canvas.className = "pack-preview-canvas";
      const context = canvas.getContext("2d");
      context.drawImage(base, 0, 0);
      context.strokeStyle = "rgba(80, 250, 123, 0.9)";
      context.lineWidth = Math.max(1, Math.round(base.width / 512));
      for (const region of atlas.regions) {
        if (region.pageIndex !== pageIndex) continue;
        const vertices = atlas.vertices.slice(region.vertexStart, region.vertexStart + region.vertexCount);
        if (!vertices.length) continue;
        context.beginPath();
        vertices.forEach((vertex, at) => {
          const x = vertex.u * canvas.width;
          const y = vertex.v * canvas.height;
          if (at === 0) context.moveTo(x, y);
          else context.lineTo(x, y);
        });
        context.closePath();
        context.stroke();
      }
      container.append(previewNote(`page ${pageIndex}: ${page.name || page.resourceId}`), canvas);
    }
    const list = document.createElement("p");
    list.className = "game-placeholder";
    list.textContent = atlas.regions.map((region) => `${region.name} (${region.sourceW}×${region.sourceH})`).join(", ");
    container.append(list);
  }

  // TrueType-style decoding: off-curve runs imply on-curve midpoints; every
  // segment is a line or one quadratic.
  function glyphPath(contours) {
    const commands = [];
    for (const raw of contours) {
      if (!raw.length) continue;
      const points = raw.map((point) => ({ ...point }));
      let startIndex = points.findIndex((point) => point.on);
      if (startIndex < 0) {
        points.unshift({
          x: (points[0].x + points[points.length - 1].x) / 2,
          y: (points[0].y + points[points.length - 1].y) / 2,
          on: 1,
        });
        startIndex = 0;
      }
      const ordered = [...points.slice(startIndex), ...points.slice(0, startIndex)];
      const start = ordered[0];
      commands.push(`M ${start.x} ${start.y}`);
      let control = null;
      for (let index = 1; index <= ordered.length; index += 1) {
        const point = ordered[index % ordered.length];
        if (point.on) {
          if (control) {
            commands.push(`Q ${control.x} ${control.y} ${point.x} ${point.y}`);
            control = null;
          } else {
            commands.push(`L ${point.x} ${point.y}`);
          }
        } else if (control) {
          const mid = { x: (control.x + point.x) / 2, y: (control.y + point.y) / 2 };
          commands.push(`Q ${control.x} ${control.y} ${mid.x} ${mid.y}`);
          control = point;
        } else {
          control = point;
        }
      }
      commands.push("Z");
    }
    return commands.join(" ");
  }

  function renderFontPreview(container, detail) {
    const font = detail.font;
    container.append(previewNote(
      `glyphs ${font.glyphCount} · units/em ${font.unitsPerEm} · ascent ${font.ascent} · descent ${font.descent}`,
    ));
    const charset = document.createElement("p");
    charset.className = "pack-font-charset";
    charset.textContent = font.charset;
    container.append(charset);

    const grid = document.createElement("div");
    grid.className = "pack-glyph-grid";
    const cell = font.unitsPerEm;
    for (const glyph of font.glyphs) {
      if (!glyph.contours || !glyph.contours.length) continue;
      const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
      svg.setAttribute("viewBox", `${-cell * 0.1} ${-font.ascent} ${cell * 1.2} ${font.ascent - font.descent}`);
      svg.classList.add("pack-glyph");
      const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
      path.setAttribute("d", glyphPath(glyph.contours));
      path.setAttribute("transform", "scale(1,-1)");
      path.setAttribute("fill", "currentColor");
      path.setAttribute("fill-rule", "nonzero");
      svg.append(path);
      svg.setAttribute("title", `U+${glyph.codepoint.toString(16).toUpperCase()}`);
      grid.append(svg);
    }
    container.append(grid);
  }

  async function renderPreview(container, entry) {
    container.textContent = "";
    container.append(previewNote("Loading preview…"));
    try {
      const detail = await fetchEntryDetail(entry.index);
      container.textContent = "";
      if (detail.kind === "texture" && detail.texture) await renderTexturePreview(container, entry.index, detail);
      else if (detail.kind === "atlas" && detail.atlas) await renderAtlasPreview(container, detail);
      else if (detail.kind === "font" && detail.font) renderFontPreview(container, detail);
      else {
        container.append(previewNote(
          `${entry.typeTag} · offset ${entry.offset} · ${entry.size} bytes · format v${entry.formatVersion} · no visual preview`,
        ));
      }
    } catch (error) {
      container.textContent = "";
      container.append(previewNote(`Preview failed: ${error?.message || error}`));
    }
  }

  function attachPreviewToggle(tr, entry) {
    tr.classList.add("pack-row-clickable");
    let detailRow = null;
    tr.addEventListener("click", () => {
      if (detailRow) {
        detailRow.remove();
        detailRow = null;
        return;
      }
      detailRow = document.createElement("tr");
      const td = document.createElement("td");
      td.colSpan = 6;
      const container = document.createElement("div");
      container.className = "pack-preview";
      td.append(container);
      detailRow.append(td);
      tr.after(detailRow);
      renderPreview(container, entry);
    });
  }

  function render(dump) {
    loadedDump = dump;
    byId("packTitle").textContent = dump.pack.rel;
    const gzTotal = (dump.entries || []).reduce((sum, entry) => sum + (entry.gzBytes || 0), 0);
    byId("packSubtitle").textContent =
      `${dump.game.id} · ${formatBytes(dump.pack.bytes)} (gz ~${formatBytes(gzTotal)}) · ` +
      `${dump.header ? dump.header.assetCount : 0} assets · pack v${dump.header ? dump.header.version : "?"}`;
    if (dump.error) return fail(dump.error);
    byId("packSummaryNote").textContent = dump.pack.namesFrom
      ? `Names resolved from ${dump.pack.namesFrom}.`
      : "No generated name header found; names shown as hashes.";

    const summaryBox = byId("packSummary");
    summaryBox.textContent = "";
    for (const [type, row] of Object.entries(dump.summary || {})) {
      const chip = document.createElement("span");
      chip.className = "pack-chip";
      const dup = row.dupCount ? ` +${row.dupCount} dup` : "";
      chip.textContent = `${type}: ${row.count} (${formatBytes(row.bytes)}${dup})`;
      summaryBox.append(chip);
    }

    const tbody = byId("packTable").querySelector("tbody");
    tbody.textContent = "";
    for (const entry of dump.entries || []) {
      const tr = document.createElement("tr");
      const notes = [];
      if (entry.dupOfIndex != null) notes.push(`dup of #${entry.dupOfIndex}`);
      if (!entry.inBounds) notes.push("OUT OF BOUNDS");
      tr.append(
        cell(String(entry.index)),
        cell(entry.name),
        cell(entry.typeTag),
        cell(formatBytes(entry.size)),
        cell(entry.gzBytes != null ? formatBytes(entry.gzBytes) : ""),
        cell(notes.join(", ")),
      );
      if (entry.dupOfIndex != null) tr.className = "pack-row-dup";
      attachPreviewToggle(tr, entry);
      tbody.append(tr);
    }
    const openIndex = new URLSearchParams(location.search).get("open");
    if (openIndex != null && tbody.children[openIndex]) {
      tbody.children[openIndex].click();
      tbody.children[openIndex].scrollIntoView({ block: "start" });
    }
  }

  async function load() {
    if (!gameId || !packPath) return fail("Missing game id or pack path in the URL.");
    const query = `game=${encodeURIComponent(gameId)}&path=${encodeURIComponent(packPath)}`;
    const response = await fetch(`/api/game-page/pack?${query}`, { cache: "no-store" });
    if (!response.ok) return fail(`Pack request failed: ${response.status}`);
    render(await response.json());
  }

  load().catch((error) => fail(error?.message || String(error)));
})();
