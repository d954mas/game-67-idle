#ifndef GAME_SAVE_TEXT_H
#define GAME_SAVE_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *data;
    size_t capacity;
    size_t used;
    bool failed;
    bool preamble_written;
} game_save_text_writer_t;

void game_save_text_writer_init(game_save_text_writer_t *writer, char *data, size_t capacity);
bool game_save_text_writer_ok(const game_save_text_writer_t *writer);
size_t game_save_text_writer_size(const game_save_text_writer_t *writer);
const char *game_save_text_writer_data(const game_save_text_writer_t *writer);
bool game_save_text_write_preamble(game_save_text_writer_t *writer);
bool game_save_text_begin_fragment(game_save_text_writer_t *writer, const char *id, int version);
bool game_save_text_write_i64(game_save_text_writer_t *writer, const char *key, int64_t value);
bool game_save_text_write_number(game_save_text_writer_t *writer, const char *key, double value);
bool game_save_text_write_bool(game_save_text_writer_t *writer, const char *key, bool value);
bool game_save_text_write_null(game_save_text_writer_t *writer, const char *key);
bool game_save_text_write_string(game_save_text_writer_t *writer, const char *key, const char *value);

typedef enum {
    GAME_SAVE_TEXT_ERROR = -1,
    GAME_SAVE_TEXT_DONE = 0,
    GAME_SAVE_TEXT_RECORD_META = 1,
    GAME_SAVE_TEXT_RECORD_FRAGMENT = 2,
    GAME_SAVE_TEXT_RECORD_FIELD = 3,
} game_save_text_result_t;

typedef struct {
    const char *key;
    size_t key_size;
    const char *value;
    size_t value_size;
    size_t source_offset;
    size_t next_offset;
    uint32_t line;
    int version;
} game_save_text_record_t;

typedef struct {
    const char *data;
    size_t size;
    size_t offset;
    uint32_t line;
    bool preamble_read;
    bool in_fragment;
    bool failed;
} game_save_text_reader_t;

void game_save_text_reader_init(game_save_text_reader_t *reader, const char *data, size_t size);
void game_save_text_fragment_reader_init(
    game_save_text_reader_t *reader, const char *data, size_t size, uint32_t first_line);
game_save_text_result_t game_save_text_reader_next(
    game_save_text_reader_t *reader, game_save_text_record_t *record,
    char *error, size_t error_capacity);
bool game_save_text_record_key_is(const game_save_text_record_t *record, const char *key);
bool game_save_text_record_i64(
    const game_save_text_record_t *record, int64_t min_value, int64_t max_value,
    int64_t *out, char *error, size_t error_capacity);
bool game_save_text_record_number(
    const game_save_text_record_t *record, double min_value, double max_value,
    double *out, char *error, size_t error_capacity);
bool game_save_text_record_bool(
    const game_save_text_record_t *record, bool *out,
    char *error, size_t error_capacity);
bool game_save_text_record_is_null(const game_save_text_record_t *record);
bool game_save_text_record_string(
    const game_save_text_record_t *record, char *out, size_t out_capacity,
    char *error, size_t error_capacity);

#endif
