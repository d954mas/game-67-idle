// View-only feedback for active marquee/region gestures and alignment guides.
export function createGestureOverlay({ getContext, getDrag, getViewport, imageToScreenPoint }) {
  function normalizedRect(a, b) {
    return { x: Math.min(a.x, b.x), y: Math.min(a.y, b.y), w: Math.abs(b.x - a.x), h: Math.abs(b.y - a.y) };
  }

  function drawGestureOverlay() {
    const drag = getDrag();
    if (!drag || (drag.mode !== "marquee" && drag.mode !== "region-create")) return;
    const context = getContext();
    const start = imageToScreenPoint(drag.startWorld, getViewport());
    const rect = normalizedRect(start, drag.lastScreen);
    const marquee = drag.mode === "marquee";
    context.save();
    context.setLineDash([4, 3]);
    context.fillStyle = marquee ? "rgba(119, 167, 255, 0.12)" : "rgba(63, 199, 186, 0.18)";
    context.strokeStyle = marquee ? "#77a7ff" : "#3fc7ba";
    context.lineWidth = marquee ? 1 : 1.5;
    context.fillRect(rect.x, rect.y, rect.w, rect.h);
    context.strokeRect(rect.x + 0.5, rect.y + 0.5, rect.w, rect.h);
    context.restore();
  }

  function drawSnapGuides(viewport) {
    const drag = getDrag();
    if (!drag || !drag.activeGuides || !drag.activeGuides.length) return;
    const context = getContext();
    context.save();
    context.strokeStyle = "#ff2d78";
    context.lineWidth = 1;
    for (const guide of drag.activeGuides) {
      context.beginPath();
      if (guide.axis === "x") {
        const top = imageToScreenPoint({ x: guide.pos, y: guide.min }, viewport);
        const bottom = imageToScreenPoint({ x: guide.pos, y: guide.max }, viewport);
        context.moveTo(top.x, top.y);
        context.lineTo(bottom.x, bottom.y);
      } else {
        const left = imageToScreenPoint({ x: guide.min, y: guide.pos }, viewport);
        const right = imageToScreenPoint({ x: guide.max, y: guide.pos }, viewport);
        context.moveTo(left.x, left.y);
        context.lineTo(right.x, right.y);
      }
      context.stroke();
    }
    context.restore();
  }

  return { drawGestureOverlay, drawSnapGuides };
}
