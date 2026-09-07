import test from "node:test";
import assert from "node:assert/strict";

import validate from "../firebase/functions/lib/validate.js";
import aggregate from "../firebase/functions/lib/aggregate.js";

const { validateBatch, parseGames } = validate;
const {
  SEEN_CAP,
  assembleReport,
  buildKey,
  emptyParts,
  funnelIncrements,
  funnelKey,
  levelsFromFunnel,
  mergePlayer,
  mergeSession,
  rawDocument,
  sessionScope,
  sumParts,
} = aggregate;

const KEY = "studio-key";
const GAMES = parseGames("example-game");
const NOW = 1_750_000_000_000;

function accept(events, overrides = {}) {
  const result = validateBatch({
    v: 1,
    key: KEY,
    game: "example-game",
    build: "v11",
    platform: "poki",
    player: "0123456789abcdef0123456789abcdef",
    session: "fedcba9876543210fedcba9876543210",
    sent_at: NOW,
    events,
    ...overrides,
  }, { key: KEY, games: GAMES, now: NOW });
  assert.equal(result.ok, true, result.error);
  return result.batch;
}

// The transaction the function runs, with plain objects standing in for
// Firestore: read, merge, write, and increment the flat counters.
class FakeStore {
  constructor() {
    this.sessions = new Map();
    this.players = new Map();
    this.funnel = new Map();
    this.raw = [];
  }

  post(batch, receivedAt = NOW) {
    const scope = `${batch.game}/${sessionScope(batch)}`;
    const sessionId = `${scope}/${batch.session}`;
    const session = mergeSession(this.sessions.get(sessionId) || null, batch, receivedAt);
    if (!session) return { accepted: 0 };

    this.raw.push(rawDocument(batch, receivedAt));
    this.sessions.set(sessionId, session);
    const playerId = `${scope}/${batch.player}`;
    this.players.set(playerId, mergePlayer(this.players.get(playerId) || null, receivedAt));

    const key = funnelKey(batch);
    const counters = this.funnel.get(key) || {};
    for (const [field, value] of Object.entries(funnelIncrements(batch))) {
      counters[field] = (counters[field] || 0) + value;
    }
    this.funnel.set(key, counters);
    return { accepted: batch.events.length };
  }
}

test("a build id cannot alias a neighbouring bucket through its separator", () => {
  assert.equal(buildKey("v11"), "v11");
  assert.equal(buildKey("v11__poki"), "v11_poki");
  assert.equal(funnelKey(accept([], { build: "v11" })), "example-game__v11__poki");
  assert.equal(sessionScope(accept([], { build: "v11" })), "v11__poki");
});

test("counters carry one flat field per funnel step", () => {
  const batch = accept([
    { t: 1, n: "level", level: 3, a: "start", boss: 0 },
    { t: 40, n: "level", level: 3, a: "complete", sec: 30, boss: 0 },
    { t: 41, n: "result", level: 3, a: "shown" },
    { t: 42, n: "result", level: 3, a: "next" },
    { t: 50, n: "merge", family: 1, tier: 4 },
    { t: 51, n: "buy", family: 2, tier: 1, price: 25, coins: 100 },
    { t: 60, n: "ad", kind: "rewarded", a: "rewarded" },
  ]);
  assert.deepEqual(funnelIncrements(batch), {
    batches: 1,
    events: 7,
    level_start_3: 1,
    level_complete_3: 1,
    level_sec_3: 30,
    result_shown: 1,
    result_next: 1,
    merges: 1,
    buys: 1,
    ad_rewarded_rewarded: 1,
  });
});

test("a value outside the enum never becomes a counter field", () => {
  const batch = accept([
    { t: 1, n: "ad", kind: "banner", a: "shown" },
    { t: 2, n: "level", level: 1, a: "abandoned" },
  ]);
  const increments = funnelIncrements(batch);
  assert.deepEqual(Object.keys(increments).sort(), ["batches", "events"]);
});

test("session fields take the extreme, whatever order the batches arrive in", () => {
  const late = accept([{ t: 200, n: "heartbeat", play: 180 }, { t: 210, n: "level", level: 7, a: "start" }]);
  const early = accept([{ t: 5, n: "heartbeat", play: 5 }, { t: 6, n: "level", level: 2, a: "start" }]);

  const first = mergeSession(null, late, NOW);
  const second = mergeSession(first, early, NOW + 1000);
  assert.equal(second.play, 180);
  assert.equal(second.last_t, 210);
  assert.equal(second.levels_reached, 7);
  assert.equal(second.started_at, NOW - 210_000);
  assert.equal(second.updated_at, NOW + 1000);
});

test("a retried batch is seen once", () => {
  const store = new FakeStore();
  const batch = accept([
    { t: 1, n: "level", level: 1, a: "start" },
    { t: 30, n: "heartbeat", play: 30 },
  ]);
  assert.equal(store.post(batch).accepted, 2);
  assert.equal(store.post(batch).accepted, 0);
  assert.equal(store.raw.length, 1);
  assert.equal(store.funnel.get(funnelKey(batch)).level_start_1, 1);
  assert.equal(store.funnel.get(funnelKey(batch)).batches, 1);
});

test("the dedupe memory stays bounded", () => {
  let session = null;
  for (let i = 0; i < SEEN_CAP + 20; i += 1) {
    session = mergeSession(session, accept([{ t: i, n: "merge", family: 1, tier: i }]), NOW + i);
  }
  assert.equal(session.seen.length, SEEN_CAP);
});

test("a first sighting keeps its timestamp while the last one moves", () => {
  const first = mergePlayer(null, NOW);
  const second = mergePlayer(first, NOW + 5000);
  assert.equal(second.first_seen, NOW);
  assert.equal(second.last_seen, NOW + 5000);
  assert.equal(mergePlayer(second, NOW - 5000).last_seen, NOW + 5000);
});

test("raw batches expire on their own", () => {
  const raw = rawDocument(accept([]), NOW);
  assert.equal(raw.received_at, NOW);
  assert.equal(raw.expire_at, NOW + 30 * 24 * 3600 * 1000);
});

test("levels come back sorted with an average over the sessions that finished", () => {
  const levels = levelsFromFunnel({
    level_start_2: 10, level_complete_2: 6, level_fail_2: 2, level_sec_2: 40,
    level_start_1: 20, level_complete_1: 18, level_sec_1: 90,
    merges: 5,
  });
  assert.deepEqual(levels.map((level) => level.level), [1, 2]);
  assert.equal(levels[0].sec_avg, 5);
  assert.equal(levels[1].sec_avg, 5);
  assert.equal(levels[1].fail, 2);
});

test("buckets are differences of the counted bounds", () => {
  const parts = emptyParts();
  parts.sessions = 100;
  parts.players = 80;
  parts.play_sum = 20000;
  parts.over = [60, 30, 10];
  const report = assembleReport(parts);
  assert.equal(report.play_avg, 200);
  assert.equal(report.play_over_180_share, 0.3);
  assert.deepEqual(report.play_buckets, { "0-60": 40, "60-180": 30, "180-600": 20, "600+": 10 });
});

test("an empty game reports zeros rather than dividing by nothing", () => {
  const report = assembleReport(emptyParts());
  assert.equal(report.sessions, 0);
  assert.equal(report.play_avg, 0);
  assert.equal(report.play_over_180_share, 0);
  assert.deepEqual(report.levels, []);
});

test("scopes add up when no build or platform is named", () => {
  const a = { sessions: 2, players: 2, play_sum: 100, over: [1, 0, 0], funnel: { level_start_1: 2 } };
  const b = { sessions: 3, players: 1, play_sum: 200, over: [2, 1, 0], funnel: { level_start_1: 3, merges: 4 } };
  const total = sumParts([a, b]);
  assert.equal(total.sessions, 5);
  assert.equal(total.play_sum, 300);
  assert.deepEqual(total.over, [3, 1, 0]);
  assert.deepEqual(total.funnel, { level_start_1: 5, merges: 4 });
});
