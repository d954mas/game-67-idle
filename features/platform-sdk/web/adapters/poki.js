const POKI_SDK_URL = "https://game-cdn.poki.com/scripts/v2/poki-sdk.js";

export function createPokiPlatformAdapter({ host }) {
  let sdkReady = null;
  let destroyed = false;

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
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

  function ready() {
    if (!sdkReady) {
      sdkReady = loadScript()
        .then((sdk) => (sdk && typeof sdk.init === "function" ? sdk.init().then(() => sdk) : null))
        .then(Boolean)
        .catch(() => false);
    }
    return sdkReady;
  }

  async function withSdk(callback) {
    const ok = await ready();
    const sdk = windowRef().PokiSDK;
    if (!ok || !sdk || destroyed) return null;
    return callback(sdk);
  }

  async function gameLoadingFinished() {
    await withSdk((sdk) => sdk.gameLoadingFinished && sdk.gameLoadingFinished());
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

  async function showInterstitial() {
    try {
      const shown = await withSdk((sdk) => {
        if (typeof sdk.commercialBreak !== "function") return false;
        return sdk.commercialBreak().then(() => true);
      });
      return shown ? { supported: true, shown: true } : { supported: false, shown: false, reason: "not_ready" };
    } catch {
      return { supported: true, shown: false, reason: "failed" };
    }
  }

  async function showRewarded() {
    try {
      const rewarded = await withSdk((sdk) => {
        if (typeof sdk.rewardedBreak !== "function") return false;
        return sdk.rewardedBreak();
      });
      return { supported: true, shown: Boolean(rewarded), rewarded: Boolean(rewarded), ...(rewarded ? {} : { reason: "skipped" }) };
    } catch {
      return { supported: true, shown: false, rewarded: false, reason: "failed" };
    }
  }

  return {
    destroy() {
      destroyed = true;
    },
    gameLoadingFinished,
    gameReady,
    gameplayStart,
    gameplayStop,
    getLocale() {
      return (host && host.navigator && host.navigator.language) || null;
    },
    loadData() {
      return Promise.resolve(null);
    },
    ready,
    saveData() {
      return Promise.resolve();
    },
    showInterstitial,
    showRewarded,
  };
}

export const createPlatformSdkAdapter = createPokiPlatformAdapter;
