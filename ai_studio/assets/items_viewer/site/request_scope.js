export function captureCatalogScope(state) {
  return {
    catalogId: state.selectedId,
    catalogRequest: state.catalogRequest,
  };
}

export function captureEditScope(state, itemId) {
  return {
    ...captureCatalogScope(state),
    itemId,
  };
}

export function scopeIsCurrent(state, scope) {
  return state.selectedId === scope.catalogId
    && state.catalogRequest === scope.catalogRequest
    && (scope.itemId === undefined || state.selectedItemId === scope.itemId);
}

export function scopedUndo(scope, inverse) {
  return { catalogId: scope.catalogId, itemId: scope.itemId, inverse };
}
