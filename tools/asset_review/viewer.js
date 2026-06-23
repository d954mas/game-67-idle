/* Asset viewer SPA — reads window.__VIEWER__ = {assets, packs, opts}.
   Static, vanilla. Grids use ONLY <img> thumbnails; exactly one live
   <model-viewer> exists at a time (in the asset modal). State lives in the URL
   hash so it survives reload and shares over a tunnel. */
(() => {
  const V = window.__VIEWER__ || { assets: [], packs: [], opts: {} };
  const DATA = V.assets || [];
  const PACKS = V.packs || [];
  const OPTS = V.opts || {};
  const REVIEW = !!OPTS.review;
  const byId = new Map(DATA.map((a) => [a.id, a]));
  const packById = new Map(PACKS.map((p) => [p.pack, p]));
  const OC = { mine: "#7dd3fc", ai: "#f0abfc", sourced: "#86efac", unknown: "#9ca3af" };
  const picked = new Set();
  const $ = (id) => document.getElementById(id);

  const ASSET_FACETS = [["kind", "Kind"], ["origin", "Origin"], ["license", "License"], ["source", "Source"], ["pack", "Pack"], ["genre", "Genre"], ["tags", "Tags"]];
  const PACK_FACETS = [["kind", "Kind"], ["source", "Source"], ["license", "License"], ["genre", "Genre"], ["style", "Style"], ["tags", "Tags"]];

  const esc = (s) => String(s == null ? "" : s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  const vals = (it, key) => { const v = it[key]; return Array.isArray(v) ? v.filter(Boolean) : (v ? [v] : []); };
  const icon = (k) => (k === "model" ? "◫" : k === "font" ? "A" : k === "audio" ? "♪" : "▦");

  let active = {};
  let q = "";
  let sort = "name";

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

  function montage(covers) {
    if (!covers || !covers.length) return '<div class="thumb"><span class="ph">▦</span></div>';
    if (covers.length === 1) return '<div class="thumb"><img loading="lazy" src="' + esc(covers[0]) + '"></div>';
    return '<div class="thumb"><div class="montage">' + covers.slice(0, 4).map((c) => '<img loading="lazy" src="' + esc(c) + '">').join("") + "</div></div>";
  }
  function packCard(p) {
    const g = (p.genre || []).concat(p.style || []).slice(0, 3).map((x) => '<span class="gchip" data-facet="genre" data-val="' + esc(x) + '">' + esc(x) + "</span>").join("");
    return '<div class="card" data-pack="' + esc(p.pack) + '">' + montage(p.covers) +
      '<div class="meta"><div class="name">' + esc(p.title || p.pack) + '</div>' +
      '<div class="row"><span class="k">' + (p.count || 0) + " assets · " + esc(p.source) + "</span></div>" +
      '<div class="row">' + (p.license ? '<span class="chip" style="background:#86efac">' + esc(p.license) + "</span>" : "") + g + "</div>" +
      "</div></div>";
  }
  function assetCard(a) {
    const t = a.thumb ? '<div class="thumb"><img loading="lazy" src="' + esc(a.thumb) + '"></div>' : '<div class="thumb"><span class="ph">' + icon(a.kind) + "</span></div>";
    let foot = "";
    if (REVIEW) foot = '<label class="pick" onclick="event.stopPropagation()"><input type="checkbox" data-pick="' + esc(a.id) + '"' + (picked.has(a.id) ? " checked" : "") + "> keep</label>";
    return '<div class="card' + (picked.has(a.id) ? " sel" : "") + '" data-asset="' + esc(a.id) + '">' + t +
      '<div class="meta"><div class="name">' + esc(a.name) + '</div><div class="row">' +
      '<span class="chip" style="background:' + (OC[a.origin] || OC.unknown) + '">' + esc(a.origin) + "</span>" +
      (a.sourceId ? '<span class="k" title="' + esc(a.sourceId) + '">↗ linked</span>' : (REVIEW ? '<span class="k" style="color:#e0c060">new</span>' : "")) +
      '<span class="k">' + esc(a.kind) + "</span></div></div>" + foot + "</div>";
  }

  function facetSidebar(list, facets) {
    let html = '<button class="ghost clear" id="clearF">Clear all filters</button>';
    for (const [key, label] of facets) {
      const m = countsFor(list, facets, key);
      if (!m.size) continue;
      const opts = [...m.entries()].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]));
      html += '<div class="fgroup"><h4>' + esc(label) + "</h4>";
      for (const [val, c] of opts) {
        const on = active[key] && active[key].has(val);
        html += '<label class="fopt"><input type="checkbox" data-fk="' + esc(key) + '" data-fv="' + esc(val) + '"' + (on ? " checked" : "") + ">" +
          "<span>" + esc(val) + '</span><span class="c">' + c + "</span></label>";
      }
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
    if (a.model) view3d = '<model-viewer camera-controls auto-rotate shadow-intensity="1" exposure="1.1" poster="' + esc(a.thumb || "") + '" src="' + esc(a.model) + '"></model-viewer>';
    else if (a.thumb) view3d = '<img src="' + esc(a.thumb) + '">';
    else view3d = '<div class="ph" style="font-size:60px">' + icon(a.kind) + "</div>";
    const kv = (k, v) => v ? '<div class="kv"><b>' + esc(k) + ":</b> " + v + "</div>" : "";
    const ins = "<h3>" + esc(a.name) + "</h3>" +
      kv("kind", esc(a.kind)) +
      kv("origin", '<span class="chip" style="background:' + (OC[a.origin] || OC.unknown) + '">' + esc(a.origin) + "</span>") +
      kv("status", a.sourceId ? "↗ linked — " + esc(a.sourceId) : (REVIEW ? "new (game-local)" : "")) +
      kv("license", esc(a.license)) +
      kv("source", esc(a.source)) +
      kv("pack", a.pack ? '<a href="#/pack/' + esc(a.pack) + '">' + esc(a.pack) + "</a>" : "") +
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
  }

  // hover preview: ONE pooled live model-viewer follows the hovered model card,
  // so you can spin a model without opening it (still a single GL context).
  let hov = null, hovTimer = 0;
  function ensureHov() {
    if (hov) return hov;
    hov = document.createElement("model-viewer");
    hov.setAttribute("camera-controls", "");
    hov.setAttribute("auto-rotate", "");
    hov.setAttribute("auto-rotate-delay", "0");
    hov.setAttribute("interaction-prompt", "none");
    hov.setAttribute("exposure", "1.1");
    hov.style.cssText = "position:fixed;z-index:30;display:none;background:#0b0d12;border:1px solid #3b4555;border-radius:10px;pointer-events:none";
    document.body.appendChild(hov);
    return hov;
  }
  function showHover(card, a) {
    const t = card.querySelector(".thumb"); if (!t) return;
    const r = t.getBoundingClientRect();
    const h = ensureHov();
    h.style.left = r.left + "px"; h.style.top = r.top + "px"; h.style.width = r.width + "px"; h.style.height = r.height + "px";
    h.setAttribute("src", a.model); h.style.display = "block";
  }
  function hideHover() { if (hov) { hov.style.display = "none"; hov.removeAttribute("src"); } }

  function syncSel() { const t = $("selText"); if (t) t.value = [...picked].join("\n"); }
  function setCount(shown, total) { const c = $("count"); if (c) c.textContent = shown + " / " + total; }
  function packSnippet(p) {
    const L = (a) => "[" + (a || []).join(", ") + "]";
    return "---\ntype: Asset Pack\ntitle: " + p.title + "\npack: " + p.pack + "\nsource: " + p.source +
      "\nkind: " + p.kind + "\nlicense: " + p.license + "\norigin: " + p.origin + "\ncount: " + p.count +
      "\ngenre: " + L(p.genre) + "\nstyle: " + L(p.style) + "\ntags: " + L(p.tags) +
      (p.cover ? "\ncover: " + p.cover : "") + (p.description ? "\ndescription: " + p.description : "") + "\n---\n\n" + (p.body || "");
  }

  function layoutHtml() { return '<div class="layout"><aside class="facets" id="facets"></aside><div><div id="abar"></div><div class="grid" id="grid"></div></div></div>'; }

  function render() {
    hideHover();
    const r = route();
    document.querySelectorAll(".tabs button").forEach((b) => b.classList.toggle("on", (b.dataset.tab === "packs" && (r.view === "packs" || r.view === "pack")) || (b.dataset.tab === "all" && r.view === "all")));
    if (r.view === "asset") { openAsset(r.arg); return; }
    if ($("modal").classList.contains("on")) closeModal();
    const app = $("app");

    if (r.view === "pack") {
      const p = packById.get(r.arg);
      const members = DATA.filter((a) => a.pack === r.arg);
      const list = sortList(filterList(members, ASSET_FACETS), "assets");
      const head = p ? '<div class="detailhead"><div class="cover">' + montage(p.covers) + "</div>" +
        '<div class="info"><h2>' + esc(p.title || p.pack) + "</h2>" +
        '<div class="metaline"><b>' + (p.count || members.length) + "</b> assets · <b>" + esc(p.source) + "</b> · " + esc(p.license) +
        (p.license_url ? ' (<a href="' + esc(p.license_url) + '">license</a>)' : "") + " · origin " + esc(p.origin) + "</div>" +
        ((p.genre || []).concat(p.style || []).length ? '<div class="row">' + (p.genre || []).concat(p.style || []).map((x) => '<span class="gchip" data-facet="genre" data-val="' + esc(x) + '">' + esc(x) + "</span>").join("") + "</div>" : "") +
        '<div class="desc">' + esc(p.description || p.body || "") + "</div>" +
        '<div class="row" style="margin-top:8px"><button class="ghost" id="editDesc">Edit description (copy _pack.md)</button></div>' +
        '<div id="editArea"></div></div></div>' : "";
      app.innerHTML = '<div class="crumbs"><a href="#/packs">All packs</a> › ' + esc(r.arg) + "</div>" + head + layoutHtml();
      $("facets").innerHTML = facetSidebar(members, ASSET_FACETS);
      $("abar").innerHTML = activeBar();
      $("grid").innerHTML = list.map(assetCard).join("");
      setCount(list.length, members.length);
      const ed = $("editDesc"); if (ed && p) ed.onclick = () => { $("editArea").innerHTML = '<textarea readonly>' + esc(packSnippet(p)) + "</textarea>"; };
      bindGrid();
      return;
    }

    if (r.view === "all") {
      const list = sortList(filterList(DATA, ASSET_FACETS), "assets");
      app.innerHTML = layoutHtml();
      $("facets").innerHTML = facetSidebar(DATA, ASSET_FACETS);
      $("abar").innerHTML = activeBar();
      $("grid").innerHTML = list.map(assetCard).join("");
      setCount(list.length, DATA.length);
      bindGrid();
      return;
    }

    const list = sortList(filterList(PACKS, PACK_FACETS), "packs");
    app.innerHTML = layoutHtml();
    $("facets").innerHTML = facetSidebar(PACKS, PACK_FACETS);
    $("abar").innerHTML = activeBar();
    $("grid").innerHTML = list.map(packCard).join("");
    setCount(list.length, PACKS.length);
    bindGrid();
  }

  function toggleFacet(k, v) { active[k] = active[k] || new Set(); active[k].has(v) ? active[k].delete(v) : active[k].add(v); if (!active[k].size) delete active[k]; render(); }
  function bindGrid() {
    const grid = $("grid");
    if (grid) {
      grid.querySelectorAll("[data-pack]").forEach((el) => el.onclick = (e) => { if (e.target.classList.contains("gchip")) { toggleFacet(e.target.dataset.facet, e.target.dataset.val); return; } go("#/pack/" + encodeURIComponent(el.dataset.pack)); });
      grid.querySelectorAll("[data-asset]").forEach((el) => {
        el.onclick = () => go("#/asset/" + encodeURIComponent(el.dataset.asset));
        const a = byId.get(el.dataset.asset);
        if (a && a.model) {
          el.addEventListener("mouseenter", () => { clearTimeout(hovTimer); hovTimer = setTimeout(() => showHover(el, a), 160); });
          el.addEventListener("mouseleave", () => { clearTimeout(hovTimer); hideHover(); });
        }
      });
      grid.querySelectorAll("[data-pick]").forEach((cb) => cb.onchange = () => { cb.checked ? picked.add(cb.dataset.pick) : picked.delete(cb.dataset.pick); syncSel(); cb.closest(".card").classList.toggle("sel", cb.checked); });
    }
    const facets = $("facets");
    if (facets) {
      facets.querySelectorAll("[data-fk]").forEach((cb) => cb.onchange = () => toggleFacet(cb.dataset.fk, cb.dataset.fv));
      const cl = $("clearF"); if (cl) cl.onclick = () => { active = {}; q = ""; const s = $("q"); if (s) s.value = ""; render(); };
    }
    const ab = $("abar");
    if (ab) ab.querySelectorAll(".ac").forEach((c) => c.onclick = () => { if (c.dataset.clearq) { q = ""; const s = $("q"); if (s) s.value = ""; render(); } else { if (active[c.dataset.fk]) { active[c.dataset.fk].delete(c.dataset.fv); if (!active[c.dataset.fk].size) delete active[c.dataset.fk]; } render(); } });
    document.querySelectorAll(".detailhead .gchip").forEach((g) => g.onclick = () => toggleFacet(g.dataset.facet, g.dataset.val));
  }

  function shell() {
    const tabs = PACKS.length ? '<div class="tabs"><button data-tab="packs">Packs</button><button data-tab="all">All assets</button></div>' : "";
    document.body.innerHTML =
      '<header><h1>' + esc(OPTS.title || "Asset library") + "</h1>" + tabs +
      '<input type="search" id="q" placeholder="поиск… sofa, fantasy, kenney">' +
      '<select id="sort"><option value="name">Name</option><option value="count">Count</option><option value="origin">Origin</option><option value="random">Random</option></select>' +
      '<span class="count" id="count"></span></header><div id="app"></div>' +
      (REVIEW ? '<div class="selbox"><span class="k">picked ids (copy &amp; send to lead):</span><textarea id="selText" readonly></textarea><button id="copySel">copy</button></div>' : "") +
      '<div id="modal"></div>';
    document.querySelectorAll(".tabs button").forEach((b) => b.onclick = () => go(b.dataset.tab === "all" ? "#/all" : "#/packs"));
    const qinput = $("q");
    let qt; qinput.oninput = () => { clearTimeout(qt); qt = setTimeout(() => { q = qinput.value.toLowerCase().trim(); render(); }, 120); };
    $("sort").onchange = (e) => { sort = e.target.value; render(); };
    if (REVIEW) $("copySel").onclick = () => { const t = $("selText"); t.select(); navigator.clipboard && navigator.clipboard.writeText(t.value); };
  }

  shell();
  window.addEventListener("hashchange", render);
  window.addEventListener("scroll", hideHover, true);
  render();
})();
