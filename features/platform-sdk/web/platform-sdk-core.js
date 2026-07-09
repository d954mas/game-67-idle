function safeCall(callback, value) {
  if (typeof callback !== "function") return;
  try {
    callback(value);
  } catch (error) {
    console.warn("[platform-sdk] backend callback failed", error);
  }
}

function noopAsync() {
  return Promise.resolve();
}

function unsupportedAd(reason = "unsupported") {
  return { supported: false, shown: false, reason };
}

function unsupportedRewarded(reason = "unsupported") {
  return { supported: false, shown: false, rewarded: false, reason };
}

function bindMethod(adapter, method, fallback) {
  if (adapter && typeof adapter[method] === "function") {
    return (...args) => adapter[method](...args);
  }
  return typeof fallback === "function" ? fallback : () => fallback;
}

export function createPlatformSdkWebBackend({
  adapterFactory,
  callbacks = {},
  config = {},
  host = globalThis,
} = {}) {
  const target = String(config.target || "local");
  const platformSdk = String(config.platformSdk || "");

  let destroyed = false;
  const emitAudioToggle = (enabled) => {
    if (!destroyed) safeCall(callbacks.onAudioToggle, enabled);
  };
  const emitPause = () => {
    if (!destroyed) safeCall(callbacks.onPause);
  };
  const emitResume = () => {
    if (!destroyed) safeCall(callbacks.onResume);
  };
  const emitVisibilityChange = (hidden) => {
    if (!destroyed) safeCall(callbacks.onVisibilityChange, hidden);
  };

  const adapter = adapterFactory
    ? adapterFactory({
        config,
        emitAudioToggle,
        emitPause,
        emitResume,
        emitVisibilityChange,
        host,
        platformSdk,
        target,
      })
    : null;
  const adapterReady = bindMethod(adapter, "ready", () => false);
  let readyPromise = null;

  return {
    destroy() {
      if (destroyed) return;
      destroyed = true;
      if (adapter && typeof adapter.destroy === "function") adapter.destroy();
    },
    gameLoadingProgress: bindMethod(adapter, "gameLoadingProgress", noopAsync),
    gameLoadingFinished: bindMethod(adapter, "gameLoadingFinished", noopAsync),
    gameReady: bindMethod(adapter, "gameReady", noopAsync),
    gameplayStart: bindMethod(adapter, "gameplayStart", noopAsync),
    gameplayStop: bindMethod(adapter, "gameplayStop", noopAsync),
    getLocale: bindMethod(adapter, "getLocale", () => null),
    hideBanner: bindMethod(adapter, "hideBanner", noopAsync),
    loadData: bindMethod(adapter, "loadData", () => Promise.resolve(null)),
    ready() {
      if (!readyPromise) {
        readyPromise = Promise.resolve(adapterReady())
          .then((ready) => !destroyed && Boolean(ready))
          .catch(() => false);
      }
      return readyPromise;
    },
    saveData: bindMethod(adapter, "saveData", noopAsync),
    showBanner: bindMethod(adapter, "showBanner", () => unsupportedAd()),
    showInterstitial: bindMethod(adapter, "showInterstitial", () => unsupportedAd()),
    showRewarded: bindMethod(adapter, "showRewarded", () => unsupportedRewarded()),
  };
}
