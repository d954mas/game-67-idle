#ifndef GAME_SAVE_WRITER_H
#define GAME_SAVE_WRITER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    GAME_SAVE_WRITER_OBJECT = 1,
    GAME_SAVE_WRITER_ARRAY = 2,
} game_save_writer_scope_t;

typedef struct {
    char *data;
    size_t capacity;
    size_t used;
    unsigned char depth;
    bool failed;
    bool root_written;
    bool needs_comma[32];
    bool awaiting_value[32];
    game_save_writer_scope_t scopes[32];
} game_save_writer_t;

void game_save_writer_init(game_save_writer_t *writer, char *data, size_t capacity);
bool game_save_writer_ok(const game_save_writer_t *writer);
/* True only after exactly one complete root value has been written. */
bool game_save_writer_complete(const game_save_writer_t *writer);
size_t game_save_writer_size(const game_save_writer_t *writer);
const char *game_save_writer_data(const game_save_writer_t *writer);
bool game_save_writer_begin_object(game_save_writer_t *writer);
bool game_save_writer_end_object(game_save_writer_t *writer);
bool game_save_writer_begin_array(game_save_writer_t *writer);
bool game_save_writer_end_array(game_save_writer_t *writer);
bool game_save_writer_key(game_save_writer_t *writer, const char *key);
bool game_save_writer_string(game_save_writer_t *writer, const char *value);
bool game_save_writer_number(game_save_writer_t *writer, double value);
bool game_save_writer_bool(game_save_writer_t *writer, bool value);
bool game_save_writer_null(game_save_writer_t *writer);
/* `json` must name a non-empty, already-valid JSON value with no embedded NUL. */
bool game_save_writer_raw_value(game_save_writer_t *writer, const char *json, size_t size);

#endif
