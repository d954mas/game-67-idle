const YANDEX_SDK_URL = "/sdk.js";

export function createYandexPlatformAdapter({ host }) {
  let sdkReady = null;
  let playerReady = null;
  let ysdkInstance = null;
  let destroyed = false;

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
  }

  function loadScript() {
    const root = windowRef();
    if (root.YaGames && typeof root.YaGames.init === "function") return Promise.resolve(root.YaGames);
    const document = documentRef();
    if (!document || !document.head || typeof document.createElement !== "function") return Promise.resolve(null);
    return new Promise((resolve) => {
      const script = document.createElement("script");
      script.async = true;
      script.src = YANDEX_SDK_URL;
      script.onload = () => resolve(root.YaGames || null);
      script.onerror = () => resolve(null);
      document.head.appendChild(script);
    });
  }

  async function sdk() {
    if (!sdkReady) {
      sdkReady = loadScript()
        .then((YaGames) => (YaGames && typeof YaGames.init === "function" ? YaGames.init() : null))
        .catch(() => null);
    }
    ysdkInstance = destroyed ? null : await sdkReady;
    return ysdkInstance;
  }

  async function ready() {
    return Boolean(await sdk());
  }

  async function player() {
    const ysdk = await sdk();
    if (!ysdk || typeof ysdk.getPlayer !== "function") return null;
    if (!playerReady) playerReady = ysdk.getPlayer().catch(() => null);
    return playerReady;
  }

  async function gameLoadingFinished() {
    const ysdk = await sdk();
    ysdk && ysdk.features && ysdk.features.LoadingAPI && ysdk.features.LoadingAPI.ready();
  }

  async function gameReady() {
    // Yandex has no separate game_ready call; the facade uses gameLoadingFinished().
  }

  async function gameplayStart() {
    const ysdk = await sdk();
    ysdk && ysdk.features && ysdk.features.GameplayAPI && ysdk.features.GameplayAPI.start();
  }

  async function gameplayStop() {
    const ysdk = await sdk();
    ysdk && ysdk.features && ysdk.features.GameplayAPI && ysdk.features.GameplayAPI.stop();
  }

  async function showInterstitial() {
    const ysdk = await sdk();
    if (!ysdk || !ysdk.adv || typeof ysdk.adv.showFullscreenAdv !== "function") {
      return { supported: false, shown: false, reason: "not_ready" };
    }

    return new Promise((resolve) => {
      ysdk.adv.showFullscreenAdv({
        callbacks: {
          onClose: (wasShown) => {
            resolve({ supported: true, shown: Boolean(wasShown), ...(wasShown ? {} : { reason: "skipped" }) });
          },
          onError: () => {
            resolve({ supported: true, shown: false, reason: "failed" });
          },
          onOpen: () => {},
        },
      });
    });
  }

  async function showRewarded() {
    const ysdk = await sdk();
    if (!ysdk || !ysdk.adv || typeof ysdk.adv.showRewardedVideo !== "function") {
      return { supported: false, shown: false, rewarded: false, reason: "not_ready" };
    }

    let rewarded = false;
    return new Promise((resolve) => {
      ysdk.adv.showRewardedVideo({
        callbacks: {
          onClose: (wasShown) => {
            resolve({
              supported: true,
              shown: Boolean(wasShown),
              rewarded,
              ...(rewarded ? {} : { reason: wasShown ? "skipped" : "failed" }),
            });
          },
          onError: () => {
            resolve({ supported: true, shown: false, rewarded: false, reason: "failed" });
          },
          onOpen: () => {},
          onRewarded: () => {
            rewarded = true;
          },
        },
      });
    });
  }

  async function loadData(key) {
    const p = await player();
    if (!p || typeof p.getData !== "function") return null;
    const data = await p.getData([key]).catch(() => null);
    return data && Object.prototype.hasOwnProperty.call(data, key) ? data[key] : null;
  }

  async function saveData(key, value) {
    const p = await player();
    if (!p || typeof p.setData !== "function") return;
    await p.setData({ [key]: value }).catch(() => {});
  }

  function getLocale() {
    return (ysdkInstance && ysdkInstance.environment && ysdkInstance.environment.i18n && ysdkInstance.environment.i18n.lang) ||
      (host && host.navigator && host.navigator.language) ||
      null;
  }

  return {
    destroy() {
      destroyed = true;
    },
    gameLoadingFinished,
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

export const createPlatformSdkAdapter = createYandexPlatformAdapter;
