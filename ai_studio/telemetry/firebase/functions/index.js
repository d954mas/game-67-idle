/* eslint-disable */
// Portal independent gameplay telemetry: one HTTP endpoint that accepts small
// event batches from a game build and answers funnel reports for the studio.
// Poki is the only portal of the three with a game-events API, so the funnel
// that has to work everywhere is ours.
//
// Hot path cost per accepted batch: two reads (session, player) inside one
// transaction and four writes (raw, session, player, funnel counters). The
// funnel doc uses increments so concurrent batches never clobber each other.
const { onRequest } = require("firebase-functions/v2/https");
const { defineString } = require("firebase-functions/params");
const { initializeApp } = require("firebase-admin/app");
const { getFirestore, FieldValue, AggregateField, Timestamp } = require("firebase-admin/firestore");

const { parseGames, validateBatch } = require("./lib/validate");
const {
  PLAY_BUCKET_BOUNDS,
  assembleReport,
  buildKey,
  emptyParts,
  funnelIncrements,
  funnelKey,
  mergePlayer,
  mergeSession,
  rawDocument,
  sessionScope,
  sumParts,
} = require("./lib/aggregate");

initializeApp();
const db = getFirestore();

// The key is a shared string compiled into the game; it stops stray traffic, it
// is not a secret. The game allowlist is what actually bounds the write surface.
const TELEMETRY_KEY = defineString("TELEMETRY_KEY");
const TELEMETRY_GAMES = defineString("TELEMETRY_GAMES");

// A report without a build or a platform fans out over every bucket of the
// game; the cap keeps a mistyped game id from turning into an unbounded read.
const MAX_REPORT_SCOPES = 200;

const RAW_ROOT = "telemetry_raw";
const SESSION_ROOT = "telemetry_sessions";
const PLAYER_ROOT = "telemetry_players";
const FUNNEL_ROOT = "telemetry_funnel";

function sessionsCollection(game, scope) {
  return db.collection(SESSION_ROOT).doc(game).collection(scope);
}

function playersCollection(game, scope) {
  return db.collection(PLAYER_ROOT).doc(game).collection(scope);
}

function parseScope(scope) {
  const cut = scope.lastIndexOf("__");
  if (cut <= 0) return null;
  return { build: scope.slice(0, cut), platform: scope.slice(cut + 2) };
}

function readBody(req) {
  const body = req.body;
  if (Buffer.isBuffer(body)) return JSON.parse(body.toString("utf8"));
  if (typeof body === "string") return JSON.parse(body);
  return body;
}

async function ingest(req, res, key, games) {
  let body;
  try {
    body = readBody(req);
  } catch (error) {
    return res.status(400).json({ error: "body is not JSON" });
  }

  const receivedAt = Date.now();
  const result = validateBatch(body, { key, games, raw: req.rawBody, now: receivedAt });
  if (!result.ok) return res.status(result.status).json({ error: result.error });

  const batch = result.batch;
  if (!batch.events.length) return res.status(200).json({ ok: true, accepted: 0 });

  const scope = sessionScope(batch);
  const sessionRef = sessionsCollection(batch.game, scope).doc(batch.session);
  const playerRef = playersCollection(batch.game, scope).doc(batch.player);
  const funnelRef = db.collection(FUNNEL_ROOT).doc(funnelKey(batch));
  const rawRef = db.collection(RAW_ROOT).doc(batch.game).collection("batches").doc();

  const increments = funnelIncrements(batch);
  const duplicate = await db.runTransaction(async (tx) => {
    const [sessionSnap, playerSnap] = await tx.getAll(sessionRef, playerRef);
    const session = mergeSession(sessionSnap.exists ? sessionSnap.data() : null, batch, receivedAt);
    if (!session) return true;

    // The TTL policy only reads a Timestamp, so the pure layer's epoch millis
    // are converted here rather than leaking a Firestore type into it.
    const raw = rawDocument(batch, receivedAt);
    tx.set(rawRef, { ...raw, expire_at: Timestamp.fromMillis(raw.expire_at) });
    tx.set(sessionRef, session);
    tx.set(playerRef, mergePlayer(playerSnap.exists ? playerSnap.data() : null, receivedAt));

    const counters = {};
    for (const [field, value] of Object.entries(increments)) counters[field] = FieldValue.increment(value);
    tx.set(funnelRef, counters, { merge: true });
    return false;
  });

  return res.status(200).json({ ok: true, accepted: duplicate ? 0 : batch.events.length });
}

async function scopeParts(game, scope) {
  const sessions = sessionsCollection(game, scope);
  const [totals, ...overs] = await Promise.all([
    sessions.aggregate({ count: AggregateField.count(), play: AggregateField.sum("play") }).get(),
    ...PLAY_BUCKET_BOUNDS.map((bound) => sessions.where("play", ">", bound).count().get()),
  ]);
  const players = await playersCollection(game, scope).count().get();
  const funnel = await db.collection(FUNNEL_ROOT).doc(`${game}__${scope}`).get();

  const parts = emptyParts();
  parts.sessions = totals.data().count || 0;
  parts.play_sum = totals.data().play || 0;
  parts.players = players.data().count || 0;
  parts.over = overs.map((snap) => snap.data().count || 0);
  parts.funnel = funnel.exists ? funnel.data() : {};
  return parts;
}

async function report(req, res, key) {
  if (req.query.key !== key) return res.status(401).json({ error: "bad key" });
  const game = typeof req.query.game === "string" ? req.query.game : "";
  if (!/^[A-Za-z0-9_-]{1,64}$/.test(game)) return res.status(400).json({ error: "bad game" });

  const wantBuild = typeof req.query.build === "string" && req.query.build ? buildKey(req.query.build) : null;
  const wantPlatform = typeof req.query.platform === "string" && req.query.platform ? req.query.platform : null;

  const collections = await db.collection(SESSION_ROOT).doc(game).listCollections();
  const scopes = collections
    .map((collection) => collection.id)
    .map((id) => ({ id, parsed: parseScope(id) }))
    .filter((entry) => entry.parsed)
    .filter((entry) => (wantBuild ? entry.parsed.build === wantBuild : true))
    .filter((entry) => (wantPlatform ? entry.parsed.platform === wantPlatform : true))
    .slice(0, MAX_REPORT_SCOPES);

  const parts = await Promise.all(scopes.map((entry) => scopeParts(game, entry.id)));
  const body = assembleReport(sumParts(parts));
  body.scopes = scopes.map((entry) => entry.parsed);
  return res.status(200).json(body);
}

exports.telemetry = onRequest({ region: ["europe-west1"] }, async (req, res) => {
  try {
    res.set("Access-Control-Allow-Origin", "*");
    res.set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set("Access-Control-Allow-Headers", "Content-Type");
    res.set("Cache-Control", "no-store");

    if (req.method === "OPTIONS") return res.status(204).send("");

    const key = TELEMETRY_KEY.value();
    const games = parseGames(TELEMETRY_GAMES.value());
    // An unset key or an empty allowlist would accept anything from anyone, so
    // a half configured deployment stores nothing at all.
    if (!key || !games.size) return res.status(503).json({ error: "telemetry is not configured" });

    if (req.method === "GET") {
      if (req.query.report === "1") return await report(req, res, key);
      return res.status(400).json({ error: "report=1 required" });
    }
    if (req.method === "POST") return await ingest(req, res, key, games);
    return res.status(405).json({ error: "method not allowed" });
  } catch (error) {
    console.error("telemetry failed", error);
    return res.status(500).json({ error: "internal error" });
  }
});
