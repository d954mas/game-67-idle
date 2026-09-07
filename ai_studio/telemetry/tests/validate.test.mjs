import test from "node:test";
import assert from "node:assert/strict";

import validate from "../firebase/functions/lib/validate.js";

const { validateBatch, parseGames, MAX_EVENTS } = validate;

const KEY = "studio-key";
const GAMES = parseGames("example-game, another-game");

function body(overrides = {}) {
  return {
    v: 1,
    key: KEY,
    game: "example-game",
    build: "v11",
    platform: "poki",
    player: "0123456789abcdef0123456789abcdef",
    session: "fedcba9876543210fedcba9876543210",
    sent_at: 1_750_000_000_000,
    events: [{ t: 0, n: "session_start", vw: 720, vh: 1280, dpr: 2, lang: "en", portrait: 1 }],
    ...overrides,
  };
}

function run(overrides, options = {}) {
  return validateBatch(body(overrides), { key: KEY, games: GAMES, now: 1_750_000_000_000, ...options });
}

test("accepts a well formed batch and drops the key from the stored shape", () => {
  const result = run();
  assert.equal(result.ok, true);
  assert.equal(result.batch.key, undefined);
  assert.equal(result.batch.game, "example-game");
  assert.deepEqual(result.batch.events[0], {
    t: 0, n: "session_start", vw: 720, vh: 1280, dpr: 2, lang: "en", portrait: 1,
  });
});

test("rejects another protocol version", () => {
  const result = run({ v: 2 });
  assert.equal(result.ok, false);
  assert.equal(result.status, 400);
});

test("a wrong key is unauthorized, not malformed", () => {
  const result = run({ key: "nope" });
  assert.equal(result.status, 401);
});

test("rejects a game outside the allowlist", () => {
  const result = run({ game: "some-other-game" });
  assert.equal(result.status, 400);
  assert.equal(result.error, "unknown game");
});

test("rejects an unknown platform", () => {
  assert.equal(run({ platform: "steam" }).status, 400);
});

test("rejects ids that are too short, too long, or carry stray characters", () => {
  assert.equal(run({ session: "short" }).status, 400);
  assert.equal(run({ player: "x".repeat(65) }).status, 400);
  assert.equal(run({ player: "bad/player/id/0123456789" }).status, 400);
  assert.equal(run({ build: "" }).status, 400);
});

test("accepts a dotted build id", () => {
  assert.equal(run({ build: "v1.2.3-rc1" }).ok, true);
});

test("rejects more than the batch cap of events", () => {
  const events = Array.from({ length: MAX_EVENTS + 1 }, (_, i) => ({ t: i, n: "merge", family: 1, tier: 2 }));
  assert.equal(run({ events }).status, 400);
});

test("rejects an oversized body before parsing costs anything", () => {
  const result = run({}, { raw: "x".repeat(64 * 1024 + 1) });
  assert.equal(result.status, 400);
  assert.equal(result.error, "body too large");
});

test("ignores an event name this function does not know", () => {
  const result = run({ events: [{ t: 1, n: "from_a_newer_build", x: 1 }, { t: 2, n: "merge", family: 1, tier: 3 }] });
  assert.equal(result.ok, true);
  assert.equal(result.dropped, 1);
  assert.equal(result.batch.events.length, 1);
});

test("rejects a field that is neither a number nor a string", () => {
  assert.equal(run({ events: [{ t: 1, n: "merge", family: true, tier: 1 }] }).status, 400);
  assert.equal(run({ events: [{ t: 1, n: "merge", family: { a: 1 }, tier: 1 }] }).status, 400);
  assert.equal(run({ events: [{ t: 1, n: "merge", family: 1, tier: Number.NaN }] }).status, 400);
});

test("rejects a string field past the field cap", () => {
  assert.equal(run({ events: [{ t: 1, n: "session_start", lang: "x".repeat(65) }] }).status, 400);
});

test("clamps numbers instead of dropping the batch", () => {
  const result = run({
    events: [
      { t: -5, n: "level", level: 99999, a: "start", boss: 7 },
      { t: 1e9, n: "heartbeat", play: 1e9 },
    ],
  });
  assert.equal(result.ok, true);
  assert.equal(result.batch.events[0].t, 0);
  assert.equal(result.batch.events[0].level, 9999);
  assert.equal(result.batch.events[0].boss, 1);
  assert.equal(result.batch.events[1].t, 24 * 3600);
  assert.equal(result.batch.events[1].play, 24 * 3600);
});

test("a device clock outside living memory falls back to server time", () => {
  const result = run({ sent_at: 42 }, { now: 1_760_000_000_000 });
  assert.equal(result.batch.sent_at, 1_760_000_000_000);
});

test("an empty event list is accepted", () => {
  const result = run({ events: [] });
  assert.equal(result.ok, true);
  assert.equal(result.batch.events.length, 0);
});
