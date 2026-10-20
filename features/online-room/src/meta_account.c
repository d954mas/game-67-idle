#include "online_room/meta_account.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

static const char *const k_paths[] = {NULL, "/api/register", "/api/login", "/api/password", "/api/logout"};

void online_meta_account_init(online_meta_account *account, online_meta_entry *entry) {
    memset(account, 0, sizeof *account);
    account->entry = entry;
}

static void wipe_strings(cJSON *body) {
    for (cJSON *item = body->child; item != NULL; item = item->next) {
        if (cJSON_IsString(item) && item->valuestring != NULL) { memset(item->valuestring, 0, strlen(item->valuestring)); }
    }
}

// Builds the body from up to two string fields and sends. The JSON holds a
// password, so every copy the client owns is wiped before it is freed.
static bool begin(online_meta_account *account, online_account_call call, bool needs_session, const char *name_a, const char *value_a,
    const char *name_b, const char *value_b) {
    online_meta_entry *entry = account->entry;
    if (account->state == ONLINE_ACCOUNT_BUSY || entry == NULL || !online_meta_entry_enabled(entry)) { return false; }
    if (needs_session && !entry->has_session) { return false; }
    char *body = NULL;
    if (name_a != NULL) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, name_a, value_a);
        cJSON_AddStringToObject(root, name_b, value_b);
        body = cJSON_PrintUnformatted(root);
        wipe_strings(root);
        cJSON_Delete(root);
        if (body == NULL) { return false; }
    }
    char url[ONLINE_META_ENTRY_URL_MAX + 32U];
    (void)snprintf(url, sizeof url, "%s%s", entry->meta_url, k_paths[call]);
    account->call = call;
    account->state = ONLINE_ACCOUNT_BUSY;
    account->status = 0U;
    account->error[0] = '\0';
    account->forgot = false;
    account->sent_player = entry->player_id;
    account->sent_session = entry->has_session;
    account->sent_account = entry->account;
    account->request = entry->http.begin(entry->http.user, url, entry->has_session ? entry->secret : NULL, body, ONLINE_ACCOUNT_TIMEOUT_MS);
    if (body != NULL) {
        memset(body, 0, strlen(body));
        free(body);
    }
    if (account->request == 0U) {
        account->state = ONLINE_ACCOUNT_FAILED;
        return false;
    }
    return true;
}

bool online_meta_account_register(online_meta_account *account, const char *login, const char *password) {
    return begin(account, ONLINE_ACCOUNT_CALL_REGISTER, true, "login", login, "password", password);
}

// The bearer is the session this device leaves behind; the metaserver deletes
// it once the login is accepted.
bool online_meta_account_login(online_meta_account *account, const char *login, const char *password) {
    return begin(account, ONLINE_ACCOUNT_CALL_LOGIN, false, "login", login, "password", password);
}

bool online_meta_account_change_password(online_meta_account *account, const char *old_password, const char *new_password) {
    return begin(account, ONLINE_ACCOUNT_CALL_PASSWORD, true, "old", old_password, "new", new_password);
}

bool online_meta_account_logout(online_meta_account *account) {
    return begin(account, ONLINE_ACCOUNT_CALL_LOGOUT, true, NULL, NULL, NULL, NULL);
}

static const char *string_field(const cJSON *body, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(body, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

// The entry still holds the session the call was sent for: the ladder may
// have entered as a fresh guest while the call was out.
static bool sent_session_held(const online_meta_account *account) {
    const online_meta_entry *entry = account->entry;
    return account->sent_session && entry->has_session && entry->player_id == account->sent_player;
}

static void forget_sent_session(online_meta_account *account, bool signed_out) {
    if (!sent_session_held(account)) { return; }
    online_meta_entry_forget(account->entry);
    account->entry->signed_out = signed_out;
    account->forgot = true;
}

void online_meta_account_feed(online_meta_account *account, uint16_t status, const uint8_t *body, size_t size) {
    if (account->state != ONLINE_ACCOUNT_BUSY) { return; }
    account->request = 0U;
    account->status = status;
    cJSON *parsed = size == 0U ? cJSON_CreateObject() : cJSON_ParseWithLength((const char *)body, size);
    const char *error = parsed != NULL ? string_field(parsed, "error") : NULL;
    if (error != NULL) { (void)snprintf(account->error, sizeof account->error, "%s", error); }
    const bool ok = parsed != NULL && (status == 200U || status == 204U);
    account->state = ok ? ONLINE_ACCOUNT_DONE : ONLINE_ACCOUNT_FAILED;
    if (ok) {
        online_meta_entry *entry = account->entry;
        const char *login = string_field(parsed, "login");
        const char *secret = string_field(parsed, "secret");
        uint64_t player_id = 0U;
        switch (account->call) {
        case ONLINE_ACCOUNT_CALL_LOGIN:
            if (!(online_meta_entry_json_id(cJSON_GetObjectItemCaseSensitive(parsed, "player_id"), &player_id) && secret != NULL &&
                    online_meta_entry_set_identity(entry, player_id, secret, login))) {
                account->state = ONLINE_ACCOUNT_FAILED;
            }
            break;
        case ONLINE_ACCOUNT_CALL_REGISTER:
            if (login != NULL && sent_session_held(account)) { online_meta_entry_note_login(entry, login); }
            break;
        case ONLINE_ACCOUNT_CALL_LOGOUT: forget_sent_session(account, false); break;
        default: break;
        }
    } else if (status == 401U || account->call == ONLINE_ACCOUNT_CALL_LOGOUT) {
        // The metaserver no longer takes the session, or a logout nobody heard:
        // this device forgets it all the same. Only an account's is a sign-out.
        forget_sent_session(account, account->call != ONLINE_ACCOUNT_CALL_LOGOUT && account->sent_account);
    }
    if (account->state == ONLINE_ACCOUNT_FAILED) {
        fprintf(stderr, "account: call %d refused (status %u %s)\n", (int)account->call, (unsigned)status, account->error);
    }
    if (parsed != NULL) {
        wipe_strings(parsed);
        cJSON_Delete(parsed);
    }
}

online_account_state online_meta_account_update(online_meta_account *account) {
    if (account->state != ONLINE_ACCOUNT_BUSY || account->request == 0U) { return account->state; }
    const online_meta_entry_http *http = &account->entry->http;
    uint16_t status = 0U;
    uint8_t *body = NULL;
    size_t size = 0U;
    if (!http->poll(http->user, account->request, &status, &body, &size)) { return account->state; }
    account->request = 0U;
    online_meta_account_feed(account, status, body, size);
    if (body != NULL) { memset(body, 0, size); }
    free(body);
    return account->state;
}
