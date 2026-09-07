/* CrazyGames SDK v3. Two of its rules shape everything below: the SDK must be
   loaded from the portal's CDN and never bundled, and off-portal
   (`environment === "disabled"`) every call throws instead of degrading, so
   each call sits behind a guard. */
const CRAZYGAMES_SDK_URL = "https://sdk.crazygames.com/crazygames-sdk-v3.js";
const AD_TIMEOUT_MS = 120000;

/* The portal reports why an ad did not play; the wrapper contract speaks in
   reasons the game can act on. A cooldown is the SDK doing its job, an
   adblocker or an unfilled slot is a slot that may fill later, and the launch
   window disables ads for a game's first weeks by design. */
const AD_ERROR_REASONS = new Map([
  ["adCooldown", "rate_limited"],
  ["adsDisabledBasicLaunch", "unsupported"],
  ["unfilled", "not_ready"],
  ["adblock", "not_ready"],
]);

export function createCrazygamesPlatformAdapter({ host, lifecycle, sdkUrl = CRAZYGAMES_SDK_URL }) {
  let sdkReady = null;
  let sdkInstance = null;
  let loadingStarted = false;
  let destroyed = false;

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
  }

  function loadScript() {
    const root = windowRef();
    if (root.CrazyGames && root.CrazyGames.SDK) return Promise.resolve(root.CrazyGames.SDK);
    const document = documentRef();
    if (!document || !document.head || typeof document.createElement !== "function") return Promise.resolve(null);
    return new Promise((resolve) => {
      const script = document.createElement("script");
      script.async = true;
      script.src = sdkUrl;
      script.onload = () => resolve((root.CrazyGames && root.CrazyGames.SDK) || null);
      script.onerror = () => resolve(null);
      document.head.appendChild(script);
    });
  }

  async function sdk() {
    if (!sdkReady) {
      sdkReady = loadScript()
        .then(async (candidate) => {
          if (!candidate || typeof candidate.init !== "function") return null;
          await candidate.init();
          /* An unrecognised host answers "disabled" and then throws on every
             later call, so the adapter reports no SDK at all rather than a
             broken one. */
          if (candidate.environment === "disabled") return null;
          return candidate;
        })
        .catch(() => null);
    }
    sdkInstance = destroyed ? null : await sdkReady;
    return sdkInstance;
  }

  /* The site carries its own mute switch, and it is a settings object rather
     than an event, so the current value is applied once and then followed. */
  function followPortalAudio(instance) {
    if (!lifecycle || typeof lifecycle.audio !== "function") return;
    const game = instance.game;
    if (!game || !game.settings) return;
    const apply = () => {
      try { lifecycle.audio(!game.settings.muteAudio); } catch { /* the facade is not up */ }
    };
    apply();
    if (typeof game.addSettingsChangeListener === "function") {
      try { game.addSettingsChangeListener(apply); } catch { /* the portal is not listening */ }
    }
  }

  async function ready() {
    const instance = await sdk();
    if (instance && !loadingStarted) {
      loadingStarted = true;
      /* The portal measures the load, and the only handle it offers is the
         interval: there is no progress fraction to report. */
      try { instance.game.loadingStart(); } catch { /* the portal is not listening */ }
      followPortalAudio(instance);
    }
    return Boolean(instance);
  }

  function adOperation(start, failedResult) {
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

      try {
        start(settle);
      } catch {
        settle(failedResult);
      }
    });
  }

  function adErrorReason(error) {
    const code = error && typeof error === "object" ? error.code : error;
    return AD_ERROR_REASONS.get(String(code || "")) || "failed";
  }

  async function gameLoadingFinished() {
    const instance = await sdk();
    if (!instance) return;
    try { instance.game.loadingStop(); } catch { /* the portal is not listening */ }
  }

  async function gameReady() {
    // CrazyGames bounds loading with loadingStart/loadingStop and has no
    // separate ready call.
  }

  async function gameplayStart() {
    const instance = await sdk();
    if (!instance) return;
    try { instance.game.gameplayStart(); } catch { /* the portal is not listening */ }
  }

  async function gameplayStop() {
    const instance = await sdk();
    if (!instance) return;
    try { instance.game.gameplayStop(); } catch { /* the portal is not listening */ }
  }

  async function showInterstitial() {
    const instance = await sdk();
    if (!instance || !instance.ad || typeof instance.ad.requestAd !== "function") {
      return { supported: false, shown: false, reason: "unsupported" };
    }

    const failed = { supported: true, shown: false, reason: "failed" };
    return adOperation((settle) => {
      let started = false;
      instance.ad.requestAd("midgame", {
        adStarted: () => { started = true; },
        adFinished: () => settle({ supported: true, shown: started }),
        adError: (error) => settle({ supported: true, shown: started, reason: adErrorReason(error) }),
      });
    }, failed);
  }

  /* The portal reports no "player closed it early" outcome, so the reward is
     tied to the one signal that exists: the ad finished. */
  async function showRewarded() {
    const instance = await sdk();
    if (!instance || !instance.ad || typeof instance.ad.requestAd !== "function") {
      return { supported: false, shown: false, rewarded: false, reason: "unsupported" };
    }

    const failed = { supported: true, shown: false, rewarded: false, reason: "failed" };
    return adOperation((settle) => {
      let started = false;
      instance.ad.requestAd("rewarded", {
        adStarted: () => { started = true; },
        adFinished: () => settle({ supported: true, shown: true, rewarded: true }),
        adError: (error) => settle({
          supported: true,
          shown: started,
          rewarded: false,
          reason: adErrorReason(error),
        }),
      });
    }, failed);
  }

  /* A CrazyGames banner is drawn into a DOM element the game owns, and a
     full-canvas build has none. Until a shell provides a container the honest
     answer is that the target does not support it. */
  async function showBanner() {
    return { supported: false, shown: false, reason: "unsupported" };
  }

  async function hideBanner() {
    const instance = await sdk();
    if (!instance || !instance.banner || typeof instance.banner.clearAllBanners !== "function") return;
    try { instance.banner.clearAllBanners(); } catch { /* nothing was drawn */ }
  }

  /* Portal storage is a synchronous localStorage stand-in, and its writes are
     debounced by the SDK, so the wrapper neither batches nor awaits them. */
  async function loadData(key) {
    const instance = await sdk();
    if (!instance || !instance.data || typeof instance.data.getItem !== "function") return null;
    try {
      const value = instance.data.getItem(key);
      return value === undefined ? null : value;
    } catch {
      return null;
    }
  }

  async function saveData(key, value) {
    const instance = await sdk();
    if (!instance || !instance.data || typeof instance.data.setItem !== "function") return;
    try { instance.data.setItem(key, value); } catch { /* over the portal's 1 MB cap */ }
  }

  function getLocale() {
    try {
      const info = sdkInstance && sdkInstance.user && sdkInstance.user.systemInfo;
      if (info && info.locale) return info.locale;
    } catch { /* the portal answers only on its own domains */ }
    return null;
  }

  return {
    destroy() {
      destroyed = true;
    },
    gameLoadingProgress() {},
    gameLoadingFinished,
    gameReady,
    gameplayStart,
    gameplayStop,
    getLocale,
    hideBanner,
    loadData,
    /* The portal has no analytics sink of its own; purchase orders are the one
       thing it tracks, and this game does not sell any. */
    measure() {},
    ready,
    saveData,
    showBanner,
    showInterstitial,
    showRewarded,
  };
}

export const createPlatformSdkAdapter = createCrazygamesPlatformAdapter;
