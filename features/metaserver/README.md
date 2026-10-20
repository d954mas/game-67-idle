# Metaserver

## Purpose

The shared online back end of a room-based game, as a Go library: guest
identity and accounts, the room registry, entry tickets, profile storage, the
heartbeat probe, minute history and the operator console. Game traffic never
passes here: after entry the client opens a WebSocket to the room and presents
the ticket in its HELLO.

A game owns its `metaserver/main.go` (the routes it mounts) and
`metaserver/cmd/admin/main.go` (its brand), its deploy, its database name and
owner, and the profile document's contents. Everything else lives here once.

## Public surface

Package `metaserver` (module `studio/metaserver`):

- `Main(mount func(s *Server, mux *http.ServeMux))` is the whole process:
  `migrate` applies the schema and provisions the login roles; no argument
  serves. Main mounts `GET /healthz`, `GET /readyz`, CORS preflight and the
  internal admin listener; `mount` registers the game's routes.
- Player handlers on `*Server`: `Guest`, `Enter`, `Register`, `Login`,
  `Password`, `Logout`; wrap each in `s.Player(limit, name, h)` with
  `PerAddress` (calls that make rows or hash passwords) or `PerSession` (entry,
  logout).
- Room handlers: `RoomStatus`, `ProfileLoad`, `ProfileSave`, `ProfileSaves`
  (the last three read `{player}` from the pattern); wrap each in
  `s.Timed(name, h)`. `name` is the label in the period log line, the minute
  history and the console.
- `PathEnter` (`/api/enter`): the heartbeat probe enters through it, so the game
  mounts `Enter` there.
- `SchemaVersion`; the login roles `RoleRuntime` (`meta_runtime`),
  `RoleAdminService` (`meta_admin_service`), `RoleBackup` (`meta_backup`).

Package `metaserver/admin`: `Main(Brand{Name, Mark})` runs the operator console;
`bootstrap-owner` and `recover-owner` are the owner's terminal ceremonies. See
[admin/README.md](admin/README.md).

The usual mount, which both current games use:

| Route | Handler | Answer |
|---|---|---|
| `POST /api/guest` | `Guest` | `{player_id, secret}` once |
| `POST /api/enter` | `Enter` | body empty or `{"page_entry": bool}`; `{room_id, url, ticket, expires_at, login}`, ticket base64url of 34 bytes; 503 when no room has a seat |
| `POST /api/register` | `Register` | body `{login, password}`; 200 `{login}`; 400 `rules`, 409 `taken` / `registered`, 503 `busy` |
| `POST /api/login` | `Login` | body `{login, password}`; 200 `{player_id, secret, login}`; 403 `wrong`, 429 `attempts`, 400 `rules`, 503 `busy` |
| `POST /api/password` | `Password` | body `{old, new}`; 204 and every other session is gone; 403, 409 `guest`, 400, 429, 503 |
| `POST /api/logout` | `Logout` | 204 |
| `POST /rooms/status` | `RoomStatus` | the shape of `testdata/room_status_v3.json` exactly; 204, 409 fenced, 400 unreadable (fences too), 401 |
| `POST /rooms/profiles/{player}/load` | `ProfileLoad` | `rev u64 LE, document`; 409 an older life, 404 no account, 413 stored document over the limit |
| `POST /rooms/profiles/{player}/save` | `ProfileSave` | `if_rev u64, save_id u64, document`; 200 `rev`, 412 `rev, room u32, incarnation u64, save_id u64`, 409 not the owner, 413, 400 |
| `POST /rooms/profiles/save` | `ProfileSaves` | 1..96 entries `account u64, if_rev u64, save_id u64, size u32, document`; 200 one `status u16, size u32, body` per entry |

Room calls carry the room's own ticket, hex, as the bearer; the routes are
plain `http://` on the stand's private network and are not for the proxy to
expose.

### Behaviour

A player starts as a guest: one `players` row and one session secret the
client keeps. A login and password can attach to that row later, which changes
nothing about the id in the tickets; `/api/login` on another device opens the
account there with a session of its own. Sessions never expire; a password
change deletes every session but the one that changed it. There is no email and
no recovery, by decision. Passwords are bcrypt hashes; wrong passwords are
counted per login and per session (10 per 15 min, then 429) before any lookup.

Rooms report themselves every `period_ms` and as soon as their seats change.
A room silent for three of its periods is not routed to. Entry goes to the
room already holding the account (a grace seat, a second tab, even in a full
room), then to the room it left within the return window (70 s), then where
there is company but no crowd (`META_ROOM_COMFORT_PERCENT`, 70); a pick books
the seat, locking the row, so a burst spreads and two picks never take one last
seat. A room lists its seats and every account whose profile it still holds
unsaved, up to three times its capacity.

The ticket is `version u8 (2) | kind u8 | subject u64 | room_id u32 |
expires_at u32 | mac[16]`, HMAC-SHA256 under the key the rooms share
(`ticket.go`; the room's C test pins the same vector). Every life of a room
carries an incarnation; a status from a life below the one on record is
refused (409), and a profile's owner is the pair (room, incarnation), so a
restarted room cannot have its predecessor's late saves land.

Profiles are opaque documents with a compare-and-swap on `rev`, idempotent
saves by save id (the SHA-256 of the document tells a retry from a reused id)
and a size limit (`META_PROFILE_MAX_BYTES`, 64 KiB). A load makes the loading
life of the room the owner; another room's saves are refused from then on.

Configuration is `META_*` (`config.go`): `META_ADDR`, `META_DATABASE_URL`,
`META_DATABASE_PASSWORD_FILE`, `META_TICKET_KEY_FILE`, `META_ROOM_URL`
(`{id}`, `{port}`, `{host}`), `META_TICKET_TTL`, `META_CALLS_PER_HOUR`,
`META_UNLIMITED_ADDRS`, `META_BEHIND_PROXY`, `META_ROOM_COMFORT_PERCENT`,
`META_PROFILE_MAX_BYTES`, `META_ADMIN_ADDR`, `META_ADMIN_NETS`,
`META_ADMIN_HOSTS`, `META_ADMIN_SERVICE_TOKEN_FILE`, `META_HEARTBEAT_URL`,
`META_HEARTBEAT_EVERY`, `META_PUBLIC_URL`, `META_TRAFFIC_MONTH_GB`,
`META_OBSERVABILITY_FILE`. The defaults are the local stand.

### Database

`schema.sql` is version 1. `migrate` records each migration's checksum and
refuses a changed one; it runs as the database's owner (checked against
`pg_database.datdba`), whatever the game names it. With
`META_RUNTIME_DB_PASSWORD_FILE`, `META_ADMIN_DB_PASSWORD_FILE` and
`META_BACKUP_DB_PASSWORD_FILE` it enables the logins of `meta_runtime`,
`meta_admin_service` and `meta_backup`. The server refuses a database below
`SchemaVersion`; with a password file it insists on `meta_runtime`.

## Validation

```text
docker compose -f features/metaserver/compose.yaml up -d --wait
go -C features/metaserver vet ./...
go -C features/metaserver test ./...
```

Every test creates and drops its own database on the PostgreSQL named by
`META_TEST_DATABASE_URL` (default: this stand) and fails, not skips, without
one. `node ai_studio/game_tools/live_games.mjs` vets and builds the
`metaserver/` of every live game that lists this feature.
`testdata/` is the room contract: the games' C tests read the same files.

## Compatibility

The wire (ticket byte 2, routes, status v3, perf v1) and the schema are shared
by every game that lists the feature; a change ships to all of them on their
next deploy. PATCH: fixes with no wire, schema or API change. MINOR: an added
route, field or migration that older rooms and clients ignore. MAJOR: a changed
wire, a removed or renamed export, or a migration that older code cannot run
against; the games move together.

## Extension points

- A game adds a route with plain `mux.HandleFunc` in its `mount`; a duplicate
  pattern panics at start.
- A different entry policy is a game handler around the exported pieces once a
  game needs one; today `Enter` is the only policy.
- A schema change is a new numbered migration next to `schema.sql`.
