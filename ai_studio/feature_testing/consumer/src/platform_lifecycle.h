#ifndef GAME_PLATFORM_LIFECYCLE_H
#define GAME_PLATFORM_LIFECYCLE_H

#include <stdbool.h>

void platform_lifecycle_init(void);
/* Records the single input adapter's per-frame gesture edge. */
bool platform_lifecycle_on_input(bool input_seen);
void platform_lifecycle_mark_gameplay_input(void);
void platform_lifecycle_update(bool playable_shell_ready, bool gameplay_allowed);
void platform_lifecycle_after_frame_present(bool playable_shell_ready);
float platform_lifecycle_loading_progress_from_pack(unsigned int received, unsigned int total, bool pack_ready);
void platform_lifecycle_shutdown(void);

#endif /* GAME_PLATFORM_LIFECYCLE_H */
