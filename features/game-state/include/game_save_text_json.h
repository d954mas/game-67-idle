#ifndef GAME_SAVE_TEXT_JSON_H
#define GAME_SAVE_TEXT_JSON_H

#include "cJSON.h"

/* Reads a stored save document into the JSON shape a save policy inspects:
   header records at the root (saved_at, save_seq and playtime_ms as decimal
   strings), each fragment under "features" with its version in "v", dotted
   field keys as nested objects. Text that is not NTGS is parsed as plain JSON.
   Needs no registered fragments and links without game_state_json.c, so it
   works with the text-only save shell, whose full validator is unavailable.
   Save transforms are not undone. An empty object writes no records and reads
   back absent. Returns NULL on malformed text; the caller owns the result. */
cJSON *game_save_text_json_parse(const char *text, char *error, int error_cap);

#endif /* GAME_SAVE_TEXT_JSON_H */
