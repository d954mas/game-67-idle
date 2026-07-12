import assert from "node:assert/strict";
import test from "node:test";

import { createAnimationPreviewController } from "../site/workspace/animation_preview.js";

test("animation preview owns one rAF loop and dispose releases its lifecycle", () => {
  const previousPerformance = globalThis.performance;
  const previousRequest = globalThis.requestAnimationFrame;
  const previousCancel = globalThis.cancelAnimationFrame;
  let now = 1000;
  let nextId = 1;
  const frames = new Map();
  const cancelled = [];
  globalThis.performance = { now: () => now };
  globalThis.requestAnimationFrame = (callback) => {
    const id = nextId++;
    frames.set(id, callback);
    return id;
  };
  globalThis.cancelAnimationFrame = (id) => {
    cancelled.push(id);
    frames.delete(id);
  };

  try {
    const elements = [{ id: "hero", animation: { duration_ms: 1000, channels: [] } }];
    let repaints = 0;
    const controller = createAnimationPreviewController({
      getElements: () => elements,
      getImageForSrc: () => null,
      repaint: () => { repaints += 1; },
    });

    controller.start("hero");
    controller.start("hero");
    assert.equal(frames.size, 1, "duplicate starts must not schedule duplicate loops");
    assert.equal(repaints, 1);

    controller.dispose();
    assert.deepEqual(cancelled, [1]);
    assert.equal(controller.isPreviewing("hero"), false);
    assert.equal(frames.size, 0);
  } finally {
    globalThis.performance = previousPerformance;
    globalThis.requestAnimationFrame = previousRequest;
    globalThis.cancelAnimationFrame = previousCancel;
  }
});

test("animation preview samples kept flipbook frames and prunes once playback", () => {
  const previousPerformance = globalThis.performance;
  const previousRequest = globalThis.requestAnimationFrame;
  const previousCancel = globalThis.cancelAnimationFrame;
  let now = 0;
  let scheduled = null;
  globalThis.performance = { now: () => now };
  globalThis.requestAnimationFrame = (callback) => { scheduled = callback; return 7; };
  globalThis.cancelAnimationFrame = () => { scheduled = null; };

  try {
    const images = new Map([
      ["a.png", { src: "a.png", complete: true, naturalWidth: 8 }],
      ["b.png", { src: "b.png", complete: true, naturalWidth: 8 }],
    ]);
    const element = {
      id: "hero",
      flipbook: {
        fps: 2,
        play_mode: "once",
        frames: [{ src: "a.png" }, { src: "skip.png", kept: false }, { src: "b.png" }],
      },
    };
    const controller = createAnimationPreviewController({
      getElements: () => [element],
      getImageForSrc: (src) => images.get(src),
      repaint: () => {},
    });
    controller.start("hero");
    assert.equal(controller.flipbookFrameFor(element).src, "a.png");
    now = 600;
    assert.equal(controller.flipbookFrameFor(element).src, "b.png");
    now = 1100;
    scheduled();
    assert.equal(controller.isPreviewing("hero"), false);
  } finally {
    globalThis.performance = previousPerformance;
    globalThis.requestAnimationFrame = previousRequest;
    globalThis.cancelAnimationFrame = previousCancel;
  }
});
