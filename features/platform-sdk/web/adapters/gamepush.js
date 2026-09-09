/* GamePush. Not a portal but a publisher SDK: one build is uploaded to its
   hosting and it is GamePush that re-serves the game to two dozen platforms,
   so the adapter can never know at build time which host it woke up on. Three
   shapes follow from that. The SDK ships from four interchangeable mirrors and
   answers through a global callback rather than a promise. `gameStart()` is the
   single readiness milestone, and the preloader ad is legal only before it.
   Saves are player fields on GamePush's own servers: a field must exist in the
   project panel under the same name the game asks for, and nothing is stored
   until `sync()` confirms it. */
const SDK_MIRRORS = Object.freeze([
  "https://gs.eponesh.com/sdk/game-score.js",
  "https://s3.gamepush.com/files/gs/sdk/game-score.js",
  "https://s3-eu.gamepush.com/sdk/game-score.js",
  "https://gamepush.com/sdk/game-score.js",
]);
/* An adblocker keeps the injected script from ever answering, and the SDK
   itself detects that condition and still expects the game to play. Past this
   deadline every call degrades to a no-op instead of hanging the boot. */
const SDK_INIT_TIMEOUT_MS = 15000;
/* How long the loading screen may wait for the SDK to appear. The publisher
   has to learn the game started, but a player whose adblocker swallowed the
   script must not sit and watch a full bar: past this the game is revealed and
   the milestone is reported later, if the SDK ever arrives. */
const SDK_REVEAL_WAIT_MS = 4000;
/* How long the loading ad has to begin. A preloader with nothing to fill it
   answers neither with an ad nor with a refusal, so an unanswered call past
   this window is treated as no ad at all. Once one is on screen it is waited
   out in full, however long it runs. */
const PRELOADER_START_MS = 3000;
const AD_TIMEOUT_MS = 120000;
const SAVE_TIMEOUT_MS = 15000;
/* The global board ranks players by their own fields; this is the one the
   publisher's overlay shows by default and the one their hosts read. */
const LEADERBOARD_FIELD = "score";
const TOP_ROWS = 20;
const AROUND_ROWS = 5;

export function createGamePushPlatformAdapter({ config, host, lifecycle }) {
  const projectId = String((config && config.gamePushProjectId) || "");
  const publicToken = String((config && config.gamePushPublicToken) || "");
  let destroyed = false;
  let sdkReady = null;
  let gp = null;
  let started = false;
  let startSignalled = null;
  let preloaderShowing = null;
  let preloaderStarted = false;
  /* Ad events arrive from the SDK as well as from this adapter's own calls, so
     pause is counted: the game resumes when the last overlay is gone. */
  let pauseDepth = 0;

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
  }

  const onVisibilityChange = () => {
    if (destroyed) return;
    const hidden = Boolean(documentRef() && documentRef().hidden);
    if (hidden) enterPause();
    else leavePause();
  };

  function audio(enabled) {
    if (!lifecycle || typeof lifecycle.audio !== "function") return;
    try { lifecycle.audio(enabled); } catch { /* the facade is not up */ }
  }

  function enterPause() {
    pauseDepth += 1;
    if (pauseDepth !== 1) return;
    audio(false);
    try { if (lifecycle) lifecycle.pause(); } catch { /* the facade is not up */ }
  }

  function leavePause() {
    if (pauseDepth === 0) return;
    pauseDepth -= 1;
    if (pauseDepth !== 0) return;
    audio(true);
    try { if (lifecycle) lifecycle.resume(); } catch { /* the facade is not up */ }
  }

  function adVisible(requestId, visible) {
    if (!lifecycle || typeof lifecycle.adVisible !== "function") return;
    try { lifecycle.adVisible(requestId, visible); } catch { /* the facade is not up */ }
  }

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

  /* The mirrors are equivalent copies of one SDK, listed so a blocked or slow
     CDN is not a dead game; the first one that answers wins and the rest are
     never asked. */
  function loadScript(callbackName) {
    const root = windowRef();
    const document = documentRef();
    if (!document || !document.head || typeof document.createElement !== "function") {
      return Promise.resolve(null);
    }
    const query = `?projectId=${encodeURIComponent(projectId)}` +
      `&publicToken=${encodeURIComponent(publicToken)}` +
      `&callback=${encodeURIComponent(callbackName)}`;
    return new Promise((resolve) => {
      let answered = false;
      root[callbackName] = (instance) => {
        if (answered) return;
        answered = true;
        resolve(instance || null);
      };
      const attempt = (index) => {
        if (answered || index >= SDK_MIRRORS.length) {
          if (!answered && index >= SDK_MIRRORS.length) resolve(null);
          return;
        }
        const script = document.createElement("script");
        script.async = true;
        script.src = SDK_MIRRORS[index] + query;
        script.onerror = () => attempt(index + 1);
        document.head.appendChild(script);
      };
      attempt(0);
    });
  }

  function initSdk() {
    const callbackName = "__gamePushAdapterInit";
    const attempt = loadScript(callbackName).then(async (instance) => {
      if (!instance || destroyed) return null;
      /* Player data loads with the SDK; a save read before this resolves would
         see an empty profile and overwrite the account with a fresh one. */
      if (instance.player && instance.player.ready) await instance.player.ready;
      return instance;
    }).catch(() => null);
    return deadline(attempt, null, SDK_INIT_TIMEOUT_MS);
  }

  async function sdk() {
    if (!sdkReady) sdkReady = initSdk();
    gp = destroyed ? null : await sdkReady;
    return gp;
  }

  /* Only the full-window formats pause the game. A sticky banner shares the
     page with a running game and must not stop it. */
  function followAds(instance) {
    const ads = instance.ads;
    if (!ads || typeof ads.on !== "function") return;
    for (const format of ["fullscreen", "rewarded", "preloader"]) {
      try {
        ads.on(`${format}:start`, () => {
          if (format === "preloader") preloaderStarted = true;
          if (!destroyed) enterPause();
        });
        ads.on(`${format}:close`, () => { if (!destroyed) leavePause(); });
      } catch { /* the SDK is not broadcasting this format */ }
    }
  }

  /* The adapter is operational whether or not the SDK ever answers, and it must
     say so at once: the facade turns a failed boot into a game that never
     leaves its loading screen, while a player with an adblocker must still
     reach the game. */
  function ready() {
    sdk().then((instance) => {
      if (!instance || destroyed) return;
      followAds(instance);
      showPreloader(instance);
    }, () => {});
    return true;
  }

  /* The preloader is the one ad allowed during loading, and only before
     `gameStart()`. It never blocks loading; the reveal waits for it because the
     format is refused once the game has started. */
  function showPreloader(instance) {
    const ads = instance.ads;
    if (preloaderShowing || started || !ads || typeof ads.showPreloader !== "function") return;
    preloaderShowing = (async () => {
      if (!ads.isPreloaderAvailable) return;
      let finished = null;
      try {
        finished = Promise.resolve(ads.showPreloader()).catch(() => null);
      } catch { return; /* an unavailable preloader is a normal outcome */ }
      const closed = await deadline(finished.then(() => true), null, PRELOADER_START_MS);
      if (closed === null && !preloaderStarted) return;
      await finished;
    })();
  }

  /* No progress channel exists: GamePush draws its own loading screen and
     learns only that the game is ready. */
  function gameLoadingProgress() {}

  function signalStart() {
    if (startSignalled) return startSignalled;
    startSignalled = (async () => {
      const instance = await sdk();
      if (!instance || destroyed || typeof instance.gameStart !== "function") return;
      /* The preloader is waited for in full once the SDK is there: it is a real
         ad, and the platform refuses the format after the start. */
      if (preloaderShowing) await preloaderShowing;
      if (destroyed) return;
      started = true;
      /* The milestone is a notification, not a handshake: nothing the game does
         next depends on the publisher acknowledging it. */
      try { Promise.resolve(instance.gameStart()).catch(() => {}); } catch { /* the host is not listening */ }
    })();
    return startSignalled;
  }

  async function reveal() {
    const pending = signalStart();
    if (await deadline(sdk(), null, SDK_REVEAL_WAIT_MS)) await pending;
  }

  async function gameLoadingFinished() {
    await reveal();
  }

  async function gameReady() {
    await reveal();
  }

  /* The gameplay interval decides where the publisher's own ad breaks may fall,
     so both edges are reported even though the game asks for its own ads. */
  async function gameplayStart() {
    const instance = await sdk();
    if (!instance || typeof instance.gameplayStart !== "function") return;
    try { await instance.gameplayStart(); } catch { /* the host is not listening */ }
  }

  async function gameplayStop() {
    const instance = await sdk();
    if (!instance || typeof instance.gameplayStop !== "function") return;
    try { await instance.gameplayStop(); } catch { /* the host is not listening */ }
  }

  async function showFullscreenAd(kind, requestId, extra) {
    const unsupported = { supported: false, shown: false, reason: "unsupported", ...extra };
    const instance = await sdk();
    const ads = instance && instance.ads;
    const available = kind === "rewarded" ? "isRewardedAvailable" : "isFullscreenAvailable";
    const method = kind === "rewarded" ? "showRewardedVideo" : "showFullscreen";
    if (!ads || typeof ads[method] !== "function") return unsupported;
    /* Availability is a live answer about inventory and frequency caps, not a
       statement about the format, so a false one is an ordinary refusal. */
    if (!ads[available]) return { supported: true, shown: false, reason: "rate_limited", ...extra };
    adVisible(requestId, true);
    const result = await deadline(
      Promise.resolve(ads[method]()).then((value) => ({ value }), () => null),
      null,
      AD_TIMEOUT_MS);
    adVisible(requestId, false);
    if (result === null) return { supported: true, shown: false, reason: "timeout", ...extra };
    if (!result.value) return { supported: true, shown: false, reason: "failed", ...extra };
    return { supported: true, shown: true, ...extra, ...(kind === "rewarded" ? { rewarded: true } : {}) };
  }

  async function showInterstitial(placement, requestId) {
    return showFullscreenAd("fullscreen", requestId, {});
  }

  async function showRewarded(placement, requestId) {
    return showFullscreenAd("rewarded", requestId, { rewarded: false });
  }

  /* The sticky banner is the publisher's own frame around the canvas: it is
     placed and refreshed by the SDK, and the game only asks for it. */
  async function showBanner() {
    const instance = await sdk();
    const ads = instance && instance.ads;
    if (!ads || typeof ads.showSticky !== "function" || !ads.isStickyAvailable) {
      return { supported: false, shown: false, reason: "unsupported" };
    }
    try {
      const shown = await ads.showSticky();
      return shown ? { supported: true, shown: true } : { supported: true, shown: false, reason: "failed" };
    } catch { return { supported: true, shown: false, reason: "failed" }; }
  }

  async function hideBanner() {
    const instance = await sdk();
    const ads = instance && instance.ads;
    if (!ads || typeof ads.closeSticky !== "function") return;
    try { await ads.closeSticky(); } catch { /* nothing was on screen */ }
  }

  async function getPlayer() {
    const anonymous = { authorized: false, name: "", avatarUrl: "" };
    try {
      const instance = await sdk();
      const player = instance && instance.player;
      if (!player) return anonymous;
      return {
        authorized: Boolean(player.isLoggedIn),
        name: String(player.name || ""),
        avatarUrl: String(player.avatar || ""),
      };
    } catch { return anonymous; }
  }

  /* The overlay resolves when it closes, whether or not the player signed in,
     so the reported flag is what decides the outcome. */
  async function login() {
    const unsupported = { supported: false, authorized: false, reason: "unsupported", name: "", avatarUrl: "" };
    try {
      const instance = await sdk();
      const player = instance && instance.player;
      if (!player || typeof player.login !== "function") return unsupported;
      await player.login();
      const now = await getPlayer();
      return {
        supported: true,
        authorized: now.authorized,
        reason: now.authorized ? "accepted" : "declined",
        name: now.name,
        avatarUrl: now.avatarUrl,
      };
    } catch {
      return { supported: true, authorized: false, reason: "failed", name: "", avatarUrl: "" };
    }
  }

  async function loadData(key) {
    try {
      const instance = await sdk();
      const player = instance && instance.player;
      if (!player || typeof player.get !== "function") return { status: "unavailable" };
      const value = player.get(String(key));
      return value === undefined || value === null || value === ""
        ? { status: "missing" } : { status: "found", value: String(value) };
    } catch { return { status: "failed" }; }
  }

  /* A field the project panel does not declare is dropped by the SDK without an
     error, which would leave the game believing it had a cloud save. Reading the
     value back is local and settles that before the write is reported. */
  async function saveData(key, value) {
    const instance = await sdk();
    const player = instance && instance.player;
    if (!player || typeof player.set !== "function" || typeof player.sync !== "function") {
      return { status: "unavailable" };
    }
    const name = String(key);
    const text = typeof value === "string" ? value : JSON.stringify(value);
    try {
      player.set(name, text);
      if (String(player.get(name)) !== text) return { status: "unavailable" };
    } catch { return { status: "unavailable" }; }
    const synced = await deadline(
      Promise.resolve(player.sync()).then(() => true, () => false),
      false,
      SAVE_TIMEOUT_MS);
    return synced ? { status: "acknowledged" } : { status: "failed" };
  }

  /* The publisher resolves the language per host platform and per player, so
     the SDK answer outranks anything the game could infer from the browser. */
  function getLocale() {
    const instance = gp;
    const language = instance && (instance.language || instance.locale);
    return language ? String(language).slice(0, 2).toLowerCase() : "en";
  }

  /* The global board is built by the publisher out of player fields, so it has
     no id to address and no console entry to create: a score is a field write
     that travels with the next sync. The board id the game passes names the
     same single ranking on every other portal and is not needed here. The
     isolated boards the panel can create -- daily, per level, tournament -- are
     a different feature this game does not use. */
  function leaderboardBoard() {
    const board = gp && gp.leaderboard;
    return !destroyed && board && typeof board.fetch === "function" ? board : null;
  }

  function leaderboardCaps() {
    const board = leaderboardBoard();
    return {
      canRead: Boolean(board),
      canWrite: Boolean(!destroyed && gp && gp.player && typeof gp.player.set === "function"),
      needsLogin: false,
      nativePopup: Boolean(board && typeof board.open === "function"),
    };
  }

  async function submitScore(boardId, scope, value) {
    const instance = await sdk();
    const player = instance && instance.player;
    if (!player || typeof player.set !== "function" || typeof player.sync !== "function") {
      return { status: "unsupported" };
    }
    const score = Math.max(0, Math.floor(Number(value) || 0));
    try {
      player.set(LEADERBOARD_FIELD, score);
      if (Number(player.get(LEADERBOARD_FIELD)) !== score) return { status: "unsupported" };
    } catch { return { status: "unsupported" }; }
    const synced = await deadline(
      Promise.resolve(player.sync()).then(() => true, () => false),
      false,
      SAVE_TIMEOUT_MS);
    return synced ? { status: "ok" } : { status: "failed" };
  }

  function leaderboardRow(entry, selfId) {
    const rank = entry.position !== undefined ? entry.position : entry.rank;
    return {
      value: Math.max(0, Math.floor(Number(entry[LEADERBOARD_FIELD]) || 0)),
      rank: Math.max(0, Math.floor(Number(rank) || 0)),
      you: Boolean(selfId !== undefined && selfId !== null && entry.id === selfId),
      name: typeof entry.name === "string" ? entry.name : "",
      avatarUrl: typeof entry.avatar === "string" ? entry.avatar : "",
      extra: "",
    };
  }

  function leaderboardRows(list, selfId) {
    return Array.isArray(list) ? list.map((entry) => leaderboardRow(entry, selfId)) : [];
  }

  async function fetchEntries() {
    const instance = await sdk();
    const board = leaderboardBoard();
    if (!board) return { status: "unsupported" };
    const query = {
      orderBy: [LEADERBOARD_FIELD],
      order: "DESC",
      includeFields: [LEADERBOARD_FIELD],
    };
    try {
      /* Two answers, because one cannot be both: a clean top ends at the top,
         while the rows around the player are only returned when the player is
         asked for. */
      const [top, near] = await Promise.all([
        board.fetch({ ...query, limit: TOP_ROWS, withMe: "none" }),
        board.fetch({ ...query, limit: 1, withMe: "first", showNearest: AROUND_ROWS }),
      ]);
      if (!top) return { status: "failed" };
      const selfId = instance && instance.player ? instance.player.id : null;
      const above = near && near.abovePlayers;
      const below = near && near.belowPlayers;
      const own = near && near.player ? leaderboardRow(near.player, selfId) : null;
      return {
        status: "ok",
        top: leaderboardRows(top.players || top.topPlayers, selfId),
        around: [...leaderboardRows(above, selfId), ...(own ? [own] : []), ...leaderboardRows(below, selfId)],
        player: own ? { rank: own.rank, value: own.value } : null,
      };
    } catch { return { status: "failed" }; }
  }

  /* The overlay is the publisher's, and it covers the canvas while it is up. */
  async function showLeaderboard() {
    const board = leaderboardBoard();
    if (!board || typeof board.open !== "function") return { status: "unsupported" };
    enterPause();
    try {
      await board.open({
        orderBy: [LEADERBOARD_FIELD],
        order: "DESC",
        limit: TOP_ROWS,
        includeFields: [LEADERBOARD_FIELD],
      });
      return { status: "ok" };
    } catch {
      return { status: "failed" };
    } finally {
      leavePause();
    }
  }

  const visibilityDocument = documentRef();
  if (visibilityDocument && typeof visibilityDocument.addEventListener === "function") {
    visibilityDocument.addEventListener("visibilitychange", onVisibilityChange);
  }

  return {
    destroy() {
      destroyed = true;
      gp = null;
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
    /* Analytics is the publisher's own funnel, reported in its panel; there is
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

export const createPlatformSdkAdapter = createGamePushPlatformAdapter;
