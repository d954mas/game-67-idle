"use strict";

// Pure shaping of one accepted batch into the documents the function writes,
// and of the read-back counters into the report body. No Firestore types here,
// so the whole aggregation is testable without an emulator.

const { EVENT_SCHEMA } = require("./validate");

const SEEN_CAP = 50;
const RAW_TTL_MS = 30 * 24 * 3600 * 1000;
const PLAY_BUCKET_BOUNDS = [60, 180, 600];
// The Poki bar the report judges a build against.
const PLAY_BAR_SECONDS = 180;
const PLAY_BAR_SHARE = 0.25;

// Document ids join their parts with a double underscore, so a build id may not
// carry one of its own without aliasing a neighbouring bucket.
function buildKey(build) {
  return String(build).replace(/_{2,}/g, "_");
}

function sessionScope(batch) {
  return `${buildKey(batch.build)}__${batch.platform}`;
}

function funnelKey(batch) {
  return `${batch.game}__${buildKey(batch.build)}__${batch.platform}`;
}

function fnv1a32(text) {
  let h = 0x811c9dc5;
  for (let i = 0; i < text.length; i += 1) {
    h ^= text.charCodeAt(i);
    h = Math.imul(h, 0x01000193);
  }
  return (h >>> 0).toString(16).padStart(8, "0");
}

// A client retry repeats the same window of events; the shape of that window is
// the only identity available, since the client sends no batch id.
function batchHash(batch) {
  const events = batch.events;
  if (!events.length) return null;
  const first = events[0].t;
  const last = events[events.length - 1].t;
  return fnv1a32(`${batch.session}|${first}|${last}|${events.length}`);
}

function enumValue(name, field, value) {
  const spec = EVENT_SCHEMA[name] && EVENT_SCHEMA[name][field];
  if (!spec || spec.kind !== "str" || !spec.values) return null;
  return typeof value === "string" && spec.values.has(value) ? value : null;
}

function levelIndex(value) {
  return typeof value === "number" && Number.isInteger(value) && value >= 0 && value <= 9999 ? value : null;
}

function funnelIncrements(batch) {
  const out = { batches: 1, events: batch.events.length };
  const bump = (field, by = 1) => {
    out[field] = (out[field] || 0) + by;
  };
  for (const event of batch.events) {
    if (event.n === "level") {
      const action = enumValue("level", "a", event.a);
      const level = levelIndex(event.level);
      if (action === null || level === null) continue;
      bump(`level_${action}_${level}`);
      if (action !== "start" && typeof event.sec === "number") bump(`level_sec_${level}`, event.sec);
    } else if (event.n === "merge") {
      bump("merges");
    } else if (event.n === "buy") {
      bump("buys");
    } else if (event.n === "result") {
      const action = enumValue("result", "a", event.a);
      if (action) bump(`result_${action}`);
    } else if (event.n === "ad") {
      const kind = enumValue("ad", "kind", event.kind);
      const action = enumValue("ad", "a", event.a);
      if (kind && action) bump(`ad_${kind}_${action}`);
    }
  }
  return out;
}

function maxEventT(batch) {
  let max = 0;
  for (const event of batch.events) if (event.t > max) max = event.t;
  return max;
}

function maxPlay(batch) {
  let max = 0;
  for (const event of batch.events) {
    if (event.n === "heartbeat" && typeof event.play === "number" && event.play > max) max = event.play;
  }
  return max;
}

function maxLevelStarted(batch) {
  let max = 0;
  for (const event of batch.events) {
    if (event.n !== "level" || enumValue("level", "a", event.a) !== "start") continue;
    const level = levelIndex(event.level);
    if (level !== null && level > max) max = level;
  }
  return max;
}

// Batches arrive out of order and get retried, so every session field takes the
// extreme it can never walk back from rather than the newest value.
function mergeSession(existing, batch, receivedAt) {
  const hash = batchHash(batch);
  const seen = Array.isArray(existing && existing.seen) ? existing.seen : [];
  if (hash && seen.includes(hash)) return null;

  const startedAt = batch.sent_at - Math.round(maxEventT(batch) * 1000);
  const nextSeen = hash ? [...seen, hash].slice(-SEEN_CAP) : seen;
  const previous = existing || {};

  return {
    player: previous.player || batch.player,
    started_at: typeof previous.started_at === "number" ? Math.min(previous.started_at, startedAt) : startedAt,
    play: Math.max(typeof previous.play === "number" ? previous.play : 0, maxPlay(batch)),
    last_t: Math.max(typeof previous.last_t === "number" ? previous.last_t : 0, maxEventT(batch)),
    levels_reached: Math.max(
      typeof previous.levels_reached === "number" ? previous.levels_reached : 0,
      maxLevelStarted(batch),
    ),
    updated_at: Math.max(typeof previous.updated_at === "number" ? previous.updated_at : 0, receivedAt),
    seen: nextSeen,
  };
}

function mergePlayer(existing, receivedAt) {
  const first = existing && typeof existing.first_seen === "number" ? existing.first_seen : receivedAt;
  const last = existing && typeof existing.last_seen === "number" ? Math.max(existing.last_seen, receivedAt) : receivedAt;
  return { first_seen: first, last_seen: last };
}

function rawDocument(batch, receivedAt) {
  return { ...batch, received_at: receivedAt, expire_at: receivedAt + RAW_TTL_MS };
}

// ---------------- report ----------------

function emptyParts() {
  return {
    sessions: 0,
    players: 0,
    play_sum: 0,
    over: [0, 0, 0],
    funnel: {},
  };
}

// `over[i]` counts sessions whose play passed PLAY_BUCKET_BOUNDS[i]; buckets are
// differences of those counts, which is one aggregation query per bound instead
// of one per bucket.
function sumParts(parts) {
  const total = emptyParts();
  for (const part of parts) {
    total.sessions += part.sessions || 0;
    total.players += part.players || 0;
    total.play_sum += part.play_sum || 0;
    for (let i = 0; i < total.over.length; i += 1) total.over[i] += (part.over && part.over[i]) || 0;
    for (const [field, value] of Object.entries(part.funnel || {})) {
      if (typeof value !== "number") continue;
      total.funnel[field] = (total.funnel[field] || 0) + value;
    }
  }
  return total;
}

function levelsFromFunnel(funnel) {
  const levels = new Map();
  const slot = (level) => {
    if (!levels.has(level)) levels.set(level, { level, start: 0, complete: 0, fail: 0, sec_sum: 0 });
    return levels.get(level);
  };
  for (const [field, value] of Object.entries(funnel || {})) {
    if (typeof value !== "number") continue;
    const match = /^level_(start|complete|fail|sec)_(\d+)$/.exec(field);
    if (!match) continue;
    const entry = slot(Number(match[2]));
    if (match[1] === "sec") entry.sec_sum += value;
    else entry[match[1]] += value;
  }
  return [...levels.values()]
    .sort((a, b) => a.level - b.level)
    .map((entry) => {
      const finished = entry.complete + entry.fail;
      return {
        level: entry.level,
        start: entry.start,
        complete: entry.complete,
        fail: entry.fail,
        sec_avg: finished ? round(entry.sec_sum / finished, 1) : 0,
      };
    });
}

function round(value, digits) {
  const scale = 10 ** digits;
  return Math.round(value * scale) / scale;
}

function assembleReport(parts) {
  const sessions = parts.sessions || 0;
  const [over60, over180, over600] = parts.over || [0, 0, 0];
  const share = sessions ? over180 / sessions : 0;
  return {
    sessions,
    players: parts.players || 0,
    play_avg: sessions ? round(parts.play_sum / sessions, 1) : 0,
    play_over_180_share: round(share, 4),
    play_buckets: {
      "0-60": Math.max(sessions - over60, 0),
      "60-180": Math.max(over60 - over180, 0),
      "180-600": Math.max(over180 - over600, 0),
      "600+": over600,
    },
    levels: levelsFromFunnel(parts.funnel),
  };
}

module.exports = {
  PLAY_BAR_SECONDS,
  PLAY_BAR_SHARE,
  PLAY_BUCKET_BOUNDS,
  RAW_TTL_MS,
  SEEN_CAP,
  assembleReport,
  batchHash,
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
};
