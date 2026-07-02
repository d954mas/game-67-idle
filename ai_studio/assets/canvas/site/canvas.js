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
  groupById,
  hooks,
  lastProjectId,
  loadProjects,
  refresh,
  refreshHistory,
  rememberLastProject,
  selectGroupOnly,
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
  reorderNodesBy,
  selectedNodeIds,
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
import { loadCanvasFonts } from "./fonts.js";

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
}

hooks.openProject = openProject;
hooks.showHome = showHome;

// ---- global keyboard ---------------------------------------------------------

function isTypingTarget(target) {
  return /^(input|textarea|select)$/i.test(target && target.tagName ? target.tagName : "");
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

  // Alt-hold peeks at the clip-ghost of a selected element's clipped-out portion — hidden
  // by default (T0224 item 6). View-state only (never journaled/persisted); onKeyUp clears
  // it. Repaint so the ghost appears immediately. Falls through (no preventDefault) so Alt
  // still composes with the z-order shortcuts (Ctrl+Alt+[ / ]).
  if (event.altKey && !state.clipGhostPeek) {
    state.clipGhostPeek = true;
    hooks.renderCanvas();
  }

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
  // Ctrl+[ backward, add Alt for to-front / to-back. Acts on the selected NODES — one
  // element or group nudges via its single-node op; a 2+ selection moves as ONE block
  // (reorderNodes, one journal entry, relative order preserved). Never in region-edit mode.
  if (meta && code === "BracketRight") {
    event.preventDefault();
    if (state.regionEditId) return;
    const ids = selectedNodeIds();
    if (ids.length >= 2) reorderNodesBy(event.altKey ? "front" : "forward");
    else if (ids.length === 1) (event.altKey ? bringNodeToFront : bringNodeForward)(ids[0]);
    return;
  }
  if (meta && code === "BracketLeft") {
    event.preventDefault();
    if (state.regionEditId) return;
    const ids = selectedNodeIds();
    if (ids.length >= 2) reorderNodesBy(event.altKey ? "back" : "backward");
    else if (ids.length === 1) (event.altKey ? sendNodeToBack : sendNodeBackward)(ids[0]);
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
    // Select, else exit isolation; else step UP one entered-group scope (selecting the
    // group just exited); finally clear element/group selection.
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
    if (state.enteredGroupId) {
      const exited = state.enteredGroupId;
      const group = groupById(exited);
      state.enteredGroupId = group ? group.parentId || null : null;
      selectGroupOnly(exited); // select the group we stepped out of, at the parent scope
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
  // Alt released: drop the clip-ghost peek (view-state only) and repaint so it hides.
  if (!event.altKey && state.clipGhostPeek) {
    state.clipGhostPeek = false;
    hooks.renderCanvas();
  }
}

// ---- boot --------------------------------------------------------------------

initHome();
initWorkspace();
initLayers();
initInspector();
initContextMenu();
initDnd();
// Load the bundled text fonts (FontFace API); gate the first text paint on
// document.fonts.ready by repainting the canvas once they resolve.
loadCanvasFonts(() => hooks.renderCanvas());

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
  }
})();
