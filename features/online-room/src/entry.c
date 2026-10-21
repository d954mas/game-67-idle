#include "online_room/entry.h"

#include <string.h>

// Each wait is its backoff times a factor drawn in [1 - J, 1 + J], so clients
// sent away together do not all come back in the same instant.
#define JITTER 0.25

static online_entry_action connect_now(online_entry *entry, double now) {
    entry->state = ONLINE_ENTRY_CONNECTING;
    entry->attempt += 1U;
    entry->attempt_at = now;
    entry->connects += 1U;
    return ONLINE_ENTRY_CONNECT;
}

// The wait before try n + 1: retry_seconds, doubled per try, capped, jittered.
static double backoff(const online_entry *entry, uint32_t attempt) {
    const double cap = (double)entry->knobs.retry_max_seconds;
    double wait = (double)entry->knobs.retry_seconds;
    for (uint32_t index = 1U; index < attempt && wait < cap; ++index) { wait *= 2.0; }
    if (wait > cap) { wait = cap; }
    const double draw = entry->random != NULL ? entry->random(entry->random_user) : 0.5;
    return wait * (1.0 - JITTER + 2.0 * JITTER * draw);
}

// A failed try: the next one after a wait, or the player's pick once they are spent.
static online_entry_action failed(online_entry *entry, double now) {
    if (entry->attempt >= entry->knobs.connect_attempts) {
        entry->state = ONLINE_ENTRY_ASKING;
        return ONLINE_ENTRY_KEEP;
    }
    entry->state = ONLINE_ENTRY_WAITING;
    entry->retry_at = now + backoff(entry, entry->attempt);
    return ONLINE_ENTRY_KEEP;
}

online_entry_action online_entry_start(online_entry *entry, online_entry_knobs knobs, bool have_room, bool solo_allowed,
    online_entry_random random, void *random_user, double now) {
    memset(entry, 0, sizeof *entry);
    entry->knobs = knobs;
    entry->random = random;
    entry->random_user = random_user;
    entry->solo_allowed = solo_allowed;
    entry->reenter_at = -1.0;
    if (have_room) { return connect_now(entry, now); }
    if (!solo_allowed) {
        entry->state = ONLINE_ENTRY_NOWHERE;
        return ONLINE_ENTRY_KEEP;
    }
    entry->state = ONLINE_ENTRY_SOLO;
    return ONLINE_ENTRY_GO_SOLO;
}

online_entry_action online_entry_opened(online_entry *entry, double now) {
    if (entry->state != ONLINE_ENTRY_CONNECTING) { return ONLINE_ENTRY_KEEP; }
    entry->state = ONLINE_ENTRY_ONLINE;
    entry->online_at = now;
    entry->ever_online = true;
    return ONLINE_ENTRY_KEEP;
}

online_entry_action online_entry_closed(online_entry *entry, uint16_t code, double now) {
    if (entry->state != ONLINE_ENTRY_CONNECTING && entry->state != ONLINE_ENTRY_ONLINE) { return ONLINE_ENTRY_KEEP; }
    const bool stood = entry->state == ONLINE_ENTRY_ONLINE && now - entry->online_at >= (double)entry->knobs.stable_online_seconds;
    entry->close_code = code;
    if (code == ONLINE_ENTRY_CLOSE_VERSION || code == ONLINE_ROOM_CLOSE_PROFILE_NEWER) {
        entry->state = ONLINE_ENTRY_OUTDATED;
        return ONLINE_ENTRY_KEEP;
    }
    // Coming again by itself would only take the account back from where it
    // plays, meet the same unreadable profile, or knock on the same full room.
    if (code == ONLINE_ROOM_CLOSE_SUPERSEDED || code == ONLINE_ROOM_CLOSE_PROFILE_BROKEN || code == ONLINE_ENTRY_CLOSE_FULL) {
        entry->state = ONLINE_ENTRY_ASKING;
        return ONLINE_ENTRY_KEEP;
    }
    // Asked to come again: at once, and once per window, so a room that keeps
    // sending the client away turns into ordinary failed tries.
    if (code == ONLINE_ROOM_CLOSE_REENTER &&
        (entry->reenter_at < 0.0 || now - entry->reenter_at >= (double)entry->knobs.reenter_window_seconds)) {
        entry->reenter_at = now;
        entry->attempt = 0U;
        return connect_now(entry, now);
    }
    // A connection that stood starts the ladder over; an accept-then-drop is a failed try.
    if (stood) {
        entry->attempt = 0U;
        return connect_now(entry, now);
    }
    return failed(entry, now);
}

online_entry_action online_entry_step(online_entry *entry, double now) {
    if (entry->state == ONLINE_ENTRY_WAITING && now >= entry->retry_at) { return connect_now(entry, now); }
    if (entry->state == ONLINE_ENTRY_CONNECTING && now - entry->attempt_at >= (double)entry->knobs.connect_timeout_seconds) {
        // The host drops the try and reports it closed with no code.
        return ONLINE_ENTRY_DISCONNECT;
    }
    return ONLINE_ENTRY_KEEP;
}

online_entry_action online_entry_choose(online_entry *entry, online_entry_choice choice, double now) {
    if (entry->state != ONLINE_ENTRY_ASKING && entry->state != ONLINE_ENTRY_WAITING) { return ONLINE_ENTRY_KEEP; }
    if (choice == ONLINE_ENTRY_CHOOSE_SOLO) {
        if (!entry->solo_allowed) { return ONLINE_ENTRY_KEEP; }
        entry->state = ONLINE_ENTRY_SOLO;
        return ONLINE_ENTRY_GO_SOLO;
    }
    // A retry while asking starts the ladder over; one while waiting only
    // brings the next try forward.
    if (entry->state == ONLINE_ENTRY_ASKING) { entry->attempt = 0U; }
    return connect_now(entry, now);
}

double online_entry_retry_in(const online_entry *entry, double now) {
    return entry->state == ONLINE_ENTRY_WAITING ? entry->retry_at - now : -1.0;
}
