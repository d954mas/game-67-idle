/* Wavedash. The host page injects `Wavedash` before the game's own code runs,
   so there is no SDK URL to load and nothing to bundle. Two rules shape the
   rest: the platform sells paid content and carries no ads at all, and
   `init()` is what reveals the game from behind the host loading screen, so it
   is called at the end of loading rather than at boot. */
const SDK_WAIT_MS = 5000;
const SDK_POLL_MS = 100;
const TOP_ROWS = 20;
const AROUND_AHEAD = 4;
const AROUND_BEHIND = 5;
const AVATAR_PX = 128;
/* The board metadata budget is 16 keys, 256 characters per string value. */
const EXTRA_MAX = 256;
const SAVE_ROOT = "saves";

export function createWavedashPlatformAdapter({ host, lifecycle }) {
  let destroyed = false;
  let sdkReady = null;
  let sdkInstance = null;
  let revealed = false;
  let muteUnsubscribe = null;
  const boardIds = new Map();

  function windowRef() {
    return (host && host.window) || host || globalThis;
  }

  function globalSdk() {
    const root = windowRef();
    const candidate = root && root.Wavedash;
    return candidate && typeof candidate.init === "function" ? candidate : null;
  }

  /* The injection is documented as happening before the game runs, but the
     page and the host frame are two scripts: wait a bounded moment rather than
     declaring the platform absent on a race. */
  function waitForSdk() {
    const present = globalSdk();
    if (present) return Promise.resolve(present);
    const root = windowRef();
    const start = (root && root.setInterval) || setInterval;
    const stop = (root && root.clearInterval) || clearInterval;
    return new Promise((resolve) => {
      const deadline = Date.now() + SDK_WAIT_MS;
      const timer = start(() => {
        const candidate = globalSdk();
        if (candidate || destroyed || Date.now() >= deadline) {
          stop(timer);
          resolve(candidate);
        }
      }, SDK_POLL_MS);
    });
  }

  async function sdk() {
    if (!sdkReady) sdkReady = waitForSdk().catch(() => null);
    sdkInstance = destroyed ? null : await sdkReady;
    return sdkInstance;
  }

  /* The host page owns the mute switch and mirrors it into the frame; a mute
     the player set there cannot be overridden, so the game only follows it. */
  function followPortalAudio(instance) {
    if (!lifecycle || typeof lifecycle.audio !== "function") return;
    const apply = (muted) => {
      try { lifecycle.audio(!muted); } catch { /* the facade is not up */ }
    };
    try { apply(Boolean(instance.isMuted && instance.isMuted())); } catch { /* not mirrored yet */ }
    const events = instance.Events;
    if (!events || typeof instance.on !== "function") return;
    try {
      muteUnsubscribe = instance.on(events.MUTE_CHANGED, (payload) => apply(Boolean(payload && payload.isMuted)));
    } catch { /* the host is not broadcasting */ }
  }

  async function ready() {
    const instance = await sdk();
    if (instance && !muteUnsubscribe) followPortalAudio(instance);
    return Boolean(instance);
  }

  /* `init()` calls `loadComplete()` internally, and load completion is what
     lifts the host loading screen. Whichever of the two loading milestones the
     game reaches first is the honest moment to reveal it. */
  function reveal(instance) {
    if (revealed || !instance) return;
    revealed = true;
    try { instance.init(); } catch { /* the host is not listening */ }
  }

  async function gameLoadingProgress(progress01) {
    const instance = await sdk();
    if (!instance || revealed || typeof instance.updateLoadProgressZeroToOne !== "function") return;
    const progress = Math.min(1, Math.max(0, Number(progress01) || 0));
    try { instance.updateLoadProgressZeroToOne(progress); } catch { /* the host is not listening */ }
  }

  async function gameLoadingFinished() {
    reveal(await sdk());
  }

  async function gameReady() {
    reveal(await sdk());
  }

  /* Wavedash measures a session by its own heartbeat and exposes no
     gameplay interval to bracket. */
  function gameplayStart() {}
  function gameplayStop() {}

  async function showInterstitial() {
    return { supported: false, shown: false, reason: "unsupported" };
  }

  async function showRewarded() {
    return { supported: false, shown: false, rewarded: false, reason: "unsupported" };
  }

  async function showBanner() {
    return { supported: false, shown: false, reason: "unsupported" };
  }

  function hideBanner() {}

  /* Cloud saves are files, not a key/value store: a write lands in the
     player's local IndexedDB file first and is acknowledged only once its
     upload to the player's remote root succeeds. */
  function savePath(key) {
    return `${SAVE_ROOT}/${String(key).replace(/[^A-Za-z0-9._-]/g, "_")}.json`;
  }

  function encodeText(text) {
    return new TextEncoder().encode(text);
  }

  function decodeBytes(bytes) {
    return new TextDecoder().decode(bytes);
  }

  async function loadData(key) {
    try {
      const instance = await sdk();
      if (!instance || typeof instance.remoteFileExists !== "function") return { status: "unavailable" };
      const path = savePath(key);
      const exists = await instance.remoteFileExists(path);
      if (!exists || !exists.success) return { status: "failed" };
      if (!exists.data) return { status: "missing" };
      const downloaded = await instance.downloadRemoteFile(path);
      if (!downloaded || !downloaded.success) return { status: "failed" };
      const bytes = await instance.readLocalFile(path);
      if (!bytes) return { status: "failed" };
      const text = decodeBytes(bytes);
      let value = text;
      try { value = JSON.parse(text); } catch { /* a plain string was stored */ }
      return { status: "found", value };
    } catch { return { status: "failed" }; }
  }

  async function saveData(key, value) {
    try {
      const instance = await sdk();
      if (!instance || typeof instance.writeLocalFile !== "function") return { status: "unavailable" };
      const path = savePath(key);
      const text = typeof value === "string" ? value : JSON.stringify(value);
      const written = await instance.writeLocalFile(path, encodeText(text));
      if (!written) return { status: "failed" };
      const uploaded = await instance.uploadRemoteFile(path);
      return uploaded && uploaded.success ? { status: "acknowledged" } : { status: "failed" };
    } catch { return { status: "failed" }; }
  }

  /* The SDK reports no portal language of its own; the frame's own navigator
     is the only signal there is. */
  function getLocale() {
    const root = windowRef();
    return (root && root.navigator && root.navigator.language) || null;
  }

  /* A player reaches the game already signed in: the host injects the account
     with the SDK, so identity is read, never requested. */
  async function getPlayer() {
    const anonymous = { authorized: false, name: "", avatarUrl: "" };
    try {
      const instance = await sdk();
      if (!instance || typeof instance.getUserId !== "function") return anonymous;
      const userId = instance.getUserId();
      if (!userId) return anonymous;
      const name = (typeof instance.getUsername === "function" && instance.getUsername()) || "";
      const avatarUrl = (typeof instance.getUserAvatarUrl === "function"
        && instance.getUserAvatarUrl(userId, AVATAR_PX)) || "";
      return { authorized: true, name, avatarUrl };
    } catch { return anonymous; }
  }

  async function login() {
    return { supported: false, authorized: false, reason: "unsupported", name: "", avatarUrl: "" };
  }

  function leaderboardCaps() {
    const instance = sdkInstance;
    const usable = Boolean(!destroyed && instance && typeof instance.uploadLeaderboardScore === "function");
    return { canRead: usable, canWrite: usable, needsLogin: false, nativePopup: false };
  }

  /* Boards are addressed by an id the backend assigns, so the name the game
     knows is resolved once per session. A board the console has not created
     yet is created on first use, ranked high-to-low. */
  async function resolveBoard(instance, boardId) {
    if (!boardId || typeof instance.getOrCreateLeaderboard !== "function") return { status: "unsupported" };
    const cached = boardIds.get(boardId);
    if (cached) return { id: cached };
    const sort = instance.LeaderboardSortOrder ? instance.LeaderboardSortOrder.DESC : 1;
    const display = instance.LeaderboardDisplayType ? instance.LeaderboardDisplayType.NUMERIC : 0;
    const response = await instance.getOrCreateLeaderboard(boardId, sort, display);
    const id = response && response.success && response.data ? response.data.id : null;
    if (!id) return { status: "failed" };
    boardIds.set(boardId, id);
    return { id };
  }

  async function submitScore(boardId, scope, value, extra) {
    try {
      const instance = await sdk();
      if (!instance) return { status: "unsupported" };
      const { id, status } = await resolveBoard(instance, boardId);
      if (!id) return { status };
      const score = Math.max(0, Math.floor(Number(value) || 0));
      const payload = typeof extra === "string" && extra ? { extra: extra.slice(0, EXTRA_MAX) } : undefined;
      const response = await instance.uploadLeaderboardScore(id, score, true, undefined, payload);
      return response && response.success ? { status: "ok" } : { status: "failed" };
    } catch { return { status: "failed" }; }
  }

  function mapRows(entries, userId) {
    if (!Array.isArray(entries)) return [];
    return entries.map((entry) => ({
      value: Math.max(0, Math.floor(Number(entry.score) || 0)),
      rank: Math.max(0, Math.floor(Number(entry.rank) || 0)),
      you: Boolean(userId && entry.userId === userId),
      name: typeof entry.username === "string" ? entry.username : "",
      avatarUrl: typeof entry.userAvatarUrl === "string" ? entry.userAvatarUrl : "",
      extra: entry.metadata && typeof entry.metadata.extra === "string" ? entry.metadata.extra : "",
    }));
  }

  async function fetchEntries(boardId) {
    try {
      const instance = await sdk();
      if (!instance) return { status: "unsupported" };
      const { id, status } = await resolveBoard(instance, boardId);
      if (!id) return { status };
      const [top, around, mine] = await Promise.all([
        instance.listLeaderboardEntries(id, 0, TOP_ROWS, false),
        instance.listLeaderboardEntriesAroundUser(id, AROUND_AHEAD, AROUND_BEHIND, false),
        instance.getMyLeaderboardEntries(id),
      ]);
      if (!top || !top.success) return { status: "failed" };
      const userId = typeof instance.getUserId === "function" ? instance.getUserId() : null;
      const ownRow = mine && mine.success ? mapRows(mine.data, userId)[0] : undefined;
      return {
        status: "ok",
        top: mapRows(top.data, userId),
        around: around && around.success ? mapRows(around.data, userId) : [],
        player: ownRow ? { rank: ownRow.rank, value: ownRow.value } : null,
      };
    } catch { return { status: "failed" }; }
  }

  /* The board lives in the game's own UI here: the host page draws no
     leaderboard of its own and offers no call to open one. */
  async function showLeaderboard() {
    return { status: "unsupported" };
  }

  return {
    destroy() {
      destroyed = true;
      if (typeof muteUnsubscribe === "function") {
        try { muteUnsubscribe(); } catch { /* the host is already gone */ }
      }
      muteUnsubscribe = null;
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
    /* Stats and achievements are the platform's own funnels, declared in its
       console; there is no sink for the game's typed measure events. */
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

export const createPlatformSdkAdapter = createWavedashPlatformAdapter;
