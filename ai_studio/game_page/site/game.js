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
    // A clean web config (e.g. wasm-release) carries the web size — it stays
    // beside the working debug build.
    const workingWeb = configs.find((config) => !SERVICE_CONFIG.test(config.name) && config.web && config !== workingDebug);
    const primary = [workingDebug, workingWeb].filter(Boolean);
    for (const config of primary) {
      body.append(buildConfigRow(config));
      const metric = configKeyMetric(config);
      if (metric) {
        dashboardCard(config.web ? "web" : "debug", config.name, metric.replace(/^exe |^web gz /, ""), "#sectionBuilds");
      }
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

    const rest = configs.filter((config) => !primary.includes(config));
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

  // #region Economy
  function economyLabel(value) {
    return String(value || "")
      .replace(/^[^.]+\./, "")
      .replace(/_/g, " ")
      .replace(/\b\w/g, (letter) => letter.toUpperCase());
  }

  function economyNumber(value) {
    return new Intl.NumberFormat("en", {
      notation: "compact",
      maximumFractionDigits: 1,
    }).format(value);
  }

  function costEntries(value) {
    if (value?.__studio_kind === "cost") return [value];
    if (value?.__studio_kind === "costs") return value.entries || [];
    return [];
  }

  function formatCost(value, itemsById) {
    if (value?.__studio_kind === "free") return "Free";
    return costEntries(value).map((entry) => {
      const itemId = entry.item?.id || "";
      const item = itemsById.get(itemId);
      return `${economyNumber(entry.count)} ${item?.name || economyLabel(itemId)}`;
    }).join(" + ");
  }

  function trackAdvance(track, itemsById) {
    const rows = track.levels?.rows || [];
    if (track.mode === "threshold") {
      const xp = rows.map((row) => row.xp_to_reach).filter((value) => Number.isFinite(value));
      return xp.length ? `${economyNumber(xp[0])} → ${economyNumber(xp.at(-1))} XP` : "—";
    }
    const costs = rows.map((row) => row.cost_to_reach).filter(Boolean);
    return costs.length
      ? `${formatCost(costs[0], itemsById)} → ${formatCost(costs.at(-1), itemsById)}`
      : "—";
  }

  function trackEffects(track) {
    const transitions = new Set(["cost_to_reach", "xp_to_reach"]);
    const effects = new Set();
    for (const row of track.levels?.rows || []) {
      for (const key of Object.keys(row)) {
        if (!transitions.has(key)) effects.add(economyLabel(key));
      }
    }
    const labels = [...effects];
    if (!labels.length) return track.mode === "threshold" ? "Rank threshold" : "—";
    return labels.length > 3
      ? `${labels.slice(0, 3).join(" · ")} +${labels.length - 3}`
      : labels.join(" · ");
  }

  function economyMetric(label, value) {
    const item = document.createElement("div");
    item.className = "economy-metric";
    const strong = document.createElement("strong");
    strong.textContent = value;
    const span = document.createElement("span");
    span.textContent = label;
    item.append(strong, span);
    return item;
  }

  function economyTable(headers) {
    const wrap = document.createElement("div");
    wrap.className = "economy-table-wrap";
    const table = document.createElement("table");
    table.className = "economy-table";
    const head = document.createElement("thead");
    const row = document.createElement("tr");
    for (const header of headers) {
      const th = document.createElement("th");
      th.scope = "col";
      th.textContent = header;
      row.append(th);
    }
    head.append(row);
    const body = document.createElement("tbody");
    table.append(head, body);
    wrap.append(table);
    return { wrap, body };
  }

  function economySection(title, action) {
    const section = document.createElement("section");
    section.className = "economy-block";
    const head = document.createElement("div");
    head.className = "economy-block-head";
    const heading = document.createElement("h3");
    heading.textContent = title;
    head.append(heading);
    if (action) head.append(action);
    section.append(head);
    return section;
  }

  const progressionColors = ["#60a5fa", "#f59e0b", "#34d399", "#f472b6", "#a78bfa", "#fb7185"];

  function svgElement(name, attributes) {
    const node = document.createElementNS("http://www.w3.org/2000/svg", name);
    for (const [key, value] of Object.entries(attributes)) node.setAttribute(key, value);
    return node;
  }

  function economyExact(value) {
    return new Intl.NumberFormat("en", {
      maximumFractionDigits: 3,
    }).format(value);
  }

  function progressionSeriesTitle(series) {
    if (series.id.endsWith(":coin_mul")) return "Coin multiplier";
    if (series.id.endsWith("_kmh")) return series.label.replace(/ Kmh$/, "");
    return series.kind === "cost" ? `${series.label} cost` : series.label;
  }

  function progressionSeriesValue(series, value) {
    if (!Number.isFinite(value)) return series.kind === "cost" ? "Start" : "—";
    const number = economyExact(value);
    if (series.id.endsWith("_mul")) return `×${number}`;
    if (series.id.endsWith("_kmh")) return `${number} km/h`;
    if (series.kind === "cost") return `${number} ${series.label}`;
    if (series.kind === "threshold") return `${number} XP`;
    return number;
  }

  function progressionSeriesCard(entry, index, rowCount, model, onPick) {
    const color = progressionColors[index % progressionColors.length];
    const points = model.normalizeSeries(entry.values);
    const stats = model.seriesAtLevel(entry, 1);
    const card = document.createElement("article");
    card.className = "economy-series-card";

    const head = document.createElement("div");
    head.className = "economy-series-head";
    const label = document.createElement("strong");
    label.textContent = progressionSeriesTitle(entry);
    const current = document.createElement("output");
    current.className = "economy-series-current";
    head.append(label, current);

    const plot = document.createElement("div");
    plot.className = "economy-series-plot";
    const scale = document.createElement("div");
    scale.className = "economy-series-scale";
    const maximum = document.createElement("span");
    maximum.textContent = progressionSeriesValue(entry, stats.max);
    const minimum = document.createElement("span");
    minimum.textContent = progressionSeriesValue(entry, stats.min);
    scale.append(maximum, minimum);

    const svg = svgElement("svg", {
      viewBox: "0 0 640 90",
      role: "img",
      "aria-label": `${progressionSeriesTitle(entry)}, ${rowCount} levels, ${minimum.textContent} to ${maximum.textContent}`,
      preserveAspectRatio: "none",
    });
    const title = svgElement("title", {});
    title.textContent = `${progressionSeriesTitle(entry)} progression`;
    svg.append(title);
    for (const ratio of [0, 0.5, 1]) {
      const y = 8 + ratio * 74;
      svg.append(svgElement("line", {
        x1: "12",
        x2: "628",
        y1: String(y),
        y2: String(y),
        class: "economy-chart-grid",
      }));
    }
    svg.append(svgElement("polyline", {
      points: points.filter(Boolean)
        .map((point) => `${12 + point.x * 616},${82 - point.y * 74}`)
        .join(" "),
      fill: "none",
      stroke: color,
      "stroke-width": "3",
      "stroke-linecap": "round",
      "stroke-linejoin": "round",
      "vector-effect": "non-scaling-stroke",
    }));
    const cursor = svgElement("line", {
      y1: "5",
      y2: "85",
      class: "economy-series-cursor",
    });
    const marker = svgElement("circle", {
      r: "5",
      fill: color,
      class: "economy-series-marker",
    });
    svg.append(cursor, marker);
    svg.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      const rect = svg.getBoundingClientRect();
      const x = (event.clientX - rect.left) / rect.width * 640;
      const ratio = Math.max(0, Math.min(1, (x - 12) / 616));
      onPick(1 + Math.round(ratio * (rowCount - 1)));
    });

    const axis = document.createElement("div");
    axis.className = "economy-mini-axis";
    const first = document.createElement("span");
    first.textContent = "Level 1";
    const last = document.createElement("span");
    last.textContent = `Level ${rowCount}`;
    axis.append(first, last);
    plot.append(scale, svg);
    card.append(head, plot, axis);

    return {
      element: card,
      update(level) {
        const summary = model.seriesAtLevel(entry, level);
        const point = points[level - 1];
        const x = 12 + (level - 1) / Math.max(rowCount - 1, 1) * 616;
        current.textContent = progressionSeriesValue(entry, summary.value);
        cursor.setAttribute("x1", String(x));
        cursor.setAttribute("x2", String(x));
        marker.setAttribute("cx", String(x));
        marker.setAttribute("cy", String(point ? 82 - point.y * 74 : 45));
        marker.style.visibility = point ? "visible" : "hidden";
      },
    };
  }

  function renderProgressionProfile(host, track, items, model) {
    host.textContent = "";
    const rows = track.levels?.rows || [];
    const series = model.progressionSeries(track, items);

    const heading = document.createElement("h4");
    heading.textContent = economyLabel(track.id);
    const note = document.createElement("p");
    note.textContent = "Choose a level to inspect exact values. Select any curve to move the level.";

    const control = document.createElement("div");
    control.className = "economy-level-control";
    const controlHead = document.createElement("div");
    controlHead.className = "economy-level-head";
    const controlLabel = document.createElement("label");
    controlLabel.htmlFor = "economyLevel";
    controlLabel.textContent = "Selected level";
    const readout = document.createElement("output");
    readout.htmlFor = "economyLevel";
    readout.className = "economy-level-readout";
    readout.setAttribute("aria-live", "polite");
    controlHead.append(controlLabel, readout);
    const slider = document.createElement("input");
    slider.id = "economyLevel";
    slider.className = "economy-level-slider";
    slider.type = "range";
    slider.min = "1";
    slider.max = String(rows.length);
    slider.step = "1";
    control.append(controlHead, slider);

    const cards = document.createElement("div");
    cards.className = "economy-series-grid";
    const views = series.map((entry, index) => progressionSeriesCard(
      entry,
      index,
      rows.length,
      model,
      (level) => {
        setLevel(level);
        slider.focus();
      },
    ));
    cards.append(...views.map((view) => view.element));

    const tableSection = document.createElement("section");
    tableSection.className = "economy-level-table-section";
    const tableHeading = document.createElement("h5");
    tableHeading.textContent = "Level values";
    const levelTable = economyTable([
      "Level",
      ...series.map(progressionSeriesTitle),
    ]);
    levelTable.wrap.classList.add("economy-level-values");
    const tableRows = model.progressionRows(series).map((progressionRow) => {
      const row = levelTable.body.insertRow();
      const levelCell = row.insertCell();
      const button = document.createElement("button");
      button.type = "button";
      button.className = "economy-level-button";
      button.textContent = String(progressionRow.level);
      button.setAttribute("aria-label", `Select level ${progressionRow.level}`);
      button.addEventListener("click", () => setLevel(progressionRow.level));
      levelCell.append(button);
      progressionRow.values.forEach((value, index) => {
        row.insertCell().textContent = progressionSeriesValue(series[index], value);
      });
      return { button, row };
    });
    tableSection.append(tableHeading, levelTable.wrap);

    let selectedTableRow;
    function setLevel(level) {
      slider.value = String(level);
      readout.textContent = `Level ${level} of ${rows.length}`;
      slider.setAttribute("aria-valuetext", [
        `Level ${level}`,
        ...series.map((entry) => {
          const value = model.seriesAtLevel(entry, level).value;
          return `${progressionSeriesTitle(entry)}: ${progressionSeriesValue(entry, value)}`;
        }),
      ].join(". "));
      for (const view of views) view.update(level);
      selectedTableRow?.row.classList.remove("is-selected");
      selectedTableRow?.button.removeAttribute("aria-current");
      selectedTableRow = tableRows[level - 1];
      selectedTableRow.row.classList.add("is-selected");
      selectedTableRow.button.setAttribute("aria-current", "true");

      const rowTop = selectedTableRow.row.offsetTop;
      const rowBottom = rowTop + selectedTableRow.row.offsetHeight;
      const headerHeight = levelTable.wrap.querySelector("thead").offsetHeight;
      if (rowTop < levelTable.wrap.scrollTop + headerHeight) {
        levelTable.wrap.scrollTop = rowTop - headerHeight;
      } else if (rowBottom > levelTable.wrap.scrollTop + levelTable.wrap.clientHeight) {
        levelTable.wrap.scrollTop = rowBottom - levelTable.wrap.clientHeight;
      }
    }

    slider.addEventListener("input", () => setLevel(Number(slider.value)));
    host.append(heading, note, control, cards, tableSection);
    setLevel(Math.ceil(rows.length / 2));
  }

  function renderEconomy(game, catalog, model) {
    const body = byId("economyBody");
    body.textContent = "";
    const items = catalog.items || [];
    const currencies = items.filter((item) => item.kind === "currency");
    const tracks = catalog.tracks || [];
    const itemsById = new Map(items.map((item) => [item.id, item]));
    const trackKinds = new Set(tracks.map((track) => track.kind));
    const validation = catalog.validate?.available
      ? (catalog.validate.ok ? "Valid" : `${catalog.validate.errors.length} errors`)
      : "Unavailable";

    const summary = document.createElement("div");
    summary.className = "economy-summary";
    summary.append(
      economyMetric("Currencies", String(currencies.length)),
      economyMetric("Tracks", String(tracks.length)),
      economyMetric("Track kinds", String(trackKinds.size)),
      economyMetric("Validation", validation),
    );
    body.append(summary);
    dashboardCard("economy", "economy tracks", String(tracks.length), "#sectionEconomy");

    const saveLink = document.createElement("a");
    saveLink.href = "#sectionState";
    saveLink.textContent = "Player holdings → Save";
    const currencySection = economySection("Currencies", saveLink);
    const currenciesTable = economyTable(["Currency", "Start", "Cap", "HUD"]);
    for (const currency of currencies) {
      const row = currenciesTable.body.insertRow();
      row.insertCell().textContent = currency.name || economyLabel(currency.id);
      row.insertCell().textContent = economyNumber(currency.base_value || 0);
      row.insertCell().textContent = currency.currency?.cap
        ? economyNumber(currency.currency.cap)
        : "No cap";
      row.insertCell().textContent = currency.currency?.hud || "—";
      row.title = currency.id;
    }
    currencySection.append(currenciesTable.wrap);
    body.append(currencySection);

    const search = document.createElement("input");
    search.className = "economy-search";
    search.type = "search";
    search.placeholder = "Filter tracks";
    search.setAttribute("aria-label", "Filter progression tracks");
    const tracksSection = economySection(`Progression tracks (${tracks.length})`, search);
    const profile = document.createElement("div");
    profile.className = "economy-profile";
    tracksSection.append(profile);
    const tracksTable = economyTable(["Track", "Kind", "Levels", "Cost or threshold", "Effects"]);
    tracksTable.wrap.classList.add("economy-tracks");
    const trackButtons = [];
    for (const track of tracks) {
      const row = tracksTable.body.insertRow();
      const effects = trackEffects(track);
      row.dataset.search = `${track.id} ${track.kind} ${effects}`.toLowerCase();
      const trackCell = row.insertCell();
      const trackButton = document.createElement("button");
      trackButton.type = "button";
      trackButton.className = "economy-track-button";
      trackButton.textContent = economyLabel(track.id);
      trackButton.title = track.id;
      trackButton.setAttribute("aria-pressed", "false");
      trackButton.addEventListener("click", () => {
        for (const button of trackButtons) {
          const selected = button === trackButton;
          button.setAttribute("aria-pressed", String(selected));
          button.closest("tr").classList.toggle("is-selected", selected);
        }
        renderProgressionProfile(profile, track, items, model);
      });
      trackButtons.push(trackButton);
      trackCell.append(trackButton);
      row.insertCell().textContent = economyLabel(track.kind);
      row.insertCell().textContent = String(track.levels?.rows?.length || 0);
      row.insertCell().textContent = trackAdvance(track, itemsById);
      row.insertCell().textContent = effects;
      row.title = track.id;
    }
    search.addEventListener("input", () => {
      const query = search.value.trim().toLowerCase();
      for (const row of tracksTable.body.rows) {
        row.hidden = Boolean(query) && !row.dataset.search.includes(query);
      }
    });
    trackButtons[0]?.click();
    tracksSection.append(tracksTable.wrap);
    body.append(tracksSection);

    const workbench = document.createElement("a");
    workbench.className = "open-link economy-workbench";
    workbench.href = `/items?catalog=${encodeURIComponent(game.storeId)}`;
    workbench.textContent = "Open Items Workbench";
    body.append(workbench);
  }

  async function loadEconomy(game) {
    const response = await fetch(
      `/api/items-viewer/catalog?id=${encodeURIComponent(game.storeId)}&include-private=1`,
      { cache: "no-store" },
    );
    if (!response.ok) throw new Error(`economy request failed: ${response.status}`);
    const [catalog, model] = await Promise.all([
      response.json(),
      import("./economy_model.mjs"),
    ]);
    renderEconomy(game, catalog, model);
    if (location.hash === "#sectionEconomy") byId("sectionEconomy").scrollIntoView();
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
    // The freshest real save opens by default — seeing the save IS the section.
    const autoOpen = (payload.slots || []).find((slot) => !slot.slot.endsWith(".bak")) || null;
    for (const slot of payload.slots || []) {
      const wrap = saveSlotRow(payload, slot);
      body.append(wrap);
      if (slot === autoOpen) wrap.querySelector(".save-content").open = true;
    }
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

    await Promise.all([
      loadBuilds().catch((error) => sectionError("buildsBody", String(error?.message || error))),
      loadEconomy(game).catch((error) => sectionError("economyBody", String(error?.message || error))),
      loadCanvases(game).catch((error) => sectionError("canvasesBody", String(error?.message || error))),
      loadSave().catch((error) => sectionError("saveBody", String(error?.message || error))),
      loadCaptures().catch((error) => sectionError("capturesBody", String(error?.message || error))),
    ]);
  }

  load().catch((error) => fail(error?.message || String(error)));
})();
