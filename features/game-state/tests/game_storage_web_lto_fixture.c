#include "game_storage_web.h"

#include <stddef.h>

int main(void) {
    char reason[GAME_STORAGE_WEB_REASON_MAX] = {0};
    int status = 0;
    int result = game_storage_web_key_exists("slot");
    result ^= game_storage_web_save("slot", "{}", reason, (int)sizeof reason);
    result ^= game_storage_web_load(
                  "slot", 1024u, &status, reason, (int)sizeof reason) != NULL;
    result ^= game_storage_web_quarantine("slot", reason, (int)sizeof reason);
    result ^= game_storage_web_probe("slot", reason, (int)sizeof reason);
    return result;
}
