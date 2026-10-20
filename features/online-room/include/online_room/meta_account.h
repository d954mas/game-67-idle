#ifndef ONLINE_ROOM_META_ACCOUNT_H
#define ONLINE_ROOM_META_ACCOUNT_H

// The account calls to the metaserver: register, log in, change the password,
// log out. One call at a time, pumped per frame over the entry's own HTTP, and
// apart from the entry's ticket machine so a login sent during a connect
// cancels nothing. A login or logout that lands changes the entry's session
// (online_room/meta_entry.h); the host then enters the room again as that player.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "online_room/meta_entry.h"

// Limits shared with the metaserver, in bytes.
#define ONLINE_ACCOUNT_LOGIN_MAX 20U
#define ONLINE_ACCOUNT_PASSWORD_MIN 8U
#define ONLINE_ACCOUNT_PASSWORD_MAX 72U
// The metaserver hashes a password behind a queue, so an account call waits
// longer than an entry step.
#define ONLINE_ACCOUNT_TIMEOUT_MS 15000U

typedef enum online_account_call {
    ONLINE_ACCOUNT_CALL_NONE,
    ONLINE_ACCOUNT_CALL_REGISTER,
    ONLINE_ACCOUNT_CALL_LOGIN,
    ONLINE_ACCOUNT_CALL_PASSWORD,
    ONLINE_ACCOUNT_CALL_LOGOUT,
} online_account_call;

typedef enum online_account_state {
    ONLINE_ACCOUNT_IDLE,
    ONLINE_ACCOUNT_BUSY,
    ONLINE_ACCOUNT_DONE,   // the call landed; the entry already reflects it
    ONLINE_ACCOUNT_FAILED, // status and error say why; status 0: no answer
} online_account_state;

typedef struct online_meta_account {
    online_meta_entry *entry;
    online_account_call call;
    online_account_state state;
    uint16_t status;
    char error[16]; // the metaserver's token: rules, wrong, taken, attempts, ...
    uint32_t request;
    bool forgot; // the answer ended the session the call was sent for
    // The session the call went out with; an answer never touches another.
    uint64_t sent_player;
    bool sent_session, sent_account;
} online_meta_account;

void online_meta_account_init(online_meta_account *account, online_meta_entry *entry);
// Each begins a call; false when nothing was sent (a call is out, no
// metaserver, or no session to act for). Passwords are used for the body and
// not kept.
bool online_meta_account_register(online_meta_account *account, const char *login, const char *password);
bool online_meta_account_login(online_meta_account *account, const char *login, const char *password);
bool online_meta_account_change_password(online_meta_account *account, const char *old_password, const char *new_password);
bool online_meta_account_logout(online_meta_account *account);
// Hears the call out; the state stays DONE or FAILED until the next call.
online_account_state online_meta_account_update(online_meta_account *account);
// One answer for the call out: `status` 0 for no answer.
void online_meta_account_feed(online_meta_account *account, uint16_t status, const uint8_t *body, size_t size);

#endif
