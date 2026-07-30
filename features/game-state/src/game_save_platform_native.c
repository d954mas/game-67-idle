#include "game_save_platform.h"

#include "game_save.h"
#include "time/nt_time.h"

#include <time.h>

int64_t game_save_platform_mono_ms(void) {
    return (int64_t)(nt_time_now() * 1000.0);
}

int64_t game_save_platform_wall_ms(void) {
    return (int64_t)time(NULL) * 1000;
}

void game_save_install_web_flush(void) {}
