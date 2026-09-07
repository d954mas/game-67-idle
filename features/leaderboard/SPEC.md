# Leaderboard feature pack — design contract

Status: approved design, not yet implemented. This file is the contract the
implementation is measured against; once the pack exists it becomes its
reference doc next to `README.md` and `INSTALL.md`.

## 1. Why a pack

Every portal answers the word "leaderboard" differently, and one of them
(CrazyGames) does not answer at all in-game. A game must not branch on portal
names. It asks for capabilities, renders what the data carries, and hides what
the portal cannot serve.

Portals also discourage or forbid outgoing HTTP to a developer's own server, so
**the portal's own board is used wherever one exists**; the HTTP backend serves
the portals that have none.

The pack is reusable only if nothing in it knows what the game's metric or its
cosmetics are. The test of every decision below: a racing game installs the pack,
declares a lap-time board sorted ascending, and writes no portal code.

## 2. Portal reality

Verified 2026-09-07 against the vendor documentation. Cells marked
`[unverified]` are read from the vendor page but not yet reproduced against a
live SDK; the draft runs in section 13 close them, and the per-portal packets in
`features/platform-sdk/references/portals/*.md` gain a leaderboard section as
part of the work (today they explicitly do not map leaderboards).

| Portal | Write | Read in-game | Login | Day scope | Extra row data |
| --- | --- | --- | --- | --- | --- |
| Yandex | `lb.setScore(name, score, extraData)`, 1 req/s | `lb.getEntries` — top 1..20, `quantityAround` 1..10, 20 req / 5 min | **required to write and to see your own row**; reading the top is anonymous | none — boards are created in the console and never reset | `extraData` string round-trips; portal adds `publicName` and `getAvatarSrc(size)` |
| CrazyGames | `user.submitScore({ encryptedScore, score })`, AES-GCM | **no read API** — the portal renders the board in its drawer, on the game page and on profiles | not stated | none | none; one board per game, invited games only `[unverified]` |
| Playgama | `bridge.leaderboards.setScore` | only when `bridge.leaderboards.type == in_game`, **decided at run time by the host platform** | not stated | none | `id, name, photo, score, rank` |
| Poki | `sendHighscore` exists but is undocumented for v2 → out of contract | none | — | none | — |

Sources: `https://yandex.com/dev/games/doc/en/sdk/sdk-leaderboard`,
`https://yandex.com/dev/games/doc/en/sdk/sdk-player`,
`https://docs.crazygames.com/sdk/leaderboards-client/`,
`https://docs.crazygames.com/sdk/leaderboards/`,
`https://wiki.playgama.com/playgama/bridge-sdk/api/leaderboards`.

Consequence for the seeding game: Yandex serves the board through the portal,
Poki/itch/local keep the existing self-hosted board, CrazyGames takes score
submissions it may never display, Playgama decides at run time and on
`playgama.com` itself currently answers `not_available`.

Avatar hosts answer `Access-Control-Allow-Origin: *`
(`https://avatars.mds.yandex.net/get-yapic/...`, HEAD request), so avatars are
fetched with `nt_http`. A shell HEAD is not proof of a browser image GET; the
draft run confirms it.

## 3. Types and constants

Everything the backends and the game share is defined here, before any code, so
two agents cannot invent two incompatible versions.

```c
#define LEADERBOARD_MAX_BOARDS   4
#define LEADERBOARD_TOP_MAX     20  /* the portal ceiling; the screen draws fewer */
#define LEADERBOARD_AROUND_MAX  10
#define LEADERBOARD_ID_MAX      32
#define LEADERBOARD_EXTRA_MAX  100  /* the game's opaque payload, Yandex extraData budget */
#define LEADERBOARD_NAME_MAX    64
#define LEADERBOARD_URL_MAX    192

typedef enum {
    LEADERBOARD_SCOPE_ALL_TIME = 0,
    LEADERBOARD_SCOPE_UTC_DAY  = 1,
    LEADERBOARD_SCOPE_COUNT    = 2,
} leaderboard_scope_t;

typedef enum {
    LEADERBOARD_RESULT_OK = 0,
    LEADERBOARD_RESULT_UNSUPPORTED = 1, /* terminal: this portal has no such board */
    LEADERBOARD_RESULT_NEEDS_LOGIN = 2, /* recoverable: the player can fix it */
    LEADERBOARD_RESULT_RATE_LIMITED = 3,/* recoverable: wait */
    LEADERBOARD_RESULT_FAILED = 4,      /* recoverable: transport or parse */
} leaderboard_result_t;

typedef struct { uint8_t index; } leaderboard_board_t; /* 0xFF = invalid */

typedef struct {
    uint32_t value;
    int      place;                        /* 0 when the source gives no rank */
    bool     you;
    char     extra[LEADERBOARD_EXTRA_MAX]; /* opaque to the pack; the game's own k=v payload */
    char     name[LEADERBOARD_NAME_MAX];   /* "" on an anonymous board */
    char     avatar[LEADERBOARD_URL_MAX];  /* "" when the portal gives none */
} leaderboard_row_t;

/* Borrowed for the duration of the completion call; the core copies what it
   keeps before returning. A backend may free its buffers immediately after. */
typedef struct {
    const leaderboard_row_t *top;
    int top_count;
    const leaderboard_row_t *around; /* neighbours of the player; NULL when the source has none */
    int around_count;
    bool has_player;
    int player_place;
    uint32_t player_value;
    const char *day; /* "YYYYMMDD" for a day scope, NULL otherwise */
} leaderboard_page_t;
```

The row carries **no game concept**. A cosmetic id and a progression level are
the game's business: it packs them into `extra` with the pack's `k=v;` codec and
unpacks them in its own screen. The codec is the reusable part, not the field
names.

## 4. Capabilities and refusal

```c
typedef struct {
    bool can_read;     /* entries can be listed inside the game */
    bool can_write;    /* scores can be submitted */
    bool needs_login;  /* writing, and the player's own row, require portal auth */
    bool native_popup; /* the portal owns the UI; we can only open it */
    uint32_t scopes;   /* bitmask of (1u << leaderboard_scope_t) */
} leaderboard_caps_t;
```

Capabilities are **per board**, because the manifest declares scopes per board,
and they are answered live by the backend — Playgama resolves its own type at
run time and a build-time answer would be wrong.

Refusal has four kinds and only one of them is terminal:

- `UNSUPPORTED` latches for the session, and **per capability**: a write refusal
  withdraws writing only. Reading a board the portal serves anonymously must
  survive a portal that refuses submissions.
- `NEEDS_LOGIN` never latches. It sets `view.needs_login`, which is what raises
  the login button — the one thing that can actually fix it.
- `RATE_LIMITED` never latches; the backend re-schedules.
- `FAILED` never latches; the existing retry and back-off cadence applies, and
  after the fast retries are spent `view.error` is set so the screen can offer a
  manual retry.

Collapsing these was the single most expensive mistake available here: an
anonymous Yandex player submitting their first score would have destroyed the
board they were allowed to read.

## 5. Game-facing API

```c
typedef struct {
    bool (*load)(const char *key, char *out, size_t out_size, void *userdata);
    void (*store)(const char *key, const char *value, void *userdata);
    void *userdata;
} leaderboard_host_t; /* the game's save; the pack owns no persistence of its own */

/* The manifest's board table, generated from leaderboards.json. Coalescing
   needs each board's sort order, and the backends need its portal id. */
typedef struct {
    const char *id;
    leaderboard_sort_t sort;
    uint32_t scopes;
    const char *portal_id;
} leaderboard_board_def_t;

typedef struct {
    leaderboard_host_t host;
    const leaderboard_board_def_t *boards;
    int board_count;
    const leaderboard_backend_t *backend;
    void *backend_userdata;
} leaderboard_config_t;

void leaderboard_init(const leaderboard_config_t *config);
void leaderboard_update(void);                    /* pump; once per frame */
void leaderboard_shutdown(void);

leaderboard_board_t leaderboard_board(const char *id); /* from the manifest */
int  leaderboard_board_count(void);

leaderboard_caps_t leaderboard_caps(leaderboard_board_t board);
bool leaderboard_available(leaderboard_board_t board);

void leaderboard_submit(leaderboard_board_t board, leaderboard_scope_t scope,
                        uint32_t value, const char *extra);
void leaderboard_view_get(leaderboard_board_t board, leaderboard_scope_t scope,
                          leaderboard_view_t *out);
void leaderboard_refresh_now(leaderboard_board_t board);
bool leaderboard_open_native(leaderboard_board_t board);

const char *leaderboard_day_label(void);
int64_t leaderboard_seconds_to_day_reset(void);
```

`leaderboard_submit` is per (board, scope): an all-time counter and today's
counter are different numbers and must not be coalesced into one. Coalescing
keeps the **better** value as the manifest's `sort` defines better — a lap-time
board sorted ascending keeps the smaller number. It never blocks and never
fails at the call site; a portal that cannot take the score withdraws or waits
per section 4.

The pack owns the anonymous id and the day counters but stores them through
`leaderboard_host_t`. Keys are `lb.id`, `lb.day`, `lb.day.<board>` and
`lb.sent.<board>.<scope>`; the game maps them into its own save. The last of
these is what stops a relaunch from re-submitting a score the portal already has.

```c
typedef struct {
    int  place;         /* -1 until known */
    uint32_t value;
    int  top_count;
    leaderboard_row_t top[LEADERBOARD_TOP_MAX];
    int  around_count;  /* 0 on sources that cannot rank neighbours */
    leaderboard_row_t around[LEADERBOARD_AROUND_MAX];
    bool loaded;
    bool error;         /* retries are spent; offer a manual retry */
    bool needs_login;   /* the top is real, the player is simply not on it yet */
} leaderboard_view_t;
```

## 6. UI state — one pure function

The screen stays in the game; the decision of what a screen may show does not.

```c
typedef struct {
    bool show_launcher; /* can_read || native_popup */
    int  tab_count;     /* popcount(caps.scopes) */
    bool show_login;
    bool show_retry;
    bool loading;
} leaderboard_ui_state_t;

leaderboard_ui_state_t leaderboard_ui_state(leaderboard_board_t board);
```

This is the seam the capability tests assert on, and the only thing a new game's
screen has to obey. Nothing in a game asks which portal is running.

## 7. Backend contract

One vtable, chosen once at init from the board manifest and the build target.

```c
typedef struct {
    leaderboard_caps_t (*caps)(leaderboard_board_t board, void *ud);
    bool (*init)(void *ud);
    bool (*submit)(leaderboard_board_t board, leaderboard_scope_t scope,
                   uint32_t value, const char *extra, void *ud);
    bool (*fetch)(leaderboard_board_t board, leaderboard_scope_t scope, void *ud);
    bool (*open_native)(leaderboard_board_t board, void *ud);
    void (*update)(void *ud);
    void (*destroy)(void *ud);
} leaderboard_backend_t;

void leaderboard_backend_complete_fetch(leaderboard_board_t board, leaderboard_scope_t scope,
                                        const leaderboard_page_t *page,
                                        leaderboard_result_t result);
void leaderboard_backend_complete_submit(leaderboard_board_t board, leaderboard_scope_t scope,
                                         leaderboard_result_t result);

/* What a backend is allowed to ask the facade for. */
const char *leaderboard_player_id(void);
const leaderboard_board_def_t *leaderboard_board_def(leaderboard_board_t board);
void leaderboard_backend_auth_changed(void); /* re-send what the portal never took */
```

A backend completes every request it starts; returning false from `submit` or
`fetch` means it never started and counts as `FAILED`. The facade owns no clock,
so `RATE_LIMITED` simply leaves the value pending for the next trigger — cadence
is the backend's business. Consecutive failures raise `view.error` after a small
budget, which a player-driven refresh clears.

Shipped backends:

- **portal** — calls the platform-sdk C entry points from section 9. It contains
  no JavaScript: portal JS belongs to the platform-sdk pack, which owns the
  adapter contract, the EM_JS bridge and the pinned release bundles.
- **http** — the anonymous self-hosted client: all-time plus UTC-day,
  locally re-estimated place. Its endpoint, obfuscation key and cadence arrive in
  its own config struct from the game.
- **mock** — deterministic in-process data for tests and local development.

`leaderboard_core` stays free of I/O: UTC-day math, the payload codec, response
parsing, the `extra` codec, place re-estimation with bucket interpolation, and
the coalescing rule. It is lifted from the seeding game with its tests.

## 8. Board manifest

Game-owned `leaderboards.json`, one source of truth for what boards exist.

```json
{
  "schema": "ai_studio.leaderboards.v1",
  "boards": [
    {
      "id": "planets",
      "metric": "planets_devoured",
      "sort": "desc",
      "scopes": ["all_time", "utc_day"],
      "portal_ids": {
        "yandex": "planets",
        "crazygames": "main",
        "playgama": { "id": "planets", "isMain": true }
      },
      "backends": {
        "yandex": "portal", "crazygames": "portal", "playgama": "portal",
        "poki": "http", "itch": "http", "local": "http"
      }
    }
  ]
}
```

`backends` names a **family**, not a capability: `portal` means "ask the portal
at run time", and a Playgama build whose host platform answers `not_available`
simply reports no capabilities. A scope the chosen family cannot serve is
dropped from `caps.scopes`, never faked.

The manifest generates the C board constants, feeds the `leaderboards` block of
`playgama-bridge-config.json`, and prints the checklist of boards a human must
create by hand — the Yandex technical name, the CrazyGames board — because
neither portal has an API for that.

## 9. platform-sdk additions

Both auth and the portal calls are platform-sdk's work, not the leaderboard's.

Player identity:

```c
bool platform_sdk_auth_supported(void);
bool platform_sdk_authorized(void);
platform_sdk_result_t platform_sdk_login(void); /* player gesture only */
const char *platform_sdk_player_name(void);     /* "" when unknown */
const char *platform_sdk_player_avatar_url(void);
```

Leaderboard entry points, mirroring the existing async ad calls — a C call, an
EM_JS bridge function, an adapter method per portal, a completion callback:

```c
platform_sdk_result_t platform_sdk_leaderboard_submit(const char *board_id, int32_t scope,
                                                      uint32_t value, const char *extra);
platform_sdk_result_t platform_sdk_leaderboard_fetch(const char *board_id, int32_t scope);
platform_sdk_result_t platform_sdk_leaderboard_open(const char *board_id);
platform_sdk_leaderboard_caps_t platform_sdk_leaderboard_caps(const char *board_id);
```

An auth-changed notification joins the existing event bridge, so screens
subscribe instead of polling. A declined login dialog is a normal outcome, not
an error. Yandex has **no logout method**; no seam may imply one.

Release bundles under `web/release/` are regenerated in the same change, or the
pinned hashes stop describing what ships.

## 10. Avatar cache — `features/remote-image`

Two consumers (board rows, settings), so it is its own small pack, and it stays
small: the working set is one screen of avatars.

```c
typedef enum { REMOTE_IMAGE_PENDING, REMOTE_IMAGE_READY, REMOTE_IMAGE_FAILED } remote_image_state_t;

void                 remote_image_init(const remote_image_config_t *cfg); /* capacity injected */
remote_image_state_t remote_image_state(const char *url);
nt_texture_t         remote_image_get(const char *url); /* 0 unless READY */
void                 remote_image_update(void);
void                 remote_image_shutdown(void);
```

- Fetch with `nt_http`, decode with the vendored `deps/stb/stb_image.h`, upload
  through `nt_gfx_make_texture` and `nt_gfx_update_texture`.
- A fixed table keyed by URL; when it is full the least recently used entry that
  was not touched this frame is dropped. Capacity is a config field so tests run
  on a table of two or three.
- Failures cache negatively with a retry delay: a broken URL must not be
  re-fetched every frame.
- At most two concurrent fetches — `NT_HTTP_MAX_REQUESTS` is 8 and the board
  poll must not be starved.
- Decoded images clamp to a max dimension; the input is an avatar, not an asset.

The three states are separate on purpose: a spinner and a fallback are different
pictures.

## 11. Test seams

Core tier, pure, no network and no GPU:

- day math, day-boundary seconds, payload codec, response parsing, place
  re-estimation (lifted with the existing tests);
- `leaderboard_ui_state` for every capability shape, including a portal that
  refuses writes but serves reads;
- refusal semantics: `UNSUPPORTED` latches per capability, `NEEDS_LOGIN` raises
  the login flag without latching, `RATE_LIMITED` and `FAILED` recover;
- coalescing under both sort orders, and that a value already sent is not sent
  again after a relaunch;
- the `extra` codec: round trip, truncation at the budget, garbage input;
- remote-image on an injected capacity of two: eviction order, a frame-hot entry
  survives, the negative cache honours its delay, the concurrency cap holds;
- manifest validation: schema, every publish target has a family, portal ids
  exist where the family is `portal`, an unservable scope is rejected at
  authoring time.

Not tested: exact board contents, row counts, layout offsets, table capacity
defaults, any balance number. Portal behaviour and visual acceptance are proven
by a run and a screenshot.

## 12. What a second game does

Declare `leaderboards.json`, implement two host callbacks against its own save,
call `leaderboard_submit` where its metric changes, and draw rows from
`leaderboard_view_t` while obeying `leaderboard_ui_state`. No portal code, no
new adapter, no fork of the row type.

## 13. Open risks

1. **Yandex save identity.** The docs do not say what happens to `player.setData`
   written by an anonymous player after that player logs in. If the id changes, a
   player who presses "log in" sees a reset game. Policy shipped regardless:
   keep the in-memory state, read the authorized account's state, keep the more
   advanced one, write it back — progress never decreases. The draft run
   confirms which case is real.
2. **Yandex and CrazyGames boards must be created by hand** in their consoles
   before any call works; `getEntries` answers 404 until then.
3. **CrazyGames leaderboards may be invite-only.** Submission code ships either
   way; it costs nothing while the portal has no board for us.
4. **`extraData` size limit is undocumented.** Budget 100 bytes and drop the
   extra rather than failing a submit.
