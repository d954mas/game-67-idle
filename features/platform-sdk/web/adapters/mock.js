export function createMockPlatformAdapter({ emitVisibilityChange = () => {}, host, target }) {
  const document = host && host.document;
  const isLocal = target === "local";
  let destroyed = false;

  const onVisibilityChange = () => {
    if (destroyed || !document) return;
    emitVisibilityChange(Boolean(document.hidden));
  };

  if (document && typeof document.addEventListener === "function") {
    document.addEventListener("visibilitychange", onVisibilityChange);
  }

  function unsupportedRewarded() {
    return { supported: false, shown: false, rewarded: false, reason: "unsupported" };
  }

  function unsupportedInterstitial() {
    return { supported: false, shown: false, reason: "unsupported" };
  }

  async function ready() {
    return true;
  }

  async function showInterstitial() {
    if (!isLocal || destroyed) return unsupportedInterstitial();
    return { supported: true, shown: true, reason: "completed" };
  }

  async function showRewarded() {
    if (!isLocal || destroyed) return unsupportedRewarded();
    return { supported: true, shown: true, rewarded: true };
  }

  async function loadData(key) {
    try {
      if (!isLocal || !host || !host.localStorage) return { status: "unavailable" };
      const raw = host.localStorage.getItem(`platform-sdk:${key}`);
      if (raw == null) return { status: "missing" };
      let value = raw;
      try { value = JSON.parse(raw); } catch {}
      return { status: "found", value };
    } catch { return { status: "failed" }; }
  }

  async function saveData(key, value) {
    try {
      if (!isLocal || !host || !host.localStorage) return { status: "unavailable" };
      host.localStorage.setItem(`platform-sdk:${key}`, JSON.stringify(value));
      return { status: "acknowledged" };
    } catch { return { status: "failed" }; }
  }

  function getLocale() {
    return (host && host.navigator && host.navigator.language) || null;
  }

  /* A fake account so the login flow can be walked without a portal. It lives
     for the page only; itch has no login and answers like it does for ads. */
  let authorized = false;
  const mockPlayer = { name: "Local Player", avatarUrl: "" };

  async function getPlayer() {
    return authorized
      ? { authorized: true, ...mockPlayer }
      : { authorized: false, name: "", avatarUrl: "" };
  }

  async function login() {
    if (!isLocal || destroyed) {
      return { supported: false, authorized: false, reason: "unsupported", name: "", avatarUrl: "" };
    }
    authorized = true;
    return { supported: true, authorized: true, reason: "accepted", ...mockPlayer };
  }

  /* A canned board for local development: fixed rivals plus the mock player's
     own best, ranked among them. itch has no board and answers like it does
     for ads. Row count and values are fixture, not design. */
  const cannedRivals = Array.from({ length: 9 }, (_, i) => ({ name: `Rival ${i + 1}`, value: 900 - i * 100 }));
  const mockScores = new Map();

  function leaderboardCaps() {
    if (!isLocal || destroyed) return { canRead: false, canWrite: false, needsLogin: false, nativePopup: false };
    return { canRead: true, canWrite: true, needsLogin: false, nativePopup: false };
  }

  async function submitScore(boardId, scope, value, extra) {
    if (!isLocal || destroyed) return { status: "unsupported" };
    const score = Math.max(0, Math.floor(Number(value) || 0));
    const key = `${boardId}:${scope}`;
    const best = mockScores.get(key);
    if (!best || score > best.value) mockScores.set(key, { value: score, extra: typeof extra === "string" ? extra : "" });
    return { status: "ok" };
  }

  async function fetchEntries(boardId, scope) {
    if (!isLocal || destroyed) return { status: "unsupported" };
    const mine = mockScores.get(`${boardId}:${scope}`);
    const rows = cannedRivals.map((rival) => ({ ...rival, avatarUrl: "", extra: "", you: false }));
    if (mine) rows.push({ name: authorized ? mockPlayer.name : "", value: mine.value, avatarUrl: "", extra: mine.extra, you: true });
    rows.sort((a, b) => b.value - a.value);
    const top = rows.map((row, i) => ({ ...row, rank: i + 1 }));
    const own = top.find((row) => row.you);
    return { status: "ok", top, around: [], player: own ? { rank: own.rank, value: own.value } : null };
  }

  async function showLeaderboard() {
    return { status: "unsupported" };
  }

  function destroy() {
    destroyed = true;
    if (document && typeof document.removeEventListener === "function") {
      document.removeEventListener("visibilitychange", onVisibilityChange);
    }
  }

  return {
    destroy,
    fetchEntries,
    gameLoadingProgress() {},
    gameLoadingFinished() {},
    gameReady() {},
    gameplayStart() {},
    gameplayStop() {},
    getLocale,
    getPlayer,
    hideBanner() {},
    leaderboardCaps,
    loadData,
    login,
    measure() {},
    ready,
    saveData,
    showBanner: unsupportedInterstitial,
    showInterstitial,
    showLeaderboard,
    showRewarded,
    submitScore,
  };
}

export const createPlatformSdkAdapter = createMockPlatformAdapter;
