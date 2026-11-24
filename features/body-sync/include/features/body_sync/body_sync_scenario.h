#ifndef FEATURES_BODY_SYNC_BODY_SYNC_SCENARIO_H
#define FEATURES_BODY_SYNC_BODY_SYNC_SCENARIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "features/body_sync/body_sync_scene.h"
#include "features/body_sync/body_sync_types.h"
#include "features/net_link/net_link_sim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A scenario: which scene, how many guests, what each player does and when, what the network
   does to each guest, and what must hold at the end. It is text, parsed here without the
   scene; names of bodies are checked against the scene when the stand is created.

   The text is untrusted: anything out of the grammar or out of range is refused with the line
   and a reason, never asserted. One statement per line; `#` starts a comment; words are
   separated by spaces or tabs; ASCII only outside comments.

   Header (each at most once; scenario and scene required):
     scenario <name>            [a-z0-9_], 1..31 characters
     scene <name>               a catalog scene or one the caller adds
     seed <0..2^64-1>           default 1: the network's and the host's random draws
     guests <0..3>              default 1; player p belongs to endpoint p, p0 to the host
     settle <time>              default 5s: how long after the last item has ended (every
                                player idle, every cut, stall and deaf over) the run may wait
                                for every endpoint's world to come to rest before it ends
     end <time>                 a hard end instead; the run stops there, rest or not
     light_mass <x>kg           the second layer on every seam, light bodies up to x kg
                                (0.1..1000); off without it
     authority <host|interaction>  who owns free bodies on every seam (body_sync_config_t
                                authority), host by default; interaction needs light_mass
     rate <host|gN> <x>hz       that endpoint steps x times a second of virtual time, 20..240
                                (59.5hz); its seam keeps BODY_SYNC_TICK_HZ; once per endpoint

   Player actions, `[at <time>] <player> <verb> <args> [as <label>]`; `speed` is a bare
   number in m/s, (0, 50]:
     walk <x> <z> [speed <v>]     walk to a floor point; done on arrival within 0.2 m, or
     walk <body> [speed <v>]      at the body within reach; failed when no progress for 2 s
     grab <body>                  the nearest point of the body within reach; optimistic: done
                                  at once, the label's result becomes granted or refused when
                                  the host answers; failed out of reach or hands full
     throw <x> <y> <z> [speed <v>] | throw <body> [speed <v>]
                                  a ballistic throw of what is held onto the point; failed
                                  when nothing is held
     drop                         let go where it is
     punch [<body>]               turn to the body if named, wind up, a cone query in front;
                                  hit when a mannequin, breakable or item is in the cone,
                                  missed otherwise; on a guest a hit is a claim the host may
                                  reject
     wait <time>                  stand still
     recall <body> [to <x> <y> <z>]  p0 only, the host's rule: body_sync_assign of the body to the
                                  host whoever owns it (a carrier lets go, a flight stops), then
                                  it is put at rest at the point, or at its spawn pose; done at
                                  once, failed when the body is gone
     remove <body>                p0 only: the host unbinds and destroys the body whoever owns
                                  it, and tells every guest; done at once, failed when gone
   An action with `at` starts at that time and cuts off what the player was doing; one without
   it starts when the player's previous action in the file ends, at 0 for the first. An action
   may instead start on an event, `<player> <verb> <args> on <event> [+<time>] [as <label>]`,
   that long after it (0 without), cutting off what runs as a timed one does; the word `on`
   ends the arguments:
     on <pN> <agent> contact <body>  the first tick pN's own world records a hit between the
                                  scene body `agent`, while pN simulates it, and the body (pN's
                                  thrown rock meeting glass); her character is never the agent
     on <label>                   the tick the earlier action with that label starts
   Its event can fire until the run ends; one that never fired leaves the action pending. An
   action after it with neither waits for it to end. A schedule
   that cannot fit in 600 s (a timed start plus the waits after it, a condition's end) is
   refused, not cut.

   Network and endpoint conditions, `at <time>` optional for net (default 0):
     net <guest> [to_host|to_guest] <field> <value> ...
         rtt <time> (both directions get half; not with a direction), delay <time> (one way),
         jitter <time>, reorder_delay <time> (each 0..10 s), loss <share>, dup <share>,
         reorder <share>, rate <bytes/s> (1 B/s or more) or rate off. Fields not named keep
         their values; a path starts clean.
     at <time> cut <guest> for <time>      nothing passes either way; 10 s or more disconnects
     at <time> stall <endpoint> for <time> the endpoint's frame stops, then it runs the missed
                                           ticks back to back, at most 8 a frame
     at <time> deaf <endpoint> for <time>  it keeps stepping and sending but reads nothing

   Checks at the end:
     expect agree_at_rest <length> [<angle>]   every guest's world matches the host's; an angle
                                                written is always a bound, 0deg included
     expect no_stuck                            see body_sync_stand.h
     expect effects_once                        every accepted claim took effect exactly once
                                                on every endpoint
     expect left <guest>                        that guest's session ended during the run; it
                                                is then left out of the three checks above,
                                                which a guest that left fails otherwise
     expect result <label> <result>             done granted refused hit missed rejected failed
     expect one_of <result> <label> <label> ... exactly one of 2..4 distinct labels ends so
     expect moved <body> <=|>= <length> [layer <length>]
                                                the scene body's travel on the host from the
                                                first tick to the last
     expect effect <body>                       an effect (a knockdown, a break) took the scene
                                                body on the host, and every guest that stayed
                                                shows it once: knocked down, or gone
     expect pace <label>                        a walk to a point keeps up with the same walk in
                                                a reference run of the scenario on one world,
                                                every player local, no network (README
                                                "Scenario format" derives its margin)
     expect <metric> <= <value>                 metric names of body_sync_metrics.h, the
                                                worst guest counts
   With `guests 0` the checks that compare a guest (agree_at_rest, left, and every metric but
   host_ticks.max and echo_ticks.max) are refused.

   Words: <player> p0..p3, <guest> g1..g3, <endpoint> host or g1..g3, <body> a scene body's
   name, <label> [a-z0-9_] 1..15 characters and unique. Numbers are decimal with an optional
   sign and fraction, no exponent. Units are required where the grammar names one:
   <time> 250ms, 1.5s or 90t (ticks of 1/60 s; times round to the nearest tick, 0..600 s, and a
   positive time that rounds to zero ticks is refused); <length> 5cm or 0.05m; <angle> 3deg;
   <share> 2% (0..100); <bytes/s> 64000B/s or 64KB/s; delay counts of a metric in ticks: 6t.
   Positions are bare metres. */

enum {
    BODY_SYNC_SCENARIO_TEXT_MAX = 65536,
    BODY_SYNC_SCENARIO_LINE_MAX = 256, /* characters, the comment included */
    BODY_SYNC_SCENARIO_ACTIONS_MAX = 512,
    BODY_SYNC_SCENARIO_NET_MAX = 128,
    BODY_SYNC_SCENARIO_EXPECTS_MAX = 32,
    BODY_SYNC_LABEL_MAX = 16,
    BODY_SYNC_METRIC_NAME_MAX = 32,
    BODY_SYNC_EXPECT_LABELS_MAX = 4,
    BODY_SYNC_GUESTS_MAX = 3,
    BODY_SYNC_ENDPOINT_HOST = 0, /* endpoints are 0 (host) and 1..3 (guest g1..g3) */
    BODY_SYNC_TICK_HZ = 60,
    BODY_SYNC_TIME_MAX_S = 600,
};

typedef enum body_sync_verb {
    BODY_SYNC_VERB_WALK = 1,
    BODY_SYNC_VERB_GRAB,
    BODY_SYNC_VERB_THROW,
    BODY_SYNC_VERB_DROP,
    BODY_SYNC_VERB_PUNCH,
    BODY_SYNC_VERB_WAIT,
    BODY_SYNC_VERB_RECALL,
    BODY_SYNC_VERB_REMOVE,
} body_sync_verb_t;

typedef enum body_sync_outcome {
    BODY_SYNC_OUTCOME_PENDING = 0, /* never started or not answered by the end */
    BODY_SYNC_OUTCOME_DONE,
    BODY_SYNC_OUTCOME_GRANTED,
    BODY_SYNC_OUTCOME_REFUSED,
    BODY_SYNC_OUTCOME_HIT,
    BODY_SYNC_OUTCOME_MISSED,
    BODY_SYNC_OUTCOME_REJECTED, /* a guest's hit the host did not accept */
    BODY_SYNC_OUTCOME_FAILED,
} body_sync_outcome_t;
/* The scenario word: "done", "granted", ...; "pending" for PENDING. */
const char *body_sync_outcome_name(body_sync_outcome_t outcome);

typedef enum body_sync_trigger {
    BODY_SYNC_TRIGGER_NONE,
    BODY_SYNC_TRIGGER_CONTACT, /* on <pN> <agent> contact <body> */
    BODY_SYNC_TRIGGER_ACTION,  /* on <label> */
} body_sync_trigger_t;

typedef struct body_sync_target {
    bool is_body;
    char body[BODY_SYNC_NAME_MAX]; /* when is_body */
    float point[3];                /* otherwise; walk leaves y at 0 */
} body_sync_target_t;

typedef struct body_sync_action {
    uint32_t line;
    char label[BODY_SYNC_LABEL_MAX]; /* "" when none */
    uint8_t player;
    bool timed;
    uint32_t tick; /* when timed */
    body_sync_verb_t verb;
    bool has_target;
    body_sync_target_t target;
    float speed;        /* m/s; zero takes the tuning's */
    uint32_t wait_ticks; /* WAIT */
    bool has_home;       /* RECALL `to`: where the host puts the body, else its spawn pose */
    float home[3];
    /* `on`: the action starts trigger_ticks after an event instead of at a time */
    body_sync_trigger_t trigger;
    uint8_t trigger_player;                 /* CONTACT: whose own world sees it */
    char trigger_agent[BODY_SYNC_NAME_MAX]; /* CONTACT: the body pN simulates that makes it */
    char trigger_body[BODY_SYNC_NAME_MAX];  /* CONTACT: the body touched */
    uint32_t trigger_action;               /* ACTION: the earlier action, by index */
    uint32_t trigger_ticks;
} body_sync_action_t;

typedef enum body_sync_net_kind {
    BODY_SYNC_NET_CONDITIONS = 1,
    BODY_SYNC_NET_CUT,
    BODY_SYNC_NET_STALL,
    BODY_SYNC_NET_DEAF,
} body_sync_net_kind_t;

/* Bits of body_sync_net_event.fields: which fields of `conditions` the line sets. */
enum {
    BODY_SYNC_FIELD_DELAY = 1u << 0,
    BODY_SYNC_FIELD_JITTER = 1u << 1,
    BODY_SYNC_FIELD_LOSS = 1u << 2,
    BODY_SYNC_FIELD_DUPLICATE = 1u << 3,
    BODY_SYNC_FIELD_REORDER = 1u << 4,
    BODY_SYNC_FIELD_REORDER_DELAY = 1u << 5,
    BODY_SYNC_FIELD_RATE = 1u << 6,
};
/* Bits of body_sync_net_event.directions. */
enum { BODY_SYNC_TO_HOST = 1u << 0, BODY_SYNC_TO_GUEST = 1u << 1 };

typedef struct body_sync_net_event {
    uint32_t line;
    uint32_t tick;
    body_sync_net_kind_t kind;
    uint8_t endpoint;   /* 1..3 for net and cut; 0..3 for stall and deaf */
    uint8_t directions; /* CONDITIONS */
    uint32_t fields;    /* CONDITIONS */
    net_link_conditions_t conditions; /* CONDITIONS: shares 0..1, times in ms */
    uint32_t duration_ticks; /* CUT, STALL, DEAF */
} body_sync_net_event_t;

typedef enum body_sync_expect_kind {
    BODY_SYNC_EXPECT_AGREE_AT_REST = 1,
    BODY_SYNC_EXPECT_NO_STUCK,
    BODY_SYNC_EXPECT_EFFECTS_ONCE,
    BODY_SYNC_EXPECT_RESULT,
    BODY_SYNC_EXPECT_ONE_OF,
    BODY_SYNC_EXPECT_METRIC_AT_MOST,
    BODY_SYNC_EXPECT_LEFT,
    BODY_SYNC_EXPECT_MOVED, /* a body's travel on the host from the first tick to the last */
    BODY_SYNC_EXPECT_PACE,  /* a walk's progress against the same walk in a local reference run */
    BODY_SYNC_EXPECT_EFFECT, /* a scene body an effect took, shown on every endpoint */
} body_sync_expect_kind_t;

typedef struct body_sync_expect {
    uint32_t line;
    body_sync_expect_kind_t kind;
    float tolerance_m;   /* AGREE_AT_REST */
    float tolerance_deg; /* AGREE_AT_REST; negative when no angle was written: rotation not checked */
    uint8_t endpoint;    /* LEFT: the guest 1..3 */
    body_sync_outcome_t outcome; /* RESULT, ONE_OF */
    uint32_t label_count;        /* RESULT: 1; ONE_OF: 2..BODY_SYNC_EXPECT_LABELS_MAX */
    char labels[BODY_SYNC_EXPECT_LABELS_MAX][BODY_SYNC_LABEL_MAX];
    char metric[BODY_SYNC_METRIC_NAME_MAX]; /* METRIC_AT_MOST */
    double limit; /* METRIC_AT_MOST, in the metric's unit: m, deg, ticks or bytes/s; MOVED in m */
    double layer_limit; /* MOVED: the bound while the second layer is on; negative: `limit` */
    char body[BODY_SYNC_NAME_MAX]; /* MOVED, EFFECT: the scene body */
    bool at_least;                 /* MOVED: >= rather than <= */
} body_sync_expect_t;

typedef struct body_sync_scenario {
    char name[BODY_SYNC_NAME_MAX];
    char scene[BODY_SYNC_NAME_MAX];
    uint64_t seed;
    uint8_t guests;
    uint32_t settle_ticks;
    /* Steps per second of virtual time each endpoint runs, in millihertz; 0 is BODY_SYNC_TICK_HZ.
       Its seam keeps tick_hz: the case of a fixed-step game that cannot keep up, or runs ahead. */
    uint32_t rate_mhz[1 + BODY_SYNC_GUESTS_MAX];
    /* The second layer's light_mass_max for every seam, kilograms; 0 off. */
    float light_mass_max;
    /* body_sync_authority_mode_t for every seam; `authority_set` once the word was read. */
    uint8_t authority;
    bool authority_set;
    bool has_end;
    uint32_t end_tick;
    uint32_t action_count;
    body_sync_action_t actions[BODY_SYNC_SCENARIO_ACTIONS_MAX]; /* in file order */
    uint32_t net_count;
    body_sync_net_event_t net[BODY_SYNC_SCENARIO_NET_MAX]; /* sorted by tick, file order within */
    uint32_t expect_count;
    body_sync_expect_t expects[BODY_SYNC_SCENARIO_EXPECTS_MAX];
} body_sync_scenario_t;

/* Parses `size` bytes of `text` (no terminator needed). False fills `error` with the first
   problem and its line; `out` is then unspecified. Checked here: the grammar, ranges, limits,
   players and guests below 1 + guests, labels unique and every expected label defined, metric
   names known. Not checked here: body names against the scene, and whether the actions make
   sense in it (body_sync_stand_create does that). */
bool body_sync_scenario_parse(const char *text, size_t size, body_sync_scenario_t *out, body_sync_error_t *error);

/* Reads the file and parses it; a missing file, a read error or more than
   BODY_SYNC_SCENARIO_TEXT_MAX bytes is a refusal with line 0. */
bool body_sync_scenario_load(const char *path, body_sync_scenario_t *out, body_sync_error_t *error);

#ifdef __cplusplus
}
#endif

#endif
