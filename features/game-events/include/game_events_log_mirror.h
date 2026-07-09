#ifndef GAME_EVENTS_LOG_MIRROR_H
#define GAME_EVENTS_LOG_MIRROR_H

#ifndef GAME_EVENTS_LOG_MIRROR
#define GAME_EVENTS_LOG_MIRROR 0
#endif

#if GAME_EVENTS_LOG_MIRROR

#include "game_event_desc.h"

void game_events_log_mirror_register_descs(const game_event_desc_t *const *descs, int count);
void game_events_log_mirror_record(void);

#endif /* GAME_EVENTS_LOG_MIRROR */
#endif /* GAME_EVENTS_LOG_MIRROR_H */
