// Game page client: reads the game id from /game/<id> and renders overview data.
(function () {
  const match = /^\/game\/([^/]+)\/?$/.exec(decodeURIComponent(location.pathname));
  const gameId = match ? match[1] : "";

  const byId = (id) => document.getElementById(id);
  const setText = (id, value) => {
    const node = byId(id);
    if (node) node.textContent = value;
  };

  function fail(message) {
    setText("gameTitle", "Game not found");
    const links = byId("overviewLinks");
    if (links) links.innerHTML = `<p class="game-placeholder game-error"></p>`;
    if (links) links.firstChild.textContent = message;
  }

  function linkRow(pill, title, description, href, linkText) {
    const row = document.createElement("article");
    row.className = "surface-row";
    const pillSpan = document.createElement("span");
    pillSpan.className = "type-pill state";
    pillSpan.textContent = pill;
    const main = document.createElement("div");
    main.className = "surface-main";
    const h4 = document.createElement("h4");
    h4.textContent = title;
    const p = document.createElement("p");
    p.textContent = description;
    main.append(h4, p);
    row.append(pillSpan, main);
    if (href) {
      const a = document.createElement("a");
      a.className = "open-link";
      a.href = href;
      a.textContent = linkText || "Open";
      row.append(a);
    }
    return row;
  }

  function gameFileHref(gameId, rel) {
    return `/game-file/${encodeURIComponent(gameId)}/${String(rel).split("/").map(encodeURIComponent).join("/")}`;
  }

  function formatBytes(bytes) {
    if (bytes == null) return "";
    if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
    if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${bytes} B`;
  }

  function formatDate(ms) {
    return ms ? new Date(ms).toISOString().slice(0, 16).replace("T", " ") : "";
  }

  function packHref(gameId, configName, rel) {
    const path = `build/${configName}/${rel}`;
    return `/game/${encodeURIComponent(gameId)}/pack?path=${encodeURIComponent(path)}`;
  }

  // Working configs stay expanded; agent/automation configs (devapi, capture,
  // testbed, profiling) collapse into a details block. Name convention only —
  // nothing game-specific.
  const SERVICE_CONFIG = /devapi|capture|testbed|profile/i;

  function configKeyMetric(config) {
    if (config.web) {
      const gz = (config.binFiles || []).reduce((sum, row) => sum + (row.gzBytes || 0), 0);
      return gz ? `web gz ${formatBytes(gz)}` : "";
    }
    const exe = (config.binFiles || [])
      .filter((row) => row.rel.endsWith(".exe") && !row.rel.includes("build_game_packs"))
      .sort((a, b) => b.bytes - a.bytes)[0];
    return exe ? `exe ${formatBytes(exe.bytes)}` : "";
  }

  function buildConfigRow(gameId, config) {
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
      firstPack ? packHref(gameId, config.name, firstPack.rel) : "",
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
  // build, and how big are the assembled PC/itch releases. Everything else is
  // one collapsed list.
  function renderBuilds(gameId, builds) {
    const body = byId("buildsBody");
    body.textContent = "";
    const configs = builds.configs || [];
    const workingDebug = configs.find((config) => !SERVICE_CONFIG.test(config.name) && /debug/i.test(config.name))
      || configs.find((config) => !SERVICE_CONFIG.test(config.name));
    if (workingDebug) body.append(buildConfigRow(gameId, workingDebug));

    const release = builds.release || [];
    const latestByTarget = new Map();
    for (const artifact of release) {
      const target = artifact.target || "release";
      if (!latestByTarget.has(target)) latestByTarget.set(target, artifact);
    }
    for (const artifact of latestByTarget.values()) body.append(releaseRow(artifact));

    const rest = configs.filter((config) => config !== workingDebug);
    const history = release.filter((artifact) => ![...latestByTarget.values()].includes(artifact));
    if (rest.length || history.length) {
      body.append(collapsedBlock(
        `Все конфиги (${rest.length}) и история релизов (${history.length})`,
        [...rest.map((config) => buildConfigRow(gameId, config)), ...history.map(releaseRow)],
      ));
    }

    if (!body.childElementCount) {
      body.innerHTML = '<p class="game-placeholder">No build configs or release artifacts found.</p>';
    }
  }

  async function loadBuilds(gameId) {
    const response = await fetch(`/api/game-page/builds?game=${encodeURIComponent(gameId)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`builds request failed: ${response.status}`);
    renderBuilds(gameId, await response.json());
  }

  function sectionError(id, message) {
    const body = byId(id);
    body.innerHTML = "";
    const p = document.createElement("p");
    p.className = "game-placeholder game-error";
    p.textContent = message;
    body.append(p);
  }

  function renderBalance(game) {
    const body = byId("balanceBody");
    body.textContent = "";
    const openLink = document.createElement("p");
    openLink.className = "game-placeholder";
    const anchor = document.createElement("a");
    anchor.className = "open-link";
    anchor.href = `/items?catalog=${encodeURIComponent(game.storeId)}`;
    anchor.textContent = "Open full Items Workbench";
    openLink.append(anchor);
    const frame = document.createElement("iframe");
    frame.className = "game-embed";
    frame.src = `/ai_studio/assets/items_viewer/site/items.html?catalog=${encodeURIComponent(game.storeId)}&embed=1`;
    frame.title = "Items Workbench";
    body.append(openLink, frame);
  }

  async function loadCanvases(game) {
    const response = await fetch(`/api/canvas/projects?store=${encodeURIComponent(game.storeId)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`canvas request failed: ${response.status}`);
    const payload = await response.json();
    const body = byId("canvasesBody");
    body.textContent = "";
    for (const project of payload.projects || []) {
      body.append(linkRow(
        "Canvas",
        project.title || project.id,
        project.id,
        `/canvas?project=${encodeURIComponent(project.id)}&store=${encodeURIComponent(game.storeId)}`,
        "Open",
      ));
    }
    if (!body.childElementCount) {
      body.innerHTML = '<p class="game-placeholder">No canvas projects for this game.</p>';
    }
  }

  async function loadState(gameId) {
    const response = await fetch(`/api/game-page/state?game=${encodeURIComponent(gameId)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`state request failed: ${response.status}`);
    const payload = await response.json();
    const body = byId("stateBody");
    body.textContent = "";
    for (const schema of payload.schemas || []) {
      body.append(linkRow(
        "Schema",
        schema.rel.replace("state/", ""),
        `${formatBytes(schema.bytes)} · ${formatDate(schema.mtimeMs)}`,
        gameFileHref(gameId, schema.rel),
        "View",
      ));
    }
    if (!body.childElementCount) {
      body.innerHTML = '<p class="game-placeholder">No state schemas found.</p>';
    }
  }

  async function loadCaptures(gameId) {
    const response = await fetch(`/api/game-page/captures?game=${encodeURIComponent(gameId)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`captures request failed: ${response.status}`);
    const payload = await response.json();
    const body = byId("capturesBody");
    body.textContent = "";
    for (const session of payload.sessions || []) {
      const row = document.createElement("article");
      row.className = "surface-row";
      if (session.previewRel) {
        const img = document.createElement("img");
        img.className = "capture-thumb";
        img.loading = "lazy";
        img.src = gameFileHref(gameId, session.previewRel);
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
      h4.textContent = `${session.shot} · ${session.stage}`;
      const p = document.createElement("p");
      p.textContent = `${formatDate(session.mtimeMs)} · ${formatBytes(session.bytes)} · ${session.rel}`;
      main.append(h4, p);
      row.append(main);
      if (session.videoRel) {
        const link = document.createElement("a");
        link.className = "open-link";
        link.href = gameFileHref(gameId, session.videoRel);
        link.textContent = "Video";
        row.append(link);
      }
      body.append(row);
    }
    if (payload.truncated) {
      body.append(Object.assign(document.createElement("p"), {
        className: "game-placeholder",
        textContent: "List truncated to the newest 100 sessions.",
      }));
    }
    if (!body.childElementCount) {
      body.innerHTML = '<p class="game-placeholder">No captures recorded.</p>';
    }
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
      links.append(linkRow("Docs", doc.label, doc.rel, gameFileHref(game.id, doc.rel), "View"));
    }
    for (const project of overview.taskboardProjects || []) {
      links.append(linkRow("Tasks", `${project.id} ${project.title}`, `Taskboard project (${project.status}, ${project.store})`, overview.links?.taskboard, "Open"));
    }
    links.append(linkRow("Assets", "Asset Viewer", "Browse this game's asset folders.", overview.links?.assetViewer, "Open"));
    if (!links.childElementCount) {
      links.innerHTML = `<p class="game-placeholder">Nothing to show yet.</p>`;
    }

    renderBalance(game);
    await Promise.all([
      loadBuilds(game.id).catch((error) => sectionError("buildsBody", String(error?.message || error))),
      loadCanvases(game).catch((error) => sectionError("canvasesBody", String(error?.message || error))),
      loadState(game.id).catch((error) => sectionError("stateBody", String(error?.message || error))),
      loadCaptures(game.id).catch((error) => sectionError("capturesBody", String(error?.message || error))),
    ]);
  }

  load().catch((error) => fail(error?.message || String(error)));
})();
