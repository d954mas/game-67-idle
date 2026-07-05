const REGISTRY_CONFIGS = [
  { id: "dashboard", label: "Обзор" },
  {
    id: "characters",
    label: "Персонажи",
    file: "characters.json",
    collection: "characters",
    createLabel: "Новый персонаж",
    subtitleKeys: ["kind", "role"],
    groupKeys: ["kind", "home_location_id", "tags"],
    filterKeys: ["kind", "home_location_id", "tags"]
  },
  {
    id: "locations",
    label: "Локации",
    file: "locations.json",
    collection: "locations",
    createLabel: "Новая локация",
    subtitleKeys: ["kind", "screen_id"],
    groupKeys: ["kind", "act"],
    filterKeys: ["kind", "act", "screen_id"]
  },
  {
    id: "items",
    label: "Предметы",
    file: "items.json",
    collection: "items",
    createLabel: "Новый предмет",
    subtitleKeys: ["kind", "slot", "rarity"],
    groupKeys: ["kind", "slot", "rarity", "tags"],
    filterKeys: ["kind", "slot", "rarity", "tags"]
  },
  {
    id: "shops",
    label: "Магазины",
    file: "services.json",
    collection: "shops",
    createLabel: "Новый магазин",
    subtitleKeys: ["keeper_character_id", "location_id"],
    groupKeys: ["location_id", "keeper_character_id"],
    filterKeys: ["location_id", "keeper_character_id"]
  },
  {
    id: "healing_services",
    label: "Лечение",
    file: "services.json",
    collection: "healing_services",
    createLabel: "Новая услуга",
    subtitleKeys: ["provider_character_id", "location_id"],
    groupKeys: ["location_id", "provider_character_id"],
    filterKeys: ["location_id", "provider_character_id"]
  },
  {
    id: "dialogues",
    label: "Диалоги",
    file: "dialogues.json",
    collection: "dialogues",
    createLabel: "Новый диалог",
    subtitleKeys: ["entry_node_id"],
    groupKeys: ["entry_node_id", "participants"],
    filterKeys: ["participants"]
  },
  {
    id: "quests",
    label: "Квесты",
    file: "quests.json",
    collection: "quests",
    createLabel: "Новый квест",
    subtitleKeys: ["type", "priority", "start_location"],
    groupKeys: ["type", "priority", "act", "start_location"],
    filterKeys: ["type", "priority", "act", "start_location"]
  },
  {
    id: "encounters",
    label: "Бой",
    file: "combat.json",
    collection: "encounters",
    createLabel: "Новый бой",
    subtitleKeys: ["location_id", "tier"],
    groupKeys: ["archetype", "role", "expected_threat_with_starter_loadout"],
    filterKeys: ["archetype", "role", "expected_threat_with_starter_loadout"]
  },
  {
    id: "assets",
    label: "Ассеты",
    file: "asset_manifest.json",
    collection: "assets",
    createLabel: "Новый ассет",
    subtitleKeys: ["kind", "status"],
    groupKeys: ["kind", "status"],
    filterKeys: ["kind", "status"]
  },
  { id: "validation", label: "Проверка" }
];

const COMMON_FIELD_KEYS = [
  "id",
  "display_name",
  "title",
  "short_description",
  "kind",
  "type",
  "role",
  "source",
  "file_path",
  "category",
  "slot",
  "rarity",
  "tier",
  "status",
  "priority",
  "act",
  "screen_id",
  "start_location",
  "summary",
  "description",
  "notes",
  "short_goal",
  "implementation_note",
  "blocked_text"
];

const MULTILINE_KEYS = new Set([
  "summary",
  "description",
  "notes",
  "short_goal",
  "implementation_note",
  "blocked_text",
  "role"
]);

const FIELD_SECTIONS = [
  {
    id: "identity",
    title: "Identity",
    keys: ["id", "display_name", "title", "short_description"]
  },
  {
    id: "classification",
    title: "Classification",
    keys: ["kind", "type", "category", "slot", "rarity", "tier", "status", "priority", "act", "role"]
  },
  {
    id: "art",
    title: "Art and source",
    keys: ["source", "file_path"]
  },
  {
    id: "links",
    title: "Links",
    keys: ["screen_id", "start_location"]
  },
  {
    id: "text",
    title: "Text",
    keys: ["summary", "description", "notes", "short_goal", "implementation_note", "blocked_text"]
  }
];

const REF_RULES = {
  asset_id: "assets",
  icon_asset_id: "assets",
  portrait_asset_id: "assets",
  full_body_asset_id: "assets",
  dialogue_face_asset_id: "assets",
  background_asset_id: "assets",
  map_node_asset_id: "assets",
  item_id: "items",
  quest_id: "quests",
  dialogue_id: "dialogues",
  character_id: "characters",
  speaker_id: "characters",
  home_location_id: "locations",
  location_id: "locations",
  start_location: "locations",
  target_location_id: "locations",
  encounter_id: "encounters",
  shop_id: "shops",
  service_id: "healing_services",
  keeper_character_id: "characters",
  provider_character_id: "characters"
};

const ARRAY_REF_RULES = {
  quest_ids: "quests",
  item_ids: "items",
  participants: "characters",
  unlock_locations: "locations",
  unlock_quests: "quests",
  unlock_encounters: "encounters",
  starter_loadout: "items",
  upgrade_examples: "items",
  grant_items: "items"
};

const ELEMENTS = {
  viewNav: document.querySelector("#viewNav"),
  searchInput: document.querySelector("#searchInput"),
  listControls: document.querySelector("#listControls"),
  entityList: document.querySelector("#entityList"),
  editorView: document.querySelector("#editorView"),
  listTitle: document.querySelector("#listTitle"),
  listCount: document.querySelector("#listCount"),
  fileStatus: document.querySelector("#fileStatus"),
  referenceSummary: document.querySelector("#referenceSummary"),
  validationList: document.querySelector("#validationList"),
  statusBar: document.querySelector("#statusBar"),
  openFolderButton: document.querySelector("#openFolderButton"),
  reloadButton: document.querySelector("#reloadButton"),
  saveButton: document.querySelector("#saveButton")
};

const state = {
  activeView: "dashboard",
  apiAvailable: false,
  folderAvailable: false,
  content: null,
  dirtyFiles: new Set(),
  dataDirectoryHandle: null,
  fileHandles: new Map(),
  selectedIds: new Map(),
  groupBy: new Map(),
  filters: new Map(),
  collapsedGroups: new Set(),
  search: "",
  rawDraft: "",
  validation: []
};

ELEMENTS.reloadButton.addEventListener("click", () => {
  if (state.dirtyFiles.size > 0 && !window.confirm("Есть несохраненные изменения. Перезагрузить данные?")) {
    return;
  }
  loadContent();
});

ELEMENTS.openFolderButton.addEventListener("click", () => openDataFolder());
ELEMENTS.saveButton.addEventListener("click", () => saveCurrentScope());
ELEMENTS.searchInput.addEventListener("input", (event) => {
  state.search = event.target.value.trim().toLowerCase();
  renderEntityList();
});

window.addEventListener("beforeunload", (event) => {
  if (state.dirtyFiles.size === 0) {
    return;
  }
  event.preventDefault();
  event.returnValue = "";
});

renderChrome();
loadContent();

async function loadContent() {
  setStatus("Loading content...");
  state.dirtyFiles.clear();
  state.fileHandles.clear();
  state.folderAvailable = false;
  state.selectedIds.clear();
  state.rawDraft = "";

  try {
    const response = await fetch("/api/content", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    state.content = await response.json();
    state.apiAvailable = true;
    setStatus("Content loaded from local editor server.");
  } catch (apiError) {
    try {
      state.content = await loadStaticContent();
      state.apiAvailable = false;
      setStatus("Content loaded read-only. Start editor server to save to disk.");
    } catch (staticError) {
      state.apiAvailable = false;
      state.content = null;
      ELEMENTS.editorView.innerHTML = `
        <div class="empty-state">
          <h2>Content is unavailable</h2>
          <p>Run: <code>node games/rb-dark-rpg/design/editor/server.mjs 5191</code></p>
          <p>Or open this page in Chrome and press <strong>Open data folder</strong>.</p>
          <p>${escapeHtml(staticError.message || apiError.message)}</p>
        </div>
      `;
      setStatus("Failed to load content.");
      renderSidePanels();
      return;
    }
  }

  validateContent();
  ensureSelection();
  renderAll();
}

async function openDataFolder() {
  if (!("showDirectoryPicker" in window)) {
    setStatus("Folder save mode is unavailable in this browser. Use the local Node server.");
    return;
  }
  if (state.dirtyFiles.size > 0 && !window.confirm("Есть несохраненные изменения. Открыть папку заново?")) {
    return;
  }

  try {
    const pickedHandle = await window.showDirectoryPicker({ mode: "readwrite" });
    const dataHandle = await resolveDataDirectory(pickedHandle);
    const content = await loadFromDirectoryHandle(dataHandle);
    state.content = content;
    state.apiAvailable = false;
    state.folderAvailable = true;
    state.dataDirectoryHandle = dataHandle;
    state.dirtyFiles.clear();
    state.selectedIds.clear();
    state.rawDraft = "";
    validateContent();
    ensureSelection();
    renderAll();
    setStatus("Content loaded from selected data folder. Save writes through browser file access.");
  } catch (error) {
    setStatus(`Folder load failed: ${error.message || error}`);
  }
}

async function resolveDataDirectory(handle) {
  try {
    await handle.getFileHandle("content_manifest.json", { create: false });
    return handle;
  } catch {
    return handle.getDirectoryHandle("data", { create: false });
  }
}

async function loadFromDirectoryHandle(dataHandle) {
  state.fileHandles.clear();
  const manifestHandle = await dataHandle.getFileHandle("content_manifest.json", { create: false });
  const manifest = await readHandleJson(manifestHandle);
  const files = { "content_manifest.json": manifest };
  state.fileHandles.set("content_manifest.json", manifestHandle);
  for (const file of manifest.load_order || []) {
    const handle = await dataHandle.getFileHandle(file, { create: false });
    files[file] = await readHandleJson(handle);
    state.fileHandles.set(file, handle);
  }
  return { manifest, files };
}

async function readHandleJson(handle) {
  const file = await handle.getFile();
  return JSON.parse(await file.text());
}

async function loadStaticContent() {
  const manifest = await fetchJson("../data/content_manifest.json");
  const files = {};
  files["content_manifest.json"] = manifest;
  for (const file of manifest.load_order || []) {
    files[file] = await fetchJson(`../data/${file}`);
  }
  return { manifest, files };
}

async function fetchJson(url) {
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Cannot load ${url}: HTTP ${response.status}`);
  }
  return response.json();
}

function renderChrome() {
  ELEMENTS.viewNav.innerHTML = "";
  for (const config of REGISTRY_CONFIGS) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "view-tab";
    button.dataset.view = config.id;
    button.innerHTML = `
      <span>${escapeHtml(config.label)}</span>
      <span class="dirty-slot"></span>
    `;
    button.addEventListener("click", () => {
      state.activeView = config.id;
      state.search = "";
      ELEMENTS.searchInput.value = "";
      ensureSelection();
      renderAll();
    });
    ELEMENTS.viewNav.appendChild(button);
  }
}

function renderAll() {
  renderChromeState();
  renderEntityList();
  renderEditor();
  renderSidePanels();
}

function renderChromeState() {
  document.body.dataset.activeView = state.activeView;
  for (const button of ELEMENTS.viewNav.querySelectorAll(".view-tab")) {
    const config = getConfig(button.dataset.view);
    button.classList.toggle("active", button.dataset.view === state.activeView);
    const dirtySlot = button.querySelector(".dirty-slot");
    const dirty = config?.file && state.dirtyFiles.has(config.file);
    dirtySlot.innerHTML = dirty ? '<span class="dirty-dot" aria-label="Unsaved"></span>' : "";
  }
  ELEMENTS.saveButton.disabled = state.dirtyFiles.size === 0;
}

function renderEntityList() {
  const config = getActiveConfig();
  const collection = getCollection(config);
  const filtered = collection.filter((entity) => entityMatchesSearch(entity, state.search) && entityMatchesFilters(config, entity));

  ELEMENTS.listTitle.textContent = config?.label || "Content";
  ELEMENTS.listCount.textContent = String(filtered.length);
  ELEMENTS.entityList.innerHTML = "";
  renderListControls(config, collection);

  if (!config?.collection) {
    ELEMENTS.entityList.innerHTML = `<div class="empty-state" style="padding: 12px;">Выберите раздел с сущностями.</div>`;
    return;
  }

  const groupBy = getActiveGroupBy(config);
  if (groupBy) {
    renderGroupedEntityList(config, filtered, groupBy);
  } else {
    for (const entity of filtered) {
      ELEMENTS.entityList.appendChild(createEntityRow(config, entity));
    }
  }

  if (filtered.length === 0) {
    ELEMENTS.entityList.innerHTML = `<div class="empty-state" style="padding: 12px;">Ничего не найдено.</div>`;
  }
}

function renderListControls(config, collection) {
  if (!config?.collection) {
    ELEMENTS.listControls.innerHTML = "";
    return;
  }

  const groupKeys = getAvailableFacetKeys(config, collection, config.groupKeys || []);
  const filterKeys = getAvailableFacetKeys(config, collection, config.filterKeys || []);
  const activeGroupBy = getActiveGroupBy(config);
  const filters = getFilterState(config);

  ELEMENTS.listControls.innerHTML = `
    <button type="button" id="createEntityButton" class="create-entity-button">${escapeHtml(config.createLabel || `New ${config.label}`)}</button>
    <label class="control-field">
      <span>Group</span>
      <select id="groupBySelect">
        <option value="">None</option>
        ${groupKeys.map((key) => `<option value="${escapeHtml(key)}" ${key === activeGroupBy ? "selected" : ""}>${escapeHtml(formatFacetName(key))}</option>`).join("")}
      </select>
    </label>
    <div class="facet-filters">
      ${filterKeys.map((key) => renderFacetFilter(config, collection, key, filters[key] || "")).join("")}
    </div>
  `;

  const groupSelect = ELEMENTS.listControls.querySelector("#groupBySelect");
  const createButton = ELEMENTS.listControls.querySelector("#createEntityButton");
  createButton.addEventListener("click", () => createEntity(config));
  groupSelect.addEventListener("change", () => {
    state.groupBy.set(config.id, groupSelect.value);
    renderEntityList();
  });

  for (const select of ELEMENTS.listControls.querySelectorAll("select[data-filter-key]")) {
    select.addEventListener("change", () => {
      const next = { ...getFilterState(config), [select.dataset.filterKey]: select.value };
      if (!select.value) {
        delete next[select.dataset.filterKey];
      }
      state.filters.set(config.id, next);
      renderEntityList();
    });
  }
}

function renderFacetFilter(config, collection, key, activeValue) {
  const options = getFacetOptions(collection, key);
  if (options.length === 0) {
    return "";
  }
  return `
    <label class="control-field compact">
      <span>${escapeHtml(formatFacetName(key))}</span>
      <select data-filter-key="${escapeHtml(key)}">
        <option value="">All</option>
        ${options.map((value) => `<option value="${escapeHtml(value)}" ${value === activeValue ? "selected" : ""}>${escapeHtml(value)}</option>`).join("")}
      </select>
    </label>
  `;
}

function renderGroupedEntityList(config, entities, groupBy) {
  const groups = new Map();
  for (const entity of entities) {
    const groupName = getPrimaryFacetValue(entity, groupBy) || "Unspecified";
    if (!groups.has(groupName)) {
      groups.set(groupName, []);
    }
    groups.get(groupName).push(entity);
  }

  for (const [groupName, groupEntities] of [...groups.entries()].sort((a, b) => a[0].localeCompare(b[0]))) {
    const key = `${config.id}:${groupBy}:${groupName}`;
    const collapsed = state.collapsedGroups.has(key);
    const header = document.createElement("button");
    header.type = "button";
    header.className = "group-header";
    header.innerHTML = `
      <span>${collapsed ? ">" : "v"} ${escapeHtml(groupName)}</span>
      <strong>${groupEntities.length}</strong>
    `;
    header.addEventListener("click", () => {
      if (collapsed) {
        state.collapsedGroups.delete(key);
      } else {
        state.collapsedGroups.add(key);
      }
      renderEntityList();
    });
    ELEMENTS.entityList.appendChild(header);

    if (!collapsed) {
      for (const entity of groupEntities) {
        ELEMENTS.entityList.appendChild(createEntityRow(config, entity));
      }
    }
  }
}

function createEntityRow(config, entity) {
  if (config.id === "characters") {
    return createCharacterCard(config, entity);
  }
  if (config.id === "quests") {
    return createQuestListCard(config, entity);
  }
  if (config.id === "dialogues") {
    return createDialogueListCard(config, entity);
  }

  const id = entity.id || "";
  const preview = getEntityPreview(entity, config);
  const button = document.createElement("button");
  button.type = "button";
  button.className = `entity-row ${preview ? "has-thumb" : ""} ${id === getSelectedId(config) ? "active" : ""}`;
  button.innerHTML = `
    ${preview ? renderThumb(preview, getEntityTitle(entity)) : ""}
    <span class="entity-text">
      <span class="entity-title">${escapeHtml(getEntityTitle(entity))}</span>
      <span class="entity-subtitle">${escapeHtml(getEntitySubtitle(config, entity))}</span>
    </span>
  `;
  button.addEventListener("click", () => {
    state.selectedIds.set(config.id, id);
    state.rawDraft = JSON.stringify(entity, null, 2);
    renderAll();
  });
  return button;
}

function createQuestListCard(config, entity) {
  const id = entity.id || "";
  const steps = entity.steps || [];
  const button = document.createElement("button");
  button.type = "button";
  button.className = `quest-list-card ${id === getSelectedId(config) ? "active" : ""}`;
  button.innerHTML = `
    <span class="content-card-head">
      <strong>${escapeHtml(getEntityTitle(entity))}</strong>
      <em>${escapeHtml(entity.type || "quest")}</em>
    </span>
    <span class="content-card-description">${escapeHtml(entity.journal?.short_goal || entity.implementation_note || entity.id)}</span>
    <span class="content-card-footer">
      <span>${steps.length} steps</span>
      <span>${escapeHtml(entity.priority || entity.start_location || "draft")}</span>
    </span>
  `;
  button.addEventListener("click", () => {
    state.selectedIds.set(config.id, id);
    state.rawDraft = JSON.stringify(entity, null, 2);
    renderAll();
  });
  return button;
}

function createDialogueListCard(config, entity) {
  const id = entity.id || "";
  const nodes = entity.nodes || [];
  const button = document.createElement("button");
  button.type = "button";
  button.className = `dialogue-list-card ${id === getSelectedId(config) ? "active" : ""}`;
  button.innerHTML = `
    <span class="content-card-head">
      <strong>${escapeHtml(getEntityTitle(entity))}</strong>
      <em>${nodes.length} nodes</em>
    </span>
    <span class="content-card-description">${escapeHtml(getDialogueParticipantsLabel(entity))}</span>
    <span class="content-card-footer">
      <span>entry: ${escapeHtml(entity.entry_node_id || "missing")}</span>
      <span>${escapeHtml(id)}</span>
    </span>
  `;
  button.addEventListener("click", () => {
    state.selectedIds.set(config.id, id);
    state.rawDraft = JSON.stringify(entity, null, 2);
    renderAll();
  });
  return button;
}

function createCharacterCard(config, entity) {
  const id = entity.id || "";
  const preview = getEntityPreview(entity, config);
  const tags = Array.isArray(entity.tags) ? entity.tags.slice(0, 4) : [];
  const button = document.createElement("button");
  button.type = "button";
  button.className = `character-card ${id === getSelectedId(config) ? "active" : ""}`;
  button.innerHTML = `
    <span class="character-card-art">
      ${preview ? `<img src="${escapeHtml(preview.url)}" alt="${escapeHtml(getEntityTitle(entity))}" loading="lazy" />` : `<span class="character-card-fallback">${escapeHtml(getInitials(getEntityTitle(entity)))}</span>`}
    </span>
    <span class="character-card-body">
      <span class="character-card-topline">
        <strong>${escapeHtml(getEntityTitle(entity))}</strong>
        <em>${escapeHtml(entity.kind || "character")}</em>
      </span>
      <span class="character-card-description">${escapeHtml(getCharacterDescription(entity))}</span>
      <span class="character-card-footer">
        <span class="character-card-tags">${tags.map((tag) => `<span>${escapeHtml(tag)}</span>`).join("")}</span>
        <span class="character-card-action">Open details</span>
      </span>
    </span>
  `;
  button.addEventListener("click", () => {
    state.selectedIds.set(config.id, id);
    state.rawDraft = JSON.stringify(entity, null, 2);
    renderAll();
  });
  return button;
}

function renderEditor() {
  const config = getActiveConfig();
  if (!state.content) {
    return;
  }
  if (state.activeView === "dashboard") {
    renderDashboard();
    return;
  }
  if (state.activeView === "validation") {
    renderValidationView();
    return;
  }
  if (!config?.collection) {
    ELEMENTS.editorView.innerHTML = `<div class="empty-state"><h2>Нет редактора для раздела</h2></div>`;
    return;
  }

  const entity = getSelectedEntity(config);
  if (!entity) {
    ELEMENTS.editorView.innerHTML = `<div class="empty-state"><h2>Нет выбранной сущности</h2></div>`;
    return;
  }
  state.rawDraft = JSON.stringify(entity, null, 2);

  const fields = COMMON_FIELD_KEYS.filter((key) => Object.prototype.hasOwnProperty.call(entity, key));
  ELEMENTS.editorView.innerHTML = `
    <div class="editor-head">
      <div>
        <h2>${escapeHtml(getEntityTitle(entity))}</h2>
        <div class="editor-meta">${escapeHtml(config.file)} / ${escapeHtml(config.collection)} / ${escapeHtml(entity.id || "")}</div>
      </div>
      <button type="button" id="downloadFileButton">Download file</button>
    </div>
    ${renderVisualPreview(entity, config)}
    ${renderEntityMapPreview(entity, config)}
    ${config.id === "items" ? renderItemWorkbench(entity) : ""}
    <div class="field-sections" id="fieldSections"></div>
    <div class="raw-editor">
      <div class="raw-editor-head">
        <h3>Object JSON</h3>
        <div class="raw-actions">
          <button type="button" id="formatJsonButton">Format</button>
          <button type="button" id="applyJsonButton">Apply JSON</button>
        </div>
      </div>
      <textarea id="rawJson" spellcheck="false"></textarea>
    </div>
  `;

  const fieldSections = ELEMENTS.editorView.querySelector("#fieldSections");
  renderFieldSections(fieldSections, fields, entity);

  for (const input of fieldSections.querySelectorAll("input[data-key], textarea[data-key], select[data-key]")) {
    input.addEventListener("input", () => {
      const key = input.dataset.key;
      entity[key] = coerceFieldValue(input.value, entity[key]);
      state.rawDraft = JSON.stringify(entity, null, 2);
      markDirty(config.file);
      validateContent();
      renderSidePanels();
      syncRawTextarea();
    });
  }
  wireItemWorkbench(config, entity);

  const rawJson = ELEMENTS.editorView.querySelector("#rawJson");
  rawJson.value = state.rawDraft || JSON.stringify(entity, null, 2);
  rawJson.addEventListener("input", () => {
    state.rawDraft = rawJson.value;
  });

  ELEMENTS.editorView.querySelector("#applyJsonButton").addEventListener("click", () => applyRawJson(config));
  ELEMENTS.editorView.querySelector("#formatJsonButton").addEventListener("click", () => formatRawJson());
  ELEMENTS.editorView.querySelector("#downloadFileButton").addEventListener("click", () => downloadFile(config.file));
  wireEditorLinks();
}

function createEntity(config) {
  if (!config?.collection || !config.file) {
    return;
  }
  const collection = getCollection(config);
  const entity = createEntityTemplate(config, collection);
  collection.push(entity);
  state.selectedIds.set(config.id, entity.id);
  state.rawDraft = JSON.stringify(entity, null, 2);
  markDirty(config.file);
  validateContent();
  renderAll();
  setStatus(`Created ${entity.id}.`);
}

function createEntityTemplate(config, collection) {
  if (config.id === "items") {
    const id = uniqueEntityId(collection, "new_gear");
    return {
      id,
      display_name: "New Gear",
      kind: "gear",
      slot: "weapon",
      stackable: false,
      icon_asset_id: "",
      price_gold: 0,
      required_level: 1,
      stats: {
        weapon_damage: 1
      },
      tags: ["draft"]
    };
  }
  if (config.id === "quests") {
    const id = uniqueEntityId(collection, "q_new_quest");
    return {
      id,
      title: "New Quest",
      type: "contract",
      act: "act_1_extra_witness",
      priority: "side",
      start_location: "hub_last_post",
      prerequisites: [],
      journal: {
        summary: "",
        short_goal: ""
      },
      steps: [],
      completion_rewards: {
        xp: 0,
        gold: 0,
        items: [],
        flags: [],
        unlocks: {}
      }
    };
  }
  if (config.id === "dialogues") {
    const id = uniqueEntityId(collection, "dlg_new");
    return {
      id,
      title: "New Dialogue",
      participants: ["player_seeker"],
      entry_node_id: "start",
      nodes: [
        {
          id: "start",
          speaker_id: "player_seeker",
          text: "",
          choices: [
            {
              id: "close",
              text: "Close"
            }
          ]
        }
      ]
    };
  }
  const id = uniqueEntityId(collection, `new_${config.id}`);
  return {
    id,
    display_name: "New Entry"
  };
}

function uniqueEntityId(collection, baseId) {
  const existing = new Set(collection.map((entity) => entity.id).filter(Boolean));
  let candidate = baseId;
  let index = 2;
  while (existing.has(candidate)) {
    candidate = `${baseId}_${index}`;
    index += 1;
  }
  return candidate;
}

function renderItemWorkbench(item) {
  const stats = item.stats || {};
  const tags = Array.isArray(item.tags) ? item.tags.join(", ") : "";
  return `
    <section class="item-workbench" aria-label="Item setup">
      <div class="map-panel-head">
        <div>
          <h3>Item setup</h3>
          <p>Gear uses four MVP slots: weapon, armour, legs, relic. Other item kinds stay in inventory.</p>
        </div>
        <div class="map-panel-badges">
          ${renderBadge(item.kind || "kind missing")}
          ${item.kind === "gear" ? renderBadge(item.slot || "slot missing") : renderBadge("inventory")}
          ${renderBadge(`level ${item.required_level || 1}`)}
        </div>
      </div>
      <div class="item-form-grid">
        <label class="field">
          <span>Kind</span>
          <select data-item-field="kind">
            ${["gear", "quest_item", "clue", "consumable", "material"].map((kind) => `<option value="${kind}" ${item.kind === kind ? "selected" : ""}>${kind}</option>`).join("")}
          </select>
        </label>
        <label class="field">
          <span>Slot</span>
          <select data-item-field="slot">
            ${["none", "weapon", "armour", "legs", "relic"].map((slot) => `<option value="${slot}" ${((item.slot || "none") === slot) ? "selected" : ""}>${slot}</option>`).join("")}
          </select>
        </label>
        <label class="field">
          <span>Required level</span>
          <input data-item-field="required_level" type="number" min="1" max="99" value="${escapeHtml(item.required_level || 1)}" />
        </label>
        <label class="field">
          <span>Price gold</span>
          <input data-item-field="price_gold" type="number" min="0" value="${escapeHtml(item.price_gold || 0)}" />
        </label>
        <label class="field">
          <span>Equipment set</span>
          <input data-item-field="equipment_set" value="${escapeHtml(item.equipment_set || "")}" />
        </label>
        <label class="field">
          <span>Icon asset</span>
          <input data-item-field="icon_asset_id" value="${escapeHtml(item.icon_asset_id || "")}" />
        </label>
        <label class="field checkbox-field">
          <input data-item-field="stackable" type="checkbox" ${item.stackable ? "checked" : ""} />
          <span>Stackable</span>
        </label>
        <label class="field">
          <span>Max stack</span>
          <input data-item-field="max_stack" type="number" min="1" value="${escapeHtml(item.max_stack || 1)}" />
        </label>
        <label class="field full">
          <span>Tags</span>
          <input data-item-field="tags" value="${escapeHtml(tags)}" />
        </label>
      </div>
      <div class="item-stats-grid">
        ${["vitality", "strength", "protection", "intuition", "weapon_damage"].map((stat) => `
          <label class="stat-field">
            <span>${escapeHtml(formatFacetName(stat))}</span>
            <input data-item-stat="${stat}" type="number" value="${escapeHtml(stats[stat] ?? 0)}" />
          </label>
        `).join("")}
      </div>
    </section>
  `;
}

function wireItemWorkbench(config, item) {
  if (config.id !== "items") {
    return;
  }
  for (const input of ELEMENTS.editorView.querySelectorAll("[data-item-field]")) {
    input.addEventListener("input", () => updateItemField(config, item, input));
    input.addEventListener("change", () => updateItemField(config, item, input));
  }
  for (const input of ELEMENTS.editorView.querySelectorAll("[data-item-stat]")) {
    input.addEventListener("input", () => {
      const stat = input.dataset.itemStat;
      const value = Number(input.value);
      item.stats = item.stats || {};
      if (Number.isFinite(value) && value !== 0) {
        item.stats[stat] = value;
      } else {
        delete item.stats[stat];
      }
      touchEditedEntity(config, item);
    });
  }
}

function updateItemField(config, item, input) {
  const field = input.dataset.itemField;
  if (field === "stackable") {
    item.stackable = input.checked;
  } else if (field === "price_gold" || field === "required_level" || field === "max_stack") {
    const value = Number(input.value);
    item[field] = Number.isFinite(value) ? value : 0;
  } else if (field === "tags") {
    item.tags = input.value.split(",").map((tag) => tag.trim()).filter(Boolean);
  } else if (field === "slot") {
    if (input.value === "none") {
      delete item.slot;
    } else {
      item.slot = input.value;
    }
  } else if (input.value) {
    item[field] = input.value;
  } else {
    delete item[field];
  }
  if (item.kind !== "gear") {
    delete item.slot;
  }
  touchEditedEntity(config, item);
}

function touchEditedEntity(config, entity) {
  state.rawDraft = JSON.stringify(entity, null, 2);
  markDirty(config.file);
  validateContent();
  renderSidePanels();
  syncRawTextarea();
}

function renderEntityMapPreview(entity, config) {
  if (config.id === "quests") {
    return renderQuestMap(entity);
  }
  if (config.id === "dialogues") {
    return renderDialogueGraph(entity);
  }
  return "";
}

function wireEditorLinks() {
  for (const button of ELEMENTS.editorView.querySelectorAll("[data-open-view]")) {
    button.addEventListener("click", () => {
      const view = button.dataset.openView;
      const id = button.dataset.openId;
      const config = getConfig(view);
      if (!config) {
        return;
      }
      state.activeView = view;
      if (id) {
        state.selectedIds.set(view, id);
      }
      state.search = "";
      ELEMENTS.searchInput.value = "";
      ensureSelection();
      renderAll();
    });
  }
}

function renderQuestMap(quest) {
  const steps = quest.steps || [];
  return `
    <section class="quest-map-panel" aria-label="Quest map">
      <div class="map-panel-head">
        <div>
          <h3>Карта квеста</h3>
          <p>${escapeHtml(quest.journal?.short_goal || quest.implementation_note || "Этапы пока не описаны.")}</p>
        </div>
        <div class="map-panel-badges">
          ${renderBadge(quest.type || "quest")}
          ${renderBadge(quest.priority || "priority missing")}
          ${renderBadge(`giver: ${getRegistryLabel("characters", quest.giver?.id) || quest.giver?.display_name || "none"}`)}
          ${renderBadge(`start: ${getRegistryLabel("locations", quest.start_location) || quest.start_location || "none"}`)}
        </div>
      </div>
      ${steps.length > 0 ? `
        <div class="quest-flow">
          ${steps.map((step, index) => renderQuestStepCard(quest, step, index, steps.length)).join("")}
        </div>
      ` : `
        <div class="map-empty">
          <strong>Нет этапов</strong>
          <span>Добавь steps в JSON ниже, и здесь появится карта прохождения.</span>
        </div>
      `}
    </section>
  `;
}

function renderQuestStepCard(quest, step, index, total) {
  const objective = step.objective || {};
  const dialogue = step.dialogue_id ? getRegistryEntity("dialogues", step.dialogue_id) : null;
  return `
    <article class="quest-step-card">
      <div class="quest-step-index">${index + 1}</div>
      <div class="quest-step-main">
        <header>
          <strong>${escapeHtml(step.title || step.id || `Step ${index + 1}`)}</strong>
          <span>${escapeHtml(step.id || "")}</span>
        </header>
        <p>${escapeHtml(step.description || step.ui_hint || "Описание этапа не заполнено.")}</p>
        <div class="step-chip-row">
          ${renderBadge(objective.type || "objective missing")}
          ${step.location_id ? renderBadge(getRegistryLabel("locations", step.location_id) || step.location_id) : ""}
          ${renderObjectiveTargetBadge(objective)}
        </div>
        ${step.dialogue_id ? `
          <button type="button" class="graph-link-button" data-open-view="dialogues" data-open-id="${escapeHtml(step.dialogue_id)}">
            Dialogue graph: ${escapeHtml(dialogue?.title || step.dialogue_id)}
          </button>
        ` : ""}
        ${renderStepOutcome(step)}
      </div>
      ${index < total - 1 ? `<div class="quest-step-connector" aria-hidden="true"></div>` : ""}
    </article>
  `;
}

function renderObjectiveTargetBadge(objective) {
  const targetId = objective.target_id || objective.item_id || objective.encounter_id || objective.object_id || "";
  if (!targetId) {
    return "";
  }
  const objectiveTargetRegistries = {
    talk_to_npc: "characters",
    return_to_npc: "characters",
    visit_location: "locations"
  };
  const registry = objective.target_id
    ? objectiveTargetRegistries[objective.type] || "characters"
    : objective.item_id
      ? "items"
      : objective.encounter_id
        ? "encounters"
        : "";
  const label = registry ? getRegistryLabel(registry, targetId) || targetId : targetId;
  return renderBadge(`target: ${label}`);
}

function renderStepOutcome(step) {
  const lines = summarizeStepOutcome(step.on_complete);
  if (lines.length === 0 && (!step.blocked_reasons || step.blocked_reasons.length === 0)) {
    return "";
  }
  return `
    <div class="step-outcome">
      ${lines.map((line) => `<span>${escapeHtml(line)}</span>`).join("")}
      ${(step.blocked_reasons || []).map((reason) => `<span class="blocked">${escapeHtml(reason.text || reason.id)}</span>`).join("")}
    </div>
  `;
}

function summarizeStepOutcome(outcome) {
  if (!outcome) {
    return [];
  }
  const lines = [];
  if (outcome.grant_items?.length) {
    lines.push(`items: ${outcome.grant_items.map((id) => getRegistryLabel("items", id) || id).join(", ")}`);
  }
  if (outcome.grant_xp) {
    lines.push(`xp: ${outcome.grant_xp}`);
  }
  if (outcome.grant_gold) {
    lines.push(`gold: ${outcome.grant_gold}`);
  }
  if (outcome.set_flags?.length) {
    lines.push(`flags: ${outcome.set_flags.join(", ")}`);
  }
  if (outcome.unlock_screens?.length) {
    lines.push(`screens: ${outcome.unlock_screens.join(", ")}`);
  }
  if (outcome.unlock_locations?.length) {
    lines.push(`locations: ${outcome.unlock_locations.map((id) => getRegistryLabel("locations", id) || id).join(", ")}`);
  }
  if (outcome.unlock_markers?.length) {
    lines.push(`markers: ${outcome.unlock_markers.join(", ")}`);
  }
  if (outcome.unlock_encounters?.length) {
    lines.push(`encounters: ${outcome.unlock_encounters.map((id) => getRegistryLabel("encounters", id) || id).join(", ")}`);
  }
  if (outcome.unlock_quests?.length) {
    lines.push(`quests: ${outcome.unlock_quests.map((id) => getRegistryLabel("quests", id) || id).join(", ")}`);
  }
  if (outcome.complete_quest) {
    lines.push("complete quest");
  }
  return lines;
}

function renderDialogueGraph(dialogue) {
  const nodes = getDialogueGraphNodes(dialogue);
  return `
    <section class="dialogue-graph-panel" aria-label="Dialogue graph">
      <div class="map-panel-head">
        <div>
          <h3>Граф диалога</h3>
          <p>${escapeHtml(getDialogueParticipantsLabel(dialogue))}</p>
        </div>
        <div class="map-panel-badges">
          ${renderBadge(`entry: ${dialogue.entry_node_id || "missing"}`)}
          ${renderBadge(`${(dialogue.nodes || []).length} nodes`)}
          ${renderBadge(`${countDialogueChoices(dialogue)} choices`)}
        </div>
      </div>
      ${nodes.length > 0 ? `
        <div class="dialogue-graph">
          ${nodes.map(({ node, depth, reachable }, index) => renderDialogueNode(dialogue, node, depth, index, reachable)).join("")}
        </div>
      ` : `
        <div class="map-empty">
          <strong>Нет узлов</strong>
          <span>Добавь nodes в JSON ниже, и здесь появится граф диалога.</span>
        </div>
      `}
    </section>
  `;
}

function renderDialogueNode(dialogue, node, depth, index, reachable) {
  const speaker = getRegistryEntity("characters", node.speaker_id);
  const speakerPreview = speaker ? getEntityPreview(speaker, getConfig("characters")) : null;
  const choices = node.choices || [];
  return `
    <article class="dialogue-node-card ${reachable ? "" : "unreachable"}" style="--node-depth: ${Math.min(depth, 4)}">
      <header>
        <span class="dialogue-node-avatar">
          ${speakerPreview ? `<img src="${escapeHtml(speakerPreview.url)}" alt="${escapeHtml(getEntityTitle(speaker))}" loading="lazy" />` : `<span>${escapeHtml(getInitials(node.speaker_id || "?"))}</span>`}
        </span>
        <span>
          <strong>${escapeHtml(node.id || `node_${index}`)}</strong>
          <em>${escapeHtml(getRegistryLabel("characters", node.speaker_id) || node.speaker_id || "speaker missing")}</em>
        </span>
        ${dialogue.entry_node_id === node.id ? `<b>ENTRY</b>` : ""}
      </header>
      <p>${escapeHtml(node.text || "Текст реплики не заполнен.")}</p>
      <div class="dialogue-choice-list">
        ${choices.length > 0 ? choices.map((choice) => renderDialogueChoice(choice)).join("") : `<span class="dialogue-terminal">End node</span>`}
      </div>
    </article>
  `;
}

function renderDialogueChoice(choice) {
  const effects = choice.effects || [];
  return `
    <div class="dialogue-choice">
      <span class="choice-text">${escapeHtml(choice.text || choice.id || "choice")}</span>
      ${choice.next_node_id ? `<span class="choice-target">→ ${escapeHtml(choice.next_node_id)}</span>` : `<span class="choice-target muted">no next node</span>`}
      ${effects.length > 0 ? `<span class="choice-effects">${effects.map((effect) => renderDialogueEffect(effect)).join("")}</span>` : ""}
    </div>
  `;
}

function renderDialogueEffect(effect) {
  if (effect.type === "advance_quest" && effect.quest_id) {
    return `
      <button type="button" class="effect-link" data-open-view="quests" data-open-id="${escapeHtml(effect.quest_id)}">
        ${escapeHtml(effect.type)}: ${escapeHtml(effect.step_id || effect.quest_id)}
      </button>
    `;
  }
  return `<span class="effect-chip">${escapeHtml(effect.type || "effect")}</span>`;
}

function getDialogueGraphNodes(dialogue) {
  const nodes = dialogue.nodes || [];
  const byId = new Map(nodes.map((node) => [node.id, node]));
  const entryId = dialogue.entry_node_id || nodes[0]?.id;
  const depths = new Map();
  const queue = entryId && byId.has(entryId) ? [{ id: entryId, depth: 0 }] : [];

  while (queue.length > 0) {
    const current = queue.shift();
    if (depths.has(current.id)) {
      continue;
    }
    depths.set(current.id, current.depth);
    const node = byId.get(current.id);
    for (const choice of node?.choices || []) {
      if (choice.next_node_id && byId.has(choice.next_node_id)) {
        queue.push({ id: choice.next_node_id, depth: current.depth + 1 });
      }
    }
  }

  return nodes
    .map((node, index) => ({
      node,
      index,
      depth: depths.has(node.id) ? depths.get(node.id) : Math.max(1, depths.size),
      reachable: depths.has(node.id)
    }))
    .sort((left, right) => left.depth - right.depth || left.index - right.index);
}

function countDialogueChoices(dialogue) {
  return (dialogue.nodes || []).reduce((count, node) => count + (node.choices || []).length, 0);
}

function renderBadge(value) {
  return `<span class="meta-badge">${escapeHtml(value)}</span>`;
}

function renderFieldSections(container, fields, entity) {
  container.innerHTML = "";
  const rendered = new Set();

  for (const section of FIELD_SECTIONS) {
    const keys = section.keys.filter((key) => fields.includes(key));
    if (keys.length === 0) {
      continue;
    }
    container.appendChild(createFieldSection(section.title, keys, entity));
    keys.forEach((key) => rendered.add(key));
  }

  const otherKeys = fields.filter((key) => !rendered.has(key));
  if (otherKeys.length > 0) {
    container.appendChild(createFieldSection("Other", otherKeys, entity));
  }
}

function createFieldSection(title, keys, entity) {
  const sectionEl = document.createElement("section");
  sectionEl.className = "field-section";
  sectionEl.innerHTML = `
    <h3>${escapeHtml(title)}</h3>
    <div class="field-grid"></div>
  `;
  const grid = sectionEl.querySelector(".field-grid");
  for (const key of keys) {
    const value = entity[key];
    if (value !== null && typeof value === "object") {
      continue;
    }
    grid.appendChild(createFieldInput(key, value));
  }
  return sectionEl;
}

function createFieldInput(key, value) {
  const wrap = document.createElement("div");
  wrap.className = `field ${MULTILINE_KEYS.has(key) ? "full" : ""}`;
  const disabled = key === "id" ? "disabled" : "";
  if (MULTILINE_KEYS.has(key)) {
    wrap.innerHTML = `
      <label for="field-${escapeHtml(key)}">${escapeHtml(formatFacetName(key))}</label>
      <textarea id="field-${escapeHtml(key)}" data-key="${escapeHtml(key)}" ${disabled}>${escapeHtml(String(value ?? ""))}</textarea>
    `;
  } else {
    wrap.innerHTML = `
      <label for="field-${escapeHtml(key)}">${escapeHtml(formatFacetName(key))}</label>
      <input id="field-${escapeHtml(key)}" data-key="${escapeHtml(key)}" value="${escapeHtml(String(value ?? ""))}" ${disabled} />
    `;
  }
  return wrap;
}

function renderDashboard() {
  const metrics = REGISTRY_CONFIGS.filter((config) => config.collection).map((config) => {
    return {
      label: config.label,
      count: getCollection(config).length
    };
  });
  const dirty = [...state.dirtyFiles];
  ELEMENTS.editorView.innerHTML = `
    <div class="dashboard">
      <div class="editor-head">
        <div>
          <h2>Обзор контента</h2>
          <div class="editor-meta">Источник: games/rb-dark-rpg/design/data</div>
        </div>
      </div>
      <div class="dashboard-grid">
        ${metrics.map((metric) => `
          <div class="metric">
            <strong>${metric.count}</strong>
            <span>${escapeHtml(metric.label)}</span>
          </div>
        `).join("")}
      </div>
      ${renderAssetStrip()}
      <div class="inspector-block">
        <h2>Changed files</h2>
        <div class="status-list">
          ${dirty.length === 0 ? '<div class="pill"><strong>Clean</strong><span>no local edits</span></div>' : dirty.map((file) => `<div class="pill"><strong>${escapeHtml(file)}</strong><span>dirty</span></div>`).join("")}
        </div>
      </div>
    </div>
  `;
}

function renderValidationView() {
  const groups = state.validation.length
    ? state.validation.map(renderValidationItem).join("")
    : `<div class="validation-item ok"><strong>OK</strong><span>Broken references were not found.</span></div>`;
  ELEMENTS.editorView.innerHTML = `
    <div class="editor-head">
      <div>
        <h2>Проверка данных</h2>
        <div class="editor-meta">Дубли id, битые ссылки, квестовые шаги, диалоговые переходы, stat keys.</div>
      </div>
    </div>
    <div class="validation-list">${groups}</div>
  `;
}

function renderSidePanels() {
  const dirty = [...state.dirtyFiles];
  ELEMENTS.fileStatus.innerHTML = `
    <div class="pill"><strong>${getStorageModeLabel()}</strong><span>${canSaveToSource() ? "save enabled" : "download fallback"}</span></div>
    <div class="pill"><strong>${dirty.length}</strong><span>changed files</span></div>
  `;
  if (dirty.length > 0) {
    ELEMENTS.fileStatus.innerHTML += dirty
      .map((file) => `<div class="pill"><strong>${escapeHtml(file)}</strong><span>dirty</span></div>`)
      .join("");
  }

  const config = getActiveConfig();
  const entity = getSelectedEntity(config);
  ELEMENTS.referenceSummary.innerHTML = entity
    ? renderReferenceSummary(entity)
    : `<div class="reference-item"><strong>No entity</strong><span>select an item</span></div>`;

  ELEMENTS.validationList.innerHTML = state.validation.length
    ? state.validation.slice(0, 12).map(renderValidationItem).join("")
    : `<div class="validation-item ok"><strong>OK</strong><span>No errors.</span></div>`;
}

function renderReferenceSummary(entity) {
  const refs = collectReferences(entity);
  if (refs.length === 0) {
    return `<div class="reference-item"><strong>No refs</strong><span>object has no known ids</span></div>`;
  }
  return refs
    .slice(0, 18)
    .map((ref) => {
      const resolved = hasRegistryId(ref.registry, ref.value);
      const asset = ref.registry === "assets" ? getAssetById(ref.value) : null;
      const url = asset ? getAssetUrl(asset) : "";
      return `
        <div class="reference-item ${resolved ? "resolved" : "missing"}">
          ${url ? renderThumb({ url, kind: asset.kind || "asset" }, ref.value) : ""}
          <strong>${escapeHtml(ref.value)}</strong>
          <span>${escapeHtml(ref.path)} -> ${escapeHtml(ref.registry)}</span>
        </div>
      `;
    })
    .join("");
}

function renderValidationItem(item) {
  return `
    <div class="validation-item ${escapeHtml(item.level)}">
      <strong>${escapeHtml(item.title)}</strong>
      <span>${escapeHtml(item.message)}</span>
    </div>
  `;
}

function renderVisualPreview(entity, config) {
  const previews = getEntityPreviews(entity, config);
  if (previews.length === 0) {
    return "";
  }
  const hero = previews[0];
  const extra = previews.slice(1, 5);
  return `
    <section class="visual-preview" aria-label="Visual preview">
      <div class="visual-hero ${hero.kind || "asset"}">
        <img src="${escapeHtml(hero.url)}" alt="${escapeHtml(hero.label)}" loading="lazy" />
      </div>
      <div class="visual-meta">
        <strong>${escapeHtml(hero.label)}</strong>
        <span>${escapeHtml(hero.assetId || hero.kind || "asset")}</span>
      </div>
      ${extra.length > 0 ? `
        <div class="visual-strip">
          ${extra.map((preview) => renderThumb(preview, preview.label)).join("")}
        </div>
      ` : ""}
    </section>
  `;
}

function renderAssetStrip() {
  const assets = getCollection(getConfig("assets"))
    .map((asset) => ({ asset, url: getAssetUrl(asset) }))
    .filter((entry) => entry.url)
    .slice(0, 12);
  if (assets.length === 0) {
    return "";
  }
  return `
    <section class="asset-strip" aria-label="Available art">
      <h3>Доступный арт</h3>
      <div class="asset-strip-grid">
        ${assets.map(({ asset, url }) => `
          <figure>
            <img src="${escapeHtml(url)}" alt="${escapeHtml(asset.id)}" loading="lazy" />
            <figcaption>${escapeHtml(asset.id)}</figcaption>
          </figure>
        `).join("")}
      </div>
    </section>
  `;
}

function renderThumb(preview, alt) {
  return `
    <span class="thumb ${escapeHtml(preview.kind || "asset")}">
      <img src="${escapeHtml(preview.url)}" alt="${escapeHtml(alt)}" loading="lazy" />
    </span>
  `;
}

function getEntityPreview(entity, config) {
  return getEntityPreviews(entity, config)[0] || null;
}

function getEntityPreviews(entity, config) {
  if (!entity) {
    return [];
  }

  if (config?.id === "assets") {
    const url = getAssetUrl(entity);
    return url ? [{ url, label: entity.id, assetId: entity.id, kind: entity.kind }] : [];
  }

  const seen = new Set();
  const refs = collectReferences(entity).filter((ref) => ref.registry === "assets");
  const previews = [];
  for (const ref of refs) {
    if (seen.has(ref.value)) {
      continue;
    }
    seen.add(ref.value);
    const asset = getAssetById(ref.value);
    const url = getAssetUrl(asset);
    if (asset && url) {
      previews.push({
        url,
        label: asset.id,
        assetId: asset.id,
        kind: asset.kind || "asset",
        path: ref.path
      });
    }
  }
  return previews.sort((a, b) => previewPriority(a) - previewPriority(b));
}

function previewPriority(preview) {
  const kind = preview.kind || "";
  if (kind === "environment") {
    return 0;
  }
  if (kind === "portrait" || kind === "character") {
    return 1;
  }
  if (kind === "item_icon") {
    return 2;
  }
  return 3;
}

function getAssetById(assetId) {
  return getCollection(getConfig("assets")).find((asset) => asset.id === assetId) || null;
}

function getAssetUrl(asset) {
  if (!asset) {
    return "";
  }
  const filePath = asset.file_path || asset.preview_path || extractAssetPath(asset.source);
  if (!filePath) {
    return "";
  }
  const normalized = filePath.replace(/\\/g, "/");
  if (/^https?:\/\//.test(normalized) || normalized.startsWith("data:")) {
    return normalized;
  }
  if (normalized.startsWith("assets/")) {
    const assetRelativePath = normalized.slice("assets/".length);
    return state.apiAvailable ? `/game-assets/${encodePath(assetRelativePath)}` : `../../${encodePath(normalized)}`;
  }
  if (normalized.startsWith("games/rb-dark-rpg/assets/")) {
    const assetRelativePath = normalized.slice("games/rb-dark-rpg/assets/".length);
    return state.apiAvailable ? `/game-assets/${encodePath(assetRelativePath)}` : `../../assets/${encodePath(assetRelativePath)}`;
  }
  return encodePath(normalized);
}

function extractAssetPath(source) {
  if (!source || typeof source !== "string") {
    return "";
  }
  const normalized = source.replace(/\\/g, "/");
  return /\.(png|jpe?g|webp|gif|svg)$/i.test(normalized) ? normalized : "";
}

function encodePath(value) {
  return value
    .split("/")
    .map((part) => encodeURIComponent(part))
    .join("/");
}

function applyRawJson(config) {
  try {
    const nextEntity = JSON.parse(state.rawDraft);
    if (!nextEntity || typeof nextEntity !== "object" || Array.isArray(nextEntity)) {
      throw new Error("Object JSON must be a JSON object.");
    }
    if (!nextEntity.id) {
      throw new Error("Object must keep an id.");
    }
    const collection = getCollection(config);
    const index = collection.findIndex((entity) => entity.id === getSelectedId(config));
    if (index < 0) {
      throw new Error("Selected entity was not found.");
    }
    collection[index] = nextEntity;
    state.selectedIds.set(config.id, nextEntity.id);
    state.rawDraft = JSON.stringify(nextEntity, null, 2);
    markDirty(config.file);
    validateContent();
    renderAll();
    setStatus(`Applied JSON for ${nextEntity.id}.`);
  } catch (error) {
    setStatus(`JSON error: ${error.message}`);
  }
}

function formatRawJson() {
  try {
    const parsed = JSON.parse(state.rawDraft);
    state.rawDraft = JSON.stringify(parsed, null, 2);
    syncRawTextarea();
    setStatus("JSON formatted.");
  } catch (error) {
    setStatus(`JSON error: ${error.message}`);
  }
}

function syncRawTextarea() {
  const rawJson = ELEMENTS.editorView.querySelector("#rawJson");
  if (rawJson && document.activeElement !== rawJson) {
    rawJson.value = state.rawDraft;
  }
}

async function saveCurrentScope() {
  const config = getActiveConfig();
  const files = config?.file ? [config.file] : [...state.dirtyFiles];
  if (files.length === 0) {
    return;
  }
  if (!canSaveToSource()) {
    for (const file of files) {
      downloadFile(file);
    }
    setStatus("Server save is unavailable. Downloaded changed JSON files.");
    return;
  }

  for (const file of files) {
    await saveFile(file);
  }
  validateContent();
  renderAll();
}

async function saveFile(file) {
  const data = state.content.files[file];
  if (state.folderAvailable) {
    const handle = state.fileHandles.get(file);
    if (!handle) {
      setStatus(`Save failed: no folder handle for ${file}.`);
      return;
    }
    const writable = await handle.createWritable();
    await writable.write(`${JSON.stringify(data, null, 2)}\n`);
    await writable.close();
    state.dirtyFiles.delete(file);
    setStatus(`Saved ${file} through browser file access.`);
    return;
  }

  const response = await fetch("/api/save", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ file, data })
  });
  if (!response.ok) {
    const payload = await response.json().catch(() => ({}));
    setStatus(`Save failed: ${payload.error || response.status}`);
    return;
  }
  state.dirtyFiles.delete(file);
  setStatus(`Saved ${file}.`);
}

function canSaveToSource() {
  return state.apiAvailable || state.folderAvailable;
}

function getStorageModeLabel() {
  if (state.apiAvailable) {
    return "Server";
  }
  if (state.folderAvailable) {
    return "Folder";
  }
  return "Read-only";
}

function downloadFile(file) {
  const data = state.content.files[file];
  const blob = new Blob([`${JSON.stringify(data, null, 2)}\n`], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = file;
  anchor.click();
  URL.revokeObjectURL(url);
}

function markDirty(file) {
  if (!file) {
    return;
  }
  state.dirtyFiles.add(file);
  renderChromeState();
  setStatus(`${file} changed.`);
}

function validateContent() {
  const issues = [];
  const registries = buildRegistrySets();

  for (const config of REGISTRY_CONFIGS.filter((item) => item.collection)) {
    const seen = new Set();
    for (const entity of getCollection(config)) {
      if (!entity.id) {
        issues.push({
          level: "error",
          title: `${config.label}: missing id`,
          message: `${config.file}/${config.collection} contains entity without id.`
        });
        continue;
      }
      if (seen.has(entity.id)) {
        issues.push({
          level: "error",
          title: `${config.label}: duplicate id`,
          message: entity.id
        });
      }
      seen.add(entity.id);
    }
  }

  for (const config of REGISTRY_CONFIGS.filter((item) => item.collection)) {
    for (const entity of getCollection(config)) {
      for (const ref of collectReferences(entity)) {
        if (!registries[ref.registry]?.has(ref.value)) {
          issues.push({
            level: "error",
            title: `Broken ref: ${ref.value}`,
            message: `${config.file}/${entity.id}/${ref.path} -> ${ref.registry}`
          });
        }
      }
    }
  }

  validateQuestSteps(issues);
  validateDialogues(issues);
  validateItemStats(issues);

  state.validation = issues;
}

function validateQuestSteps(issues) {
  const objectiveTypes = new Set(state.content.files["quests.json"]?.objective_types || []);
  for (const quest of getCollection(getConfig("quests"))) {
    for (const step of quest.steps || []) {
      if (!step.id || !step.title) {
        issues.push({
          level: "warn",
          title: `Quest step incomplete: ${quest.id}`,
          message: step.id || "missing step id"
        });
      }
      if (!step.objective?.type) {
        issues.push({
          level: "error",
          title: `Quest step has no objective: ${quest.id}`,
          message: step.id || "missing step id"
        });
      } else if (objectiveTypes.size > 0 && !objectiveTypes.has(step.objective.type)) {
        issues.push({
          level: "error",
          title: `Unknown objective type: ${step.objective.type}`,
          message: `${quest.id}/${step.id}`
        });
      }
      validateQuestObjectiveRefs(quest, step, issues);
    }
  }
}

function validateQuestObjectiveRefs(quest, step, issues) {
  const objective = step.objective || {};
  const objectiveTargetRules = {
    talk_to_npc: ["target_id", "characters"],
    return_to_npc: ["target_id", "characters"],
    visit_location: ["target_id", "locations"],
    win_encounter: ["encounter_id", "encounters"],
    equip_item: ["item_id", "items"],
    collect_item: ["item_id", "items"]
  };
  const rule = objectiveTargetRules[objective.type];
  if (!rule) {
    return;
  }
  const [field, registry] = rule;
  const value = objective[field];
  if (!value) {
    issues.push({
      level: "error",
      title: `Quest objective missing target: ${quest.id}`,
      message: `${step.id}/${objective.type} needs ${field}`
    });
    return;
  }
  if (!hasRegistryId(registry, value)) {
    issues.push({
      level: "error",
      title: `Quest objective broken ref: ${value}`,
      message: `${quest.id}/${step.id}/${field} -> ${registry}`
    });
  }
}

function validateDialogues(issues) {
  for (const dialogue of getCollection(getConfig("dialogues"))) {
    const nodes = new Set((dialogue.nodes || []).map((node) => node.id));
    if (dialogue.entry_node_id && !nodes.has(dialogue.entry_node_id)) {
      issues.push({
        level: "error",
        title: `Dialogue entry missing: ${dialogue.id}`,
        message: dialogue.entry_node_id
      });
    }
    for (const node of dialogue.nodes || []) {
      for (const choice of node.choices || []) {
        if (choice.next_node_id && !nodes.has(choice.next_node_id)) {
          issues.push({
            level: "error",
            title: `Dialogue node link missing: ${dialogue.id}`,
            message: `${node.id}/${choice.id} -> ${choice.next_node_id}`
          });
        }
      }
    }
  }
}

function validateItemStats(issues) {
  const combat = state.content.files["combat.json"] || {};
  const statKeys = new Set([
    ...Object.keys(combat.stat_definitions || {}),
    ...(combat.visible_stats || []).map((stat) => stat.id).filter(Boolean)
  ]);
  if (statKeys.size === 0) {
    return;
  }
  for (const item of getCollection(getConfig("items"))) {
    const statSources = [item.stats, item.stat_bonus, item.stat_bonuses].filter(Boolean);
    for (const statSource of statSources) {
      for (const statKey of Object.keys(statSource)) {
        if (!statKeys.has(statKey)) {
          issues.push({
            level: "error",
            title: `Unknown stat: ${statKey}`,
            message: `${item.id} uses stat not defined in combat.json`
          });
        }
      }
    }
  }
}

function collectReferences(value, path = []) {
  const refs = [];
  if (Array.isArray(value)) {
    value.forEach((entry, index) => {
      refs.push(...collectReferences(entry, [...path, String(index)]));
    });
    return refs;
  }
  if (!value || typeof value !== "object") {
    return refs;
  }
  for (const [key, entry] of Object.entries(value)) {
    const nextPath = [...path, key];
    if (typeof entry === "string" && REF_RULES[key]) {
      refs.push({ path: nextPath.join("."), registry: REF_RULES[key], value: entry });
    } else if (Array.isArray(entry) && ARRAY_REF_RULES[key]) {
      for (const item of entry) {
        if (typeof item === "string") {
          refs.push({ path: nextPath.join("."), registry: ARRAY_REF_RULES[key], value: item });
        }
      }
    }
    refs.push(...collectReferences(entry, nextPath));
  }
  return refs;
}

function buildRegistrySets() {
  const sets = {};
  for (const config of REGISTRY_CONFIGS.filter((item) => item.collection)) {
    sets[config.id] = new Set(getCollection(config).map((entity) => entity.id).filter(Boolean));
  }
  return sets;
}

function hasRegistryId(registry, id) {
  const config = getConfig(registry);
  return getCollection(config).some((entity) => entity.id === id);
}

function ensureSelection() {
  const config = getActiveConfig();
  if (!config?.collection) {
    return;
  }
  const collection = getCollection(config);
  const current = state.selectedIds.get(config.id);
  if (!current || !collection.some((entity) => entity.id === current)) {
    const first = collection[0];
    if (first?.id) {
      state.selectedIds.set(config.id, first.id);
      state.rawDraft = JSON.stringify(first, null, 2);
    }
  }
}

function getActiveConfig() {
  return getConfig(state.activeView);
}

function getConfig(id) {
  return REGISTRY_CONFIGS.find((config) => config.id === id);
}

function getCollection(config) {
  if (!config?.file || !config.collection || !state.content?.files?.[config.file]) {
    return [];
  }
  return state.content.files[config.file][config.collection] || [];
}

function getRegistryEntity(registry, id) {
  if (!id) {
    return null;
  }
  return getCollection(getConfig(registry)).find((entity) => entity.id === id) || null;
}

function getRegistryLabel(registry, id) {
  const entity = getRegistryEntity(registry, id);
  return entity ? getEntityTitle(entity) : "";
}

function getDialogueParticipantsLabel(dialogue) {
  const participants = dialogue?.participants || [];
  if (participants.length === 0) {
    return "No participants";
  }
  return participants
    .map((id) => getRegistryLabel("characters", id) || id)
    .join(" / ");
}

function getSelectedId(config) {
  return state.selectedIds.get(config?.id);
}

function getSelectedEntity(config) {
  if (!config?.collection) {
    return null;
  }
  const selectedId = getSelectedId(config);
  return getCollection(config).find((entity) => entity.id === selectedId) || null;
}

function getEntityTitle(entity) {
  return entity.display_name || entity.title || entity.name || entity.id || "Untitled";
}

function getCharacterDescription(entity) {
  return entity.short_description
    || entity.description
    || entity.role
    || entity.home_location_id
    || "No short description yet.";
}

function getInitials(value) {
  const parts = String(value || "")
    .split(/\s+/)
    .filter(Boolean)
    .slice(0, 2);
  if (parts.length === 0) {
    return "?";
  }
  return parts.map((part) => part[0]).join("").toUpperCase();
}

function getEntitySubtitle(config, entity) {
  const values = (config.subtitleKeys || [])
    .map((key) => entity[key])
    .filter((value) => value !== undefined && value !== null && value !== "");
  return values.length ? values.join(" / ") : entity.id || "";
}

function entityMatchesSearch(entity, search) {
  if (!search) {
    return true;
  }
  const haystack = [
    entity.id,
    entity.display_name,
    entity.title,
    entity.kind,
    entity.type,
    entity.role,
    entity.description
  ]
    .filter(Boolean)
    .join(" ")
    .toLowerCase();
  return haystack.includes(search);
}

function entityMatchesFilters(config, entity) {
  const filters = getFilterState(config);
  for (const [key, value] of Object.entries(filters)) {
    if (!value) {
      continue;
    }
    const values = getFacetValues(entity, key);
    if (!values.includes(value)) {
      return false;
    }
  }
  return true;
}

function getFilterState(config) {
  return state.filters.get(config?.id) || {};
}

function getActiveGroupBy(config) {
  if (!config?.collection) {
    return "";
  }
  if (state.groupBy.has(config.id)) {
    return state.groupBy.get(config.id);
  }
  return (config.groupKeys || [])[0] || "";
}

function getAvailableFacetKeys(config, collection, candidateKeys) {
  return candidateKeys.filter((key) => getFacetOptions(collection, key).length > 0);
}

function getFacetOptions(collection, key) {
  const values = new Set();
  for (const entity of collection) {
    for (const value of getFacetValues(entity, key)) {
      values.add(value);
    }
  }
  return [...values].sort((a, b) => a.localeCompare(b));
}

function getPrimaryFacetValue(entity, key) {
  return getFacetValues(entity, key)[0] || "";
}

function getFacetValues(entity, key) {
  const raw = readEntityPath(entity, key);
  if (Array.isArray(raw)) {
    return raw
      .map((value) => normalizeFacetValue(value))
      .filter(Boolean);
  }
  const value = normalizeFacetValue(raw);
  return value ? [value] : [];
}

function readEntityPath(entity, key) {
  if (!key.includes(".")) {
    return entity[key];
  }
  return key.split(".").reduce((current, part) => current?.[part], entity);
}

function normalizeFacetValue(value) {
  if (value === undefined || value === null || value === "") {
    return "";
  }
  if (typeof value === "object") {
    return value.id || value.type || "";
  }
  return String(value);
}

function formatFacetName(key) {
  return key
    .replace(/\./g, " / ")
    .replace(/_/g, " ")
    .replace(/\b\w/g, (letter) => letter.toUpperCase());
}

function coerceFieldValue(nextValue, previousValue) {
  if (typeof previousValue === "number") {
    const parsed = Number(nextValue);
    return Number.isFinite(parsed) ? parsed : previousValue;
  }
  if (typeof previousValue === "boolean") {
    return nextValue === "true";
  }
  return nextValue;
}

function setStatus(message) {
  ELEMENTS.statusBar.textContent = message;
}

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}
