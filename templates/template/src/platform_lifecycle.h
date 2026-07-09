#ifndef GAME_PLATFORM_LIFECYCLE_H
#define GAME_PLATFORM_LIFECYCLE_H

#include <stdbool.h>

void platform_lifecycle_init(void);
void platform_lifecycle_after_input_poll(void);
void platform_lifecycle_mark_gameplay_input(void);
void platform_lifecycle_update(bool playable_shell_ready, bool gameplay_allowed);
void platform_lifecycle_shutdown(void);

#endif /* GAME_PLATFORM_LIFECYCLE_H */
