#ifndef ONLINE_ROOM_ENTRY_H
#define ONLINE_ROOM_ENTRY_H

// The client's way into a room: the connection ladder. Pure policy over a
// link the host owns; it says what to do next and the host does it, so the
// ladder runs the same under a test's clock and random source as under the
// frame's.
//
// With no room to reach the client plays solo. With one it connects; a try
// that fails, hangs past the timeout, or is accepted and then dropped before
// the connection has stood for the knobs' stable span, counts as failed: it
// waits (each wait twice the last, capped, then jittered) and tries again, up
// to the knobs' attempts in a row; then the player is asked to retry or, when
// solo is built in, play solo, and nothing is chosen for them. A connection
// that stood the stable span and drops starts the ladder over. A room that
// closes with ONLINE_ROOM_CLOSE_REENTER (full, or draining to shut down) is
// entered again at once, but only once per re-enter window: a second 4100
// inside it is a failed try. A refusal for the protocol version, or a profile
// a newer build wrote, means this build is out of date: the player is asked
// to reload. A room that is full, says the account plays on elsewhere, or
// that its profile does not read, is not tried again by itself: the player
// picks.

#include <stdbool.h>
#include <stdint.h>

#include "online_room/close.h"

// network-core's version and seat refusals; the room's own codes are in online_room/close.h.
#define ONLINE_ENTRY_CLOSE_VERSION 4002U
#define ONLINE_ENTRY_CLOSE_FULL 4006U

// The ladder's tuning, which a game keeps with its content.
typedef struct online_entry_knobs {
    uint32_t connect_attempts;
    float connect_timeout_seconds;
    float retry_seconds;
    float retry_max_seconds;
    float stable_online_seconds;
    float reenter_window_seconds;
} online_entry_knobs;

typedef enum online_entry_state {
    ONLINE_ENTRY_CONNECTING, // a try is in flight
    ONLINE_ENTRY_ONLINE,
    ONLINE_ENTRY_WAITING,    // until retry_at, then the next try
    ONLINE_ENTRY_ASKING,     // the tries are spent: the player picks retry, or solo when it is built in
    ONLINE_ENTRY_OUTDATED,   // the room speaks a newer protocol: the player reloads
    ONLINE_ENTRY_SOLO,
    ONLINE_ENTRY_NOWHERE,    // no room to reach and no solo built in
} online_entry_state;

typedef enum online_entry_action {
    ONLINE_ENTRY_KEEP,       // nothing to do this frame
    ONLINE_ENTRY_CONNECT,    // open a new connection to the room
    ONLINE_ENTRY_DISCONNECT, // give up the try in flight (it hung)
    ONLINE_ENTRY_GO_SOLO,    // leave the room behind and play in this process
} online_entry_action;

typedef enum online_entry_choice {
    ONLINE_ENTRY_CHOOSE_RETRY,
    ONLINE_ENTRY_CHOOSE_SOLO,
} online_entry_choice;

// A uniform draw in [0, 1) for the jitter of the waits.
typedef double (*online_entry_random)(void *user);

typedef struct online_entry {
    online_entry_knobs knobs;
    online_entry_random random;
    void *random_user;
    bool solo_allowed;    // this build has a solo room
    online_entry_state state;
    uint32_t attempt;     // the try in flight or last made, from 1
    double attempt_at;    // when it was made
    double online_at;     // when the connection opened
    double retry_at;      // when WAITING ends
    double reenter_at;    // when the last 4100 was entered again at once; negative for never
    uint16_t close_code;  // the last close; 0 for a try that never connected
    bool ever_online;
    uint32_t connects;    // tries made, over the whole session
} online_entry;

// The first action: CONNECT when there is a room to reach, GO_SOLO without one
// when solo is built in, KEEP (state NOWHERE) when neither.
online_entry_action online_entry_start(online_entry *entry, online_entry_knobs knobs, bool have_room, bool solo_allowed,
    online_entry_random random, void *random_user, double now);
online_entry_action online_entry_opened(online_entry *entry, double now);
// The connection closed or the try failed; `code` is the close code, 0 when none came.
online_entry_action online_entry_closed(online_entry *entry, uint16_t code, double now);
// Once a frame: a hung try, or a wait that is over.
online_entry_action online_entry_step(online_entry *entry, double now);
// The player's pick while ASKING, or while WAITING (a retry then tries at once
// without starting the ladder over); KEEP in any other state and for solo when
// it is not built in.
online_entry_action online_entry_choose(online_entry *entry, online_entry_choice choice, double now);
// Seconds until the next try while WAITING; negative otherwise.
double online_entry_retry_in(const online_entry *entry, double now);

#endif
