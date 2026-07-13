// Conservative screen-space culling for workspace render passes. The caller owns
// project state; this domain owns only footprint calculation and viewport rejection.
const CULL_MARGIN = 64;
const PROMPT_PREVIEW_MAX = 40;

export function createRenderCulling({ getViewportSize, imageToScreenPoint, isAnimationPreviewing, rotatedCorners }) {
  function screenAABBOffscreen(x0, y0, x1, y1) {
    const { width, height } = getViewportSize();
    return x1 < -CULL_MARGIN || y1 < -CULL_MARGIN || x0 > width + CULL_MARGIN || y0 > height + CULL_MARGIN;
  }

  function elementScreenAABB(element, viewport) {
    const rotation = Number(element.rotation) || 0;
    if (rotation) {
      let x0 = Infinity;
      let y0 = Infinity;
      let x1 = -Infinity;
      let y1 = -Infinity;
      for (const corner of rotatedCorners(element)) {
        const point = imageToScreenPoint(corner, viewport);
        x0 = Math.min(x0, point.x);
        y0 = Math.min(y0, point.y);
        x1 = Math.max(x1, point.x);
        y1 = Math.max(y1, point.y);
      }
      return { x0, y0, x1, y1 };
    }
    const origin = imageToScreenPoint({ x: element.x, y: element.y }, viewport);
    return {
      x0: origin.x,
      y0: origin.y,
      x1: origin.x + element.w * viewport.scale,
      y1: origin.y + element.h * viewport.scale,
    };
  }

  function elementCullable(element, viewport, editingElement) {
    if (isAnimationPreviewing(element.id)) return false;
    if (editingElement && element.id === editingElement.id) return false;
    const box = elementScreenAABB(element, viewport);
    return screenAABBOffscreen(box.x0, box.y0, box.x1, box.y1);
  }

  function groupBoxScreenAABB(group, viewport) {
    const origin = imageToScreenPoint({ x: group.x, y: group.y }, viewport);
    return {
      x0: origin.x,
      y0: origin.y,
      x1: origin.x + group.w * viewport.scale,
      y1: origin.y + group.h * viewport.scale,
    };
  }

  function groupChromeScreenAABB(group, viewport) {
    const box = groupBoxScreenAABB(group, viewport);
    let chromeWidth = String(group.name || "Group").length * 12 + 12;
    if (group.recipe || group.style || group.anim) {
      chromeWidth = Math.max(chromeWidth + 4 + 90, PROMPT_PREVIEW_MAX * 11 + 14);
    }
    return { x0: box.x0, y0: box.y0, x1: Math.max(box.x1, box.x0 + chromeWidth), y1: box.y1 };
  }

  return { elementCullable, groupBoxScreenAABB, groupChromeScreenAABB, screenAABBOffscreen };
}
