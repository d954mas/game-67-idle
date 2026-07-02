const maxHistoryDepth = 80;

function cloneSnapshot(snapshot) {
  return {
    regions: JSON.parse(JSON.stringify(snapshot.regions || [])),
    selectedId: snapshot.selectedId || null,
  };
}

function sameSnapshot(a, b) {
  return JSON.stringify(a || {}) === JSON.stringify(b || {});
}

export function createRegionHistory(snapshot) {
  return {
    current: cloneSnapshot(snapshot),
    undo: [],
    redo: [],
  };
}

export function historyCanUndo(history) {
  return history.undo.length > 0;
}

export function historyCanRedo(history) {
  return history.redo.length > 0;
}

export function historyPush(history, snapshot) {
  const next = cloneSnapshot(snapshot);
  if (sameSnapshot(history.current, next)) return history;
  return {
    current: next,
    undo: [...history.undo, history.current].slice(-maxHistoryDepth),
    redo: [],
  };
}

export function historyUndo(history) {
  if (!historyCanUndo(history)) return { history, snapshot: cloneSnapshot(history.current) };
  const snapshot = history.undo[history.undo.length - 1];
  const undo = history.undo.slice(0, -1);
  const nextHistory = {
    current: snapshot,
    undo,
    redo: [history.current, ...history.redo].slice(0, maxHistoryDepth),
  };
  return { history: nextHistory, snapshot: cloneSnapshot(snapshot) };
}

export function historyRedo(history) {
  if (!historyCanRedo(history)) return { history, snapshot: cloneSnapshot(history.current) };
  const snapshot = history.redo[0];
  const redo = history.redo.slice(1);
  const nextHistory = {
    current: snapshot,
    undo: [...history.undo, history.current].slice(-maxHistoryDepth),
    redo,
  };
  return { history: nextHistory, snapshot: cloneSnapshot(snapshot) };
}
