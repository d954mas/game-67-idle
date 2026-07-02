// Canvas page controller. Boots the shared state, wires the feature modules
// (home, workspace, layers, inspector, context menu, drag&drop), owns view
// routing (home grid <-> workspace) with deep links + last-opened restore, and
// the global keyboard shortcuts. Holds no rendering/business logic of its own —
// it delegates to the modules and the shared actions.
import {
  api,
  clearImageCache,
  clearSelection,
  el,
  elementById,
  enterRegionEdit,
  exitRegionEdit,
  hooks,
  lastProjectId,
  loadProjects,
  refresh,
  refreshHistory,
  rememberLastProject,
  selectOnly,
  setProjectParam,
  setStatus,
  state,
} from "./app.js";
import {
  createGroupFromSelection,
  deleteSelectedElements,
  deleteSelectedRegions,
  redo,
  undo,
} from "./actions.js";
import { initHome, render as renderHome } from "./home.js";
import { fit, initWorkspace, setTool, zoomTo } from "./workspace.js";
import { initLayers } from "./layers_panel.js";
import { initInspector } from "./inspector.js";
import { closeContextMenu, initContextMenu } from "./context_menu.js";
import { initDnd } from "./dnd.js";

// ---- view routing ------------------------------------------------------------

function enterWorkspace() {
  el("home-view").classList.add("hidden");
  el("workspace-view").classList.remove("hidden");
}

async function showHome() {
  closeContextMenu();
  state.project = null;
  clearSelection();
  rememberLastProject(null);
  setProjectParam(null);
  el("workspace-view").classList.add("hidden");
  el("home-view").classList.remove("hidden");
  await loadProjects();
  renderHome();
  setStatus("Ready.");
}

// Open a project into the workspace. `select` is the optional ?select=<id> debug
// hook (documented in the README) that pre-selects one element for screenshots;
// `regions` is the sibling ?regions=<id> hook that opens straight into region-edit
// isolation (mode B) with the element's first region selected, for screenshots.
async function openProject(id, { select, regions } = {}) {
  closeContextMenu();
  try {
    state.project = (await api("GET", `/projects/${id}`)).project;
  } catch (error) {
    setStatus(error.message, true);
    await showHome();
    return;
  }
  clearSelection();
  clearImageCache();
  if (regions) {
    const element = elementById(regions);
    if (element && (element.regions || []).length) {
      enterRegionEdit(regions);
      state.selectedRegionIds = new Set([element.regions[0].id]);
    }
  } else if (select && elementById(select)) {
    selectOnly(select);
  }
  await refreshHistory();
  rememberLastProject(id);
  setProjectParam(id);
  enterWorkspace();
  fit();
  refresh();
  setStatus(`Opened ${state.project.title}.`);
}

hooks.openProject = openProject;
hooks.showHome = showHome;

// ---- global keyboard ---------------------------------------------------------

function isTypingTarget(target) {
  return /^(input|textarea|select)$/i.test(target && target.tagName ? target.tagName : "");
}

function onKeyDown(event) {
  const key = event.key.toLowerCase();
  const meta = event.ctrlKey || event.metaKey;
  if (isTypingTarget(event.target)) return; // let inspector/inline fields type freely

  // Home view: nothing to do beyond letting inputs work.
  if (!state.project) return;

  if (meta && key === "z") {
    event.preventDefault();
    if (event.shiftKey) redo();
    else undo();
    return;
  }
  if (meta && key === "y") {
    event.preventDefault();
    redo();
    return;
  }
  if (meta && key === "g") {
    event.preventDefault();
    createGroupFromSelection("New screen");
    return;
  }
  if (meta) return; // leave other browser shortcuts alone

  if (key === " ") {
    if (!state.spacePan) {
      state.spacePan = true;
      el("stage").classList.add("space-pan");
    }
    event.preventDefault();
    return;
  }
  if (key === "v") return setTool("select");
  if (key === "h") return setTool("pan");
  if (key === "0") {
    event.preventDefault();
    return fit();
  }
  if (key === "1") {
    event.preventDefault();
    return zoomTo(1);
  }
  if (key === "2") {
    event.preventDefault();
    return zoomTo(2);
  }
  if (key === "escape") {
    // Escape never leaves the project. It unwinds one level at a time: close the
    // menu, then exit region-edit isolation (clearing region selection + mode),
    // then clear element/group selection.
    closeContextMenu();
    if (state.regionEditId) {
      exitRegionEdit();
      refresh();
      return;
    }
    clearSelection();
    refresh();
    return;
  }
  if (key === "delete" || key === "backspace") {
    // In region-edit mode, Delete only removes selected regions (never the image
    // being edited); otherwise it removes selected elements.
    if (state.regionEditId) {
      if (state.selectedRegionIds.size) {
        event.preventDefault();
        deleteSelectedRegions();
      }
    } else if (state.selectedIds.size) {
      event.preventDefault();
      deleteSelectedElements();
    }
  }
}

function onKeyUp(event) {
  if (event.key === " ") {
    state.spacePan = false;
    el("stage").classList.remove("space-pan");
  }
}

// ---- boot --------------------------------------------------------------------

initHome();
initWorkspace();
initLayers();
initInspector();
initContextMenu();
initDnd();

el("back-projects").addEventListener("click", () => showHome());
window.addEventListener("keydown", onKeyDown);
window.addEventListener("keyup", onKeyUp);

(async () => {
  try {
    await loadProjects();
  } catch (error) {
    setStatus(error.message, true);
    return;
  }
  const params = new URLSearchParams(window.location.search);
  const projectParam = params.get("project");
  const selectParam = params.get("select") || undefined;
  const regionsParam = params.get("regions") || undefined;
  const known = (id) => state.projects.some((project) => project.id === id);

  if (projectParam && known(projectParam)) {
    await openProject(projectParam, { select: selectParam, regions: regionsParam });
  } else if (lastProjectId() && known(lastProjectId())) {
    await openProject(lastProjectId());
  } else {
    renderHome();
    setStatus("Ready. Create or open a project.");
  }
})();
