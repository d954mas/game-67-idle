#include "steam_net_link_closes.h"

#include "steam_net_link_common.h"

#include <string.h>

static void ask(steam_net_link_close_actions_t *out, steam_net_link_close_do_t what, uint32_t handle, int code) {
    if (out->count < sizeof out->items / sizeof out->items[0]) {
        out->items[out->count++] = (steam_net_link_close_action_t){.what = what, .handle = handle, .code = code};
    }
}

static bool socket_needed(const steam_net_link_closes_t *closes, uint32_t socket, const steam_net_link_close_entry_t *besides) {
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX; i++) {
        const steam_net_link_close_entry_t *e = &closes->entries[i];
        if (e->used && e != besides && e->socket == socket) {
            return true;
        }
    }
    return false;
}

/* The entry leaves; an orphaned socket no other entry needs leaves with it. */
static void drop(steam_net_link_closes_t *closes, steam_net_link_close_entry_t *e, steam_net_link_close_actions_t *out) {
    if (e->socket_orphaned && e->socket != 0 && !socket_needed(closes, e->socket, e)) {
        ask(out, STEAM_NET_LINK_DO_CLOSE_SOCKET, e->socket, 0);
    }
    memset(e, 0, sizeof *e);
}

/* Steam is asked to close; the hold lets the close be answered before the entry leaves. */
static void tell(steam_net_link_close_entry_t *e, int32_t ping_ms, int64_t now_us, steam_net_link_close_actions_t *out) {
    ask(out, STEAM_NET_LINK_DO_CLOSE, e->conn, e->code);
    e->told = true;
    e->release_us = now_us + steam_net_link_final_hold_us(e->timeout_ms, ping_ms);
}

static int64_t due_us(const steam_net_link_close_entry_t *e) { return e->told ? e->release_us : e->deadline_us; }

void steam_net_link_closes_take(steam_net_link_closes_t *closes, uint32_t conn, int code, int32_t timeout_ms, int64_t close_wait_us, uint32_t socket,
                                const steam_net_link_address_t *local, int64_t now_us, steam_net_link_close_actions_t *out) {
    steam_net_link_close_entry_t *slot = NULL, *first = NULL;
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX && !slot; i++) {
        steam_net_link_close_entry_t *e = &closes->entries[i];
        if (!e->used) {
            slot = e;
        } else if (!first || due_us(e) < due_us(first)) {
            first = e;
        }
    }
    if (!slot) {
        /* Full: the close due first is cut. */
        if (!first->told) {
            ask(out, STEAM_NET_LINK_DO_CLOSE, first->conn, first->code);
        }
        drop(closes, first, out);
        slot = first;
    }
    *slot = (steam_net_link_close_entry_t){.used = true, .conn = conn, .socket = socket, .code = code, .timeout_ms = timeout_ms, .deadline_us = now_us + close_wait_us};
    if (local) {
        slot->local = *local;
    }
}

bool steam_net_link_closes_peer_left(steam_net_link_closes_t *closes, uint32_t conn, steam_net_link_close_actions_t *out) {
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX; i++) {
        steam_net_link_close_entry_t *e = &closes->entries[i];
        if (e->used && !e->told && e->conn == conn) {
            /* Nothing is left to deliver to a peer that is gone. */
            ask(out, STEAM_NET_LINK_DO_FREE, conn, 0);
            drop(closes, e, out);
            return true;
        }
    }
    return false;
}

void steam_net_link_closes_frame(steam_net_link_closes_t *closes, int64_t now_us, steam_net_link_close_probe_fn probe, void *user, steam_net_link_close_actions_t *out) {
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX; i++) {
        steam_net_link_close_entry_t *e = &closes->entries[i];
        if (!e->used) {
            continue;
        }
        if (e->told) {
            if (now_us >= e->release_us) {
                drop(closes, e, out);
            }
            continue;
        }
        const steam_net_link_close_probe_t seen = probe(user, e->conn);
        if (seen.state == STEAM_NET_LINK_CLOSE_GONE) {
            ask(out, STEAM_NET_LINK_DO_FREE, e->conn, 0);
            drop(closes, e, out);
        } else if (seen.state == STEAM_NET_LINK_CLOSE_ACKED || now_us >= e->deadline_us) {
            tell(e, seen.ping_ms, now_us, out);
        }
    }
}

bool steam_net_link_closes_keep_socket(steam_net_link_closes_t *closes, uint32_t socket) {
    bool kept = false;
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX && socket != 0; i++) {
        steam_net_link_close_entry_t *e = &closes->entries[i];
        if (e->used && e->socket == socket) {
            e->socket_orphaned = true;
            kept = true;
        }
    }
    return kept;
}

bool steam_net_link_closes_turns_away(const steam_net_link_closes_t *closes, uint32_t socket) {
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX && socket != 0; i++) {
        const steam_net_link_close_entry_t *e = &closes->entries[i];
        if (e->used && e->socket_orphaned && e->socket == socket) {
            return true;
        }
    }
    return false;
}

bool steam_net_link_closes_take_port(steam_net_link_closes_t *closes, const steam_net_link_address_t *local, steam_net_link_close_actions_t *out) {
    bool took = false;
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX; i++) {
        steam_net_link_close_entry_t *e = &closes->entries[i];
        if (e->used && e->socket_orphaned && memcmp(&e->local, local, sizeof *local) == 0) {
            if (!e->told) {
                ask(out, STEAM_NET_LINK_DO_CLOSE, e->conn, e->code);
            }
            drop(closes, e, out);
            took = true;
        }
    }
    return took;
}

bool steam_net_link_closes_any(const steam_net_link_closes_t *closes) {
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX; i++) {
        if (closes->entries[i].used) {
            return true;
        }
    }
    return false;
}

void steam_net_link_closes_forget(steam_net_link_closes_t *closes) { memset(closes, 0, sizeof *closes); }

steam_net_link_dial_t steam_net_link_dial_decision(int64_t now_us, int64_t deadline_us, steam_net_link_certificate_t certificate) {
    if (now_us >= deadline_us || certificate == STEAM_NET_LINK_CERTIFICATE_FAILED) {
        return STEAM_NET_LINK_DIAL_GIVE_UP;
    }
    return certificate == STEAM_NET_LINK_CERTIFICATE_CURRENT ? STEAM_NET_LINK_DIAL_NOW : STEAM_NET_LINK_DIAL_WAIT;
}
