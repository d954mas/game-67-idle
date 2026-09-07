#include "features/leaderboard/leaderboard_internal.h"

#include "base64/nt_base64.h"

#include <cJSON.h>
#include <stdio.h>
#include <string.h>

/* Pure half of the leaderboard client: everything here is a function of its
 * arguments, so the core tests cover it without game state or a network. */

void lb_utc_day(time_t now, char out[LB_DAY_STR_MAX]) {
    /* civil_from_days (H. Hinnant): no dependence on the libc's local tables,
     * so native, wasm and tests all agree on the day boundary. */
    int64_t z = (int64_t)now / 86400;
    if ((int64_t)now < 0 && ((int64_t)now % 86400) != 0) {
        z -= 1;
    }
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint64_t doe = (uint64_t)(z - era * 146097);
    const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t y = (int64_t)yoe + era * 400;
    const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint64_t mp = (5 * doy + 2) / 153;
    const uint64_t d = doy - (153 * mp + 2) / 5 + 1;
    const uint64_t m = mp < 10 ? mp + 3 : mp - 9;
    snprintf(out, LB_DAY_STR_MAX, "%04d%02u%02u", (int)(y + (m <= 2)), (unsigned)m, (unsigned)d);
}

int64_t lb_day_number(const char *day) {
    if (day == NULL || strlen(day) != 8) {
        return -1;
    }
    for (int i = 0; i < 8; i++) {
        if (day[i] < '0' || day[i] > '9') {
            return -1;
        }
    }
    const int y = (day[0] - '0') * 1000 + (day[1] - '0') * 100 + (day[2] - '0') * 10 + (day[3] - '0');
    const int m = (day[4] - '0') * 10 + (day[5] - '0');
    const int d = (day[6] - '0') * 10 + (day[7] - '0');
    if (m < 1 || m > 12 || d < 1 || d > 31) {
        return -1;
    }
    /* days_from_civil (H. Hinnant), the exact inverse of lb_utc_day */
    const int64_t yy = y - (m <= 2);
    const int64_t era = (yy >= 0 ? yy : yy - 399) / 400;
    const uint64_t yoe = (uint64_t)(yy - era * 400);
    const uint64_t doy = (uint64_t)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const uint64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

uint32_t lb_encode_payload(const char *json, const char *key, char *out, uint32_t out_cap) {
    uint8_t buf[512];
    const uint32_t len = (uint32_t)strlen(json);
    const uint32_t key_len = (uint32_t)strlen(key);
    if (len > sizeof buf || key_len == 0) {
        return 0;
    }
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(json[i] ^ key[i % key_len]);
    }
    uint32_t n = nt_base64_encode(buf, len, out, out_cap ? out_cap - 1 : 0);
    if (n == 0) {
        return 0;
    }
    /* base64url without padding: the payload travels in a query parameter */
    uint32_t w = 0;
    for (uint32_t i = 0; i < n; i++) {
        char c = out[i];
        if (c == '+') {
            c = '-';
        } else if (c == '/') {
            c = '_';
        } else if (c == '=') {
            continue;
        }
        out[w++] = c;
    }
    out[w] = '\0';
    return w;
}

/* Every field the pack does not interpret rides along as an opaque pair; a
 * pair that does not fit the budget is dropped rather than failing the row. */
static void pack_row_extra(const cJSON *row, char extra[LEADERBOARD_EXTRA_MAX]) {
    extra[0] = '\0';
    const cJSON *field;
    cJSON_ArrayForEach(field, row) {
        if (field->string == NULL || strcmp(field->string, "user_id") == 0 ||
            strcmp(field->string, "value") == 0) {
            continue;
        }
        if (cJSON_IsString(field)) {
            (void)leaderboard_extra_set(extra, LEADERBOARD_EXTRA_MAX, field->string, field->valuestring);
        } else if (cJSON_IsNumber(field)) {
            char text[32];
            const long long whole = (long long)field->valuedouble;
            if ((double)whole == field->valuedouble) {
                snprintf(text, sizeof text, "%lld", whole);
            } else {
                snprintf(text, sizeof text, "%g", field->valuedouble);
            }
            (void)leaderboard_extra_set(extra, LEADERBOARD_EXTRA_MAX, field->string, text);
        }
    }
}

static bool parse_board(const cJSON *node, lb_board_t *out) {
    memset(out, 0, sizeof *out);
    out->user_place = -1;
    if (!cJSON_IsObject(node)) {
        return false;
    }
    const cJSON *place = cJSON_GetObjectItemCaseSensitive(node, "userPlace");
    if (cJSON_IsNumber(place) && place->valuedouble >= 1) {
        out->user_place = (int)place->valuedouble;
    }
    const cJSON *top = cJSON_GetObjectItemCaseSensitive(node, "top");
    if (cJSON_IsArray(top)) {
        const cJSON *row;
        cJSON_ArrayForEach(row, top) {
            if (out->top_count >= LB_TOP_MAX - 1) {
                break;
            }
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(row, "user_id");
            const cJSON *value = cJSON_GetObjectItemCaseSensitive(row, "value");
            if (!cJSON_IsString(id) || !cJSON_IsNumber(value) || value->valuedouble < 0) {
                continue;
            }
            lb_top_entry_t *e = &out->top[out->top_count++];
            snprintf(e->user_id, sizeof e->user_id, "%s", id->valuestring);
            e->value = (uint32_t)value->valuedouble;
            pack_row_extra(row, e->extra);
        }
    }
    const cJSON *interp = cJSON_GetObjectItemCaseSensitive(node, "interpolation");
    if (cJSON_IsArray(interp)) {
        const cJSON *row;
        cJSON_ArrayForEach(row, interp) {
            if (out->interp_count >= LB_INTERP_MAX) {
                break;
            }
            const cJSON *value = cJSON_GetObjectItemCaseSensitive(row, "value");
            const cJSON *count = cJSON_GetObjectItemCaseSensitive(row, "count");
            if (!cJSON_IsNumber(value) || !cJSON_IsNumber(count)) {
                continue;
            }
            lb_interp_entry_t *e = &out->interp[out->interp_count++];
            e->value = (uint32_t)(value->valuedouble < 0 ? 0 : value->valuedouble);
            e->count = (uint32_t)(count->valuedouble < 0 ? 0 : count->valuedouble);
        }
    }
    return true;
}

bool lb_parse_response(const char *json, uint32_t len, const char *root_key, lb_response_t *out) {
    memset(out, 0, sizeof *out);
    if (root_key == NULL) {
        return false;
    }
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (root == NULL) {
        return false;
    }
    bool ok = false;
    const cJSON *board = cJSON_GetObjectItemCaseSensitive(root, root_key);
    if (cJSON_IsObject(board)) {
        const cJSON *all = cJSON_GetObjectItemCaseSensitive(board, "all");
        const cJSON *day = cJSON_GetObjectItemCaseSensitive(board, "day");
        const cJSON *day_str = cJSON_GetObjectItemCaseSensitive(day, "day");
        if (parse_board(all, &out->all) && parse_board(day, &out->day) &&
            cJSON_IsString(day_str) && lb_day_number(day_str->valuestring) >= 0) {
            snprintf(out->day_str, sizeof out->day_str, "%s", day_str->valuestring);
            out->valid = true;
            ok = true;
        }
    }
    cJSON_Delete(root);
    return ok;
}

static int approx_place(const lb_board_t *board, uint32_t value) {
    for (int i = 0; i < board->interp_count; i++) {
        if (value <= board->interp[i].value) {
            const lb_interp_entry_t upper = board->interp[i];
            const lb_interp_entry_t lower =
                i > 0 ? board->interp[i - 1]
                      : (lb_interp_entry_t){.value = 0, .count = upper.count + 100};
            const int64_t range_value = (int64_t)upper.value - (int64_t)lower.value;
            const int64_t range_count = (int64_t)lower.count - (int64_t)upper.count;
            if (range_value <= 0) {
                return (int)upper.count + 1;
            }
            const double t = (double)(value - lower.value) / (double)range_value;
            int place = (int)((double)lower.count - t * (double)range_count) + 1;
            return place < 1 ? 1 : place;
        }
    }
    return 1;
}

int64_t lb_seconds_until_day_end(time_t now) {
    const int64_t t = (int64_t)now;
    const int64_t into_day = ((t % 86400) + 86400) % 86400;
    return 86400 - into_day;
}

int lb_recalc_place(lb_board_t *board, const char *user_id, const char *extra, uint32_t value) {
    int self = -1;
    for (int i = 0; i < board->top_count; i++) {
        if (strcmp(board->top[i].user_id, user_id) == 0) {
            self = i;
            break;
        }
    }
    const bool appended = self == -1;
    if (appended) {
        if (board->top_count >= LB_TOP_MAX) {
            return board->user_place; /* full board can only happen through misuse */
        }
        self = board->top_count++;
        snprintf(board->top[self].user_id, LB_USER_ID_MAX, "%s", user_id);
    }
    board->top[self].value = value;
    /* The live payload, not the one the server last saw: what the player just
     * changed shows on the board without waiting for the next poll. */
    snprintf(board->top[self].extra, LEADERBOARD_EXTRA_MAX, "%s", extra != NULL ? extra : "");
    /* insertion sort, desc; ties keep order so the player does not jump above
     * equals on a pure re-render */
    for (int i = 1; i < board->top_count; i++) {
        const lb_top_entry_t e = board->top[i];
        int j = i - 1;
        while (j >= 0 && board->top[j].value < e.value) {
            board->top[j + 1] = board->top[j];
            j--;
        }
        board->top[j + 1] = e;
    }
    int place = -1;
    for (int i = 0; i < board->top_count; i++) {
        if (strcmp(board->top[i].user_id, user_id) == 0) {
            place = i + 1;
            break;
        }
    }
    /* Appended below a full window means "somewhere past it": the real
     * neighbors were cut at the server's top size, so estimate instead. A
     * player the server itself ranked last keeps that exact place. */
    if (appended && place == board->top_count && board->top_count >= LB_TOP_MAX - 1 &&
        board->interp_count > 0) {
        const int est = approx_place(board, value);
        if (est > place) {
            place = est;
        }
    }
    board->user_place = place;
    return place;
}
