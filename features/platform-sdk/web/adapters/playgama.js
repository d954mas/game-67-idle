const PLAYGAMA_BRIDGE_URL = "https://bridge.playgama.com/v2/stable/playgama-bridge.js";
const AD_TIMEOUT_MS = 120000;
const BANNER_POSITION = "bottom";

export function createPlaygamaPlatformAdapter({ host, lifecycle }) {
  let bridgeReady = null;
  let bridge = null;
  let destroyed = false;
  let hasStartedGameplay = false;
  let loadingPercent = null;
  const pendingAds = new Set();

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function documentRef() {
    return (host && host.document) || (windowRef() && windowRef().document);
  }

  /* A late-arriving result can depend on state the ad flow collected while it
     ran, so the fallback is asked for its value at settle time instead of being
     captured when the operation starts. */
  function adResult(result) {
    return typeof result === "function" ? result() : result;
  }

  function adOperation(start, failedResult) {
    return new Promise((resolve) => {
      let cleanup = null;
      let settled = false;
      const root = windowRef();
      const cancel = () => settle(adResult(failedResult));
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
        settle(adResult(failedResult));
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

  function notifyLifecycle(method, ...args) {
    if (!lifecycle || typeof lifecycle[method] !== "function") return;
    try {
      lifecycle[method](...args);
    } catch {}
  }

  /* Subscribing is not enough for audio: the event fires only on later changes,
     so the value that is already in effect has to be applied by hand or a game
     that starts inside a muted portal frame keeps playing sound. */
  function applyAudioState(isEnabled) {
    if (typeof isEnabled !== "boolean") return;
    notifyLifecycle("audio", isEnabled);
  }

  /* The portal pauses the game without the player touching it: Bridge folds ad
     opens, tab switches and system pauses into one aggregated state. Edges are
     forwarded raw because the C facade owns pause/resume dedupe. */
  function subscribePortalState() {
    const platform = bridge && bridge.platform;
    if (!platform || typeof platform.on !== "function") return;
    platform.on(eventName("PAUSE_STATE_CHANGED", "pause_state_changed"), (isPaused) => {
      if (destroyed) return;
      notifyLifecycle(isPaused ? "pause" : "resume");
    });
    platform.on(eventName("AUDIO_STATE_CHANGED", "audio_state_changed"), (isEnabled) => {
      if (destroyed) return;
      applyAudioState(isEnabled);
    });
    applyAudioState(platform.isAudioEnabled);
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

      try {
        subscribePortalState();
      } catch {}
      sendLoadingProgress();
      return true;
    })();
    return bridgeReady;
  }

  async function ready() {
    if (destroyed) return false;
    return Boolean(await initBridge()) && !destroyed;
  }

  function sendLoadingProgress() {
    if (destroyed || loadingPercent === null) return;
    if (!bridge || typeof bridge.setGameLoadingProgress !== "function") return;
    try {
      bridge.setGameLoadingProgress(loadingPercent);
    } catch {}
  }

  /* Bridge owns the loading overlay and hides it 700 ms after init unless the
     game reports progress, which would leave the player on a blank canvas while
     wasm and asset packs still load. Progress starts before the bridge exists,
     so the last value is kept and replayed once it does. Bridge counts percent,
     the facade counts 0..1. */
  function gameLoadingProgress(progress01) {
    const clamped = Math.max(0, Math.min(1, Number(progress01) || 0));
    loadingPercent = Math.round(clamped * 100);
    sendLoadingProgress();
  }

  async function gameLoadingFinished() {
    if (!(await ready())) return;
    loadingPercent = 100;
    sendLoadingProgress();
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
      bridge.platform.sendMessage("level_paused");
    } catch {}
  }

  async function showInterstitial(placement) {
    if (!(await ready()) || !bridge.advertisement) {
      return { supported: false, shown: false, reason: "not_ready" };
    }
    if (!bridge.advertisement.isInterstitialSupported) {
      return { supported: false, shown: false, reason: "unsupported" };
    }
    const failed = { supported: true, shown: false, reason: "failed" };
    /* Bridge reports the ordinary throttle as `failed`: the minimum delay
       between interstitials, the initial delay after game_ready, and a disabled
       placement all land here. Reporting it as a failure would make analytics
       and retry logic read routine pacing as breakage. */
    const throttled = { supported: true, shown: false, reason: "rate_limited" };
    return adOperation((settle, setCleanup) => {
      const name = eventName("INTERSTITIAL_STATE_CHANGED", "interstitial_state_changed");
      const handler = (state) => {
        if (state === "closed") settle({ supported: true, shown: true });
        else if (state === "failed") settle(throttled);
      };
      bridge.advertisement.on(name, handler);
      setCleanup(() => {
        if (typeof bridge.advertisement.off === "function") bridge.advertisement.off(name, handler);
      });
      bridge.advertisement.showInterstitial(placement || undefined);
    }, failed);
  }

  async function showRewarded(placement) {
    if (!(await ready()) || !bridge.advertisement) {
      return { supported: false, shown: false, rewarded: false, reason: "not_ready" };
    }
    if (!bridge.advertisement.isRewardedSupported) {
      return { supported: false, shown: false, rewarded: false, reason: "unsupported" };
    }
    /* `rewarded` is what earns the reward and `closed` is the terminal state.
       A platform adapter that stops at `rewarded` must not cost the player the
       reward, so every later exit — a failure, the timeout, adapter teardown —
       still resolves with the latched reward. */
    let rewarded = false;
    const failed = { supported: true, shown: false, rewarded: false, reason: "failed" };
    const earned = { supported: true, shown: false, rewarded: true };
    return adOperation((settle, setCleanup) => {
      const name = eventName("REWARDED_STATE_CHANGED", "rewarded_state_changed");
      const handler = (state) => {
        if (state === "rewarded") rewarded = true;
        if (state === "closed") {
          settle(rewarded
            ? { supported: true, shown: true, rewarded: true }
            : { supported: true, shown: true, rewarded: false, reason: "skipped" });
        } else if (state === "failed") {
          settle(rewarded ? earned : failed);
        }
      };
      bridge.advertisement.on(name, handler);
      setCleanup(() => {
        if (typeof bridge.advertisement.off === "function") bridge.advertisement.off(name, handler);
      });
      bridge.advertisement.showRewarded(placement || undefined);
    }, () => (rewarded ? earned : failed));
  }

  /* A sticky banner is the one extra ad block the portal rules allow next to
     the fullscreen formats, and the portal draws it over the game itself. */
  async function showBanner(placement) {
    if (!(await ready()) || !bridge.advertisement || typeof bridge.advertisement.showBanner !== "function") {
      return { supported: false, shown: false, reason: "unsupported" };
    }
    if (!bridge.advertisement.isBannerSupported) {
      return { supported: false, shown: false, reason: "unsupported" };
    }
    const failed = { supported: true, shown: false, reason: "failed" };
    return adOperation((settle, setCleanup) => {
      const name = eventName("BANNER_STATE_CHANGED", "banner_state_changed");
      const handler = (state) => {
        if (state === "shown") settle({ supported: true, shown: true });
        else if (state === "hidden") settle({ supported: true, shown: false, reason: "hidden" });
        else if (state === "failed") settle(failed);
      };
      bridge.advertisement.on(name, handler);
      setCleanup(() => {
        if (typeof bridge.advertisement.off === "function") bridge.advertisement.off(name, handler);
      });
      bridge.advertisement.showBanner(BANNER_POSITION, placement || undefined);
    }, failed);
  }

  async function hideBanner() {
    if (!(await ready()) || !bridge.advertisement || typeof bridge.advertisement.hideBanner !== "function") return;
    try {
      bridge.advertisement.hideBanner();
    } catch {}
  }

  async function loadData(key) {
    if (!(await ready()) || !bridge.storage || typeof bridge.storage.get !== "function") return null;
    /* The second argument is tryParseJson, not a storage type. The wrapper
       stores JSON strings and parses them here so a plain string value that is
       not JSON survives unchanged. */
    const value = await bridge.storage.get(key, false).catch(() => null);
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

  /* The host platform, not the build, decides what a board can do, and it
     says so through `bridge.leaderboards.type` once the bridge is up:
     `in_game` reads and writes, `native` and `native_popup` write and let the
     platform draw the board (the latter through a popup this adapter can
     open), `not_available` is a complete answer. Read on every call, because a
     cached answer would be the build deciding. */
  function leaderboardType() {
    const lb = bridge && bridge.leaderboards;
    return lb && typeof lb.type === "string" ? lb.type : "not_available";
  }

  function leaderboardCaps() {
    const lb = bridge && bridge.leaderboards;
    const none = { canRead: false, canWrite: false, needsLogin: false, nativePopup: false };
    if (destroyed || !lb || typeof lb.setScore !== "function") return none;
    const type = leaderboardType();
    if (type === "in_game") return { ...none, canRead: typeof lb.getEntries === "function", canWrite: true };
    if (type === "native") return { ...none, canWrite: true };
    if (type === "native_popup") return { ...none, canWrite: true, nativePopup: typeof lb.showNativePopup === "function" };
    return none;
  }

  async function submitScore(boardId, scope, value) {
    if (!(await ready())) return { status: "failed" };
    if (!leaderboardCaps().canWrite) return { status: "unsupported" };
    const score = Math.max(0, Math.floor(Number(value) || 0));
    try {
      await bridge.leaderboards.setScore(boardId, score);
      return { status: "ok" };
    } catch {
      return { status: "failed" };
    }
  }

  function playerId() {
    try {
      const p = bridge && bridge.player;
      return p && p.id != null ? String(p.id) : "";
    } catch {
      return "";
    }
  }

  async function fetchEntries(boardId) {
    if (!(await ready())) return { status: "failed" };
    if (!leaderboardCaps().canRead) return { status: "unsupported" };
    let entries;
    try {
      entries = await bridge.leaderboards.getEntries(boardId);
    } catch {
      return { status: "failed" };
    }
    const myId = playerId();
    const top = (Array.isArray(entries) ? entries : []).map((entry) => {
      const e = entry || {};
      return {
        value: Math.max(0, Math.floor(Number(e.score) || 0)),
        rank: Number(e.rank) | 0,
        you: Boolean(myId && e.id != null && String(e.id) === myId),
        name: String(e.name || ""),
        avatarUrl: String(e.photo || ""),
        extra: "",
      };
    });
    const mine = top.find((row) => row.you);
    return { status: "ok", top, around: [], player: mine ? { rank: mine.rank, value: mine.value } : null };
  }

  async function showLeaderboard(boardId) {
    if (!(await ready())) return { status: "failed" };
    if (!leaderboardCaps().nativePopup) return { status: "unsupported" };
    try {
      await bridge.leaderboards.showNativePopup(boardId);
      return { status: "ok" };
    } catch {
      return { status: "failed" };
    }
  }

  return {
    destroy() {
      destroyed = true;
      for (const cancel of pendingAds) cancel();
    },
    fetchEntries,
    gameLoadingProgress,
    gameLoadingFinished,
    gameReady,
    gameplayStart,
    gameplayStop,
    getLocale,
    /* The bridge exposes a player, but nothing the game does depends on
       Playgama auth, so the portal login stays out of the game. */
    getPlayer() {
      return Promise.resolve({ authorized: false, name: "", avatarUrl: "" });
    },
    hideBanner,
    leaderboardCaps,
    loadData,
    login() {
      return Promise.resolve({ supported: false, authorized: false, reason: "unsupported", name: "", avatarUrl: "" });
    },
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

export const createPlatformSdkAdapter = createPlaygamaPlatformAdapter;
