/* Pikabu Games. The SDK is served from the portal's own origin and refuses to
   work anywhere except a Pikabu game page or a Studio test link, so there is
   nothing to self-host and no offline exercise of it. Three shapes drive the
   rest: `gameStarted()` is the only readiness milestone (no progress channel
   exists), the preloader ad is legal solely before it, and the platform ships
   neither a key/value store nor a board — cloud saves are the game's own
   backend, addressed by the signed player identity. */
const SDK_URL = "https://games.pikabu.ru/sdk/sdk.js";
/* An adblocker can keep the injected script from ever answering. The portal
   requires an adblocked player to still play, so every call degrades to a
   no-op past this deadline instead of hanging the boot. */
const SDK_INIT_TIMEOUT_MS = 10000;
const AD_TIMEOUT_MS = 120000;
const SAVE_TIMEOUT_MS = 15000;

export function createPikabuPlatformAdapter({ config, host, lifecycle }) {
  const saveEndpoint = String((config && config.saveEndpoint) || "");
  let destroyed = false;
  let sdkReady = null;
  let sdkInstance = null;
  let started = false;
  let preloaderShowing = null;
  let signedData = null;

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
  }

  /* The platform broadcasts no lifecycle event of its own, yet requires the
     game to pause and fall silent when the tab goes away. The document is the
     only signal there is, so this adapter owns that duty. */
  const onVisibilityChange = () => {
    if (destroyed || !lifecycle) return;
    const hidden = Boolean(documentRef() && documentRef().hidden);
    audio(!hidden);
    try {
      if (hidden) lifecycle.pause();
      else lifecycle.resume();
    } catch { /* the facade is not up */ }
  };

  function deadline(operation, failedResult, ms) {
    return new Promise((resolve) => {
      let settled = false;
      const root = windowRef();
      const timer = (root.setTimeout || setTimeout)(() => settle(failedResult), ms);

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
    if (root.PkbSDK && typeof root.PkbSDK.init === "function") return Promise.resolve(root.PkbSDK);
    const document = documentRef();
    if (!document || !document.head || typeof document.createElement !== "function") return Promise.resolve(null);
    return new Promise((resolve) => {
      const script = document.createElement("script");
      script.async = true;
      script.src = SDK_URL;
      script.onload = () => resolve(root.PkbSDK || null);
      script.onerror = () => resolve(null);
      document.head.appendChild(script);
    });
  }

  /* `PkbSDK.init()` is documented as a once-per-game call, so the promise is
     the singleton and every later caller awaits the same one. */
  function initSdk() {
    const attempt = loadScript().then(async (global) => {
      if (!global || destroyed) return null;
      const instance = await global.init();
      return instance || null;
    }).catch(() => null);
    return deadline(attempt, null, SDK_INIT_TIMEOUT_MS);
  }

  async function sdk() {
    if (!sdkReady) sdkReady = initSdk();
    sdkInstance = destroyed ? null : await sdkReady;
    return sdkInstance;
  }

  /* The identity behind a save changes exactly once, when an anonymous player
     signs in mid-session; the token is re-read then so writes land under the
     account the portal now reports. */
  function followAuth(instance) {
    if (typeof instance.on !== "function") return;
    try {
      instance.on("userAuthorized", () => { signedData = null; });
    } catch { /* the portal is not broadcasting */ }
  }

  async function ready() {
    const instance = await sdk();
    if (!instance) return false;
    followAuth(instance);
    showPreloader(instance);
    return true;
  }

  /* The preloader is the one ad the platform allows during loading, and only
     before `gameStarted()`. It never blocks loading; the reveal waits for it
     because the portal forbids the format once the game has started. */
  function showPreloader(instance) {
    const ads = instance.ads;
    if (preloaderShowing || started || !ads || !ads.preloader) return;
    preloaderShowing = (async () => {
      try {
        if (!ads.preloader.isSupported || !(await ads.preloader.canShow())) return;
        audio(false);
        await ads.preloader.show();
      } catch { /* an unavailable preloader is a normal outcome */ } finally {
        audio(true);
      }
    })();
  }

  function audio(enabled) {
    if (!lifecycle || typeof lifecycle.audio !== "function") return;
    try { lifecycle.audio(enabled); } catch { /* the facade is not up */ }
  }

  function adVisible(requestId, visible) {
    if (!lifecycle || typeof lifecycle.adVisible !== "function") return;
    try { lifecycle.adVisible(requestId, visible); } catch { /* the facade is not up */ }
  }

  /* No progress channel exists: the host draws its own loading screen and
     learns only that the game is ready. */
  function gameLoadingProgress() {}

  async function reveal() {
    const instance = await sdk();
    if (!instance || started || typeof instance.gameStarted !== "function") return;
    if (preloaderShowing) await preloaderShowing;
    if (started || destroyed) return;
    started = true;
    try { instance.gameStarted(); } catch { /* the host is not listening */ }
  }

  async function gameLoadingFinished() {
    await reveal();
  }

  async function gameReady() {
    await reveal();
  }

  /* The platform measures a session on its own and exposes no gameplay
     interval to bracket. */
  function gameplayStart() {}
  function gameplayStop() {}

  /* `canShow()` is required before every show, and a false answer is an
     ordinary outcome of frequency limits or a busy host UI, not a failure of
     the format. */
  async function showAd(kind, requestId, extra) {
    const unsupported = { supported: false, shown: false, reason: "unsupported", ...extra };
    const instance = await sdk();
    const ad = instance && instance.ads && instance.ads[kind];
    if (!ad || typeof ad.show !== "function") return unsupported;
    if (!ad.isSupported) return unsupported;
    const failed = { supported: true, shown: false, reason: "failed", ...extra };
    try {
      if (!(await ad.canShow())) return failed;
    } catch { return failed; }
    adVisible(requestId, true);
    const result = await deadline(
      Promise.resolve(ad.show()).catch(() => null),
      null,
      AD_TIMEOUT_MS);
    adVisible(requestId, false);
    if (!result) return { ...failed, reason: "timeout" };
    if (!result.rendered) {
      return result.reason === "NOT_SUPPORTED" ? unsupported : failed;
    }
    return { supported: true, shown: true, ...extra, ...(kind === "rewarded" ? { rewarded: Boolean(result.reward) } : {}) };
  }

  async function showInterstitial(placement, requestId) {
    return showAd("fullscreen", requestId, {});
  }

  async function showRewarded(placement, requestId) {
    return showAd("rewarded", requestId, { rewarded: false });
  }

  /* The SDK carries no banner format; the three it documents are all
     full-window. */
  async function showBanner() {
    return { supported: false, shown: false, reason: "unsupported" };
  }

  function hideBanner() {}

  async function getPlayer() {
    const anonymous = { authorized: false, name: "", avatarUrl: "" };
    try {
      const instance = await sdk();
      const player = instance && instance.player;
      if (!player) return anonymous;
      return {
        authorized: Boolean(player.isAuthorized),
        name: String(player.name || ""),
        avatarUrl: String(player.avatar || ""),
      };
    } catch { return anonymous; }
  }

  /* The dialog resolves when it closes, whether or not the player signed in,
     so the flag is what decides the outcome. */
  async function login() {
    const unsupported = { supported: false, authorized: false, reason: "unsupported", name: "", avatarUrl: "" };
    try {
      const instance = await sdk();
      if (!instance || !instance.auth || typeof instance.auth.openAuthDialog !== "function") return unsupported;
      await instance.auth.openAuthDialog();
      const player = await getPlayer();
      return {
        supported: true,
        authorized: player.authorized,
        reason: player.authorized ? "accepted" : "declined",
        name: player.name,
        avatarUrl: player.avatarUrl,
      };
    } catch {
      return { supported: true, authorized: false, reason: "failed", name: "", avatarUrl: "" };
    }
  }

  /* Saves live on the game's own backend: the platform has no store, and the
     signed token is what lets that backend trust the player id it writes
     under. An anonymous player is signed too, so a save exists before login. */
  async function playerToken() {
    if (signedData) return signedData;
    const instance = await sdk();
    const player = instance && instance.player;
    if (!player || typeof player.getSignedData !== "function") return null;
    try {
      signedData = (await player.getSignedData()) || null;
    } catch { signedData = null; }
    return signedData;
  }

  async function saveRequest(body) {
    const root = windowRef();
    if (!saveEndpoint || typeof root.fetch !== "function") return null;
    const token = await playerToken();
    if (!token) return null;
    const request = root.fetch(saveEndpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ signedData: token, ...body }),
    }).then((response) => (response && response.ok ? response.json() : null));
    return deadline(request.catch(() => null), null, SAVE_TIMEOUT_MS);
  }

  async function loadData(key) {
    if (!saveEndpoint) return { status: "unavailable" };
    const answer = await saveRequest({ op: "load", key: String(key) });
    if (!answer) return { status: "failed" };
    if (answer.status === "missing") return { status: "missing" };
    if (answer.status !== "found") return { status: "failed" };
    return { status: "found", value: answer.value };
  }

  async function saveData(key, value) {
    if (!saveEndpoint) return { status: "unavailable" };
    const text = typeof value === "string" ? value : JSON.stringify(value);
    const answer = await saveRequest({ op: "save", key: String(key), value: text });
    return answer && answer.status === "saved" ? { status: "acknowledged" } : { status: "failed" };
  }

  /* The platform is Russian-language by requirement, and the SDK reports no
     locale of its own. */
  function getLocale() {
    return "ru";
  }

  /* No board exists in the SDK: a game that wants one brings its own backend,
     the same way it brings its saves. */
  function leaderboardCaps() {
    return { canRead: false, canWrite: false, needsLogin: false, nativePopup: false };
  }

  async function submitScore() {
    return { status: "unsupported" };
  }

  async function fetchEntries() {
    return { status: "unsupported" };
  }

  async function showLeaderboard() {
    return { status: "unsupported" };
  }

  const visibilityDocument = documentRef();
  if (visibilityDocument && typeof visibilityDocument.addEventListener === "function") {
    visibilityDocument.addEventListener("visibilitychange", onVisibilityChange);
  }

  return {
    destroy() {
      destroyed = true;
      sdkInstance = null;
      if (visibilityDocument && typeof visibilityDocument.removeEventListener === "function") {
        visibilityDocument.removeEventListener("visibilitychange", onVisibilityChange);
      }
    },
    fetchEntries,
    gameLoadingFinished,
    gameLoadingProgress,
    gameReady,
    gameplayStart,
    gameplayStop,
    getLocale,
    getPlayer,
    hideBanner,
    leaderboardCaps,
    loadData,
    login,
    /* Analytics is the portal's own funnel, reported in the Studio; there is
       no sink for the game's typed measure events. */
    measure() {},
    ready,
    saveData,
    showBanner,
    showInterstitial,
    showLeaderboard,
    showRewarded,
    submitScore,
  };
}

export const createPlatformSdkAdapter = createPikabuPlatformAdapter;
