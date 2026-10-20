#include "online_room/meta_entry.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

// The longest honest chain: enter refused (the secret unknown), guest, enter.
#define MAX_HOPS 3U

static bool copy_bounded(char *dst, size_t cap, const char *src) {
    const size_t length = strlen(src);
    if (length + 1U > cap) { return false; }
    memcpy(dst, src, length + 1U);
    return true;
}

// The secret is base64url from the metaserver; anything else would break the
// stored JSON and the Authorization header alike.
static bool secret_charset(const char *text) {
    for (const char *at = text; *at != '\0'; ++at) {
        const bool ok = (*at >= 'A' && *at <= 'Z') || (*at >= 'a' && *at <= 'z') || (*at >= '0' && *at <= '9') || *at == '-' || *at == '_';
        if (!ok) { return false; }
    }
    return text[0] != '\0';
}

bool online_meta_entry_json_id(const cJSON *node, uint64_t *out) {
    if (!cJSON_IsNumber(node)) { return false; }
    const double value = node->valuedouble;
    if (!(value >= 1.0) || value > 9007199254740992.0 || value != floor(value)) { return false; }
    *out = (uint64_t)value;
    return true;
}

static const char *string_field(const cJSON *body, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(body, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

// A stored session is the secret of the metaserver that issued it and goes to
// no other; a record without its issuer is trusted to the build's own only.
static void load_session(online_meta_entry *entry) {
    if (entry->storage.load == NULL) { return; }
    char *text = entry->storage.load(entry->storage.user, entry->slot);
    if (text == NULL) { return; }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (root == NULL) { return; }
    const char *meta = string_field(root, "meta");
    const bool ours = meta != NULL ? strcmp(meta, entry->meta_url) == 0 : !entry->foreign;
    const char *secret = string_field(root, "secret");
    uint64_t player_id = 0U;
    if (ours && online_meta_entry_json_id(cJSON_GetObjectItemCaseSensitive(root, "player_id"), &player_id) && secret != NULL && secret_charset(secret) &&
        copy_bounded(entry->secret, sizeof entry->secret, secret)) {
        entry->player_id = player_id;
        entry->has_session = true;
        entry->account = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "account"));
    }
    cJSON_Delete(root);
}

static void session_text(const online_meta_entry *entry, char *text, size_t cap) {
    (void)snprintf(text, cap, "{\"player_id\":%llu,\"secret\":\"%s\",\"account\":%s,\"meta\":\"%s\"}", (unsigned long long)entry->player_id,
        entry->secret, entry->account ? "true" : "false", entry->meta_url);
}

static void store_to(const online_meta_entry *entry, const char *slot, const char *text) {
    // Whoever wrote the page URL chose this metaserver: nothing it issues
    // replaces the session of the game's own.
    if (entry->foreign || entry->storage.store == NULL) { return; }
    entry->storage.store(entry->storage.user, slot, text);
}

static void store_session(const online_meta_entry *entry) {
    char text[512];
    session_text(entry, text, sizeof text);
    store_to(entry, entry->slot, text);
}

void online_meta_entry_slot(const char *meta_url, const char *home, char out[ONLINE_META_ENTRY_SLOT_MAX]) {
    size_t length = meta_url != NULL ? strlen(meta_url) : 0U;
    while (length > 0U && meta_url[length - 1U] == '/') { --length; }
    size_t home_length = home != NULL ? strlen(home) : 0U;
    while (home_length > 0U && home[home_length - 1U] == '/') { --home_length; }
    if (home_length == length && (length == 0U || memcmp(meta_url, home, length) == 0)) {
        (void)snprintf(out, ONLINE_META_ENTRY_SLOT_MAX, "%s", ONLINE_META_ENTRY_SLOT);
        return;
    }
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < length; ++index) {
        hash ^= (uint8_t)meta_url[index];
        hash *= 16777619U;
    }
    (void)snprintf(out, ONLINE_META_ENTRY_SLOT_MAX, "%s_%08x", ONLINE_META_ENTRY_SLOT, (unsigned)hash);
}

void online_meta_entry_init(online_meta_entry *entry, const char *meta_url, bool foreign, const online_meta_entry_http *http,
    const online_meta_entry_storage *storage, const char *slot) {
    memset(entry, 0, sizeof *entry);
    entry->http = *http;
    if (storage != NULL) { entry->storage = *storage; }
    (void)snprintf(entry->slot, sizeof entry->slot, "%s", slot != NULL ? slot : ONLINE_META_ENTRY_SLOT);
    (void)snprintf(entry->previous, sizeof entry->previous, "%s%s", entry->slot, ONLINE_META_ENTRY_PREVIOUS_SUFFIX);
    if (meta_url == NULL || meta_url[0] == '\0' || !copy_bounded(entry->meta_url, sizeof entry->meta_url, meta_url)) {
        entry->meta_url[0] = '\0';
        return;
    }
    size_t length = strlen(entry->meta_url);
    while (length > 0U && entry->meta_url[length - 1U] == '/') { entry->meta_url[--length] = '\0'; }
    entry->foreign = foreign;
    load_session(entry);
}

bool online_meta_entry_enabled(const online_meta_entry *entry) { return entry->meta_url[0] != '\0'; }

static void fail(online_meta_entry *entry, const char *why) {
    entry->failure = why;
    entry->state = ONLINE_META_ENTRY_FAILED;
    entry->step = ONLINE_META_ENTRY_STEP_NONE;
    fprintf(stderr, "meta_entry: %s (status %u)\n", why, (unsigned)entry->status);
}

static void start_step(online_meta_entry *entry, online_meta_entry_step step) {
    if (entry->hops >= MAX_HOPS) {
        fail(entry, "too many steps");
        return;
    }
    char url[ONLINE_META_ENTRY_URL_MAX + 32U];
    (void)snprintf(url, sizeof url, "%s%s", entry->meta_url, step == ONLINE_META_ENTRY_STEP_GUEST ? "/api/guest" : "/api/enter");
    entry->step = step;
    entry->hops += 1U;
    entry->state = ONLINE_META_ENTRY_BUSY;
    const char *json = step == ONLINE_META_ENTRY_STEP_ENTER && entry->page_entry ? "{\"page_entry\":true}" : NULL;
    entry->request = entry->http.begin(entry->http.user, url, step == ONLINE_META_ENTRY_STEP_ENTER ? entry->secret : NULL, json,
        ONLINE_META_ENTRY_TIMEOUT_MS);
    if (entry->request == 0U) { fail(entry, "request not sent"); }
}

void online_meta_entry_cancel(online_meta_entry *entry) {
    if (entry->request != 0U && entry->http.cancel != NULL) { entry->http.cancel(entry->http.user, entry->request); }
    entry->request = 0U;
    entry->step = ONLINE_META_ENTRY_STEP_NONE;
    entry->state = ONLINE_META_ENTRY_IDLE;
}

void online_meta_entry_begin(online_meta_entry *entry) {
    online_meta_entry_cancel(entry);
    if (!online_meta_entry_enabled(entry)) {
        fail(entry, "no metaserver");
        return;
    }
    entry->hops = 0U;
    entry->status = 0U;
    entry->failure = NULL;
    entry->ticket_size = 0U;
    start_step(entry, entry->has_session ? ONLINE_META_ENTRY_STEP_ENTER : ONLINE_META_ENTRY_STEP_GUEST);
}

// base64url without padding, as the metaserver sends the ticket.
static size_t decode_base64url(const char *text, uint8_t *out, size_t capacity) {
    size_t bits = 0U;
    uint32_t accumulator = 0U;
    size_t written = 0U;
    for (const char *at = text; *at != '\0'; ++at) {
        uint32_t value;
        if (*at >= 'A' && *at <= 'Z') {
            value = (uint32_t)(*at - 'A');
        } else if (*at >= 'a' && *at <= 'z') {
            value = (uint32_t)(*at - 'a') + 26U;
        } else if (*at >= '0' && *at <= '9') {
            value = (uint32_t)(*at - '0') + 52U;
        } else if (*at == '-') {
            value = 62U;
        } else if (*at == '_') {
            value = 63U;
        } else {
            return 0U;
        }
        accumulator = (accumulator << 6U) | value;
        bits += 6U;
        if (bits >= 8U) {
            bits -= 8U;
            if (written == capacity) { return 0U; }
            out[written++] = (uint8_t)(accumulator >> bits);
            accumulator &= (1U << bits) - 1U;
        }
    }
    return written;
}

static void set_login(online_meta_entry *entry, const char *login) {
    if (login == NULL || !copy_bounded(entry->login, sizeof entry->login, login)) { entry->login[0] = '\0'; }
}

static void finish_guest(online_meta_entry *entry, const cJSON *body) {
    const char *secret = string_field(body, "secret");
    uint64_t player_id = 0U;
    if (entry->status != 201U || !online_meta_entry_json_id(cJSON_GetObjectItemCaseSensitive(body, "player_id"), &player_id) || secret == NULL ||
        !secret_charset(secret) || !copy_bounded(entry->secret, sizeof entry->secret, secret)) {
        fail(entry, entry->status == 429U ? "too many calls" : "guest refused");
        return;
    }
    entry->player_id = player_id;
    entry->has_session = true;
    entry->account = false;
    store_session(entry);
    start_step(entry, ONLINE_META_ENTRY_STEP_ENTER);
}

static void finish_enter(online_meta_entry *entry, const cJSON *body) {
    if (entry->status == 401U) {
        const char *error = string_field(body, "error");
        if (error == NULL || strcmp(error, "unknown_session") != 0) {
            // Not the metaserver saying it forgot the session: a proxy, a
            // deploy half up. The session stays, and the try fails.
            fail(entry, "enter refused the session");
            return;
        }
        // The metaserver no longer knows this session (revoked, logged out,
        // gone): a fresh guest is the only way on, and an account's player
        // can log in again. The old record waits aside until the guest is in.
        if (entry->has_session && !entry->starting_over) {
            char text[512];
            session_text(entry, text, sizeof text);
            store_to(entry, entry->previous, text);
            entry->starting_over = true;
        }
        entry->signed_out = entry->account;
        entry->has_session = false;
        entry->account = false;
        entry->login[0] = '\0';
        start_step(entry, ONLINE_META_ENTRY_STEP_GUEST);
        return;
    }
    const char *url = string_field(body, "url");
    const char *ticket = string_field(body, "ticket");
    uint64_t room_id = 0U;
    uint64_t expires = 0U;
    if (entry->status != 200U || url == NULL || ticket == NULL || (strncmp(url, "ws://", 5U) != 0 && strncmp(url, "wss://", 6U) != 0) ||
        !copy_bounded(entry->room_url, sizeof entry->room_url, url) || !online_meta_entry_json_id(cJSON_GetObjectItemCaseSensitive(body, "room_id"), &room_id) ||
        room_id > UINT32_MAX) {
        fail(entry, entry->status == 429U ? "too many calls" : entry->status == 503U ? "no room has a seat" : "entry refused");
        return;
    }
    entry->ticket_size = decode_base64url(ticket, entry->ticket, sizeof entry->ticket);
    if (entry->ticket_size != ONLINE_META_ENTRY_TICKET_SIZE || !online_meta_entry_json_id(cJSON_GetObjectItemCaseSensitive(body, "expires_at"), &expires)) {
        entry->ticket_size = 0U;
        fail(entry, "ticket malformed");
        return;
    }
    entry->room_id = (uint32_t)room_id;
    entry->ticket_expires_at = expires;
    set_login(entry, string_field(body, "login"));
    if (entry->login[0] != '\0' && !entry->account) {
        // A login attached elsewhere: from now on a refusal is a sign-out.
        entry->account = true;
        store_session(entry);
    }
    if (entry->starting_over) {
        store_to(entry, entry->previous, "{}");
        entry->starting_over = false;
    }
    entry->state = ONLINE_META_ENTRY_READY;
    entry->step = ONLINE_META_ENTRY_STEP_NONE;
}

void online_meta_entry_feed(online_meta_entry *entry, uint16_t status, const uint8_t *body, size_t size) {
    if (entry->state != ONLINE_META_ENTRY_BUSY) { return; }
    entry->request = 0U;
    entry->status = status;
    if (status == 0U) {
        fail(entry, "metaserver unreachable");
        return;
    }
    cJSON *parsed = size == 0U ? cJSON_CreateObject() : cJSON_ParseWithLength((const char *)body, size);
    if (parsed == NULL) {
        fail(entry, "answer is not json");
        return;
    }
    if (entry->step == ONLINE_META_ENTRY_STEP_GUEST) {
        finish_guest(entry, parsed);
    } else {
        finish_enter(entry, parsed);
    }
    cJSON_Delete(parsed);
}

bool online_meta_entry_set_identity(online_meta_entry *entry, uint64_t player_id, const char *secret, const char *login) {
    if (!secret_charset(secret) || !copy_bounded(entry->secret, sizeof entry->secret, secret)) { return false; }
    entry->player_id = player_id;
    entry->has_session = true;
    entry->signed_out = false;
    set_login(entry, login);
    entry->account = entry->login[0] != '\0';
    store_session(entry);
    return true;
}

void online_meta_entry_note_login(online_meta_entry *entry, const char *login) {
    set_login(entry, login);
    entry->account = entry->login[0] != '\0';
    store_session(entry);
}

void online_meta_entry_forget(online_meta_entry *entry) {
    entry->has_session = false;
    entry->account = false;
    entry->signed_out = false;
    entry->login[0] = '\0';
    memset(entry->secret, 0, sizeof entry->secret);
    store_to(entry, entry->slot, "{}");
}

online_meta_entry_state online_meta_entry_update(online_meta_entry *entry) {
    if (entry->state != ONLINE_META_ENTRY_BUSY || entry->request == 0U) { return entry->state; }
    uint16_t status = 0U;
    uint8_t *body = NULL;
    size_t size = 0U;
    if (!entry->http.poll(entry->http.user, entry->request, &status, &body, &size)) { return entry->state; }
    entry->request = 0U;
    online_meta_entry_feed(entry, status, body, size);
    free(body);
    return entry->state;
}
