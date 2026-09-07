# Telemetry

One gameplay funnel that works on every portal we ship to. Poki is the only one
of Poki, Yandex Games and Playgama with a game-events API, so the studio runs its
own endpoint: a game posts small event batches to a Cloud Function, the function
folds them into Firestore counters, and `report.mjs` prints the funnel per build
and platform.

Layout:

- `firebase/` — the deployable project (function, rules, indexes), mirroring the
  another-game leaderboards project.
- `firebase/functions/lib/` — the pure validation and aggregation modules; they
  hold every rule and are tested without Firebase.
- `lib/render.mjs` — report table rendering, also pure.
- `report.mjs` — the studio side command.
- `tests/` — `node --test` coverage for all three.

## Ingest protocol

`POST <function-url>` with `Content-Type: application/json`. CORS allows `*`
with `POST, GET, OPTIONS` and a `Content-Type` header; `OPTIONS` answers 204.

```json
{
  "v": 1,
  "key": "<api key>",
  "game": "example-game",
  "build": "v11",
  "platform": "poki",
  "player": "<32 hex uuid>",
  "session": "<32 hex uuid>",
  "sent_at": 1750000000000,
  "events": [{ "t": 12.5, "n": "level", "level": 3, "a": "start", "boss": 0 }]
}
```

`platform` is one of `poki`, `yandex`, `playgama`, `local`, `itch`. `sent_at` is
unix milliseconds. Every event is flat: `t` is seconds since session start, `n`
is the name, and the rest are numbers or strings of at most 64 characters.

| event | fields |
| --- | --- |
| `session_start` | `vw`, `vh`, `dpr`, `lang` (`en`/`ru`), `portrait` (0/1) |
| `heartbeat` | `play` — gameplay seconds so far, sent every 30 s while playing |
| `level` | `level`, `a` (`start`/`complete`/`fail`), `sec` on complete and fail, `boss` (0/1) |
| `merge` | `family` (1..3), `tier` |
| `buy` | `family`, `tier`, `price`, `coins` after the purchase |
| `result` | `level`, `a` (`shown`/`next`) |
| `ad` | `kind` (`interstitial`/`rewarded`), `a` (`shown`/`rewarded`/`skipped`/`failed`) |

Answer on success: `{"ok":true,"accepted":<n>}` with `Cache-Control: no-store`.
A refused batch answers `{"error":"..."}` with:

- `401` — the key does not match.
- `400` — `v` is not 1, the game is not in the allowlist, an id is missing or
  malformed, the platform is unknown, the batch carries more than 200 events,
  the body is over 64 KB, or a field value is neither a number nor a string.
- `405` — a method other than GET, POST or OPTIONS.
- `503` — the function has no key or no game allowlist configured; a half
  configured deployment stores nothing.

What the function tolerates instead of refusing:

- An unknown event name is dropped and the rest of the batch is kept, so a newer
  build can add an event before the function knows it.
- Numbers are clamped to their field range rather than rejected.
- A `sent_at` outside 2001..2096 is replaced with the server clock.
- A value outside the enum of `a` or `kind` keeps the event in the raw store but
  never reaches a counter: those strings become Firestore field names.

Ids (`game`, `player`, `session`) are `[A-Za-z0-9_-]`; `player` and `session` are
8 to 64 characters, `game` 1 to 64 and drawn from the allowlist. `build` also
accepts dots, since build ids are version strings.

## Storage

All of it is server side; `firestore.rules` denies every client read and write,
and the function's admin SDK bypasses the rules.

| path | holds |
| --- | --- |
| `telemetry_raw/{game}/batches/{autoId}` | the accepted batch plus `received_at` and `expire_at` (30 days) |
| `telemetry_sessions/{game}/{build}__{platform}/{session}` | `player`, `started_at`, `play`, `last_t`, `levels_reached`, `updated_at`, `seen` |
| `telemetry_players/{game}/{build}__{platform}/{player}` | `first_seen`, `last_seen` |
| `telemetry_funnel/{game}__{build}__{platform}` | flat counters: `batches`, `events`, `level_start_<n>`, `level_complete_<n>`, `level_fail_<n>`, `level_sec_<n>`, `result_shown`, `result_next`, `merges`, `buys`, `ad_<kind>_<a>` |

One request is one transaction: two reads (the session and the player document)
and four writes (raw, session, player, funnel). Session fields take the extreme
they cannot walk back from — earliest `started_at`, largest `play`, `last_t` and
`levels_reached` — because batches arrive out of order. Funnel counters use
`FieldValue.increment`, so concurrent batches never clobber one another.

A retried batch is recognised by the shape of its event window: a hash of the
session id, the first and last `t`, and the event count, kept in the session's
`seen` array (most recent 50). A repeat writes nothing and answers
`{"ok":true,"accepted":0}`.

### Raw batch TTL

`firestore.indexes.json` declares the TTL on `expire_at` for the `batches`
collection group, so `firebase deploy --only firestore` installs the policy. If
the policy has to be set by hand:

```sh
gcloud firestore fields ttls update expire_at --collection-group=batches --enable-ttl
```

## Report

`GET <function-url>?report=1&key=<key>&game=<game-id>` and optionally
`&build=<build>&platform=<portal>`. Omitting `build` or `platform` aggregates
every matching bucket of that game (at most 200 buckets per call). The body:

```json
{
  "sessions": 1000, "players": 820, "play_avg": 214.5, "play_over_180_share": 0.312,
  "play_buckets": { "0-60": 300, "60-180": 388, "180-600": 250, "600+": 62 },
  "levels": [{ "level": 1, "start": 1000, "complete": 900, "fail": 60, "sec_avg": 41.2 }],
  "scopes": [{ "build": "v11", "platform": "poki" }]
}
```

`scopes` lists what was aggregated; the rest is the funnel. The studio side:

```sh
node ai_studio/telemetry/report.mjs --url <function-url> --key <key> \
  --game example-game --build v11 --platform poki
```

It prints sessions, players, average playtime, the share over three minutes, the
play buckets, a per level table with drop-off to the next level, and a verdict
against Poki's bar: average over 180 s and at least a quarter of sessions over
180 s. `--json` prints the raw body instead.

## First time project setup (lead)

1. Create a Firebase project on the Blaze plan and enable Firestore.
2. `firebase login`
3. `firebase use --add <project-id>` from `ai_studio/telemetry/firebase`.
4. `npm --prefix functions install`
5. Set the two parameters. Either write `firebase/functions/.env` (gitignored):

   ```
   TELEMETRY_KEY=<a long random string>
   TELEMETRY_GAMES=example-game,another-game
   ```

   or leave the file out and answer the prompts `firebase deploy` raises for
   both parameters; the answers are stored in `.env.<project-id>`.
6. `firebase deploy --only functions,firestore`
7. Note the printed URL. It is
   `https://europe-west1-<project-id>.cloudfunctions.net/telemetry`, and the
   generation 2 alias `https://telemetry-<hash>-ew.a.run.app` also serves it.

Adding a game later means extending `TELEMETRY_GAMES` and deploying the function
again; nothing else changes.

## Handoff to a game

A game build reads `GAME_TELEMETRY_URL` and `GAME_TELEMETRY_KEY` at build time.
An empty URL keeps the client dormant, which is what a local development build
wants. The game side documents how it batches, retries and reports the platform;
this module owns only the endpoint and the report.

## Cost

- Ingest: 2 reads and 4 writes per batch. A 10 minute session at one batch every
  30 s is about 20 batches, so 40 reads and 80 writes per session.
- Report: per bucket, one aggregation query for count and play sum, three count
  queries for the play bounds, one count query for the players, and one document
  read for the funnel. An aggregation query is billed as a read for every 1000
  index entries it scans, not one per session.
- Raw batches carry the storage; the 30 day TTL is what keeps it flat.

## Tests

```sh
node --test ai_studio/telemetry/tests/*.test.mjs
```

`node ai_studio/studio.mjs verify --domain harness` runs the same tests as the
`studio.telemetry` check.
