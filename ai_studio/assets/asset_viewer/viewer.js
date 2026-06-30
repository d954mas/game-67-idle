/* Asset viewer SPA — reads window.__VIEWER__ = {assets, packs, opts}.
   Static, vanilla. Grids use ONLY <img> thumbnails; exactly one live
   <model-viewer> exists at a time (in the asset modal). State lives in the URL
   hash so it survives reload and shares over a tunnel. */
(() => {
  let V = window.__VIEWER__ || { assets: [], packs: [], opts: {} };
  let DATA = V.assets || [];
  let PACKS = V.packs || [];
  let OPTS = V.opts || {};
  let REVIEW = !!OPTS.review;
  let byId = new Map(DATA.map((a) => [a.id, a]));
  let packById = new Map(PACKS.map((p) => [p.pack, p]));
  const DYNAMIC = !!window.__ASSET_VIEWER_DYNAMIC__;
  const OC = { mine: "#7dd3fc", ai: "#f0abfc", sourced: "#86efac", unknown: "#9ca3af" };
  const picked = new Set();
  const $ = (id) => document.getElementById(id);
  const ROOT = document.getElementById("assetViewerRoot") || document.body;

  const ASSET_FACETS = [["kind", "Kind"], ["origin", "Origin"], ["license", "License"], ["source", "Source"], ["pack", "Pack"], ["genre", "Genre"], ["tags", "Tags"]];
  const PACK_FACETS = [["kind", "Kind"], ["source", "Source"], ["license", "License"], ["genre", "Genre"], ["style", "Style"], ["tags", "Tags"]];
  const RENDER_STEP = 240;

  const esc = (s) => String(s == null ? "" : s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  const vals = (it, key) => { const v = it[key]; return Array.isArray(v) ? v.filter(Boolean) : (v ? [v] : []); };
  const icon = (k) => (k === "model" ? "◫" : k === "font" ? "A" : k === "audio" ? "♪" : "▦");

  let active = {};
  let q = "";
  let sort = "name";
  let sources = [];
  let selectedSourceId = "";
  let allAssetsLoaded = false;
  let renderCap = RENDER_STEP;
  let lastRouteKey = "";
  let loadedAssetsKey = "";
  let lazyTotal = 0;
  let lazyFacets = null;
  const loadedPacks = new Set();
  const loadingAssetScopes = new Map();
  const modelLoads = new Map();

  // synthetic "no pack" bundle so loose assets aren't hidden from the Packs view
  const LOOSE = "__loose__";
  function loosePack() {
    const loose = DATA.filter((c) => !c.pack);
    if (!loose.length) return null;
    const uniq = (k) => [...new Set(loose.flatMap((c) => vals(c, k)))];
    return { pack: LOOSE, title: "Loose assets (no pack)", source: "various", kind: uniq("kind"), license: uniq("license"), origin: uniq("origin"), genre: [], style: [], tags: uniq("tags"), count: loose.length, covers: loose.map((c) => c.thumb).filter(Boolean).slice(0, 4), coverImg: "" };
  }
  function allPacks() { const lp = loosePack(); return lp ? [...PACKS, lp] : PACKS; }

  function route() {
    const h = (location.hash || "").replace(/^#\/?/, "");
    const [view, ...rest] = h.split("/");
    const arg = decodeURIComponent(rest.join("/") || "");
    if (view === "pack" && arg) return { view: "pack", arg };
    if (view === "asset" && arg) return { view: "asset", arg };
    if (view === "all") return { view: "all" };
    return { view: PACKS.length ? "packs" : "all" };
  }
  const go = (h) => { location.hash = h; };

  async function readJson(path, options) {
    const response = await fetch(path, { cache: "no-store", ...options });
    const body = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(body.error || `${path} returned ${response.status}`);
    return body;
  }

  function applyViewer(nextViewer) {
    V = nextViewer || { assets: [], packs: [], opts: {} };
    DATA = V.assets || [];
    PACKS = V.packs || [];
    OPTS = V.opts || {};
    REVIEW = !!OPTS.review;
    byId = new Map(DATA.map((a) => [a.id, a]));
    packById = new Map(PACKS.map((p) => [p.pack, p]));
    active = {};
    q = "";
    picked.clear();
    allAssetsLoaded = !OPTS.assetsLazy;
    resetRenderCap();
    lastRouteKey = "";
    loadedAssetsKey = "";
    lazyTotal = 0;
    lazyFacets = null;
    loadedPacks.clear();
    loadingAssetScopes.clear();
    modelLoads.clear();
  }

  function appendAssets(assets) {
    const seen = new Set(DATA.map((asset) => asset.id));
    for (const asset of assets || []) {
      if (seen.has(asset.id)) continue;
      DATA.push(asset);
      byId.set(asset.id, asset);
      seen.add(asset.id);
    }
  }

  function assetQueryKey(pack = "") {
    const filters = Object.keys(active).sort().map((key) => key + "=" + [...active[key]].sort().join("|")).join("&");
    return [OPTS.sourceId || "", pack, q, sort, filters].join("::");
  }

  async function loadAssetsForScope(pack = "", append = false) {
    if (!DYNAMIC || !OPTS.assetsLazy || !OPTS.sourceId) return;
    const key = assetQueryKey(pack);
    const scope = key + (append ? "::append" : "");
    if (!append && loadedAssetsKey === key) return;
    if (append && DATA.length >= lazyTotal) return;
    if (loadingAssetScopes.has(scope)) return loadingAssetScopes.get(scope);
    const loadPromise = (async () => {
      const queryParams = new URLSearchParams({
        sourceId: OPTS.sourceId,
        offset: String(append ? DATA.length : 0),
        limit: String(RENDER_STEP),
        q,
        sort,
      });
      if (pack) queryParams.set("pack", pack);
      for (const filterKey of Object.keys(active)) {
        for (const value of active[filterKey]) queryParams.append(`filter.${filterKey}`, value);
      }
      const payload = await readJson(`/api/asset-viewer/assets?${queryParams.toString()}`);
      if (append) appendAssets(payload.assets || []);
      else {
        DATA = payload.assets || [];
        byId = new Map(DATA.map((a) => [a.id, a]));
      }
      loadedAssetsKey = key;
      lazyTotal = Number(payload.total || DATA.length);
      lazyFacets = payload.facets || null;
      allAssetsLoaded = DATA.length >= lazyTotal;
    })();
    loadingAssetScopes.set(scope, loadPromise);
    try {
      return await loadPromise;
    } finally {
      if (loadingAssetScopes.get(scope) === loadPromise) loadingAssetScopes.delete(scope);
    }
  }

  function selectedSourceBody(source) {
    if (source && source.custom) return { type: "game", path: source.path };
    return { sourceId: selectedSourceId };
  }

  function updateSourceQuery(sourceId) {
    const url = new URL(location.href);
    url.searchParams.set("source", sourceId);
    history.replaceState(null, "", url.pathname + url.search + location.hash);
  }

  async function loadSource(sourceId) {
    const source = sources.find((item) => item.id === sourceId);
    if (!source || !source.available) return;
    selectedSourceId = source.id;
    setViewerBusy(`Loading ${source.label}...`);
    const payload = await readJson("/api/asset-viewer/open", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(selectedSourceBody(source)),
    });
    applyViewer(payload.viewer);
    selectedSourceId = payload.source.id;
    updateSourceQuery(selectedSourceId);
    shell();
    const nextHash = PACKS.length ? "#/packs" : "#/all";
    if (location.hash === nextHash) render();
    else go(nextHash);
  }

  async function reindexSelectedSource() {
    const source = sources.find((item) => item.id === selectedSourceId);
    const button = $("refreshSource");
    if (!source || !source.available) return;
    const currentHash = location.hash || "";
    if (button) button.disabled = true;
    setViewerBusy(source.mode === "library" ? `Reindexing ${source.label}...` : `Refreshing ${source.label}...`);
    try {
      const payload = await readJson("/api/asset-viewer/refresh", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(selectedSourceBody(source)),
      });
      if (payload.refresh && payload.refresh.mode === "index") {
        setViewerBusy(`Indexed ${payload.refresh.assetCount} assets.`);
      } else {
        setViewerBusy(`Refreshed ${source.label}.`);
      }
      await loadSource(source.id);
      if (currentHash) go(currentHash);
    } catch (error) {
      setViewerBusy(error.message);
    } finally {
      if (button) button.disabled = false;
    }
  }

  async function refreshSelectedPreviews() {
    const source = sources.find((item) => item.id === selectedSourceId);
    const button = $("refreshPreviews");
    if (!source || !source.available) return;
    const currentHash = location.hash || "";
    if (button) button.disabled = true;
    setViewerBusy(`Refreshing previews for ${source.label}...`);
    try {
      const payload = await readJson("/api/asset-viewer/previews/refresh", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(selectedSourceBody(source)),
      });
      const previews = payload.previews || {};
      const details = [
        previews.copiedImages ? `${previews.copiedImages} images` : "",
        previews.renderedModels ? `${previews.renderedModels} models` : "",
        previews.skippedModels ? `${previews.skippedModels} skipped` : "",
      ].filter(Boolean).join(", ");
      const warning = previews.warning ? ` ${previews.warning}` : "";
      setViewerBusy(details ? `Preview refresh: ${details}.${warning}` : `Previews are current.${warning}`);
      await loadSource(source.id);
      if (currentHash) go(currentHash);
    } catch (error) {
      setViewerBusy(error.message);
    } finally {
      if (button) button.disabled = false;
    }
  }

  function setViewerBusy(text) {
    const status = $("sourceStatus");
    if (status) status.textContent = text;
  }

  function searchPass(it) {
    if (!q) return true;
    const hay = [it.name, (it.tags || []).join(" "), (it.genre || []).join(" "), (it.style || []).join(" "), it.pack, it.source, it.id, it.description, it.title].join(" ").toLowerCase();
    return hay.includes(q);
  }
  function passes(it, facets, exceptKey) {
    for (const [key] of facets) {
      if (key === exceptKey) continue;
      const sel = active[key];
      if (sel && sel.size) { const iv = vals(it, key); if (!iv.some((v) => sel.has(v))) return false; }
    }
    return true;
  }
  function filterList(list, facets) { return list.filter((it) => passes(it, facets, null) && searchPass(it)); }
  function countsFor(list, facets, key) {
    const base = list.filter((it) => passes(it, facets, key) && searchPass(it));
    const m = new Map();
    for (const it of base) for (const v of vals(it, key)) m.set(v, (m.get(v) || 0) + 1);
    return m;
  }
  function sortList(list, kind) {
    const a = list.slice();
    if (sort === "name") a.sort((x, y) => (x.name || x.pack || "").localeCompare(y.name || y.pack || ""));
    else if (sort === "count" && kind === "packs") a.sort((x, y) => (y.count || 0) - (x.count || 0));
    else if (sort === "origin") a.sort((x, y) => (x.origin || "").localeCompare(y.origin || ""));
    else if (sort === "random") a.sort((x, y) => (x.id || x.pack).localeCompare(y.id || y.pack) * (Math.random() < 0.5 ? -1 : 1));
    return a;
  }

  // inner cover content only (no .thumb wrapper): vendor cover image, else a
  // montage of member thumbnails, else a placeholder.
  function coverInner(p) {
    if (p.coverImg) return '<img loading="lazy" decoding="async" src="' + esc(p.coverImg) + '">';
    const c = p.covers || [];
    if (!c.length) return '<span class="ph">▦</span>';
    if (c.length === 1) return '<img loading="lazy" decoding="async" src="' + esc(c[0]) + '">';
    return '<div class="montage">' + c.slice(0, 4).map((x) => '<img loading="lazy" decoding="async" src="' + esc(x) + '">').join("") + "</div>";
  }
  function packCard(p) {
    const g = (p.genre || []).concat(p.style || []).slice(0, 3).map((x) => '<span class="gchip" data-facet="genre" data-val="' + esc(x) + '">' + esc(x) + "</span>").join("");
    return '<div class="card" data-pack="' + esc(p.pack) + '"><div class="thumb">' + coverInner(p) + "</div>" +
      '<div class="meta"><div class="name">' + esc(p.title || p.pack) + '</div>' +
      '<div class="row"><span class="k">' + (p.count || 0) + " assets · " + esc(p.source) + "</span></div>" +
      '<div class="row">' + (p.license ? '<span class="chip" style="background:#86efac">' + esc(p.license) + "</span>" : "") + g + "</div>" +
      "</div></div>";
  }
  function packHref(pack) {
    return "#/pack/" + encodeURIComponent(pack);
  }
  function packChips(a) {
    const packs = a.packs && a.packs.length ? a.packs : (a.pack ? [a.pack] : []);
    if (!packs.length) return "";
    return '<div class="packchips" aria-label="Asset pack memberships">' + packs.map((pack) => {
      const primary = pack === a.primaryPack;
      const current = pack === a.pack;
      const title = current ? "Current pack" : (primary ? "Primary pack" : "Also in pack");
      return '<a class="packchip' + (primary ? " primary" : "") + (current ? " current" : "") + '" data-pack-link="' + esc(pack) + '" title="' + title + ': ' + esc(pack) + '" href="' + packHref(pack) + '">' + esc(pack) + "</a>";
    }).join("") + "</div>";
  }
  function assetCard(a) {
    const t = a.thumb ? '<div class="thumb"><img loading="lazy" decoding="async" src="' + esc(a.thumb) + '"></div>' : '<div class="thumb"><span class="ph">' + icon(a.kind) + "</span></div>";
    let foot = "";
    if (REVIEW) foot = '<label class="pick" onclick="event.stopPropagation()"><input type="checkbox" data-pick="' + esc(a.id) + '"' + (picked.has(a.id) ? " checked" : "") + "> keep</label>";
    return '<div class="card' + (picked.has(a.id) ? " sel" : "") + '" data-asset="' + esc(a.id) + '">' + t +
      '<div class="meta"><div class="name">' + esc(a.name) + '</div><div class="row">' +
      '<span class="chip" style="background:' + (OC[a.origin] || OC.unknown) + '">' + esc(a.origin) + "</span>" +
      (a.sourceId ? '<span class="k" title="' + esc(a.sourceId) + '">↗ linked</span>' : (REVIEW ? '<span class="k" style="color:#e0c060">new</span>' : "")) +
      '<span class="k">' + esc(a.kind) + "</span></div>" + packChips(a) + "</div>" + foot + "</div>";
  }

  const FACET_CAP = 40; // high-cardinality facets (tags) get capped to the top-N by count
  function facetSidebar(list, facets) {
    let html = '<button class="ghost clear" id="clearF">Clear all filters</button>';
    for (const [key, label] of facets) {
      const m = countsFor(list, facets, key);
      if (!m.size) continue;
      let opts = [...m.entries()].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]));
      const total = opts.length;
      if (total > FACET_CAP) {
        const sel = active[key] || new Set();
        opts = opts.slice(0, FACET_CAP).concat(opts.slice(FACET_CAP).filter(([v]) => sel.has(v)));
      }
      html += '<div class="fgroup"><h4>' + esc(label) + "</h4>";
      for (const [val, c] of opts) {
        const on = active[key] && active[key].has(val);
        html += '<label class="fopt"><input type="checkbox" data-fk="' + esc(key) + '" data-fv="' + esc(val) + '"' + (on ? " checked" : "") + ">" +
          "<span>" + esc(val) + '</span><span class="c">' + c + "</span></label>";
      }
      if (total > FACET_CAP) html += '<div class="more">+' + (total - FACET_CAP) + " more — use search</div>";
      html += "</div>";
    }
    return html;
  }
  function activeBar() {
    const chips = [];
    for (const k in active) for (const v of active[k]) chips.push('<span class="ac" data-fk="' + esc(k) + '" data-fv="' + esc(v) + '">' + esc(k) + ": " + esc(v) + " ✕</span>");
    if (q) chips.push('<span class="ac" data-clearq="1">search: ' + esc(q) + " ✕</span>");
    return chips.length ? '<div class="active-bar">' + chips.join("") + "</div>" : "";
  }

  function closeModal() { const m = $("modal"); m.classList.remove("on"); m.innerHTML = ""; }
  function openAsset(id) {
    const a = byId.get(id);
    const m = $("modal");
    if (!a) { closeModal(); return; }
    let view3d;
    if (a.model) view3d = '<model-viewer camera-controls auto-rotate environment-image="studio_env.hdr" tone-mapping="neutral" exposure="1" shadow-intensity="1" shadow-softness="0.8" camera-orbit="45deg 55deg auto" poster="' + esc(a.thumb || "") + '" src="' + esc(a.model) + '"></model-viewer>';
    else if (a.thumb) view3d = '<img src="' + esc(a.thumb) + '">';
    else view3d = '<div class="ph" style="font-size:60px">' + icon(a.kind) + "</div>";
    const kv = (k, v) => v ? '<div class="kv"><b>' + esc(k) + ":</b> " + v + "</div>" : "";
    const ins = "<h3>" + esc(a.name) + "</h3>" +
      kv("kind", esc(a.kind)) +
      kv("origin", '<span class="chip" style="background:' + (OC[a.origin] || OC.unknown) + '">' + esc(a.origin) + "</span>") +
      kv("status", a.sourceId ? "↗ linked — " + esc(a.sourceId) : (REVIEW ? "new (game-local)" : "")) +
      kv("license", esc(a.license)) +
      kv("source", esc(a.source)) +
      kv("pack", a.pack ? '<a href="' + packHref(a.pack) + '">' + esc(a.pack) + "</a>" : "") +
      kv("packs", (a.packs || []).length > 1 ? (a.packs || []).map((pack) => '<a href="' + packHref(pack) + '">' + esc(pack) + "</a>").join(", ") : "") +
      kv("tags", (a.tags || []).map(esc).join(", ")) +
      kv("path", esc(a.relpath || a.id)) +
      '<div class="row" style="margin-top:10px"><button id="copyId">copy id</button>' +
      (REVIEW ? '<button class="ghost" id="pickBtn">' + (picked.has(a.id) ? "unpick" : "pick") + "</button>" : "") + "</div>";
    m.innerHTML = '<button class="close ghost" id="closeM">✕ close</button><div class="sheet"><div class="view3d">' + view3d + '</div><div class="ins">' + ins + "</div></div>";
    m.classList.add("on");
    $("closeM").onclick = () => history.back();
    m.onclick = (e) => { if (e.target === m) history.back(); };
    const ci = $("copyId"); if (ci) ci.onclick = () => navigator.clipboard && navigator.clipboard.writeText(a.id);
    const pb = $("pickBtn"); if (pb) pb.onclick = () => { picked.has(a.id) ? picked.delete(a.id) : picked.add(a.id); syncSel(); pb.textContent = picked.has(a.id) ? "unpick" : "pick"; };
    if (DYNAMIC && a.kind === "model" && !a.model && OPTS.sourceId) loadModelForAsset(a);
  }

  async function ensureModelForAsset(asset) {
    if (!asset || asset.kind !== "model") return "";
    if (asset.model) return asset.model;
    if (!DYNAMIC || !OPTS.sourceId) return "";
    if (!modelLoads.has(asset.id)) {
      modelLoads.set(asset.id, readJson(`/api/asset-viewer/model?sourceId=${encodeURIComponent(OPTS.sourceId)}&id=${encodeURIComponent(asset.id)}`)
        .then((payload) => {
          asset.model = payload.model || "";
          return asset.model;
        })
        .catch(() => ""));
    }
    return modelLoads.get(asset.id);
  }

  async function loadModelForAsset(asset) {
    const view = document.querySelector("#modal .view3d");
    if (!view) return;
    const model = await ensureModelForAsset(asset);
    if (!model || !document.querySelector("#modal .view3d")) return;
    view.innerHTML = '<model-viewer camera-controls auto-rotate environment-image="studio_env.hdr" tone-mapping="neutral" exposure="1" shadow-intensity="1" shadow-softness="0.8" camera-orbit="45deg 55deg auto" poster="' + esc(asset.thumb || "") + '" src="' + esc(model) + '"></model-viewer>';
  }

  // hover preview: ONE pooled live model-viewer follows the hovered model card,
  // so you can spin a model without opening it (still a single GL context).
  let hov = null, hovTimer = 0, hideTimer = 0, hoverRequest = 0;
  // isometric camera + the SAME shared studio HDR the PNG thumbnails are baked
  // with, so the live 3D and the preview are lit by one source (high warm key,
  // low ambient → side faces stay darker than the top, edges readable).
  const ISO = { "environment-image": "studio_env.hdr", "tone-mapping": "neutral", exposure: "1", "shadow-intensity": "0.6", "shadow-softness": "0.8", "camera-orbit": "45deg 55deg auto" };
  function ensureHov() {
    if (hov) return hov;
    hov = document.createElement("model-viewer");
    hov.setAttribute("camera-controls", "");
    hov.setAttribute("auto-rotate", "");
    hov.setAttribute("auto-rotate-delay", "0");
    hov.setAttribute("interaction-prompt", "none");
    for (const k in ISO) hov.setAttribute(k, ISO[k]);
    hov.style.cssText = "position:fixed;z-index:30;display:none;background:#0b0d12;border:1px solid #3b4555;border-radius:10px;pointer-events:auto;cursor:grab";
    hov.addEventListener("mouseenter", () => clearTimeout(hideTimer));
    hov.addEventListener("mouseleave", hideHover);
    // plain click (no drag) opens the modal; press + move rotates (model-viewer).
    let down = null;
    hov.addEventListener("pointerdown", (e) => { down = { x: e.clientX, y: e.clientY }; });
    hov.addEventListener("pointerup", (e) => { if (down && Math.hypot(e.clientX - down.x, e.clientY - down.y) < 6 && hov.dataset.aid) go("#/asset/" + encodeURIComponent(hov.dataset.aid)); down = null; });
    document.body.appendChild(hov);
    return hov;
  }
  function showHover(card, a) {
    const t = card.querySelector(".thumb"); if (!t) return;
    const r = t.getBoundingClientRect();
    const h = ensureHov();
    h.dataset.aid = a.id;
    h.style.left = r.left + "px"; h.style.top = r.top + "px"; h.style.width = r.width + "px"; h.style.height = r.height + "px";
    h.setAttribute("src", a.model); h.style.display = "block";
  }
  async function showHoverModel(card, asset, requestId) {
    const model = await ensureModelForAsset(asset);
    if (!model || requestId !== hoverRequest || !card.isConnected) return;
    showHover(card, asset);
  }
  function hideHover() { if (hov) { hov.style.display = "none"; hov.removeAttribute("src"); } }

  function syncSel() { const t = $("selText"); if (t) t.value = [...picked].join("\n"); }
  function resetRenderCap() { renderCap = RENDER_STEP; }
  function routeKey(routeState) { return routeState.view + ":" + (routeState.arg || ""); }
  function limitList(list) { return list.slice(0, renderCap); }
  function setCount(shown, total, rendered = shown) {
    const c = $("count");
    if (!c) return;
    c.textContent = rendered < shown ? rendered + " / " + shown + " shown (" + total + " total)" : shown + " / " + total;
  }
  function renderMore(listLength) {
    const more = $("gridMore");
    if (!more) return;
    if (listLength <= renderCap) {
      more.innerHTML = "";
      return;
    }
    const next = Math.min(RENDER_STEP, listLength - renderCap);
    more.innerHTML = '<button class="ghost" id="showMore">Show ' + next + " more</button>";
  }

  function lazyFacetSidebar(facetDefs) {
    if (!lazyFacets) return facetSidebar(DATA, facetDefs);
    let html = '<button class="ghost clear" id="clearF">Clear all filters</button>';
    for (const [key, label] of facetDefs) {
      const rows = lazyFacets[key] || [];
      if (!rows.length) continue;
      html += '<div class="fgroup"><h4>' + esc(label) + "</h4>";
      for (const row of rows.slice(0, FACET_CAP)) {
        const on = active[key] && active[key].has(row.value);
        html += '<label class="fopt"><input type="checkbox" data-fk="' + esc(key) + '" data-fv="' + esc(row.value) + '"' + (on ? " checked" : "") + ">" +
          "<span>" + esc(row.value) + '</span><span class="c">' + row.count + "</span></label>";
      }
      if (rows.length > FACET_CAP) html += '<div class="more">+' + (rows.length - FACET_CAP) + " more — use search</div>";
      html += "</div>";
    }
    return html;
  }

  function renderLazyMore(pack = "") {
    const more = $("gridMore");
    if (!more) return;
    if (!OPTS.assetsLazy || DATA.length >= lazyTotal) {
      more.innerHTML = "";
      return;
    }
    const next = Math.min(RENDER_STEP, lazyTotal - DATA.length);
    more.innerHTML = '<button class="ghost" id="showMore" data-pack="' + esc(pack) + '">Show ' + next + " more</button>";
  }
  function packSnippet(p) {
    const L = (a) => "[" + (a || []).join(", ") + "]";
    return "---\ntype: Asset Pack\ntitle: " + p.title + "\npack: " + p.pack + "\nsource: " + p.source +
      "\nkind: " + p.kind + "\nlicense: " + p.license + "\norigin: " + p.origin + "\ncount: " + p.count +
      "\ngenre: " + L(p.genre) + "\nstyle: " + L(p.style) + "\ntags: " + L(p.tags) +
      (p.cover ? "\ncover: " + p.cover : "") + (p.description ? "\ndescription: " + p.description : "") + "\n---\n\n" + (p.body || "");
  }

  function layoutHtml() { return '<div class="layout"><aside class="facets" id="facets"></aside><div><div id="abar"></div><div class="grid" id="grid"></div><div class="morebar" id="gridMore"></div></div></div>'; }

  function render() {
    hideHover();
    const r = route();
    const nextRouteKey = routeKey(r);
    if (nextRouteKey !== lastRouteKey) {
      resetRenderCap();
      lastRouteKey = nextRouteKey;
    }
    document.querySelectorAll(".tabs button").forEach((b) => b.classList.toggle("on", (b.dataset.tab === "packs" && (r.view === "packs" || r.view === "pack")) || (b.dataset.tab === "all" && r.view === "all")));
    if (r.view === "asset") { openAsset(r.arg); return; }
    if ($("modal").classList.contains("on")) closeModal();
    const app = $("app");

    if (r.view === "pack") {
      const isLoose = r.arg === LOOSE;
      const p = isLoose ? loosePack() : packById.get(r.arg);
      if (OPTS.assetsLazy && !isLoose && loadedAssetsKey !== assetQueryKey(r.arg)) {
        app.innerHTML = '<div class="empty-state">Loading pack assets...</div>';
        loadAssetsForScope(r.arg).then(render).catch((error) => {
          app.innerHTML = '<div class="empty-state error">' + esc(error.message) + "</div>";
        });
        return;
      }
      const members = isLoose ? DATA.filter((a) => !a.pack) : DATA.filter((a) => a.pack === r.arg);
      const list = OPTS.assetsLazy && !isLoose ? members : sortList(filterList(members, ASSET_FACETS), "assets");
      const visible = OPTS.assetsLazy && !isLoose ? list : limitList(list);
      const linkedCount = isLoose ? 0 : members.filter((a) => a.primaryPack && a.primaryPack !== r.arg && (a.packs || []).includes(r.arg)).length;
      const directCount = Math.max(0, members.length - linkedCount);
      const membershipLine = linkedCount ? '<div class="metaline">' + directCount + " primary / " + linkedCount + " linked from other packs</div>" : "";
      const head = p ? '<div class="detailhead"><div class="cover">' + coverInner(p) + "</div>" +
        '<div class="info"><h2>' + esc(p.title || p.pack) + "</h2>" +
        '<div class="metaline"><b>' + (p.count || members.length) + "</b> assets · <b>" + esc(p.source) + "</b> · " + esc(p.license) +
        (p.license_url ? ' (<a href="' + esc(p.license_url) + '">license</a>)' : "") + " · origin " + esc(p.origin) + "</div>" +
        membershipLine +
        ((p.genre || []).concat(p.style || []).length ? '<div class="row">' + (p.genre || []).concat(p.style || []).map((x) => '<span class="gchip" data-facet="genre" data-val="' + esc(x) + '">' + esc(x) + "</span>").join("") + "</div>" : "") +
        '<div class="desc">' + esc(p.description || p.body || "") + "</div>" +
        '<div class="row" style="margin-top:8px"><button class="ghost" id="viewPackRecord" type="button" aria-expanded="false">View pack record</button></div>' +
        '<div id="packRecordArea"></div></div></div>' : "";
      app.innerHTML = '<div class="crumbs"><a href="#/packs">All packs</a> › ' + esc(p ? p.title : r.arg) + "</div>" + head + layoutHtml();
      $("facets").innerHTML = OPTS.assetsLazy && !isLoose ? lazyFacetSidebar(ASSET_FACETS) : facetSidebar(members, ASSET_FACETS);
      $("abar").innerHTML = activeBar();
      $("grid").innerHTML = visible.map(assetCard).join("");
      if (OPTS.assetsLazy && !isLoose) renderLazyMore(r.arg);
      else renderMore(list.length);
      setCount(OPTS.assetsLazy && !isLoose ? lazyTotal : list.length, OPTS.assetsLazy && !isLoose ? lazyTotal : members.length, visible.length);
      const recordButton = $("viewPackRecord");
      const recordArea = $("packRecordArea");
      if (recordButton && recordArea && p) {
        recordButton.onclick = () => {
          const open = recordButton.getAttribute("aria-expanded") === "true";
          recordButton.setAttribute("aria-expanded", open ? "false" : "true");
          recordButton.textContent = open ? "View pack record" : "Hide pack record";
          recordArea.innerHTML = open ? "" : '<textarea readonly aria-label="Pack record">' + esc(packSnippet(p)) + "</textarea>";
        };
      }
      bindGrid();
      return;
    }

    if (r.view === "all") {
      if (OPTS.assetsLazy && loadedAssetsKey !== assetQueryKey("")) {
        app.innerHTML = '<div class="empty-state">Loading assets...</div>';
        loadAssetsForScope("").then(render).catch((error) => {
          app.innerHTML = '<div class="empty-state error">' + esc(error.message) + "</div>";
        });
        return;
      }
      const list = OPTS.assetsLazy ? DATA : sortList(filterList(DATA, ASSET_FACETS), "assets");
      const visible = OPTS.assetsLazy ? list : limitList(list);
      app.innerHTML = layoutHtml();
      $("facets").innerHTML = OPTS.assetsLazy ? lazyFacetSidebar(ASSET_FACETS) : facetSidebar(DATA, ASSET_FACETS);
      $("abar").innerHTML = activeBar();
      $("grid").innerHTML = visible.map(assetCard).join("");
      if (OPTS.assetsLazy) renderLazyMore("");
      else renderMore(list.length);
      setCount(OPTS.assetsLazy ? lazyTotal : list.length, OPTS.assetsLazy ? lazyTotal : DATA.length, visible.length);
      bindGrid();
      return;
    }

    const allP = allPacks();
    const list = sortList(filterList(allP, PACK_FACETS), "packs");
    const visible = limitList(list);
    app.innerHTML = layoutHtml();
    $("facets").innerHTML = facetSidebar(allP, PACK_FACETS);
    $("abar").innerHTML = activeBar();
    $("grid").innerHTML = visible.map(packCard).join("");
    renderMore(list.length);
    setCount(list.length, allP.length, visible.length);
    bindGrid();
  }

  function toggleFacet(k, v) { active[k] = active[k] || new Set(); active[k].has(v) ? active[k].delete(v) : active[k].add(v); if (!active[k].size) delete active[k]; resetRenderCap(); render(); }
  function bindGrid() {
    const grid = $("grid");
    if (grid) {
      grid.querySelectorAll("[data-pack]").forEach((el) => el.onclick = (e) => { if (e.target.classList.contains("gchip")) { toggleFacet(e.target.dataset.facet, e.target.dataset.val); return; } go("#/pack/" + encodeURIComponent(el.dataset.pack)); });
      grid.querySelectorAll("[data-pack-link]").forEach((el) => el.onclick = (e) => { e.stopPropagation(); });
      grid.querySelectorAll("[data-asset]").forEach((el) => {
        el.onclick = () => go("#/asset/" + encodeURIComponent(el.dataset.asset));
        const a = byId.get(el.dataset.asset);
        if (a && a.kind === "model") {
          el.addEventListener("mouseenter", () => {
            clearTimeout(hovTimer);
            clearTimeout(hideTimer);
            const requestId = ++hoverRequest;
            hovTimer = setTimeout(() => showHoverModel(el, a, requestId), 160);
          });
          el.addEventListener("mouseleave", () => {
            hoverRequest++;
            clearTimeout(hovTimer);
            hideTimer = setTimeout(hideHover, 140);
          });
        }
      });
      grid.querySelectorAll("[data-pick]").forEach((cb) => cb.onchange = () => { cb.checked ? picked.add(cb.dataset.pick) : picked.delete(cb.dataset.pick); syncSel(); cb.closest(".card").classList.toggle("sel", cb.checked); });
    }
    const facets = $("facets");
    if (facets) {
      facets.querySelectorAll("[data-fk]").forEach((cb) => cb.onchange = () => toggleFacet(cb.dataset.fk, cb.dataset.fv));
      const cl = $("clearF"); if (cl) cl.onclick = () => { active = {}; q = ""; resetRenderCap(); const s = $("q"); if (s) s.value = ""; render(); };
    }
    const ab = $("abar");
    if (ab) ab.querySelectorAll(".ac").forEach((c) => c.onclick = () => { resetRenderCap(); if (c.dataset.clearq) { q = ""; const s = $("q"); if (s) s.value = ""; render(); } else { if (active[c.dataset.fk]) { active[c.dataset.fk].delete(c.dataset.fv); if (!active[c.dataset.fk].size) delete active[c.dataset.fk]; } render(); } });
    document.querySelectorAll(".detailhead .gchip").forEach((g) => g.onclick = () => toggleFacet(g.dataset.facet, g.dataset.val));
    const showMore = $("showMore");
    if (showMore) showMore.onclick = () => {
      if (OPTS.assetsLazy) {
        showMore.disabled = true;
        loadAssetsForScope(showMore.dataset.pack || "", true).then(render).catch((error) => {
          showMore.textContent = error.message;
          showMore.disabled = false;
        });
      } else {
        renderCap += RENDER_STEP;
        render();
      }
    };
  }

  function shell() {
    const tabs = PACKS.length ? '<div class="tabs"><button data-tab="packs">Packs</button><button data-tab="all">All assets</button></div>' : "";
    const selectedSource = sources.find((source) => source.id === selectedSourceId);
    const refreshButton = DYNAMIC && selectedSource && selectedSource.available ? '<button class="ghost compact" id="refreshSource" type="button">Refresh</button>' : "";
    const previewButton = DYNAMIC && selectedSource && selectedSource.available ? '<button class="ghost compact" id="refreshPreviews" type="button">Refresh previews</button>' : "";
    const sourceOptions = (type, label) => {
      const items = sources.filter((source) => source.type === type);
      if (!items.length) return "";
      return '<optgroup label="' + esc(label) + '">' + items.map((source) => '<option value="' + esc(source.id) + '"' + (source.id === selectedSourceId ? " selected" : "") + (source.available ? "" : " disabled") + ">" + esc(source.label) + (source.available ? "" : " (missing)") + "</option>").join("") + "</optgroup>";
    };
    const sourceControl = DYNAMIC ? '<label class="sourcepick"><span>Source</span><select id="sourceSelect">' +
      sourceOptions("library", "Libraries") + sourceOptions("template", "Templates") + sourceOptions("game", "Games") +
      "</select></label>" + refreshButton + previewButton + '<span class="source-status" id="sourceStatus">' + esc(OPTS.sourcePath || "") + "</span>" : "";
    ROOT.innerHTML =
      '<header><h1>' + esc(OPTS.title || "Asset library") + "</h1>" + sourceControl + tabs +
      '<input type="search" id="q" placeholder="поиск… sofa, fantasy, kenney">' +
      '<select id="sort"><option value="name">Name</option><option value="count">Count</option><option value="origin">Origin</option><option value="random">Random</option></select>' +
      '<span class="count" id="count"></span></header><div id="app"></div>' +
      (REVIEW ? '<div class="selbox"><span class="k">picked ids (copy &amp; send to lead):</span><textarea id="selText" readonly></textarea><button id="copySel">copy</button></div>' : "") +
      '<div id="modal"></div>';
    const sourceSelect = $("sourceSelect");
    if (sourceSelect) {
      sourceSelect.onchange = async (event) => {
        sourceSelect.disabled = true;
        try {
          await loadSource(event.target.value);
        } catch (error) {
          setViewerBusy(error.message);
          sourceSelect.disabled = false;
        }
      };
    }
    const reindexButtonEl = $("refreshSource");
    if (reindexButtonEl) reindexButtonEl.onclick = reindexSelectedSource;
    const refreshPreviewsButtonEl = $("refreshPreviews");
    if (refreshPreviewsButtonEl) refreshPreviewsButtonEl.onclick = refreshSelectedPreviews;
    document.querySelectorAll(".tabs button").forEach((b) => b.onclick = () => go(b.dataset.tab === "all" ? "#/all" : "#/packs"));
    const qinput = $("q");
    if (qinput) qinput.value = q;
    let qt; qinput.oninput = () => { clearTimeout(qt); qt = setTimeout(() => { q = qinput.value.toLowerCase().trim(); resetRenderCap(); render(); }, 120); };
    $("sort").onchange = (e) => { sort = e.target.value; resetRenderCap(); render(); };
    if (REVIEW) $("copySel").onclick = () => { const t = $("selText"); t.select(); navigator.clipboard && navigator.clipboard.writeText(t.value); };
  }

  async function initDynamic() {
    ROOT.innerHTML = '<header><h1>Asset Viewer</h1><span class="source-status">Loading sources...</span></header><div class="empty-state">Loading asset sources.</div>';
    try {
      const payload = await readJson("/api/asset-viewer/sources");
      sources = payload.sources || [];
      const requested = new URL(location.href).searchParams.get("source");
      const source = sources.find((item) => item.id === requested && item.available) || sources.find((item) => item.available);
      if (!source) {
        shell();
        document.getElementById("app").innerHTML = '<div class="empty-state">No available asset sources.</div>';
        return;
      }
      selectedSourceId = source.id;
      await loadSource(source.id);
    } catch (error) {
      ROOT.innerHTML = '<header><h1>Asset Viewer</h1></header><div class="empty-state error">' + esc(error.message) + "</div>";
    }
  }

  window.addEventListener("hashchange", render);
  window.addEventListener("scroll", hideHover, true);
  if (DYNAMIC) initDynamic();
  else {
    shell();
    render();
  }
})();
