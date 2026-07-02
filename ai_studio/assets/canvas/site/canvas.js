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
  bringNodeForward,
  bringNodeToFront,
  createGroupFromSelection,
  deleteSelectedElements,
  deleteSelectedRegions,
  redo,
  sendNodeBackward,
  sendNodeToBack,
  undo,
} from "./actions.js";
import { initHome, render as renderHome } from "./home.js";
import {
  cancelPolygonDraft,
  finishPolygonDraft,
  fit,
  initWorkspace,
  popPolygonVertex,
  setRegionTool,
  setTool,
  zoomTo,
} from "./workspace.js";
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
    if (element) {
      enterRegionEdit(regions);
      if ((element.regions || []).length) state.selectedRegionIds = new Set([element.regions[0].id]);
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

// The single selected NODE id for z-order shortcuts — the selected group, or the one
// selected element — or null. Null in region-edit mode (the keys belong to region work)
// and for a multi-element selection (a batched multi-node reorder would be N journal
// entries, so Ctrl+[/] stays a single-node gesture; use the layers drag / Order menu per
// node for a multi-selection).
function soloNodeId() {
  if (state.regionEditId) return null;
  if (state.selectedGroupId) return state.selectedGroupId;
  return state.selectedIds.size === 1 ? [...state.selectedIds][0] : null;
}

function onKeyDown(event) {
  // Shortcuts match physical keys (event.code), NOT event.key: event.key is
  // layout-dependent — under a Cyrillic layout Ctrl+Z arrives as key "я", the
  // comparison misses, preventDefault never runs, and the browser default wins.
  const code = event.code;
  const key = event.key.toLowerCase();
  const meta = event.ctrlKey || event.metaKey;
  if (isTypingTarget(event.target)) return; // let inspector/inline fields type freely

  // Home view: nothing to do beyond letting inputs work.
  if (!state.project) return;

  // A polygon draft owns Ctrl+Z / Backspace / Delete (pop the last placed vertex) and
  // Enter (close). This MUST run before global undo AND its region-edit clamp so a draft
  // edit never reaches the journal — the draft is pure UI state.
  if (state.regionEditId && state.regionTool === "polygon" && state.polygonDraft.length) {
    if ((meta && code === "KeyZ") || code === "Backspace" || code === "Delete") {
      event.preventDefault();
      popPolygonVertex();
      return;
    }
  }
  if (state.regionEditId && state.regionTool === "polygon" && code === "Enter") {
    event.preventDefault();
    finishPolygonDraft(); // no-op below 3 points
    return;
  }

  if (meta && code === "KeyZ") {
    event.preventDefault();
    if (event.shiftKey) redo();
    else undo();
    return;
  }
  if (meta && code === "KeyY") {
    event.preventDefault();
    redo();
    return;
  }
  if (meta && code === "KeyG") {
    event.preventDefault();
    createGroupFromSelection("New group");
    return;
  }
  // Z-order by physical key (event.code so it works on any layout): Ctrl+] forward,
  // Ctrl+[ backward, add Alt for to-front / to-back. Applies to a single selected NODE —
  // an element OR a group — among its merged siblings, never in region-edit mode.
  if (meta && code === "BracketRight") {
    event.preventDefault();
    const id = soloNodeId();
    if (id) (event.altKey ? bringNodeToFront : bringNodeForward)(id);
    return;
  }
  if (meta && code === "BracketLeft") {
    event.preventDefault();
    const id = soloNodeId();
    if (id) (event.altKey ? sendNodeToBack : sendNodeBackward)(id);
    return;
  }
  if (meta) return; // leave other browser shortcuts alone

  if (code === "Space") {
    if (!state.spacePan) {
      state.spacePan = true;
      el("stage").classList.add("space-pan");
    }
    event.preventDefault();
    return;
  }
  if (code === "KeyV") {
    // V = Select globally; inside region-edit it also drops the draw tool back to Select.
    setTool("select");
    if (state.regionEditId) setRegionTool("select");
    return;
  }
  if (code === "KeyH") return setTool("pan");
  if (code === "Digit0" || code === "Numpad0") {
    event.preventDefault();
    return fit();
  }
  if (code === "Digit1" || code === "Numpad1") {
    event.preventDefault();
    return zoomTo(1);
  }
  if (code === "Digit2" || code === "Numpad2") {
    event.preventDefault();
    return zoomTo(2);
  }
  if (key === "escape") {
    // Escape never leaves the project. It unwinds one level at a time: close the menu;
    // inside region-edit, cancel an open polygon draft, else drop a draw tool back to
    // Select, else exit isolation; finally clear element/group selection.
    closeContextMenu();
    if (state.regionEditId) {
      if (state.regionTool === "polygon" && state.polygonDraft.length) {
        cancelPolygonDraft();
        return;
      }
      if (state.regionTool !== "select") {
        setRegionTool("select");
        refresh();
        return;
      }
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
  if (event.code === "Space") {
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
