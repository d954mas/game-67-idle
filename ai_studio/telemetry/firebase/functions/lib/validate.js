"use strict";

// The wire contract shared with the C client inside every shipped build.
// A bound here may be widened but never narrowed: a build already in a portal
// cannot be re-released, and a narrowed rule turns its telemetry into 400s.

const MAX_BODY_BYTES = 64 * 1024;
const MAX_EVENTS = 200;
const MAX_FIELD_CHARS = 64;
// A session clock past a day is a stuck timer, not a player; clamping keeps it
// out of the averages without losing the rest of the batch.
const MAX_T_SECONDS = 24 * 3600;
const NUM_MIN = -1e9;
const NUM_MAX = 1e12;

const GAME_RE = /^[A-Za-z0-9_-]{1,64}$/;
// Build ids come from version strings, so dots are accepted beyond the id charset.
const BUILD_RE = /^[A-Za-z0-9_.-]{1,64}$/;
const UUID_RE = /^[A-Za-z0-9_-]{8,64}$/;
const PLATFORMS = new Set(["poki", "yandex", "playgama", "local", "itch"]);

// Timestamps outside this window are a broken device clock; the server time is
// used instead so a session still lands in the right build bucket.
const SENT_AT_MIN = 1_000_000_000_000;
const SENT_AT_MAX = 4_000_000_000_000;

function num(min, max, int) {
  return { kind: "num", min, max, int: Boolean(int) };
}

function str(...values) {
  return values.length ? { kind: "str", values: new Set(values) } : { kind: "str" };
}

// Every field that reaches a Firestore field name must come from this table:
// counter names are built from `a` and `kind`, so an unlisted value would let a
// client mint arbitrary document fields.
const EVENT_SCHEMA = Object.freeze({
  session_start: {
    vw: num(0, 16384, true),
    vh: num(0, 16384, true),
    dpr: num(0, 16),
    lang: str(),
    portrait: num(0, 1, true),
  },
  heartbeat: { play: num(0, MAX_T_SECONDS) },
  level: {
    level: num(0, 9999, true),
    a: str("start", "complete", "fail"),
    sec: num(0, MAX_T_SECONDS),
    boss: num(0, 1, true),
  },
  merge: { family: num(0, 64, true), tier: num(0, 999, true) },
  buy: {
    family: num(0, 64, true),
    tier: num(0, 999, true),
    price: num(0, NUM_MAX),
    coins: num(0, NUM_MAX),
  },
  result: { level: num(0, 9999, true), a: str("shown", "next") },
  ad: {
    kind: str("interstitial", "rewarded"),
    a: str("shown", "rewarded", "skipped", "failed"),
  },
});

function fail(status, error) {
  return { ok: false, status, error };
}

function clampNumber(value, spec) {
  const min = spec ? spec.min : NUM_MIN;
  const max = spec ? spec.max : NUM_MAX;
  let out = Math.min(Math.max(value, min), max);
  if (spec && spec.int) out = Math.round(out);
  return out;
}

function parseGames(raw) {
  return new Set(
    String(raw || "")
      .split(",")
      .map((item) => item.trim())
      .filter(Boolean),
  );
}

function normalizeEvent(raw) {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    return { error: "event must be an object" };
  }
  if (typeof raw.n !== "string" || !raw.n) return { error: "event name missing" };
  if (typeof raw.t !== "number" || !Number.isFinite(raw.t)) return { error: "event t must be a number" };

  const schema = EVENT_SCHEMA[raw.n];
  // An unknown name is a newer client talking to an older function: keep the
  // batch, drop the event.
  if (!schema) return { skip: true };

  const out = { t: clampNumber(raw.t, num(0, MAX_T_SECONDS)), n: raw.n };
  for (const [key, value] of Object.entries(raw)) {
    if (key === "t" || key === "n") continue;
    if (typeof value === "number") {
      if (!Number.isFinite(value)) return { error: `field ${key} is not finite` };
      out[key] = clampNumber(value, schema[key] && schema[key].kind === "num" ? schema[key] : null);
    } else if (typeof value === "string") {
      if (value.length > MAX_FIELD_CHARS) return { error: `field ${key} is too long` };
      out[key] = value;
    } else {
      return { error: `field ${key} must be a number or a string` };
    }
  }
  return { event: out };
}

// `body` is the parsed JSON object; `raw` is the untouched request text used for
// the size check, since the parsed object no longer carries its byte cost.
function validateBatch(body, options = {}) {
  const { key, games, raw, now = Date.now() } = options;
  const allowed = games instanceof Set ? games : parseGames(games);

  if (typeof raw === "string" && Buffer.byteLength(raw, "utf8") > MAX_BODY_BYTES) {
    return fail(400, "body too large");
  }
  if (typeof raw !== "string" && raw && typeof raw.length === "number" && raw.length > MAX_BODY_BYTES) {
    return fail(400, "body too large");
  }
  if (!body || typeof body !== "object" || Array.isArray(body)) return fail(400, "body must be a JSON object");
  if (body.v !== 1) return fail(400, "unsupported protocol version");
  if (!key || typeof body.key !== "string" || body.key !== key) return fail(401, "bad key");
  if (typeof body.game !== "string" || !GAME_RE.test(body.game)) return fail(400, "bad game");
  if (allowed.size && !allowed.has(body.game)) return fail(400, "unknown game");
  if (typeof body.build !== "string" || !BUILD_RE.test(body.build)) return fail(400, "bad build");
  if (body.build === "." || body.build === "..") return fail(400, "bad build");
  if (typeof body.platform !== "string" || !PLATFORMS.has(body.platform)) return fail(400, "bad platform");
  if (typeof body.player !== "string" || !UUID_RE.test(body.player)) return fail(400, "bad player");
  if (typeof body.session !== "string" || !UUID_RE.test(body.session)) return fail(400, "bad session");
  if (!Array.isArray(body.events)) return fail(400, "events must be an array");
  if (body.events.length > MAX_EVENTS) return fail(400, "too many events");

  const sentAt = typeof body.sent_at === "number"
    && Number.isFinite(body.sent_at)
    && body.sent_at >= SENT_AT_MIN
    && body.sent_at <= SENT_AT_MAX
    ? Math.round(body.sent_at)
    : now;

  const events = [];
  let dropped = 0;
  for (const raw_event of body.events) {
    const result = normalizeEvent(raw_event);
    if (result.error) return fail(400, result.error);
    if (result.skip) {
      dropped += 1;
      continue;
    }
    events.push(result.event);
  }

  return {
    ok: true,
    batch: {
      v: 1,
      game: body.game,
      build: body.build,
      platform: body.platform,
      player: body.player,
      session: body.session,
      sent_at: sentAt,
      events,
    },
    dropped,
  };
}

module.exports = {
  EVENT_SCHEMA,
  MAX_BODY_BYTES,
  MAX_EVENTS,
  MAX_FIELD_CHARS,
  MAX_T_SECONDS,
  PLATFORMS,
  parseGames,
  validateBatch,
};
