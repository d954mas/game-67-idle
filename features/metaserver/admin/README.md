# Operator console (package admin)

A game runs it as its own binary: `admin.Main(admin.Brand{Name, Mark})`, the
brand naming the page and the owner's TOTP entry. It is a separate operator web app with read-only access to game data. It uses its own PostgreSQL pool,
reads a narrow internal metaserver snapshot, and never receives browser/game
analytics. It is disabled unless `ADMIN_ENABLED=1`.

Current pages: overview, player search and saved revision, rooms, server metrics
from room statuses, and process/database health. The metrics page stores one
fleet snapshot per UTC minute, shows gaps explicitly, and retains raw occupancy
for 90 days. A room status counts connected seats; it does not establish unique
players, DAU, retention, or playtime. The interface labels those figures as
unavailable until server lifecycle events are collected with coverage tracking.

Configuration:

| Variable | Purpose |
|---|---|
| `ADMIN_ENABLED=1` | Explicit startup gate |
| `ADMIN_ORIGIN` | Exact HTTPS origin used by the browser and Origin check |
| `ADMIN_ADDR` | HTTP listener behind an HTTPS reverse proxy, default `:8082` |
| `ADMIN_DATABASE_URL` | Connection as the restricted `meta_admin_service` role |
| `ADMIN_DATABASE_PASSWORD_FILE` | Separate password file for that PostgreSQL role |
| `ADMIN_TOTP_KEY_FILE` | File containing a 64-character hex AES key for encrypted TOTP secrets |
| `ADMIN_META_TOKEN_FILE` | File containing the internal metaserver bearer token |
| `ADMIN_META_URL` | Internal metaserver admin listener, default `http://meta:8081` |

The metaserver needs `META_ADMIN_SERVICE_TOKEN_FILE` with the same token and an
admin listener reachable only on the private service network. The web app
refuses to start if the database role has game table privileges, schema change
privileges, audit mutation privileges, or permission to change owner credentials.
The migration creates `meta_admin_service` as `NOLOGIN`; the one-shot migration
job enables its login only when `META_ADMIN_DB_PASSWORD_FILE` is configured.
Do not use the game database role for the web service.

`bootstrap-owner` is an interactive one-off command run as the database owner
(the migrate job's role, checked against the database's owner) with the TOTP key; `recover-owner`
replaces that owner's password and factor, revokes sessions and appends an audit
record. These commands require a real terminal and never run in the web app.
The enrollment code is consumed, so wait for the next code before first login.
The owner credential stays with the database, the migrate job and the local
bootstrap ceremony.

A player card shows the saved profile's sections as saved: the console knows no
game's fields.

`GET /admin-api/v1/overview` and `/rooms` read the live metaserver snapshot.
`GET /admin-api/v1/players` and `/players/{id}` read only the `ops.v_players`
projection; detail reads append an audit record. `GET
/admin-api/v1/audience/occupancy` reads bounded 24-hour hourly aggregates of
minute snapshots. `GET /admin-api/v1/capacity?range=24h|7d|30d[&host=]` reads
the per-minute capacity history (`activity.v_*_minutes`) in 1-minute,
10-minute or hourly points: fleet seats, CPU mean and peak, traffic and API
requests, each point `ok`, `partial` or `missing`; without `host` it adds every
machine's newest minute and range summary against the CPU norms, API totals by
route and the month's traffic ledger from the metaserver. Login uses Argon2id, TOTP, an audited and revocable session,
strict host-only Secure cookie, and an Origin check. Logout also requires CSRF.

Verification is the library's: see `../README.md#validation`.
