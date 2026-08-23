// Game page client: reads the game id from /game/<id> and renders each
// section from its /api/game-page endpoint. Shared row/format helpers come
// from studio_shell.js (window.studioShell).
(function () {
  const match = /^\/game\/([^/]+)\/?$/.exec(decodeURIComponent(location.pathname));
  const gameId = match ? match[1] : "";
  const { formatBytes, formatDate, linkRow } = window.studioShell;

  const byId = (id) => document.getElementById(id);
  const setText = (id, value) => {
    const node = byId(id);
    if (node) node.textContent = value;
  };

  function placeholder(text, error) {
    const p = document.createElement("p");
    p.className = `game-placeholder${error ? " game-error" : ""}`;
    p.textContent = text;
    return p;
  }

  function sectionError(id, message) {
    const body = byId(id);
    body.textContent = "";
    body.append(placeholder(message, true));
  }

  function fail(message) {
    setText("gameTitle", "Game not found");
    const links = byId("overviewLinks");
    if (links) {
      links.textContent = "";
      links.append(placeholder(message, true));
    }
  }

  function gameFileHref(rel) {
    return `/game-file/${encodeURIComponent(gameId)}/${String(rel).split("/").map(encodeURIComponent).join("/")}`;
  }

  function packHref(configName, rel) {
    const path = `build/${configName}/${rel}`;
    return `/game/${encodeURIComponent(gameId)}/pack?path=${encodeURIComponent(path)}`;
  }

  // #region Dashboard — the "understand at a glance" strip above the sections.
  const dashboardCards = new Map();

  function dashboardCard(id, label, value, href) {
    const strip = byId("gameDashboard");
    let card = dashboardCards.get(id);
    if (!card) {
      card = document.createElement(href ? "a" : "div");
      card.className = "game-dash-card";
      if (href) card.href = href;
      strip.append(card);
      dashboardCards.set(id, card);
    }
    card.textContent = "";
    const valueEl = document.createElement("strong");
    valueEl.textContent = value;
    const labelEl = document.createElement("span");
    labelEl.textContent = label;
    card.append(valueEl, labelEl);
  }
  // #endregion

  // #region Builds
  // Working configs vs agent/automation configs (devapi, capture, testbed,
  // profiling) — a name convention laid down by the template, not per game.
  const SERVICE_CONFIG = /devapi|capture|testbed|profile/i;

  function configKeyMetric(config) {
    if (config.web) {
      const gz = (config.binFiles || []).reduce((sum, row) => sum + (row.gzBytes || 0), 0);
      return gz ? `web gz ${formatBytes(gz)}` : "";
    }
    const exe = (config.binFiles || [])
      .filter((row) => /\/[^/.]+$|\.exe$/.test(row.rel) && !row.rel.includes("build_game_packs"))
      .sort((a, b) => b.bytes - a.bytes)[0];
    return exe ? `exe ${formatBytes(exe.bytes)}` : "";
  }

  function buildConfigRow(config) {
    const files = [...(config.packs || []), ...(config.binFiles || [])];
    const description = files
      .map((row) => {
        const gz = row.gzBytes != null ? ` (gz ${formatBytes(row.gzBytes)})` : "";
        return `${row.rel} — ${formatBytes(row.bytes)}${gz}`;
      })
      .join(" · ");
    const firstPack = (config.packs || [])[0];
    const metric = configKeyMetric(config);
    return linkRow(
      config.web ? "Web" : "Native",
      metric ? `${config.name} — ${metric}` : config.name,
      `${formatDate(config.freshnessMs)} · ${description}`,
      firstPack ? packHref(config.name, firstPack.rel) : "",
      "Packs",
    );
  }

  function releaseRow(artifact) {
    const missing = artifact.present ? "" : " · file missing";
    return linkRow(
      "Release",
      `${artifact.target || "release"} — ${formatBytes(artifact.bytes)}`,
      `${artifact.file || artifact.manifest} · ${formatDate(artifact.mtimeMs)}${missing}`,
      "",
      "",
    );
  }

  function collapsedBlock(label, rows) {
    const details = document.createElement("details");
    details.className = "game-collapsed";
    const summary = document.createElement("summary");
    summary.textContent = label;
    details.append(summary, ...rows);
    return details;
  }

  // The section answers exactly three questions: how big is the working debug
  // build, and how big are the assembled releases per target. Everything else
  // is one collapsed list.
  function renderBuilds(builds) {
    const body = byId("buildsBody");
    body.textContent = "";
    const configs = builds.configs || [];
    const workingDebug = configs.find((config) => !SERVICE_CONFIG.test(config.name) && /debug/i.test(config.name))
      || configs.find((config) => !SERVICE_CONFIG.test(config.name));
    if (workingDebug) {
      body.append(buildConfigRow(workingDebug));
      const metric = configKeyMetric(workingDebug);
      if (metric) dashboardCard("debug", `${workingDebug.name}`, metric.replace(/^exe |^web gz /, ""), "#sectionBuilds");
    }

    const release = builds.release || [];
    const latestByTarget = new Map();
    for (const artifact of release) {
      const target = artifact.target || "release";
      if (!latestByTarget.has(target)) latestByTarget.set(target, artifact);
    }
    for (const [target, artifact] of latestByTarget) {
      body.append(releaseRow(artifact));
      dashboardCard(`release:${target}`, target, formatBytes(artifact.bytes), "#sectionBuilds");
    }

    const rest = configs.filter((config) => config !== workingDebug);
    const history = release.filter((artifact) => ![...latestByTarget.values()].includes(artifact));
    if (rest.length || history.length) {
      body.append(collapsedBlock(
        `Все конфиги (${rest.length}) и история релизов (${history.length})`,
        [...rest.map(buildConfigRow), ...history.map(releaseRow)],
      ));
    }

    if (!body.childElementCount) {
      body.append(placeholder("No build configs or release artifacts found."));
    }
  }

  async function loadBuilds() {
    const response = await fetch(`/api/game-page/builds?game=${encodeURIComponent(gameId)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`builds request failed: ${response.status}`);
    renderBuilds(await response.json());
  }
  // #endregion

  // #region Balance — compact by default, the full Workbench on demand.
  function renderBalance(game) {
    const body = byId("balanceBody");
    body.textContent = "";
    body.append(linkRow(
      "Items",
      "Items Workbench",
      "Catalog tables, curves, charts, and semantic edits for this game.",
      `/items?catalog=${encodeURIComponent(game.storeId)}`,
      "Open",
    ));
    const details = collapsedBlock("Развернуть Workbench здесь", []);
    // The iframe loads only when opened — the collapsed page stays light.
    details.addEventListener("toggle", () => {
      if (!details.open || details.querySelector("iframe")) return;
      const frame = document.createElement("iframe");
      frame.className = "game-embed";
      frame.src = `/ai_studio/assets/items_viewer/site/items.html?catalog=${encodeURIComponent(game.storeId)}&embed=1`;
      frame.title = "Items Workbench";
      details.append(frame);
    }, { once: false });
    body.append(details);
  }
  // #endregion

  async function loadCanvases(game) {
    const response = await fetch(`/api/canvas/projects?store=${encodeURIComponent(game.storeId)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`canvas request failed: ${response.status}`);
    const payload = await response.json();
    const body = byId("canvasesBody");
    body.textContent = "";
    for (const project of payload.projects || []) {
      const row = linkRow(
        "Canvas",
        project.title || project.id,
        formatDate(project.updatedAt || project.updated || 0) || "canvas project",
        `/canvas?project=${encodeURIComponent(project.id)}&store=${encodeURIComponent(game.storeId)}`,
        "Open",
      );
      row.title = project.id;
      body.append(row);
    }
    dashboardCard("canvases", "canvases", String((payload.projects || []).length), "#sectionCanvases");
    if (!body.childElementCount) body.append(placeholder("No canvas projects for this game."));
  }

  // #region Save — the parsed player save, rendered as a collapsible tree.
  function jsonTree(key, value, depth) {
    if (value === null || typeof value !== "object") {
      const line = document.createElement("div");
      line.className = "save-line";
      const keyEl = document.createElement("span");
      keyEl.className = "save-key";
      keyEl.textContent = key;
      line.append(keyEl, document.createTextNode(` ${JSON.stringify(value)}`));
      return line;
    }
    const isArray = Array.isArray(value);
    const entries = isArray ? value.map((item, at) => [String(at), item]) : Object.entries(value);
    const details = document.createElement("details");
    details.className = "save-node";
    if (depth < 2) details.open = true;
    const summary = document.createElement("summary");
    summary.textContent = `${key} ${isArray ? `[${entries.length}]` : `{${entries.length}}`}`;
    details.append(summary);
    for (const [childKey, childValue] of entries) {
      details.append(jsonTree(childKey, childValue, depth + 1));
    }
    return details;
  }

  function saveSlotRow(payload, slot) {
    const row = document.createElement("article");
    row.className = "surface-row save-slot";
    const pill = document.createElement("span");
    pill.className = "type-pill state";
    pill.textContent = slot.slot.endsWith(".bak") ? "Backup" : "Save";
    const main = document.createElement("div");
    main.className = "surface-main";
    const h4 = document.createElement("h4");
    const meta = slot.meta || {};
    const seq = meta.saveSeq != null ? ` — seq ${meta.saveSeq}, v${meta.saveVersion}` : (slot.malformed ? " — malformed" : "");
    h4.textContent = `${slot.slot}${seq}`;
    const p = document.createElement("p");
    const savedAt = meta.savedAt ? ` · saved ${formatDate(meta.savedAt)}` : "";
    p.textContent = `${formatBytes(slot.bytes)} · ${formatDate(slot.mtimeMs)}${savedAt}`;
    main.append(h4, p);
    row.append(pill, main);

    const details = collapsedBlock("содержимое", []);
    details.classList.add("save-content");
    details.addEventListener("toggle", async () => {
      if (!details.open || details.childElementCount > 1) return;
      details.append(placeholder("Loading save…"));
      try {
        const response = await fetch(
          `/api/game-page/save?game=${encodeURIComponent(gameId)}&slot=${encodeURIComponent(slot.slot)}`,
          { cache: "no-store" },
        );
        if (!response.ok) throw new Error(`save request failed: ${response.status}`);
        const save = await response.json();
        details.querySelector(".game-placeholder")?.remove();
        if (save.error) details.append(placeholder(save.error, true));
        else details.append(jsonTree(slot.slot, save.content, 0));
      } catch (error) {
        details.querySelector(".game-placeholder")?.remove();
        details.append(placeholder(String(error?.message || error), true));
      }
    });

    const wrap = document.createElement("div");
    wrap.className = "save-slot-wrap";
    wrap.append(row, details);
    return wrap;
  }

  async function loadSave() {
    const body = byId("saveBody");
    const [savesResponse, stateResponse] = await Promise.all([
      fetch(`/api/game-page/saves?game=${encodeURIComponent(gameId)}`, { cache: "no-store" }),
      fetch(`/api/game-page/state?game=${encodeURIComponent(gameId)}`, { cache: "no-store" }),
    ]);
    if (!savesResponse.ok) throw new Error(`saves request failed: ${savesResponse.status}`);
    const payload = await savesResponse.json();
    body.textContent = "";
    (payload.slots || []).forEach((slot, index) => {
      const wrap = saveSlotRow(payload, slot);
      body.append(wrap);
      // The freshest real save opens by default — seeing the save IS the section.
      if (index === 0 && !slot.slot.endsWith(".bak")) {
        wrap.querySelector(".save-content").open = true;
      }
    });
    if (!(payload.slots || []).length) {
      body.append(placeholder(payload.savesRoot
        ? `No saves found in ${payload.savesRoot}.`
        : "No native save location available."));
    } else {
      const fresh = payload.slots[0];
      dashboardCard("save", "last save", formatDate(fresh.meta?.savedAt || fresh.mtimeMs).slice(0, 10) || "—", "#sectionState");
    }

    if (stateResponse.ok) {
      const state = await stateResponse.json();
      const schemas = state.schemas || [];
      if (schemas.length) {
        body.append(collapsedBlock(`Схемы состояния (${schemas.length})`, schemas.map((schema) => linkRow(
          "Schema",
          schema.rel.split("/").pop(),
          `${formatBytes(schema.bytes)} · ${formatDate(schema.mtimeMs)}`,
          gameFileHref(schema.rel),
          "View",
        ))));
      }
    }
  }
  // #endregion

  async function loadCaptures() {
    const response = await fetch(`/api/game-page/captures?game=${encodeURIComponent(gameId)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`captures request failed: ${response.status}`);
    const payload = await response.json();
    const body = byId("capturesBody");
    body.textContent = "";
    const sessions = payload.sessions || [];
    for (const session of sessions) {
      const row = document.createElement("article");
      row.className = "surface-row";
      if (session.previewRel) {
        const img = document.createElement("img");
        img.className = "capture-thumb";
        img.loading = "lazy";
        img.src = gameFileHref(session.previewRel);
        row.append(img);
      } else {
        const pill = document.createElement("span");
        pill.className = "type-pill state";
        pill.textContent = "Rec";
        row.append(pill);
      }
      const main = document.createElement("div");
      main.className = "surface-main";
      const h4 = document.createElement("h4");
      h4.textContent = session.label;
      const p = document.createElement("p");
      p.textContent = `${formatDate(session.mtimeMs)} · ${formatBytes(session.bytes)} · ${session.rel}`;
      main.append(h4, p);
      row.append(main);
      if (session.videoRel) {
        const link = document.createElement("a");
        link.className = "open-link";
        link.href = gameFileHref(session.videoRel);
        link.textContent = "Video";
        row.append(link);
      }
      body.append(row);
    }
    if (sessions.length) {
      dashboardCard("capture", "last capture", formatDate(sessions[0].mtimeMs).slice(0, 10), "#sectionCaptures");
    }
    if (payload.truncated) body.append(placeholder("List truncated to the newest 100 takes."));
    if (!body.childElementCount) body.append(placeholder("No captures recorded."));
  }

  async function load() {
    if (!gameId) return fail("No game id in the URL.");
    const response = await fetch(`/api/game-page/overview?game=${encodeURIComponent(gameId)}`, { cache: "no-store" });
    if (!response.ok) return fail(`Overview request failed: ${response.status}`);
    const overview = await response.json();
    const game = overview.game || {};
    document.title = `${game.title || game.id} — AI Studio`;
    setText("gameTitle", game.title || game.id);
    setText("gameId", game.id || "");
    setText("gameVisibility", game.visibility || "");
    setText("gameVersion", game.version ? `v${game.version}` : "");
    setText("gameRoot", game.root || "");

    const links = byId("overviewLinks");
    links.textContent = "";
    for (const doc of overview.designDocs || []) {
      links.append(linkRow("Docs", doc.label, doc.rel, gameFileHref(doc.rel), "View"));
    }
    for (const project of overview.taskboardProjects || []) {
      links.append(linkRow("Tasks", `${project.id} ${project.title}`, `Taskboard project (${project.status}, ${project.store})`, "/taskboard/", "Open"));
    }
    links.append(linkRow("Assets", "Asset Viewer", "Browse this game's asset folders.", `/asset_viewer/?source=${encodeURIComponent(game.storeId)}`, "Open"));
    if (!links.childElementCount) links.append(placeholder("Nothing to show yet."));

    renderBalance(game);
    await Promise.all([
      loadBuilds().catch((error) => sectionError("buildsBody", String(error?.message || error))),
      loadCanvases(game).catch((error) => sectionError("canvasesBody", String(error?.message || error))),
      loadSave().catch((error) => sectionError("saveBody", String(error?.message || error))),
      loadCaptures().catch((error) => sectionError("capturesBody", String(error?.message || error))),
    ]);
  }

  load().catch((error) => fail(error?.message || String(error)));
})();
