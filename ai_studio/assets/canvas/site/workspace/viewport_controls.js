// Workspace viewport commands and their small DOM synchronization surface.
export function createViewportControls({
  clamp,
  elements,
  fitViewport,
  groups,
  isElementHidden,
  isNodeHidden,
  queryTools,
  render,
  resizeCanvas,
  screenToImagePoint,
  state,
  uiElement,
}) {
  function contentBounds() {
    const boxes = [
      ...elements().filter((element) => !isElementHidden(element)),
      ...groups().filter((group) => !isNodeHidden(state.project, group)),
    ];
    if (!boxes.length) return { x: 0, y: 0, width: 1024, height: 768 };
    let minX = Infinity;
    let minY = Infinity;
    let maxX = -Infinity;
    let maxY = -Infinity;
    for (const box of boxes) {
      minX = Math.min(minX, box.x);
      minY = Math.min(minY, box.y);
      maxX = Math.max(maxX, box.x + box.w);
      maxY = Math.max(maxY, box.y + box.h);
    }
    return { x: minX, y: minY, width: Math.max(1, maxX - minX), height: Math.max(1, maxY - minY) };
  }

  function fit() {
    resizeCanvas();
    const bounds = contentBounds();
    const base = fitViewport({
      imageWidth: bounds.width,
      imageHeight: bounds.height,
      frameWidth: state.cssWidth,
      frameHeight: state.cssHeight,
      padding: 48,
    });
    state.viewport = {
      scale: base.scale,
      offsetX: base.offsetX - bounds.x * base.scale,
      offsetY: base.offsetY - bounds.y * base.scale,
    };
    render();
  }

  function zoomTo(scale) {
    const center = { x: state.cssWidth / 2, y: state.cssHeight / 2 };
    const world = screenToImagePoint(center, state.viewport);
    const next = clamp(scale, 0.05, 12);
    state.viewport = { scale: next, offsetX: center.x - world.x * next, offsetY: center.y - world.y * next };
    render();
  }

  function syncTopBar() {
    const title = uiElement("ws-title");
    if (title && state.project) title.textContent = state.project.title;
    const undoButton = uiElement("undo");
    const redoButton = uiElement("redo");
    if (undoButton) undoButton.disabled = !state.history.canUndo;
    if (redoButton) redoButton.disabled = !state.history.canRedo;
    for (const button of queryTools()) button.classList.toggle("active", button.dataset.tool === state.tool);
  }

  return { fit, syncTopBar, zoomTo };
}
