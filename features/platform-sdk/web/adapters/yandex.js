const YANDEX_SDK_URL = "/sdk.js";
const AD_TIMEOUT_MS = 120000;
/* The portal's own leaderboard quotas: one setScore per second, twenty
   getEntries per five minutes. A write waits for its slot, because the last
   score of a session must not be lost to a one-second window; a read over
   quota is refused at once, because the screen still has its last page. */
const SET_SCORE_INTERVAL_MS = 1000;
const GET_ENTRIES_WINDOW_MS = 5 * 60 * 1000;
const GET_ENTRIES_PER_WINDOW = 20;
const ENTRIES_TOP = 20;
const ENTRIES_AROUND = 10;
const AVATAR_SIZE = "small";

export function createYandexPlatformAdapter({ host, lifecycle, sdkUrl = YANDEX_SDK_URL, now = () => Date.now() }) {
  let sdkReady = null;
  let playerReady = null;
  let ysdkInstance = null;
  let portalLocale = null;
  let destroyed = false;
  let portalPauseInstalled = false;
  let setScoreChain = Promise.resolve();
  let lastSetScoreAt = -Infinity;
  const getEntriesTimes = [];

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
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

  function loadScript() {
    const root = windowRef();
    if (root.YaGames && typeof root.YaGames.init === "function") return Promise.resolve(root.YaGames);
    const document = documentRef();
    if (!document || !document.head || typeof document.createElement !== "function") return Promise.resolve(null);
    return new Promise((resolve) => {
      const script = document.createElement("script");
      script.async = true;
      script.src = sdkUrl;
      script.onload = () => resolve(root.YaGames || null);
      script.onerror = () => resolve(null);
      document.head.appendChild(script);
    });
  }

  function notifyPause() {
    if (!destroyed && lifecycle && typeof lifecycle.pause === "function") lifecycle.pause();
  }

  function notifyResume() {
    if (!destroyed && lifecycle && typeof lifecycle.resume === "function") lifecycle.resume();
  }

  /* Bound before the SDK exists, because a pause that arrives during loading is
     still a pause, and the documented channel is only reachable once init has
     resolved. Both channels reach the same facade, which ignores a second pause
     while it is already paused. */
  function installPortalPauseEvents() {
    if (portalPauseInstalled) return;
    const root = windowRef();
    if (!root || typeof root.addEventListener !== "function") return;
    portalPauseInstalled = true;
    root.addEventListener("game_api_pause", notifyPause);
    root.addEventListener("game_api_resume", notifyResume);
    /* Focus and visibility are different questions, and the portal asks both:
       sound must stop when the game loses focus even though the tab is still
       on screen. The game runs in the portal's frame, so a click on the page
       around it blurs the game without hiding it. */
    root.addEventListener("blur", notifyPause);
    root.addEventListener("focus", notifyResume);
  }

  /* The channel the portal documents and its console checks for. The window
     events above are what the dev proxy actually dispatches, so the adapter
     listens on both rather than betting on one. */
  function subscribePortalPause(ysdk) {
    if (!ysdk || typeof ysdk.on !== "function") return;
    try {
      ysdk.on("game_api_pause", notifyPause);
      ysdk.on("game_api_resume", notifyResume);
    } catch { /* an SDK build without the event surface */ }
  }

  async function sdk() {
    if (!sdkReady) {
      installPortalPauseEvents();
      sdkReady = loadScript()
        .then((YaGames) => (YaGames && typeof YaGames.init === "function" ? YaGames.init() : null))
        .then((ysdk) => {
          readPortalLocale(ysdk);
          subscribePortalPause(ysdk);
          return ysdk;
        })
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

  function isAuthorized(p) {
    try {
      return Boolean(p && typeof p.isAuthorized === "function" && p.isAuthorized());
    } catch {
      return false;
    }
  }

  /* Only an authorized player has a name and a photo the game may show; an
     anonymous one has an id and nothing else. The player object owns getPhoto;
     getAvatarSrc belongs to leaderboard entries. */
  function identity(p) {
    const authorized = isAuthorized(p);
    let name = "";
    let avatarUrl = "";
    if (authorized) {
      try { name = String((typeof p.getName === "function" && p.getName()) || ""); } catch { name = ""; }
      try { avatarUrl = String((typeof p.getPhoto === "function" && p.getPhoto("medium")) || ""); } catch { avatarUrl = ""; }
    }
    return { authorized, name, avatarUrl };
  }

  async function getPlayer() {
    return identity(await player());
  }

  /* The dialog rejects when the player closes it; that is a decline, not an
     error, so nothing is logged. After it settles the cached player object is
     dropped: the anonymous one keeps answering as anonymous and would keep
     writing player data under the anonymous id. */
  async function login() {
    const ysdk = await sdk();
    if (!ysdk || !ysdk.auth || typeof ysdk.auth.openAuthDialog !== "function") {
      return { supported: false, authorized: false, reason: "unsupported", name: "", avatarUrl: "" };
    }
    const before = await player();
    if (isAuthorized(before)) return { supported: true, reason: "accepted", ...identity(before) };

    let opened = true;
    try {
      await ysdk.auth.openAuthDialog();
    } catch {
      opened = false;
    }
    if (destroyed) return { supported: true, authorized: false, reason: "failed", name: "", avatarUrl: "" };
    playerReady = null;
    const after = identity(await player());
    if (!opened || !after.authorized) {
      return { supported: true, authorized: false, reason: "declined", name: "", avatarUrl: "" };
    }
    return { supported: true, reason: "accepted", ...after };
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

    const failed = { supported: true, shown: false, reason: "failed" };
    return adOperation((settle) => {
      ysdk.adv.showFullscreenAdv({
        callbacks: {
          onClose: (wasShown) => {
            settle(wasShown
              ? { supported: true, shown: true }
              : { supported: true, shown: false, reason: "skipped" });
          },
          onError: () => {
            settle(failed);
          },
          onOpen: () => {},
        },
      });
    }, failed);
  }

  async function showRewarded() {
    const ysdk = await sdk();
    if (!ysdk || !ysdk.adv || typeof ysdk.adv.showRewardedVideo !== "function") {
      return { supported: false, shown: false, rewarded: false, reason: "not_ready" };
    }

    let rewarded = false;
    const failed = { supported: true, shown: false, rewarded: false, reason: "failed" };
    return adOperation((settle) => {
      ysdk.adv.showRewardedVideo({
        callbacks: {
          onClose: (wasShown) => {
            if (rewarded) {
              settle({ supported: true, shown: Boolean(wasShown), rewarded: true });
            } else if (wasShown) {
              settle({ supported: true, shown: true, rewarded: false, reason: "skipped" });
            } else {
              settle(failed);
            }
          },
          onError: () => {
            settle(failed);
          },
          onOpen: () => {},
          onRewarded: () => {
            rewarded = true;
          },
        },
      });
    }, failed);
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

  /* The portal's language is read the moment the SDK answers, not when some
     later screen happens to ask: Yandex checks that environment.i18n.lang is
     touched while the game loads (requirement 2.14). */
  function readPortalLocale(ysdk) {
    const lang = ysdk && ysdk.environment && ysdk.environment.i18n && ysdk.environment.i18n.lang;
    if (lang) portalLocale = lang;
    return portalLocale;
  }

  /* Only the portal's answer. A browser language returned from here would be
     indistinguishable from a portal one to the game, and the fallback belongs
     where the game can see it is a fallback. */
  function getLocale() {
    return readPortalLocale(ysdkInstance) || null;
  }

  /* The portal picks where a sticky banner sits (right on desktop, top or
     bottom on a phone) and draws it over the game. The console option
     "use the API for the sticky banner" has to be on, and when it is not the
     portal answers with a reason instead of a banner. */
  async function showBanner() {
    const ysdk = await sdk();
    if (!ysdk || !ysdk.adv || typeof ysdk.adv.showBannerAdv !== "function") {
      return { supported: false, shown: false, reason: "unsupported" };
    }
    const status = await ysdk.adv.showBannerAdv().catch(() => null);
    if (!status) return { supported: true, shown: false, reason: "failed" };
    return {
      supported: true,
      shown: Boolean(status.stickyAdvIsShowing),
      reason: status.stickyAdvIsShowing ? undefined : (status.reason || "failed"),
    };
  }

  async function hideBanner() {
    const ysdk = await sdk();
    if (!ysdk || !ysdk.adv || typeof ysdk.adv.hideBannerAdv !== "function") return;
    await ysdk.adv.hideBannerAdv().catch(() => {});
  }

  /* Boards are created by hand in the console and never reset, so the portal
     serves exactly one all-time board per name: read by anyone, written only
     by an authorized player, no popup of its own. Nothing is known about a
     board until the SDK is up, and "nothing" is the honest answer then. */
  function leaderboardCaps() {
    const lb = ysdkInstance && ysdkInstance.leaderboards;
    if (destroyed || !lb) return { canRead: false, canWrite: false, needsLogin: false, nativePopup: false };
    return {
      canRead: typeof lb.getEntries === "function",
      canWrite: typeof lb.setScore === "function",
      needsLogin: true,
      nativePopup: false,
    };
  }

  function errorText(error) {
    if (!error) return "";
    const code = error && typeof error === "object" ? error.code : "";
    return `${code || ""} ${error.message || error}`.toLowerCase();
  }

  /* Only an explicit signal refuses: a missing board is gone for the session,
     an auth complaint is the player's to fix, everything else may pass later. */
  function refusalStatus(error) {
    const text = errorText(error);
    if (/not[ _]found|404/.test(text)) return "unsupported";
    if (/auth|unauthori|not[ _]logged|login/.test(text)) return "needs_login";
    return "failed";
  }

  function wait(ms) {
    const root = windowRef();
    return new Promise((resolve) => (root.setTimeout || setTimeout)(resolve, ms));
  }

  /* Writes queue behind each other so two coalesced submits a frame apart
     become two portal calls a second apart, not one refused call. */
  function submitScore(boardId, scope, value, extra) {
    const run = setScoreChain.then(() => submitScoreNow(boardId, value, extra));
    setScoreChain = run.catch(() => {});
    return run;
  }

  async function submitScoreNow(boardId, value, extra) {
    const ysdk = await sdk();
    const lb = ysdk && ysdk.leaderboards;
    if (destroyed || !lb || typeof lb.setScore !== "function") return { status: "unsupported" };
    /* An anonymous player never reaches the portal: the answer is known, and
       the write is the one thing a login can fix. */
    if (!isAuthorized(await player())) return { status: "needs_login" };

    const gap = now() - lastSetScoreAt;
    if (gap < SET_SCORE_INTERVAL_MS) await wait(SET_SCORE_INTERVAL_MS - gap);
    if (destroyed) return { status: "failed" };
    lastSetScoreAt = now();

    const score = Math.max(0, Math.floor(Number(value) || 0));
    const payload = typeof extra === "string" && extra.length > 0 ? extra : undefined;
    try {
      await lb.setScore(boardId, score, payload);
      return { status: "ok" };
    } catch (error) {
      const status = refusalStatus(error);
      if (status !== "failed" || payload === undefined) return { status };
    }
    /* A rejected payload costs the extra, never the score. */
    await wait(SET_SCORE_INTERVAL_MS);
    if (destroyed) return { status: "failed" };
    lastSetScoreAt = now();
    try {
      await lb.setScore(boardId, score);
      return { status: "ok" };
    } catch (error) {
      return { status: refusalStatus(error) };
    }
  }

  function entriesQuotaOpen() {
    const cutoff = now() - GET_ENTRIES_WINDOW_MS;
    while (getEntriesTimes.length > 0 && getEntriesTimes[0] <= cutoff) getEntriesTimes.shift();
    return getEntriesTimes.length < GET_ENTRIES_PER_WINDOW;
  }

  function entryRow(entry, myId, userRank) {
    const p = (entry && entry.player) || {};
    let avatarUrl = "";
    try { avatarUrl = String((typeof p.getAvatarSrc === "function" && p.getAvatarSrc(AVATAR_SIZE)) || ""); } catch { avatarUrl = ""; }
    const rank = Number(entry.rank) | 0;
    return {
      value: Math.max(0, Math.floor(Number(entry.score) || 0)),
      rank,
      you: Boolean((myId && p.uniqueID === myId) || (userRank > 0 && rank === userRank)),
      name: String(p.publicName || ""),
      avatarUrl,
      extra: typeof entry.extraData === "string" ? entry.extraData : "",
    };
  }

  /* The top is anonymous; the player's own row and the neighbours around it
     exist only for an authorized player, and asking for them anonymously is
     an error, not an empty answer. The first range is the top, every later
     range is the neighbourhood. */
  async function fetchEntries(boardId) {
    const ysdk = await sdk();
    const lb = ysdk && ysdk.leaderboards;
    if (destroyed || !lb || typeof lb.getEntries !== "function") return { status: "unsupported" };
    if (!entriesQuotaOpen()) return { status: "rate_limited" };
    getEntriesTimes.push(now());

    const p = await player();
    const authorized = isAuthorized(p);
    let myId = "";
    if (authorized) {
      try { myId = String((typeof p.getUniqueID === "function" && p.getUniqueID()) || ""); } catch { myId = ""; }
    }
    const options = { quantityTop: ENTRIES_TOP, includeUser: authorized };
    if (authorized) options.quantityAround = ENTRIES_AROUND;

    let result;
    try {
      result = await lb.getEntries(boardId, options);
    } catch (error) {
      return { status: refusalStatus(error) };
    }
    const entries = Array.isArray(result && result.entries) ? result.entries : [];
    const userRank = Number(result && result.userRank) | 0;
    const ranges = Array.isArray(result && result.ranges) ? result.ranges : [];
    const topSize = ranges.length > 0 && ranges[0] && Number(ranges[0].start) === 0
      ? Math.min(entries.length, Number(ranges[0].size) | 0)
      : entries.length;
    const rows = entries.map((entry) => entryRow(entry, myId, userRank));
    const top = rows.slice(0, topSize);
    const around = rows.slice(topSize);
    const mine = rows.find((row) => row.you);
    return {
      status: "ok",
      top,
      around,
      player: userRank > 0 ? { rank: userRank, value: mine ? mine.value : 0 } : null,
    };
  }

  async function showLeaderboard() {
    return { status: "unsupported" };
  }

  return {
    destroy() {
      destroyed = true;
    },
    fetchEntries,
    gameLoadingProgress() {},
    gameLoadingFinished,
    gameReady,
    gameplayStart,
    gameplayStop,
    getLocale,
    getPlayer,
    hideBanner,
    leaderboardCaps,
    loadData,
    login,
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

export const createPlatformSdkAdapter = createYandexPlatformAdapter;
