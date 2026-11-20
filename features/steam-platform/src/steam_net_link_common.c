#include "steam_net_link_common.h"

#include <math.h>
#include <string.h>

enum {
    DEFAULT_MAX_RELIABLE = 16384,
    STEAM_MAX_MESSAGE = 524288,
    DEFAULT_SEND_BUFFER = 524288,
    SEND_BUFFER_MAX = 64 * 1024 * 1024,
    FAKE_MS_MAX = 10000,
};

static const float DEFAULT_TIMEOUT_S = 10.0F;
static const float TIMEOUT_MIN_S = 0.25F;
static const float TIMEOUT_MAX_S = 600.0F;
static const float DEFAULT_CLOSE_WAIT_S = 5.0F;
static const float CLOSE_WAIT_MAX_S = 60.0F;

int steam_net_link_reason_to_steam(uint32_t reason) {
    if (reason >= NET_LINK_REASON_APP_FIRST && reason <= NET_LINK_REASON_APP_LAST) {
        return (int)reason;
    }
    return STEAM_NET_LINK_END_GENERIC;
}

uint32_t steam_net_link_reason_from_steam(bool by_peer, int end_reason, bool was_connected) {
    if (by_peer) {
        if (end_reason >= NET_LINK_REASON_APP_FIRST && end_reason <= NET_LINK_REASON_APP_LAST) {
            return (uint32_t)end_reason;
        }
        if (end_reason == STEAM_NET_LINK_END_REFUSED) {
            return NET_LINK_REASON_REFUSED;
        }
        if (end_reason == STEAM_NET_LINK_END_GENERIC || end_reason == STEAM_NET_LINK_END_SHUTDOWN) {
            /* Before the accept it is a host going away while the guest knocked. */
            return was_connected ? NET_LINK_REASON_REMOTE_CLOSE : NET_LINK_REASON_UNREACHABLE;
        }
        return NET_LINK_REASON_TRANSPORT;
    }
    if (end_reason == STEAM_NET_LINK_END_TIMEOUT || end_reason == STEAM_NET_LINK_END_REMOTE_TIMEOUT) {
        return was_connected ? NET_LINK_REASON_TIMEOUT : NET_LINK_REASON_UNREACHABLE;
    }
    return NET_LINK_REASON_TRANSPORT;
}

static bool settle_seconds(float value, float fallback, float most, int32_t *out_ms) {
    if (value == 0.0F) {
        value = fallback;
    }
    if (!(value >= TIMEOUT_MIN_S && value <= most)) {
        return false;
    }
    *out_ms = (int32_t)lroundf(value * 1000.0F);
    return true;
}

bool steam_net_link_settle(const steam_net_link_config_t *config, bool host, steam_net_link_settings_t *out) {
    if (!config || !out || config->lanes.count < 1 || config->lanes.count > NET_LINK_LANES_MAX) {
        return false;
    }
    steam_net_link_settings_t settled = {.lanes = config->lanes, .local_without_proof = config->local_without_proof};
    for (uint8_t lane = config->lanes.count; lane < NET_LINK_LANES_MAX; lane++) {
        settled.lanes.priority[lane] = 0;
    }
    settled.max_peers = host ? config->max_peers : 1;
    if (settled.max_peers < 1 || settled.max_peers > STEAM_NET_LINK_PEERS_MAX) {
        return false;
    }
    settled.max_reliable = config->max_reliable_bytes ? config->max_reliable_bytes : DEFAULT_MAX_RELIABLE;
    settled.send_buffer = config->send_buffer_bytes ? config->send_buffer_bytes : DEFAULT_SEND_BUFFER;
    if (settled.max_reliable > STEAM_MAX_MESSAGE || settled.send_buffer < settled.max_reliable || settled.send_buffer > SEND_BUFFER_MAX) {
        return false;
    }
    if (!settle_seconds(config->timeout_s, DEFAULT_TIMEOUT_S, TIMEOUT_MAX_S, &settled.timeout_ms)
        || !settle_seconds(config->connect_timeout_s, DEFAULT_TIMEOUT_S, TIMEOUT_MAX_S, &settled.connect_timeout_ms)
        || !settle_seconds(config->close_wait_s, DEFAULT_CLOSE_WAIT_S, CLOSE_WAIT_MAX_S, &settled.close_wait_ms)) {
        return false;
    }
    *out = settled;
    return true;
}

int64_t steam_net_link_keepalive_us(int32_t timeout_ms) { return (int64_t)timeout_ms * 1000 / 4; }

int64_t steam_net_link_final_hold_us(int32_t timeout_ms, int32_t ping_ms) {
    const int64_t floor_us = 100000, cap_us = 3000000;
    /* Clamped before any product, so no input wraps. */
    const int64_t keepalive_us = timeout_ms > 0 ? steam_net_link_keepalive_us(timeout_ms < 600000 ? timeout_ms : 600000) : 0;
    const int64_t ping_us = ping_ms > 0 ? (int64_t)(ping_ms < 60000 ? ping_ms : 60000) * 1000 : 0;
    const int64_t hold_us = 2 * keepalive_us + 4 * ping_us;
    return hold_us < floor_us ? floor_us : hold_us > cap_us ? cap_us : hold_us;
}

uint8_t steam_net_link_keepalive_lane(const net_link_lanes_t *lanes) {
    uint8_t first = 0;
    for (uint8_t lane = 1; lane < lanes->count && lane < NET_LINK_LANES_MAX; lane++) {
        if (lanes->priority[lane] < lanes->priority[first]) {
            first = lane;
        }
    }
    return first;
}

bool steam_net_link_address_names_port(const char *text) {
    if (!text) {
        return false;
    }
    const char *last_colon = strrchr(text, ':');
    const char *last_dot = strrchr(text, '.');
    /* "[v6]:port", or "v4:port"; "::ffff:1.2.3.4" has its colons before the dots. */
    return strchr(text, '[') != NULL || (last_colon && last_dot && last_colon > last_dot);
}

net_link_peer_t steam_net_link_mint_peer(uint32_t *counter, uint32_t slot) {
    if (*counter == 0 || *counter >= (1U << (32 - STEAM_NET_LINK_SLOT_BITS))) {
        *counter = 1;
    }
    return (*counter)++ << STEAM_NET_LINK_SLOT_BITS | (slot & ((1U << STEAM_NET_LINK_SLOT_BITS) - 1));
}

uint32_t steam_net_link_peer_slot(net_link_peer_t peer) { return peer & ((1U << STEAM_NET_LINK_SLOT_BITS) - 1); }

int64_t steam_net_link_user_data(uint32_t link_index, uint32_t link_serial, net_link_peer_t peer) {
    return (int64_t)((uint64_t)(link_serial & STEAM_NET_LINK_SERIAL_MAX) << 35 | (uint64_t)(link_index & 7U) << 32 | peer);
}

bool steam_net_link_user_data_split(int64_t user_data, uint32_t *link_index, uint32_t *link_serial, net_link_peer_t *peer) {
    if (user_data < 0) {
        return false;
    }
    const uint64_t bits = (uint64_t)user_data;
    *link_serial = (uint32_t)(bits >> 35);
    *link_index = (uint32_t)(bits >> 32) & 7U;
    *peer = (net_link_peer_t)bits;
    return *link_serial != 0;
}

steam_net_link_verdict_t steam_net_link_judge(uint32_t lane, uint32_t lane_count, int64_t size, bool reliable, uint32_t max_unreliable, uint32_t max_reliable) {
    if (size == 0) {
        /* Any lane net-link knows: the sender's first lane need not be one this link has. */
        return reliable && lane < NET_LINK_LANES_MAX ? STEAM_NET_LINK_KEEPALIVE : STEAM_NET_LINK_HOSTILE;
    }
    if (size < 0 || lane >= lane_count || size > (int64_t)(reliable ? max_reliable : max_unreliable)) {
        return STEAM_NET_LINK_HOSTILE;
    }
    return STEAM_NET_LINK_ADMIT;
}

void steam_net_link_keepalive_allowance_start(steam_net_link_keepalive_allowance_t *allowance, int64_t now_us) {
    *allowance = (steam_net_link_keepalive_allowance_t){.filled_us = now_us, .left = STEAM_NET_LINK_KEEPALIVE_BURST};
}

bool steam_net_link_keepalive_allowed(steam_net_link_keepalive_allowance_t *allowance, int64_t now_us) {
    const int64_t each_us = 1000000 / STEAM_NET_LINK_KEEPALIVE_PER_S;
    if (now_us > allowance->filled_us) {
        const int64_t earned = (now_us - allowance->filled_us) / each_us;
        const int64_t room = (int64_t)STEAM_NET_LINK_KEEPALIVE_BURST - allowance->left;
        if (earned >= room) {
            allowance->left = STEAM_NET_LINK_KEEPALIVE_BURST;
            allowance->filled_us = now_us;
        } else {
            allowance->left += (uint32_t)earned;
            allowance->filled_us += earned * each_us;
        }
    }
    if (allowance->left == 0) {
        return false;
    }
    allowance->left--;
    return true;
}

void steam_net_link_ledger_owe_connected(steam_net_link_ledger_t *ledger, uint32_t slot) {
    if (slot < STEAM_NET_LINK_PEERS_MAX && ledger->connected[slot] == 0) {
        ledger->connected[slot] = ++ledger->next_stamp;
    }
}

bool steam_net_link_ledger_owe_disconnected(steam_net_link_ledger_t *ledger, uint32_t slot) {
    if (slot >= STEAM_NET_LINK_PEERS_MAX || ledger->disconnected[slot] != 0) {
        return false;
    }
    ledger->disconnected[slot] = ++ledger->next_stamp;
    return true;
}

bool steam_net_link_ledger_owes_connected(const steam_net_link_ledger_t *ledger, uint32_t slot) { return slot < STEAM_NET_LINK_PEERS_MAX && ledger->connected[slot] != 0; }

bool steam_net_link_ledger_next(const steam_net_link_ledger_t *ledger, uint32_t *slot, net_link_event_type_t *type) {
    uint64_t best = 0;
    for (uint32_t s = 0; s < STEAM_NET_LINK_PEERS_MAX; s++) {
        /* A slot's CONNECTED goes before its DISCONNECTED, whichever was stamped first. */
        const bool connected = ledger->connected[s] != 0;
        const uint64_t stamp = connected ? ledger->connected[s] : ledger->disconnected[s];
        if (stamp != 0 && (best == 0 || stamp < best)) {
            best = stamp;
            *slot = s;
            *type = connected ? NET_LINK_EVENT_CONNECTED : NET_LINK_EVENT_DISCONNECTED;
        }
    }
    return best != 0;
}

void steam_net_link_ledger_paid(steam_net_link_ledger_t *ledger, uint32_t slot, net_link_event_type_t type) {
    if (slot >= STEAM_NET_LINK_PEERS_MAX) {
        return;
    }
    if (type == NET_LINK_EVENT_CONNECTED) {
        ledger->connected[slot] = 0;
    } else if (type == NET_LINK_EVENT_DISCONNECTED) {
        steam_net_link_ledger_forget(ledger, slot);
    }
}

void steam_net_link_ledger_forget(steam_net_link_ledger_t *ledger, uint32_t slot) {
    if (slot < STEAM_NET_LINK_PEERS_MAX) {
        ledger->connected[slot] = 0;
        ledger->disconnected[slot] = 0;
    }
}

static float share_pct(float share) {
    if (!(share > 0.0F)) {
        return 0.0F;
    }
    return share >= 1.0F ? 100.0F : share * 100.0F;
}

static int32_t whole_ms(float ms) {
    if (!(ms > 0.0F)) {
        return 0;
    }
    return ms >= (float)FAKE_MS_MAX ? FAKE_MS_MAX : (int32_t)lroundf(ms);
}

steam_net_link_fake_values_t steam_net_link_fake_values(const steam_net_link_fake_t *fake) {
    steam_net_link_fake_values_t values = {0};
    if (fake) {
        values.loss_pct = share_pct(fake->loss);
        values.lag_ms = whole_ms(fake->lag_ms);
        values.reorder_pct = share_pct(fake->reorder);
        values.duplicate_pct = share_pct(fake->duplicate);
    }
    return values;
}

int32_t steam_net_link_reorder_ms(float reorder_ms) { return whole_ms(reorder_ms); }
