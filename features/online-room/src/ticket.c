#include "online_room/ticket.h"

#include <string.h>

#include "net_codec.h"
#include "net_hmac.h"

static void mac_of(const uint8_t key[ONLINE_TICKET_KEY_SIZE], const uint8_t *signed_part, uint8_t out[NET_SHA256_SIZE]) {
    net_hmac_sha256(key, ONLINE_TICKET_KEY_SIZE, signed_part, ONLINE_TICKET_SIGNED_SIZE, out);
}

void online_ticket_encode(const online_ticket *ticket, const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint8_t out[ONLINE_TICKET_SIZE]) {
    net_writer_t writer;
    net_writer_init(&writer, out, ONLINE_TICKET_SIZE);
    net_write_u8(&writer, ONLINE_TICKET_VERSION);
    net_write_u8(&writer, ticket->kind);
    net_write_u64(&writer, ticket->subject);
    net_write_u32(&writer, ticket->room_id);
    net_write_u32(&writer, ticket->expires_at);
    uint8_t mac[NET_SHA256_SIZE];
    mac_of(key, out, mac);
    memcpy(out + ONLINE_TICKET_SIGNED_SIZE, mac, ONLINE_TICKET_MAC_SIZE);
}

online_ticket_result online_ticket_verify(const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint32_t room_id, const uint8_t *data, size_t size,
    uint32_t now, online_ticket *out) {
    if (data == NULL || size != ONLINE_TICKET_SIZE || data[0] != ONLINE_TICKET_VERSION) { return ONLINE_TICKET_MALFORMED; }
    if (data[1] != ONLINE_TICKET_KIND_PLAYER && data[1] != ONLINE_TICKET_KIND_PAGE_ENTRY) { return ONLINE_TICKET_WRONG_KIND; }
    uint8_t mac[NET_SHA256_SIZE];
    mac_of(key, data, mac);
    if (!net_equal_constant_time(mac, data + ONLINE_TICKET_SIGNED_SIZE, ONLINE_TICKET_MAC_SIZE)) { return ONLINE_TICKET_FORGED; }
    net_reader_t reader;
    net_reader_init(&reader, data + 2U, ONLINE_TICKET_SIGNED_SIZE - 2U);
    online_ticket ticket = {.kind = data[1]};
    ticket.subject = net_read_u64(&reader);
    ticket.room_id = net_read_u32(&reader);
    ticket.expires_at = net_read_u32(&reader);
    if (ticket.room_id != room_id) { return ONLINE_TICKET_WRONG_ROOM; }
    if ((uint64_t)ticket.expires_at + ONLINE_TICKET_SKEW_SECONDS < (uint64_t)now) { return ONLINE_TICKET_EXPIRED; }
    if (ticket.subject == 0U) { return ONLINE_TICKET_NO_SUBJECT; }
    if (out != NULL) { *out = ticket; }
    return ONLINE_TICKET_OK;
}

void online_ticket_bearer(const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint32_t room_id, uint64_t incarnation, uint32_t now,
    char out[ONLINE_TICKET_BEARER_CHARS + 1U]) {
    const online_ticket ticket = {
        .kind = ONLINE_TICKET_KIND_ROOM, .subject = incarnation, .room_id = room_id, .expires_at = now + ONLINE_TICKET_BEARER_SECONDS};
    uint8_t bytes[ONLINE_TICKET_SIZE];
    online_ticket_encode(&ticket, key, bytes);
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0U; index < ONLINE_TICKET_SIZE; ++index) {
        out[index * 2U] = digits[bytes[index] >> 4U];
        out[index * 2U + 1U] = digits[bytes[index] & 0x0FU];
    }
    out[ONLINE_TICKET_BEARER_CHARS] = '\0';
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

bool online_ticket_key_parse(const char *text, uint8_t key[ONLINE_TICKET_KEY_SIZE]) {
    size_t at = 0U;
    for (size_t index = 0U; index < ONLINE_TICKET_KEY_SIZE; ++index) {
        const int high = hex_value(text[at]);
        const int low = high < 0 ? -1 : hex_value(text[at + 1U]);
        if (low < 0) { return false; }
        key[index] = (uint8_t)((high << 4) | low);
        at += 2U;
    }
    while (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n') { ++at; }
    return text[at] == '\0';
}

online_ticket_result online_ticket_check(online_ticket_seen *seen, const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint32_t room_id, const uint8_t *data,
    size_t size, uint32_t now, online_ticket *out) {
    online_ticket ticket;
    const online_ticket_result result = online_ticket_verify(key, room_id, data, size, now, &ticket);
    if (result != ONLINE_TICKET_OK) { return result; }
    // A genuine ticket is named even when it is refused below, for the log.
    if (out != NULL) { *out = ticket; }
    const uint8_t *mac = data + ONLINE_TICKET_SIGNED_SIZE;
    bool free_entry = false;
    uint32_t held = 0U;
    for (uint32_t index = 0U; index < ONLINE_TICKET_SEEN_MAX; ++index) {
        if (seen->until[index] != 0U && seen->until[index] < now) { seen->until[index] = 0U; }
        if (seen->until[index] == 0U) {
            free_entry = true;
            continue;
        }
        if (memcmp(seen->macs[index], mac, ONLINE_TICKET_MAC_SIZE) == 0) { return ONLINE_TICKET_REPEATED; }
        held += seen->subjects[index] == ticket.subject ? 1U : 0U;
    }
    if (held >= ONLINE_TICKET_PER_ACCOUNT) { return ONLINE_TICKET_CAPPED; }
    if (!free_entry) {
        seen->full += 1U;
        return ONLINE_TICKET_FULL;
    }
    return ONLINE_TICKET_OK;
}

void online_ticket_remember(online_ticket_seen *seen, const uint8_t *data, const online_ticket *ticket, uint32_t now) {
    for (uint32_t index = 0U; index < ONLINE_TICKET_SEEN_MAX; ++index) {
        if (seen->until[index] != 0U && seen->until[index] >= now) { continue; }
        memcpy(seen->macs[index], data + ONLINE_TICKET_SIGNED_SIZE, ONLINE_TICKET_MAC_SIZE);
        seen->subjects[index] = ticket->subject;
        seen->until[index] = ticket->expires_at + ONLINE_TICKET_SKEW_SECONDS;
        return;
    }
}

online_ticket_result online_ticket_admit(online_ticket_seen *seen, const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint32_t room_id, const uint8_t *data,
    size_t size, uint32_t now, online_ticket *out) {
    online_ticket ticket;
    const online_ticket_result result = online_ticket_check(seen, key, room_id, data, size, now, &ticket);
    if (result != ONLINE_TICKET_OK) { return result; }
    online_ticket_remember(seen, data, &ticket, now);
    if (out != NULL) { *out = ticket; }
    return ONLINE_TICKET_OK;
}
