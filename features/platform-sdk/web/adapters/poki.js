const POKI_SDK_URL = "https://game-cdn.poki.com/scripts/v2/poki-sdk.js";
const AD_TIMEOUT_MS = 120000;

// An adblocker can leave PokiSDK.init() pending forever while it waits for ad
// scripts; the game must never hang on that promise, so past this deadline the
// adapter degrades to no-ops (Poki requires adblocked players to be playable).
const POKI_INIT_TIMEOUT_MS = 10000;

export function createPokiPlatformAdapter({ host }) {
  let sdkReady = null;
  let sdkInstance = null;
  let destroyed = false;
  let lastLoadingProgress = 0;
  let lastSentLoadingProgress = -1;
  const loadingProgressPayload = { percentageDone: 0 };

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
  }

  function withAdDeadline(operation, failedResult) {
    return new Promise((resolve) => {
      let settled = false;
      const root = windowRef();
      const timer = (root.setTimeout || setTimeout)(() => settle(failedResult), AD_TIMEOUT_MS);

      function settle(result) {
        if (settled) return;
        settled = true;
        (root.clearTimeout || clearTimeout)(timer);
        resolve(result);
      }

      operation.then(settle, () => settle(failedResult));
    });
  }

  function loadScript() {
    const root = windowRef();
    if (root.PokiSDK && typeof root.PokiSDK.init === "function") return Promise.resolve(root.PokiSDK);
    const document = documentRef();
    if (!document || !document.head || typeof document.createElement !== "function") return Promise.resolve(null);
    return new Promise((resolve) => {
      const script = document.createElement("script");
      script.async = true;
      script.src = POKI_SDK_URL;
      script.onload = () => resolve(root.PokiSDK || null);
      script.onerror = () => resolve(null);
      document.head.appendChild(script);
    });
  }

  // Resolves whether the SDK is usable. The ADAPTER stays operational either
  // way: a missing/blocked/hung SDK only turns its calls into no-ops.
  function sdkAvailable() {
    if (!sdkReady) {
      const init = loadScript()
        .then((sdk) => (sdk && typeof sdk.init === "function" ? sdk.init().then(() => sdk) : null))
        .catch(() => null);
      sdkReady = new Promise((resolve) => {
        const timer = setTimeout(() => resolve(null), POKI_INIT_TIMEOUT_MS);
        init.then((sdk) => {
          clearTimeout(timer);
          resolve(sdk);
        });
      }).then((sdk) => {
        sdkInstance = destroyed ? null : sdk;
        flushLoadingProgress();
        return sdkInstance;
      });
    }
    return sdkReady;
  }

  // Host init gate: adapter readiness, not SDK readiness. Reporting the SDK
  // outcome here would put platform_sdk into BOOT_FAILED under an adblocker
  // and game_loading_finished would refuse forever — the loading overlay
  // would never lift for adblocked players.
  function ready() {
    void sdkAvailable();
    return true;
  }

  async function withSdk(callback) {
    const sdk = await sdkAvailable();
    if (!sdk || destroyed) return null;
    return callback(sdk);
  }

  async function gameLoadingFinished() {
    await withSdk((sdk) => sdk.gameLoadingFinished && sdk.gameLoadingFinished());
  }

  function flushLoadingProgress() {
    if (!sdkInstance || destroyed || lastLoadingProgress <= lastSentLoadingProgress) return;
    lastSentLoadingProgress = lastLoadingProgress;
    if (typeof sdkInstance.gameLoadingProgress === "function") {
      loadingProgressPayload.percentageDone = lastLoadingProgress;
      try {
        sdkInstance.gameLoadingProgress(loadingProgressPayload);
      } catch {}
    }
  }

  function gameLoadingProgress(progress01) {
    const progress = Math.max(0, Math.min(1, Number(progress01) || 0));
    if (progress < lastLoadingProgress) return;
    lastLoadingProgress = progress;
    void sdkAvailable();
    flushLoadingProgress();
  }

  async function gameReady() {
    // Poki has no separate game_ready call; the facade uses gameLoadingFinished().
  }

  async function gameplayStart() {
    await withSdk((sdk) => sdk.gameplayStart && sdk.gameplayStart());
  }

  async function gameplayStop() {
    await withSdk((sdk) => sdk.gameplayStop && sdk.gameplayStop());
  }

  async function measure(category, what, action) {
    await withSdk((sdk) => {
      if (typeof sdk.measure === "function") sdk.measure(category, what, action);
    });
  }

  async function showInterstitial() {
    const failed = { supported: true, shown: false, reason: "failed" };
    const shown = await withAdDeadline(withSdk((sdk) => {
      if (typeof sdk.commercialBreak !== "function") return null;
      return sdk.commercialBreak().then(() => true);
    }), failed);
    if (shown === failed) return failed;
    return shown ? { supported: true, shown: true } : { supported: false, shown: false, reason: "not_ready" };
  }

  async function showRewarded() {
    const failed = { supported: true, shown: false, rewarded: false, reason: "failed" };
    const rewarded = await withAdDeadline(withSdk((sdk) => {
      if (typeof sdk.rewardedBreak !== "function") return null;
      return sdk.rewardedBreak();
    }), failed);
    if (rewarded === failed) return failed;
    if (rewarded === null) {
      return { supported: false, shown: false, rewarded: false, reason: "not_ready" };
    }
    if (!rewarded) {
      return { supported: true, shown: false, rewarded: false, reason: "skipped" };
    }
    return { supported: true, shown: true, rewarded: true };
  }

  return {
    destroy() {
      destroyed = true;
    },
    gameLoadingProgress,
    gameLoadingFinished,
    gameReady,
    gameplayStart,
    gameplayStop,
    getLocale() {
      return (host && host.navigator && host.navigator.language) || null;
    },
    hideBanner() {
      return Promise.resolve();
    },
    loadData() {
      return Promise.resolve(null);
    },
    measure,
    ready,
    saveData() {
      return Promise.resolve();
    },
    showBanner() {
      return { supported: false, shown: false, reason: "unsupported" };
    },
    showInterstitial,
    showRewarded,
  };
}

export const createPlatformSdkAdapter = createPokiPlatformAdapter;
