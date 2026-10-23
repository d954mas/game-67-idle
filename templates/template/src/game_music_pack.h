#ifndef GAME_MUSIC_PACK_H
#define GAME_MUSIC_PACK_H

#include <stdbool.h>
#include "resource/nt_resource.h"

void game_music_pack_init(void);
void game_music_pack_after_present(void);
void game_music_pack_update(bool wanted);
nt_pack_state_t game_music_pack_state(void);
void game_music_pack_shutdown(void);

#endif
