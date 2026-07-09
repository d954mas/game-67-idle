const PLAYGAMA_BRIDGE_URL = "https://bridge.playgama.com/v1/stable/playgama-bridge.js";

export function createPlaygamaPlatformAdapter({ host }) {
  let bridgeReady = null;
  let bridge = null;
  let destroyed = false;

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
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
    return Boolean(await initBridge());
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
      bridge.platform.sendMessage("gameplay_started");
    } catch {}
  }

  async function gameplayStop() {
    if (!(await ready())) return;
    try {
      bridge.platform.sendMessage("gameplay_stopped");
    } catch {}
  }

  async function showInterstitial(placement) {
    if (!(await ready()) || !bridge.advertisement || !bridge.advertisement.isInterstitialSupported) {
      return { supported: false, shown: false, reason: "not_ready" };
    }
    return new Promise((resolve) => {
      let settled = false;
      const name = eventName("INTERSTITIAL_STATE_CHANGED", "interstitial_state_changed");
      const handler = (state) => {
        if (settled) return;
        if (state === "closed" || state === "failed") {
          settled = true;
          if (typeof bridge.advertisement.off === "function") bridge.advertisement.off(name, handler);
          resolve({ supported: true, shown: state === "closed", ...(state === "closed" ? {} : { reason: "failed" }) });
        }
      };
      bridge.advertisement.on(name, handler);
      bridge.advertisement.showInterstitial(placement || undefined);
    });
  }

  async function showRewarded(placement) {
    if (!(await ready()) || !bridge.advertisement || !bridge.advertisement.isRewardedSupported) {
      return { supported: false, shown: false, rewarded: false, reason: "not_ready" };
    }
    return new Promise((resolve) => {
      let rewarded = false;
      let settled = false;
      const name = eventName("REWARDED_STATE_CHANGED", "rewarded_state_changed");
      const handler = (state) => {
        if (settled) return;
        if (state === "rewarded") rewarded = true;
        if (state === "closed" || state === "failed") {
          settled = true;
          if (typeof bridge.advertisement.off === "function") bridge.advertisement.off(name, handler);
          resolve({
            supported: true,
            shown: state === "closed",
            rewarded,
            ...(rewarded ? {} : { reason: state === "failed" ? "failed" : "skipped" }),
          });
        }
      };
      bridge.advertisement.on(name, handler);
      bridge.advertisement.showRewarded(placement || undefined);
    });
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
    },
    gameLoadingProgress() {},
    gameLoadingFinished() {},
    gameReady,
    gameplayStart,
    gameplayStop,
    getLocale,
    loadData,
    ready,
    saveData,
    showInterstitial,
    showRewarded,
  };
}

export const createPlatformSdkAdapter = createPlaygamaPlatformAdapter;
