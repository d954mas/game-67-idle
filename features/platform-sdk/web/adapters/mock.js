export function createMockPlatformAdapter({ emitVisibilityChange, host, target }) {
  const document = host && host.document;
  const isLocal = target === "local";
  let destroyed = false;

  const onVisibilityChange = () => {
    if (destroyed || !document) return;
    emitVisibilityChange(Boolean(document.hidden));
  };

  if (document && typeof document.addEventListener === "function") {
    document.addEventListener("visibilitychange", onVisibilityChange);
  }

  function unsupportedRewarded() {
    return { supported: false, shown: false, rewarded: false, reason: "unsupported" };
  }

  function unsupportedInterstitial() {
    return { supported: false, shown: false, reason: "unsupported" };
  }

  async function ready() {
    return true;
  }

  async function showInterstitial() {
    if (!isLocal || destroyed) return unsupportedInterstitial();
    return { supported: true, shown: true, reason: "completed" };
  }

  async function showRewarded() {
    if (!isLocal || destroyed) return unsupportedRewarded();
    return { supported: true, shown: true, rewarded: true };
  }

  async function loadData(key) {
    if (!isLocal || !host || !host.localStorage) return null;
    const raw = host.localStorage.getItem(`platform-sdk:${key}`);
    if (raw == null) return null;
    try {
      return JSON.parse(raw);
    } catch {
      return raw;
    }
  }

  async function saveData(key, value) {
    if (!isLocal || !host || !host.localStorage) return;
    host.localStorage.setItem(`platform-sdk:${key}`, JSON.stringify(value));
  }

  function getLocale() {
    return (host && host.navigator && host.navigator.language) || null;
  }

  function destroy() {
    destroyed = true;
    if (document && typeof document.removeEventListener === "function") {
      document.removeEventListener("visibilitychange", onVisibilityChange);
    }
  }

  return {
    destroy,
    gameLoadingProgress() {},
    gameLoadingFinished() {},
    gameReady() {},
    gameplayStart() {},
    gameplayStop() {},
    getLocale,
    loadData,
    ready,
    saveData,
    showInterstitial,
    showRewarded,
  };
}

export const createPlatformSdkAdapter = createMockPlatformAdapter;
