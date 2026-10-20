# Room ↔ metaserver contract

What a room (`online_room/ticket.h`, `online_room/meta.h`) and its client (`online_room/meta_entry.h`,
`online_room/entry.h`) use from the metaserver: tickets, entry, status, and the profile store with
compare-and-swap on `rev`. Source of truth: `features/metaserver/ticket.go`, `rooms.go`, `store.go`;
tests in `features/metaserver/admission_test.go`.

## Trust model

One key, 32 bytes, shared by the metaserver and every room (`META_TICKET_KEY_FILE`, 64 hex digits).
A holder of the key can mint any ticket; rooms and the metaserver are trusted alike. The profile
store keeps the room's document opaque and checks no seal or MAC: the room is the authority over
the document's content.

## Ticket version 2

34 bytes, little-endian:

```
version u8 (= 2) | kind u8 | subject u64 | room_id u32 | expires_at u32 | mac[16]
mac = first 16 bytes of HMAC-SHA256(key, the 18 bytes before it)
```

| kind | subject | issued by | used for |
|---|---|---|---|
| `0x54` player | the account key: the player id, never 0 | metaserver, `/api/enter` | the client's HELLO to the room |
| `0x52` room | the room's incarnation: this process's start time in ms, never 0, below 2^63 | the room itself, fresh per call | `Authorization: Bearer <hex of the 34 bytes>` on every room call |

A room bearer's `expires_at` must be at most now + 300 s by the room's clock (the old room's
`ROOM_TICKET_LIFE`). The metaserver refuses with 401 a bearer that expires more than 330 s ahead of
its own clock (300 s plus 30 s for a room clock running ahead), on both versions, so a leaked bearer
is worth minutes at most. A room further ahead than that seats nobody anyway, because every player
ticket already looks expired to it.

Version 1 (33 bytes, no version byte, first byte is the kind) is the old room's. The lengths differ
and the first byte of a version 2 ticket is `0x02`, which no version 1 kind takes, so each room
refuses the other's ticket before the mac.

Test vector (key `000102…1f`, player, subject `0x0102030405060708`, room 7, expires 1700000030):
`02540807060504030201070000001ef1536528d7b8b8d3fa973c90b19ce275f8c85c`.

The room verifies a player ticket in this order, all before a seat is touched: length 34 and
version 2; kind `0x54`; mac in constant time; `room_id` is its own; `expires_at + 5 ≥ now` (5 s of
clock skew each way); subject not 0. The TTL is `META_TICKET_TTL` (30 s default, 5 s..2 min).

Replay rule (room side): a player ticket is single-use. The room remembers the mac of every ticket it
accepted until that ticket's `expires_at + 5 s` and refuses a ticket whose mac it has seen. Without
this, a ticket read off the wire or out of a log seats its account again within TTL + skew and
supersedes the real player. The memory is bounded by the entry rate times 35 s; a new process starts
empty, which is safe because the metaserver issues tickets only for rooms that report.

## Entry (client → metaserver)

`POST /api/enter`, `Authorization: Bearer <session secret>`, body ignored.

| status | body | client does |
|---|---|---|
| 200 | `{"room_id", "url", "ticket": base64url(34 bytes), "expires_at", "login"}` | opens `url`, sends the ticket in HELLO |
| 401 | `{"error":"unknown_session"}` | the metaserver no longer knows the secret: new guest or log in; the client keeps the old record aside until the new guest has entered |
| 401 | `{"error":"session"}`, or any other body | no bearer, or not a secret (a proxy, a broken deploy): keep the session and retry |
| 429 | `{"error"}` | back off, retry |
| 503 | `{"error"}` | no room of this generation has a seat, or the database is slow: retry with backoff |

Only rooms that reported with a version 2 room ticket are candidates; `/api/enter` sends the old
client only to version 1 rooms. Placement is the old rule: the room whose last status lists the
account (in `accounts`, full or not), then the room it left within 70 s, then company under
`META_ROOM_COMFORT_PERCENT`.

## Status (room → metaserver)

`POST /rooms/status` with a version 2 room bearer and the status body version 3
(`features/metaserver/testdata/room_status_v3.json`) — unchanged from the old room. The bearer's version is
recorded, and one life (room id + incarnation) is one version: a status of the same life with the
other version is refused with 409. `accounts` must list every seat that holds an account, including
seats in grace, because entry routes a returning or second login by it.

A version 2 room also lists, after its seats, every account whose profile it still holds unsaved: a
seat whose profile has not come yet, an occupant leaving, a last save still out. A player whose grace
ran out while its last save is out is routed back to the room that holds it, never to a room that
would load the state from before. Its list is bounded at three times `capacity` (the seats, and the
room's last saves at twice its seats, `ONLINE_ROOM_PARTING_MAX`); a version 1 room's list stays
bounded at `capacity`. A capacity of a few dozen seats keeps its body well under the 4096-byte cap.

## Seats: supersede and resume

The metaserver sends every entry of an account to the room whose status lists it, so both cases
are resolved inside one room by the account key in the ticket:

- **Supersede**: a ticket for an account that already has a connected seat. The newer socket takes
  the seat; the older socket is closed as superseded. The world player, its state and its profile
  rev stay; no load.
- **Resume**: a ticket for an account whose seat is in grace. The socket takes the seat back; no
  load.
- **Across rooms** (the account's room stopped reporting and the login was placed elsewhere): the
  new room loads the profile and becomes its owner; the old room's next save gets 409 and the old
  room lets that seat go. Loss window: what the old seat earned after its last landed save, at most
  one save interval of play (30 s), is lost. The new room starts from the last stored document.

## Profile store (room → metaserver)

Both calls carry the version 2 room bearer; `{account}` is the account key in decimal. Version 1
bearers get 401 here, and version 2 bearers get 401 on the old routes. Bodies are binary.

### Load

`POST /rooms/profiles/{account}/load`, empty body. Makes this life of this room the owner,
unless it is refused: no refused load moves ownership.

| status | body | room does |
|---|---|---|
| 200 | `rev u64 LE \| document` | seat the player. `rev` 0 comes with no document (a first visit); any other rev always comes with its non-empty document (a table CHECK enforces it) |
| 413 | text | the stored document is over the current `META_PROFILE_MAX_BYTES` (the limit was lowered): refuse the join, alert. Nothing is claimed |
| 400 | text | bug: `{account}` not a positive integer |
| 401 | text | wrong key, expired or wrong-version bearer, clock off: fatal for the room build |
| 404 | text | no such account: refuse the join |
| 409 | text | this life is older than the newest the registry knows for this room id (its status), or than the owning life of this room: this process is stale, refuse the join. It cannot reclaim the profile by way of another room either |
| 500, timeout | text | refuse the join (the client enters again); do not seat without a load |

### Save

`POST /rooms/profiles/{account}/save`, body `if_rev u64 LE | save_id u64 LE | document`.
`if_rev` is the rev the room last loaded or last got back from a save. `save_id` names this save
attempt: non-zero and unique within one life of the room for that account. Suggested:
`seat_generation << 32 | per-seat sequence`. The metaserver scopes it by the room id and
incarnation of the bearer, so ids never collide across rooms or lives.

A retry is the same request byte for byte: same `if_rev`, same `save_id`, same document. A room whose
state for the seat changed since the attempt (anything the document encodes) sends a new save with a
new `save_id`, never the old id with a new document. The metaserver keeps the SHA-256 of the stored
document with its writer: the old `save_id` carrying another document is answered 412, not DONE.

| status | body | meaning, room does |
|---|---|---|
| 200 | `rev u64 LE` | landed (DONE): the rev this save produced, `if_rev + 1`; use it as the next `if_rev` |
| 412 | `rev u64 LE \| room u32 LE \| incarnation u64 LE \| save_id u64 LE` (28 bytes) | CONFLICT: the stored rev is not `if_rev`, nothing written; the tail names the save that wrote the stored rev (zeros for rev 0). Load again and decide |
| 409 | text | another room or a newer life of this room loaded the profile since: the seat was superseded; stop saving it, let it go |
| 413 | text | the document is over `META_PROFILE_MAX_BYTES` (default 64 KiB, 1 KiB..1 MiB): a room bug |
| 400 | text | no document (empty), a body under 17 bytes, `save_id` 0, or `if_rev` ≥ 2^63 |
| 401 | text | as for load |
| 500, timeout | text | outcome unknown: keep the seat dirty, retry the same save (same `if_rev`, same `save_id`) |

Guarantees:

- Every write increments `rev` by exactly one; a refused save writes nothing; a save that reached
  the database lands even if the room hung up. A load writes no rev.
- Idempotent: a save whose `save_id` (with this bearer's room and incarnation) wrote the stored rev
  with the same document is answered 200 with that rev again and writes nothing, whoever owns the
  profile now. A retry after a lost answer therefore never looks like a foreign writer.
- Of saves racing on one `if_rev` exactly one lands; the others get 412 naming it, and retries of the
  winner get its 200.
- A 412 is never DONE, whoever it names. Only a 200 means this attempt landed. A 412 naming this
  room, this life and this exact `save_id` means the id was reused with another document (a room bug).
  A 412 naming any other `save_id` of this room and life is one of its own earlier saves, not this
  attempt. Either way the stored document is not the one this attempt sent: load before deciding.
- Only the writer of the current rev is remembered: a retry of a save that landed and was then
  overwritten by another save gets 412 (or 409), naming the later writer.
- One state per answer: the row is locked before the decision, so the refusal and its writer come
  from the state the save was judged against.

### Saves in one call

`POST /rooms/profiles/save` carries many saves: 1..96 entries of
`account u64 LE | if_rev u64 LE | save_id u64 LE | size u32 LE | document`. The answer is 200 with
one `status u16 LE | size u32 LE | body` per entry, in order; each entry's status and body are what
the single save above answers it (an account of 0 is that entry's 400). Every entry is its own
compare-and-swap with every guarantee above: a batch is not all or nothing, and an entry answered
500 is retried as it was. Entries of one account in one call are saved as if sent one after the
other, in order. The metaserver writes every entry that lands with one database statement, which
locks the rows in account order, so calls racing on the same accounts never deadlock.

The whole call answers 400 for a body that is not whole entries or holds more than 96, 413 past
96 × (28 + `META_PROFILE_MAX_BYTES`), 401 as for load. Any answer to the whole call other than 200,
and a timeout, is no verdict on any entry: the room sends each again as it was, which the
idempotency above already covers.

The room sends one such call per tick that has saves: the period's changed seats (every 30 s
in the shipped games), a purchase, the last saves of seats let go, a drain's saves and the
retries heard that tick. The single-save route stays for rooms built before it.

The room must not load an account while its own save for that account is in flight (a seat freed
after grace with its last save pending, then the same account back): the pending save would move the
rev under the new seat, whose first save would then get 412.

## Timeouts and limits

- The metaserver gives every store call 2 s; the room should wait at least 3 s per call.
- Status: at most 4096 bytes. Load answers are up to 8 bytes + the stored document.
- Room calls are not rate-limited; they are authenticated before any body is read.
- Accepted: a load the room gave up at its deadline may still reach the database and make this
  room life the owner. The room holds no seat for it; the player's next entry loads again, and a
  seat of another room loses its profile at its next save (409), as for any later load.
- The room waits 3 s per load or save, gives a failed or timed-out save again byte for byte under
  the same `save_id`, and sends a seat away to enter again (4100) on a load that fails, times out or
  answers 404; a stored document it cannot read closes with 4103, one a newer build wrote with 4102.

## Not here yet

- The client's solo cloud sync and its credential: this store serves rooms only.
- The heartbeat probe walks a version 2 room only as far as its socket layer: `/api/enter`, the
  `wss://` port and certificate, and a HELLO with its ticket and protocol version 0, refused with
  4002 (`features/metaserver/probe.go`). It walks the old rooms whole while only they report.
