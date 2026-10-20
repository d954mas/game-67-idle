#include "online_room/meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "net_codec.h"

// A 412's body: the stored rev, then the writer of it (room, incarnation, save id).
#define CONFLICT_BYTES 28U
// An entry of a batch's answer: status u16, the size of its body u32.
#define ANSWER_HEAD_BYTES 6U

_Static_assert(ONLINE_META_BATCH_MAX <= 96U, "a call of saves holds no more entries than the metaserver takes (saveBatchMax)");

// The authority of `url` after its scheme: [begin, end), and where its host begins.
static bool authority_of(const char *url, const char **begin, const char **host, const char **end) {
    const char *scheme_end = strstr(url, "://");
    if (scheme_end == NULL) { return false; }
    *begin = scheme_end + 3;
    *end = *begin + strcspn(*begin, "/?#\\");
    *host = *begin;
    for (const char *at = *begin; at < *end; ++at) {
        if (*at == '@') { *host = at + 1; }
    }
    return true;
}

bool online_meta_url_allowed(const char *url, bool insecure) {
    if (strncmp(url, "https://", 8U) == 0) { return true; }
    if (strncmp(url, "http://", 7U) != 0) { return false; }
    if (insecure) { return true; }
    const char *begin = NULL;
    const char *host = NULL;
    const char *end = NULL;
    if (!authority_of(url, &begin, &host, &end)) { return false; }
    // The host runs to the port's colon, past an IPv6 literal's brackets.
    const char *host_end = host;
    if (*host == '[') {
        while (host_end < end && *host_end != ']') { ++host_end; }
        if (host_end == end) { return false; }
        ++host_end;
    } else {
        while (host_end < end && *host_end != ':') { ++host_end; }
    }
    if (host_end < end) {
        if (*host_end != ':' || host_end + 1 == end) { return false; }
        for (const char *digit = host_end + 1; digit < end; ++digit) {
            if (*digit < '0' || *digit > '9') { return false; }
        }
    }
    static const char *const k_local[] = {"127.0.0.1", "localhost", "[::1]", "meta"};
    const size_t length = (size_t)(host_end - host);
    for (size_t index = 0U; index < sizeof k_local / sizeof k_local[0]; ++index) {
        if (strlen(k_local[index]) == length && memcmp(host, k_local[index], length) == 0) { return true; }
    }
    return false;
}

void online_meta_url_loggable(const char *url, char *out, size_t cap) {
    const char *begin = NULL;
    const char *host = NULL;
    const char *end = NULL;
    if (!authority_of(url, &begin, &host, &end) || host == begin) {
        (void)snprintf(out, cap, "%s", url);
        return;
    }
    (void)snprintf(out, cap, "%.*s%s", (int)(begin - url), url, host);
}

static uint32_t now_unix(const online_meta *meta) {
    if (meta->config.clock != NULL) { return meta->config.clock(meta->config.clock_user); }
    return (uint32_t)time(NULL);
}

bool online_meta_init(online_meta *meta, const online_meta_config *config) {
    memset(meta, 0, sizeof *meta);
    meta->config = *config;
    const size_t length = strlen(config->base_url);
    if (length >= sizeof meta->base_url) { return false; }
    memcpy(meta->base_url, config->base_url, length + 1U);
    meta->config.base_url = meta->base_url;
    return true;
}

static uint32_t free_request(const online_meta *meta) {
    uint32_t slot = ONLINE_META_REQUESTS;
    for (uint32_t index = 0U; index < ONLINE_META_REQUESTS && slot == ONLINE_META_REQUESTS; ++index) {
        if (meta->requests[index].id == 0U) { slot = index; }
    }
    return slot;
}

// Request `slot` under a new id, which is returned.
static uint32_t take_request(online_meta *meta, uint32_t slot, online_meta_call kind, uint32_t handle) {
    meta->next_id += 1U;
    if (meta->next_id == 0U) { meta->next_id = 1U; }
    meta->requests[slot].id = meta->next_id;
    meta->requests[slot].handle = handle;
    meta->requests[slot].call = (uint8_t)kind;
    meta->requests[slot].batch = 0U;
    meta->requests[slot].entry = 0U;
    return meta->next_id;
}

// A call under a fresh bearer; its transport handle, 0 when it could not start.
static uint32_t begin(online_meta *meta, const char *path, const void *body, size_t size) {
    char url[ONLINE_META_URL_MAX + 64U];
    if (snprintf(url, sizeof url, "%s%s", meta->base_url, path) >= (int)sizeof url) { return 0U; }
    char bearer[ONLINE_TICKET_BEARER_CHARS + 1U];
    online_ticket_bearer(meta->config.key, meta->config.room_id, meta->config.incarnation, now_unix(meta), bearer);
    const online_meta_transport *transport = meta->config.transport;
    const uint32_t handle = transport->begin(transport->user, url, bearer, body, size, ONLINE_META_TIMEOUT_MS);
    if (handle != 0U) { meta->stats.calls += 1U; }
    return handle;
}

// A call of its own; the store's request id, 0 when it could not start.
static uint32_t call(online_meta *meta, online_meta_call kind, const char *path, const void *body, size_t size) {
    const uint32_t slot = free_request(meta);
    if (slot == ONLINE_META_REQUESTS) { return 0U; }
    const uint32_t handle = begin(meta, path, body, size);
    return handle == 0U ? 0U : take_request(meta, slot, kind, handle);
}

static uint32_t slot_of(const online_meta *meta, uint32_t id) {
    for (uint32_t index = 0U; id != 0U && index < ONLINE_META_REQUESTS; ++index) {
        if (meta->requests[index].id == id) { return index; }
    }
    return ONLINE_META_REQUESTS;
}

static uint32_t store_load(void *user, uint64_t account) {
    char path[64];
    (void)snprintf(path, sizeof path, "/rooms/profiles/%llu/load", (unsigned long long)account);
    return call(user, ONLINE_META_LOAD, path, NULL, 0U);
}

// The pending saves go in one call. One that cannot start is over at once
// with no answer, which each of its saves hears as a failure.
static void store_send(void *user) {
    online_meta *meta = user;
    if (meta->pending_count == 0U) { return; }
    uint32_t batch = 0U;
    while (batch < ONLINE_META_REQUESTS && meta->batches[batch].waiting != 0U) { batch += 1U; }
    // Every save of a batch holds a request of its own, so there are never more batches than requests.
    if (batch == ONLINE_META_REQUESTS) { __builtin_trap(); }
    uint32_t waiting = 0U;
    for (uint32_t index = 0U; index < ONLINE_META_REQUESTS; ++index) {
        if (meta->requests[index].id != 0U && meta->requests[index].call == ONLINE_META_SAVE && meta->requests[index].batch == 0U) {
            meta->requests[index].batch = batch + 1U;
            waiting += 1U;
        }
    }
    // A save cancelled before the send still goes with the others (alone,
    // nothing goes): its save id keeps a later save of the account harmless.
    if (waiting > 0U) {
        const uint32_t handle = begin(meta, "/rooms/profiles/save", meta->pending, meta->pending_size);
        meta->batches[batch] = (online_meta_batch){.handle = handle, .waiting = waiting, .over = handle == 0U};
    }
    meta->pending_size = 0U;
    meta->pending_count = 0U;
}

// A save joins the pending call; a full one goes first.
static uint32_t store_save(void *user, uint64_t account, int64_t if_rev, uint64_t save_id, const char *doc, size_t size) {
    online_meta *meta = user;
    if (size > ONLINE_ROOM_PROFILE_TEXT_MAX || if_rev < 0) { return 0U; }
    if (meta->pending_count == ONLINE_META_BATCH_MAX) { store_send(meta); }
    const uint32_t slot = free_request(meta);
    if (slot == ONLINE_META_REQUESTS) { return 0U; }
    uint8_t *entry = meta->pending + meta->pending_size;
    net_writer_t writer;
    net_writer_init(&writer, entry, ONLINE_META_BATCH_ENTRY_HEAD);
    net_write_u64(&writer, account);
    net_write_u64(&writer, (uint64_t)if_rev);
    net_write_u64(&writer, save_id);
    net_write_u32(&writer, (uint32_t)size);
    memcpy(entry + ONLINE_META_BATCH_ENTRY_HEAD, doc, size);
    meta->pending_size += ONLINE_META_BATCH_ENTRY_HEAD + size;
    const uint32_t id = take_request(meta, slot, ONLINE_META_SAVE, 0U);
    meta->requests[slot].entry = meta->pending_count;
    meta->pending_count += 1U;
    return id;
}

// A save done with its batch. The last one frees it, and forgets a call
// nobody waits for any more.
static void leave_batch(online_meta *meta, uint32_t batch) {
    online_meta_batch *entry = &meta->batches[batch - 1U];
    entry->waiting -= 1U;
    if (entry->waiting != 0U) { return; }
    if (!entry->over) {
        const online_meta_transport *transport = meta->config.transport;
        transport->cancel(transport->user, entry->handle);
    }
    free(entry->body);
    *entry = (online_meta_batch){0};
}

static void unexpected(online_meta *meta, const char *what, uint16_t status) {
    if (status == 401U) {
        // The key, the clock or the bearer's version: every call fails alike until the room is fixed.
        meta->stats.unauthorized += 1U;
    } else {
        meta->stats.unexpected += 1U;
    }
    fprintf(stderr, "meta: %s answered %u\n", what, (unsigned)status);
}

static online_store_status load_answer(online_meta *meta, uint16_t status, const uint8_t *body, size_t size, char *doc, size_t cap,
    online_store_answer *answer) {
    switch (status) {
    case 200U: break;
    case 409U: return ONLINE_STORE_SUPERSEDED;
    case 413U: unexpected(meta, "a load", status); return ONLINE_STORE_REFUSED;
    case 400U: unexpected(meta, "a load", status); return ONLINE_STORE_REFUSED;
    case 0U:
    case 404U: // no such account: the player enters again through the metaserver
    case 500U: return ONLINE_STORE_FAILED;
    default: unexpected(meta, "a load", status); return ONLINE_STORE_FAILED;
    }
    if (size < 8U) {
        unexpected(meta, "a load's body", status);
        return ONLINE_STORE_FAILED;
    }
    net_reader_t reader;
    net_reader_init(&reader, body, 8U);
    const uint64_t rev = net_read_u64(&reader);
    const size_t doc_size = size - 8U;
    // A document this build could not hold is not one it can read.
    if (rev > (uint64_t)INT64_MAX || doc_size > cap || doc == NULL) { return ONLINE_STORE_REFUSED; }
    if (doc_size > 0U) { memcpy(doc, body + 8U, doc_size); }
    answer->rev = (int64_t)rev;
    answer->size = doc_size;
    return ONLINE_STORE_DONE;
}

static online_store_status save_answer(online_meta *meta, uint16_t status, const uint8_t *body, size_t size, online_store_answer *answer) {
    net_reader_t reader;
    switch (status) {
    case 200U: {
        // A landed save whose answer does not read is as unknown as a lost one: it goes again the same.
        if (size != 8U) {
            unexpected(meta, "a save's body", status);
            return ONLINE_STORE_FAILED;
        }
        net_reader_init(&reader, body, size);
        const uint64_t rev = net_read_u64(&reader);
        if (rev == 0U || rev > (uint64_t)INT64_MAX) {
            unexpected(meta, "a save's rev", status);
            return ONLINE_STORE_FAILED;
        }
        answer->rev = (int64_t)rev;
        return ONLINE_STORE_DONE;
    }
    case 412U: {
        if (size != CONFLICT_BYTES) { break; }
        net_reader_init(&reader, body, size);
        const uint64_t rev = net_read_u64(&reader);
        if (rev > (uint64_t)INT64_MAX) {
            unexpected(meta, "a stale answer's rev", status);
            return ONLINE_STORE_FAILED;
        }
        answer->rev = (int64_t)rev;
        const uint32_t room = net_read_u32(&reader);
        const uint64_t incarnation = net_read_u64(&reader);
        const uint64_t save_id = net_read_u64(&reader);
        // Only this room life's own saves are named: another life's ids mean nothing here.
        answer->writer = room == meta->config.room_id && incarnation == meta->config.incarnation ? save_id : 0U;
        return ONLINE_STORE_CONFLICT;
    }
    case 409U: return ONLINE_STORE_SUPERSEDED;
    case 413U:
    case 400U: unexpected(meta, "a save", status); return ONLINE_STORE_REFUSED;
    case 0U:
    case 500U: return ONLINE_STORE_FAILED;
    default: unexpected(meta, "a save", status); return ONLINE_STORE_FAILED;
    }
    // A stale answer that does not read is no verdict: the save goes again, as
    // after no answer, and a drain's wait bounds it. Only 413 and 400 refuse.
    unexpected(meta, "a stale answer's body", status);
    return ONLINE_STORE_FAILED;
}

// A save's own entry of its batch's answer (`status u16 | size u32 | body`
// per entry, in order). A call not answered 200 says nothing of any one
// save: each goes again as it was.
static online_store_status batch_answer(online_meta *meta, const online_meta_batch *batch, uint32_t entry, online_store_answer *answer) {
    if (batch->status != 200U) { return ONLINE_STORE_FAILED; }
    size_t at = 0U;
    for (uint32_t index = 0U; batch->size - at >= ANSWER_HEAD_BYTES; ++index) {
        net_reader_t reader;
        net_reader_init(&reader, batch->body + at, ANSWER_HEAD_BYTES);
        const uint16_t status = net_read_u16(&reader);
        const uint32_t size = net_read_u32(&reader);
        if (size > batch->size - at - ANSWER_HEAD_BYTES) { break; }
        if (index == entry) { return save_answer(meta, status, batch->body + at + ANSWER_HEAD_BYTES, size, answer); }
        at += ANSWER_HEAD_BYTES + size;
    }
    // A landed save whose answer does not read is as unknown as a lost one: it goes again the same.
    unexpected(meta, "a batch's body", batch->status);
    return ONLINE_STORE_FAILED;
}

static online_store_status save_poll(online_meta *meta, uint32_t slot, online_store_answer *answer) {
    const uint32_t batch = meta->requests[slot].batch;
    if (batch == 0U) { return ONLINE_STORE_PENDING; }
    online_meta_batch *entry = &meta->batches[batch - 1U];
    if (!entry->over) {
        const online_meta_transport *transport = meta->config.transport;
        if (!transport->poll(transport->user, entry->handle, &entry->status, &entry->body, &entry->size)) { return ONLINE_STORE_PENDING; }
        entry->over = true;
        // Heard once for the call, not once per save in it.
        if (entry->status != 200U && entry->status != 0U && entry->status != 500U) { unexpected(meta, "a call of saves", entry->status); }
    }
    *answer = (online_store_answer){0};
    const online_store_status result = batch_answer(meta, entry, meta->requests[slot].entry, answer);
    meta->requests[slot].id = 0U;
    leave_batch(meta, batch);
    return result;
}

static online_store_status store_poll(void *user, uint32_t request, char *doc, size_t cap, online_store_answer *answer) {
    online_meta *meta = user;
    const uint32_t slot = slot_of(meta, request);
    if (slot == ONLINE_META_REQUESTS) { return ONLINE_STORE_FAILED; }
    if (meta->requests[slot].call == ONLINE_META_SAVE) { return save_poll(meta, slot, answer); }
    const online_meta_transport *transport = meta->config.transport;
    uint16_t status = 0U;
    uint8_t *body = NULL;
    size_t size = 0U;
    if (!transport->poll(transport->user, meta->requests[slot].handle, &status, &body, &size)) { return ONLINE_STORE_PENDING; }
    meta->requests[slot].id = 0U;
    *answer = (online_store_answer){0};
    const online_store_status result = load_answer(meta, status, body, size, doc, cap, answer);
    free(body);
    return result;
}

static void store_cancel(void *user, uint32_t request) {
    online_meta *meta = user;
    const uint32_t slot = slot_of(meta, request);
    if (slot == ONLINE_META_REQUESTS) { return; }
    meta->requests[slot].id = 0U;
    if (meta->requests[slot].call == ONLINE_META_SAVE) {
        if (meta->requests[slot].batch != 0U) { leave_batch(meta, meta->requests[slot].batch); }
        return;
    }
    const online_meta_transport *transport = meta->config.transport;
    transport->cancel(transport->user, meta->requests[slot].handle);
}

void online_meta_store(online_meta *meta, online_profile_store *out) {
    *out = (online_profile_store){
        .user = meta, .load = store_load, .save = store_save, .poll = store_poll, .cancel = store_cancel, .send = store_send};
}

bool online_meta_status_send(online_meta *meta, const char *body, size_t size, uint64_t now_ms) {
    if (meta->status_request != 0U || size > ONLINE_META_STATUS_MAX_BODY) { return false; }
    meta->status_request = call(meta, ONLINE_META_STATUS, "/rooms/status", body, size);
    meta->status_sent_ms = now_ms;
    return meta->status_request != 0U;
}

void online_meta_status_poll(online_meta *meta, uint64_t now_ms) {
    const uint32_t slot = slot_of(meta, meta->status_request);
    if (slot == ONLINE_META_REQUESTS) {
        meta->status_request = 0U;
        return;
    }
    const online_meta_transport *transport = meta->config.transport;
    uint16_t status = 0U;
    uint8_t *body = NULL;
    size_t size = 0U;
    if (!transport->poll(transport->user, meta->requests[slot].handle, &status, &body, &size)) { return; }
    free(body);
    meta->requests[slot].id = 0U;
    meta->status_request = 0U;
    meta->status_round_trip_ms = (uint32_t)(now_ms - meta->status_sent_ms);
    // No answer is the next status's problem.
    if (status == 200U || status == 204U || status == 0U) { return; }
    // 400 fences the room: the metaserver routes nobody here until a status reads.
    if (status == 400U) { meta->stats.status_refused += 1U; }
    unexpected(meta, "the status", status);
}
