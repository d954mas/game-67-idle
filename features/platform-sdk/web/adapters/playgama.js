const PLAYGAMA_BRIDGE_URL = "https://bridge.playgama.com/v1/stable/playgama-bridge.js";
const AD_TIMEOUT_MS = 120000;

export function createPlaygamaPlatformAdapter({ host }) {
  let bridgeReady = null;
  let bridge = null;
  let destroyed = false;
  let hasStartedGameplay = false;
  const pendingAds = new Set();

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
  }

  function adOperation(start, failedResult) {
    return new Promise((resolve) => {
      let cleanup = null;
      let settled = false;
      const root = windowRef();
      const cancel = () => settle(failedResult);
      const timer = (root.setTimeout || setTimeout)(cancel, AD_TIMEOUT_MS);

      function setCleanup(next) {
        cleanup = next;
        if (settled && cleanup) {
          try {
            cleanup();
          } catch {}
          cleanup = null;
        }
      }

      function settle(result) {
        if (settled) return;
        settled = true;
        pendingAds.delete(cancel);
        (root.clearTimeout || clearTimeout)(timer);
        if (cleanup) {
          try {
            cleanup();
          } catch {}
          cleanup = null;
        }
        resolve(result);
      }

      pendingAds.add(cancel);
      try {
        start(settle, setCleanup);
      } catch {
        settle(failedResult);
      }
    });
  }

  function loadScript() {
    const root = windowRef();
    if (root.bridge) return Promise.resolve(root.bridge);
    const document = documentRef();
    if (!document || !document.head || typeof document.createElement !== "function") return Promise.resolve(null);
    return new Promise((resolve) => {
      const script = document.createElement("script");
      script.async = true;
      script.src = PLAYGAMA_BRIDGE_URL;
      script.onload = () => resolve(root.bridge || null);
      script.onerror = () => resolve(null);
      document.head.appendChild(script);
    });
  }

  function eventName(group, fallback) {
    return bridge && bridge.EVENT_NAME && bridge.EVENT_NAME[group] ? bridge.EVENT_NAME[group] : fallback;
  }

  async function initBridge() {
    if (bridgeReady) return bridgeReady;
    bridgeReady = (async () => {
      const loaded = await loadScript();
      if (!loaded || destroyed) return false;
      bridge = loaded;
      try {
        await bridge.initialize();
      } catch {
        return false;
      }
      if (destroyed) return false;

      return true;
    })();
    return bridgeReady;
  }

  async function ready() {
    if (destroyed) return false;
    return Boolean(await initBridge()) && !destroyed;
  }

  async function gameReady() {
    if (!(await ready())) return;
    try {
      bridge.platform.sendMessage("game_ready");
    } catch {}
  }

  async function gameplayStart() {
    if (!(await ready())) return;
    try {
      bridge.platform.sendMessage(hasStartedGameplay ? "level_resumed" : "level_started");
      hasStartedGameplay = true;
    } catch {}
  }

  async function gameplayStop() {
    if (!(await ready())) return;
    try {
      bridge.platform.sendMessage("level_pause");
    } catch {}
  }

  async function showInterstitial(placement) {
    if (!(await ready()) || !bridge.advertisement || !bridge.advertisement.isInterstitialSupported) {
      return { supported: false, shown: false, reason: "not_ready" };
    }
    const failed = { supported: true, shown: false, reason: "failed" };
    return adOperation((settle, setCleanup) => {
      const name = eventName("INTERSTITIAL_STATE_CHANGED", "interstitial_state_changed");
      const handler = (state) => {
        if (state === "closed") settle({ supported: true, shown: true });
        else if (state === "failed") settle(failed);
      };
      bridge.advertisement.on(name, handler);
      setCleanup(() => {
        if (typeof bridge.advertisement.off === "function") bridge.advertisement.off(name, handler);
      });
      bridge.advertisement.showInterstitial(placement || undefined);
    }, failed);
  }

  async function showRewarded(placement) {
    if (!(await ready()) || !bridge.advertisement || !bridge.advertisement.isRewardedSupported) {
      return { supported: false, shown: false, rewarded: false, reason: "not_ready" };
    }
    const failed = { supported: true, shown: false, rewarded: false, reason: "failed" };
    return adOperation((settle, setCleanup) => {
      let rewarded = false;
      const name = eventName("REWARDED_STATE_CHANGED", "rewarded_state_changed");
      const handler = (state) => {
        if (state === "rewarded") rewarded = true;
        if (state === "closed") {
          settle(rewarded
            ? { supported: true, shown: true, rewarded: true }
            : { supported: true, shown: true, rewarded: false, reason: "skipped" });
        } else if (state === "failed") {
          settle(rewarded
            ? { supported: true, shown: false, rewarded: true }
            : failed);
        }
      };
      bridge.advertisement.on(name, handler);
      setCleanup(() => {
        if (typeof bridge.advertisement.off === "function") bridge.advertisement.off(name, handler);
      });
      bridge.advertisement.showRewarded(placement || undefined);
    }, failed);
  }

  async function loadData(key) {
    if (!(await ready()) || !bridge.storage || typeof bridge.storage.get !== "function") return null;
    const value = await bridge.storage.get(key, undefined, false).catch(() => null);
    if (value == null) return null;
    try {
      return JSON.parse(value);
    } catch {
      return value;
    }
  }

  async function saveData(key, value) {
    if (!(await ready()) || !bridge.storage || typeof bridge.storage.set !== "function") return;
    await bridge.storage.set(key, typeof value === "string" ? value : JSON.stringify(value)).catch(() => {});
  }

  function getLocale() {
    return (bridge && bridge.platform && bridge.platform.language) ||
      (host && host.navigator && host.navigator.language) ||
      null;
  }

  return {
    destroy() {
      destroyed = true;
      for (const cancel of pendingAds) cancel();
    },
    gameLoadingProgress() {},
    gameLoadingFinished() {},
    gameReady,
    gameplayStart,
    gameplayStop,
    getLocale,
    hideBanner() {},
    loadData,
    measure() {},
    ready,
    saveData,
    showBanner() {
      return { supported: false, shown: false, reason: "unsupported" };
    },
    showInterstitial,
    showRewarded,
  };
}

export const createPlatformSdkAdapter = createPlaygamaPlatformAdapter;
