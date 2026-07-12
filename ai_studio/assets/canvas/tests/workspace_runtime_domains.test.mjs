import assert from "node:assert/strict";
import test from "node:test";

import { createGestureOverlay } from "../site/workspace/gesture_overlay.js";
import { createRenderCulling } from "../site/workspace/render_culling.js";
import { createViewportControls } from "../site/workspace/viewport_controls.js";

test("render culling preserves preview/edit exceptions and rejects offscreen boxes", () => {
  const previewing = new Set(["animated"]);
  const culling = createRenderCulling({
    getViewportSize: () => ({ width: 100, height: 80 }),
    imageToScreenPoint: (point, viewport) => ({ x: point.x * viewport.scale, y: point.y * viewport.scale }),
    isAnimationPreviewing: (id) => previewing.has(id),
    rotatedCorners: (element) => [
      { x: element.x, y: element.y },
      { x: element.x + element.w, y: element.y },
      { x: element.x + element.w, y: element.y + element.h },
      { x: element.x, y: element.y + element.h },
    ],
  });
  const viewport = { scale: 1 };
  const offscreen = { id: "plain", x: 200, y: 200, w: 10, h: 10 };

  assert.equal(culling.elementCullable(offscreen, viewport, null), true);
  assert.equal(culling.elementCullable({ ...offscreen, id: "animated" }, viewport, null), false);
  assert.equal(culling.elementCullable(offscreen, viewport, { id: "plain" }), false);
  assert.equal(culling.screenAABBOffscreen(-10, -10, 10, 10), false);
});

test("gesture overlay draws marquee and both snap-guide axes from live drag state", () => {
  const operations = [];
  const context = new Proxy({}, {
    get(target, key) {
      if (key in target) return target[key];
      return (...args) => operations.push([key, ...args]);
    },
    set(target, key, value) { target[key] = value; return true; },
  });
  let drag = {
    mode: "marquee",
    startWorld: { x: 10, y: 20 },
    lastScreen: { x: 4, y: 8 },
    activeGuides: [
      { axis: "x", pos: 5, min: 1, max: 9 },
      { axis: "y", pos: 7, min: 2, max: 8 },
    ],
  };
  const overlay = createGestureOverlay({
    getContext: () => context,
    getDrag: () => drag,
    getViewport: () => ({ scale: 1 }),
    imageToScreenPoint: (point) => ({ ...point }),
  });

  overlay.drawGestureOverlay();
  overlay.drawSnapGuides({ scale: 1 });
  assert.deepEqual(operations.find(([name]) => name === "fillRect"), ["fillRect", 4, 8, 6, 12]);
  assert.equal(operations.filter(([name]) => name === "moveTo").length, 2);
  assert.equal(operations.filter(([name]) => name === "lineTo").length, 2);
  drag = null;
  assert.doesNotThrow(() => overlay.drawGestureOverlay());
});

test("viewport controls fit visible content, clamp zoom, and sync top-bar state", () => {
  const state = {
    cssWidth: 200,
    cssHeight: 100,
    history: { canUndo: false, canRedo: true },
    project: { title: "Demo" },
    tool: "select",
    viewport: { scale: 1, offsetX: 0, offsetY: 0 },
  };
  const nodes = new Map([
    ["ws-title", {}],
    ["undo", {}],
    ["redo", {}],
  ]);
  const tools = [
    { dataset: { tool: "select" }, classList: { toggle(name, active) { this[name] = active; } } },
    { dataset: { tool: "hand" }, classList: { toggle(name, active) { this[name] = active; } } },
  ];
  let renders = 0;
  const controls = createViewportControls({
    clamp: (value, min, max) => Math.max(min, Math.min(max, value)),
    elements: () => [{ x: 10, y: 20, w: 40, h: 20 }],
    fitViewport: () => ({ scale: 2, offsetX: 30, offsetY: 40 }),
    groups: () => [{ x: -10, y: 0, w: 5, h: 5 }],
    isElementHidden: () => false,
    isNodeHidden: () => false,
    queryTools: () => tools,
    render: () => { renders += 1; },
    resizeCanvas: () => {},
    screenToImagePoint: (point, viewport) => ({
      x: (point.x - viewport.offsetX) / viewport.scale,
      y: (point.y - viewport.offsetY) / viewport.scale,
    }),
    state,
    uiElement: (id) => nodes.get(id),
  });

  controls.fit();
  assert.deepEqual(state.viewport, { scale: 2, offsetX: 50, offsetY: 40 });
  controls.zoomTo(99);
  assert.equal(state.viewport.scale, 12);
  assert.equal(renders, 2);
  controls.syncTopBar();
  assert.equal(nodes.get("ws-title").textContent, "Demo");
  assert.equal(nodes.get("undo").disabled, true);
  assert.equal(nodes.get("redo").disabled, false);
  assert.equal(tools[0].classList.active, true);
  assert.equal(tools[1].classList.active, false);
});
