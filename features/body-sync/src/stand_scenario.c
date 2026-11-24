#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync_scenario.h"
#include "stand_logic.h"

enum { WORDS_MAX = 40, WORD_MAX = 48 };

typedef struct line_words {
    uint32_t count;
    char word[WORDS_MAX][WORD_MAX];
} line_words_t;

typedef struct parser {
    body_sync_scenario_t *out;
    body_sync_error_t *error;
    uint32_t line;
    bool seen_scenario, seen_scene, seen_seed, seen_guests, seen_settle, seen_end;
    uint32_t guests_line;
    uint32_t rate_line[1 + BODY_SYNC_GUESTS_MAX];
    uint32_t settle_line;
} parser_t;

__attribute__((format(printf, 2, 3))) static bool refuse(parser_t *p, const char *format, ...) {
    if (p->error) {
        p->error->line = p->line;
        va_list args;
        va_start(args, format);
        vsnprintf(p->error->text, sizeof(p->error->text), format, args);
        va_end(args);
    }
    return false;
}

static const char *outcome_names[] = {"pending", "done", "granted", "refused", "hit", "missed", "rejected", "failed"};

const char *body_sync_outcome_name(body_sync_outcome_t outcome) {
    return (unsigned)outcome < sizeof(outcome_names) / sizeof(outcome_names[0]) ? outcome_names[outcome] : "pending";
}

static bool outcome_word(const char *word, body_sync_outcome_t *out) {
    for (uint32_t i = 1; i < sizeof(outcome_names) / sizeof(outcome_names[0]); i++) {
        if (strcmp(word, outcome_names[i]) == 0) {
            *out = (body_sync_outcome_t)i;
            return true;
        }
    }
    return false;
}

/* Words are checked for length before they are copied; this only bounds the copy. */
static void copy_text(char *dst, size_t size, const char *src) {
    size_t n = strlen(src);
    n = n >= size ? size - 1 : n;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool name_word(const char *word, size_t max_length) {
    size_t length = strlen(word);
    if (length == 0 || length > max_length) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        char c = word[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

/* A decimal with optional sign and fraction, then a unit suffix written to `unit`. No exponent,
   no hex, no inf or nan: strtod would take all of those. */
static bool quantity(const char *word, double *value, const char **unit) {
    const char *c = word;
    bool negative = false;
    if (*c == '+' || *c == '-') {
        negative = *c == '-';
        c++;
    }
    double whole = 0.0;
    int digits = 0;
    while (*c >= '0' && *c <= '9') {
        whole = whole * 10.0 + (double)(*c - '0');
        if (++digits > 15) {
            return false;
        }
        c++;
    }
    double fraction = 0.0, scale = 1.0;
    int fraction_digits = 0;
    if (*c == '.') {
        c++;
        while (*c >= '0' && *c <= '9') {
            if (++fraction_digits > 9) {
                return false;
            }
            scale *= 0.1;
            fraction += (double)(*c - '0') * scale;
            c++;
        }
        if (fraction_digits == 0) {
            return false;
        }
    }
    if (digits == 0) {
        return false;
    }
    *value = (negative ? -1.0 : 1.0) * (whole + fraction);
    *unit = c;
    return true;
}

static bool bare_number(const char *word, double *value) {
    const char *unit;
    return quantity(word, value, &unit) && *unit == '\0';
}

/* A <time> in seconds, before it is turned into ticks or milliseconds. */
static bool seconds_word(parser_t *p, const char *word, double *seconds) {
    double v;
    const char *unit;
    if (!quantity(word, &v, &unit)) {
        return refuse(p, "'%s' is not a time (250ms, 1.5s, 90t)", word);
    }
    if (strcmp(unit, "ms") == 0) {
        *seconds = v / 1000.0;
    } else if (strcmp(unit, "s") == 0) {
        *seconds = v;
    } else if (strcmp(unit, "t") == 0) {
        if (v != floor(v)) {
            return refuse(p, "'%s': ticks are whole", word);
        }
        *seconds = v / (double)BODY_SYNC_TICK_HZ;
    } else {
        return refuse(p, "'%s' needs a time unit: ms, s or t", word);
    }
    return true;
}

static bool time_word(parser_t *p, const char *word, uint32_t *ticks) {
    double seconds = 0.0;
    if (!seconds_word(p, word, &seconds)) {
        return false;
    }
    if (!(seconds >= 0.0 && seconds <= (double)BODY_SYNC_TIME_MAX_S)) {
        return refuse(p, "'%s' is outside 0..600 s", word);
    }
    *ticks = (uint32_t)floor(seconds * (double)BODY_SYNC_TICK_HZ + 0.5);
    if (seconds > 0.0 && *ticks == 0) {
        return refuse(p, "'%s' rounds to zero ticks of 1/60 s", word);
    }
    return true;
}

static bool length_word(parser_t *p, const char *word, float *metres) {
    double v;
    const char *unit;
    if (!quantity(word, &v, &unit) || (strcmp(unit, "m") != 0 && strcmp(unit, "cm") != 0)) {
        return refuse(p, "'%s' is not a length (5cm, 0.05m)", word);
    }
    v = strcmp(unit, "cm") == 0 ? v / 100.0 : v;
    if (!(v >= 0.0 && v <= 1000.0)) {
        return refuse(p, "'%s' is outside 0..1000 m", word);
    }
    *metres = (float)v;
    return true;
}

static bool angle_word(parser_t *p, const char *word, float *degrees) {
    double v;
    const char *unit;
    if (!quantity(word, &v, &unit) || strcmp(unit, "deg") != 0 || !(v >= 0.0 && v <= 180.0)) {
        return refuse(p, "'%s' is not an angle of 0..180deg", word);
    }
    *degrees = (float)v;
    return true;
}

static bool share_word(parser_t *p, const char *word, float *share) {
    double v;
    const char *unit;
    if (!quantity(word, &v, &unit) || strcmp(unit, "%") != 0 || !(v >= 0.0 && v <= 100.0)) {
        return refuse(p, "'%s' is not a share of 0..100%%", word);
    }
    *share = (float)(v / 100.0);
    return true;
}

static bool rate_word(parser_t *p, const char *word, double *bytes_per_s) {
    double v;
    const char *unit;
    if (!quantity(word, &v, &unit) || (strcmp(unit, "B/s") != 0 && strcmp(unit, "KB/s") != 0)) {
        return refuse(p, "'%s' is not a rate (64000B/s, 64KB/s)", word);
    }
    v = strcmp(unit, "KB/s") == 0 ? v * 1000.0 : v;
    if (!(v >= 0.0 && v <= 4.0e9)) {
        return refuse(p, "'%s' is outside 0..4e9 B/s", word);
    }
    *bytes_per_s = v;
    return true;
}

static bool position_word(parser_t *p, const char *word, float *value) {
    double v;
    if (!bare_number(word, &v) || !(v >= -1000.0 && v <= 1000.0)) {
        return refuse(p, "'%s' is not a position in metres within 1000", word);
    }
    *value = (float)v;
    return true;
}

static bool player_word(const char *word, uint8_t *player) {
    if (word[0] == 'p' && word[1] >= '0' && word[1] <= '3' && word[2] == '\0') {
        *player = (uint8_t)(word[1] - '0');
        return true;
    }
    return false;
}

static bool guest_word(const char *word, uint8_t *guest) {
    if (word[0] == 'g' && word[1] >= '1' && word[1] <= '3' && word[2] == '\0') {
        *guest = (uint8_t)(word[1] - '0');
        return true;
    }
    return false;
}

static bool endpoint_word(const char *word, uint8_t *endpoint) {
    if (strcmp(word, "host") == 0) {
        *endpoint = BODY_SYNC_ENDPOINT_HOST;
        return true;
    }
    return guest_word(word, endpoint);
}

static bool body_word(parser_t *p, const char *word, body_sync_target_t *target) {
    if (!name_word(word, BODY_SYNC_NAME_MAX - 1)) {
        return refuse(p, "'%s' is not a body name", word);
    }
    target->is_body = true;
    copy_text(target->body, sizeof(target->body), word);
    return true;
}

static bool speed_tail(parser_t *p, const line_words_t *w, uint32_t at, uint32_t end, float *speed) {
    if (at == end) {
        return true;
    }
    double v;
    if (end - at != 2 || strcmp(w->word[at], "speed") != 0) {
        return refuse(p, "unexpected '%s'", w->word[at]);
    }
    if (!bare_number(w->word[at + 1], &v) || !(v > 0.0 && v <= 50.0)) {
        return refuse(p, "'%s' is not a speed in (0, 50] m/s", w->word[at + 1]);
    }
    *speed = (float)v;
    return true;
}

/* `on <pN> contact <body> [+<time>]` or `on <label> [+<time>]`, words at..end after the `on`. */
static bool trigger(parser_t *p, const line_words_t *w, uint32_t at, uint32_t end, body_sync_action_t *a) {
    const body_sync_scenario_t *s = p->out;
    if (end > at && w->word[end - 1][0] == '+') {
        if (!time_word(p, w->word[end - 1] + 1, &a->trigger_ticks)) {
            return false;
        }
        end--;
    }
    uint8_t player;
    if (end - at == 3 && player_word(w->word[at], &player) && strcmp(w->word[at + 1], "contact") == 0) {
        body_sync_target_t touched;
        if (!body_word(p, w->word[at + 2], &touched)) {
            return false;
        }
        a->trigger = BODY_SYNC_TRIGGER_CONTACT;
        a->trigger_player = player;
        copy_text(a->trigger_body, sizeof(a->trigger_body), touched.body);
        return true;
    }
    if (end - at == 1 && !player_word(w->word[at], &player)) {
        if (!name_word(w->word[at], BODY_SYNC_LABEL_MAX - 1)) {
            return refuse(p, "'%s' is not a label ([a-z0-9_], at most 15)", w->word[at]);
        }
        for (uint32_t i = 0; i < s->action_count; i++) {
            if (strcmp(s->actions[i].label, w->word[at]) == 0) {
                a->trigger = BODY_SYNC_TRIGGER_ACTION;
                a->trigger_action = i;
                return true;
            }
        }
        return refuse(p, "no earlier action is labelled '%s'", w->word[at]);
    }
    return refuse(p, "on takes '<player> contact <body> [+<time>]' or '<label> [+<time>]'%s", "");
}

static bool action(parser_t *p, const line_words_t *w, uint32_t first, bool timed, uint32_t tick) {
    body_sync_scenario_t *s = p->out;
    if (s->action_count >= BODY_SYNC_SCENARIO_ACTIONS_MAX) {
        return refuse(p, "more than 512 actions%s", "");
    }
    body_sync_action_t a;
    memset(&a, 0, sizeof(a));
    a.line = p->line;
    a.timed = timed;
    a.tick = tick;
    player_word(w->word[first], &a.player);
    uint32_t end = w->count;
    if (end >= first + 2 && strcmp(w->word[end - 2], "as") == 0) {
        if (!name_word(w->word[end - 1], BODY_SYNC_LABEL_MAX - 1)) {
            return refuse(p, "'%s' is not a label ([a-z0-9_], at most 15)", w->word[end - 1]);
        }
        for (uint32_t i = 0; i < s->action_count; i++) {
            if (strcmp(s->actions[i].label, w->word[end - 1]) == 0) {
                return refuse(p, "label '%s' is used twice", w->word[end - 1]);
            }
        }
        copy_text(a.label, sizeof(a.label), w->word[end - 1]);
        end -= 2;
    }
    for (uint32_t i = first + 2; i < end; i++) {
        if (strcmp(w->word[i], "on") == 0) {
            if (timed) {
                return refuse(p, "an action starts at a time or on an event, not both%s", "");
            }
            if (!trigger(p, w, i + 1, end, &a)) {
                return false;
            }
            end = i;
            break;
        }
    }
    if (first + 1 >= end) {
        return refuse(p, "a player needs a verb%s", "");
    }
    const char *verb = w->word[first + 1];
    uint32_t arg = first + 2;
    double number;
    if (strcmp(verb, "walk") == 0) {
        a.verb = BODY_SYNC_VERB_WALK;
        a.has_target = true;
        if (arg < end && bare_number(w->word[arg], &number)) {
            if (arg + 1 >= end) {
                return refuse(p, "walk needs <x> <z>%s", "");
            }
            if (!position_word(p, w->word[arg], &a.target.point[0]) || !position_word(p, w->word[arg + 1], &a.target.point[2])) {
                return false;
            }
            arg += 2;
        } else if (arg < end) {
            if (!body_word(p, w->word[arg], &a.target)) {
                return false;
            }
            arg += 1;
        } else {
            return refuse(p, "walk needs <x> <z> or a body%s", "");
        }
        if (!speed_tail(p, w, arg, end, &a.speed)) {
            return false;
        }
    } else if (strcmp(verb, "grab") == 0) {
        a.verb = BODY_SYNC_VERB_GRAB;
        a.has_target = true;
        if (arg + 1 != end) {
            return refuse(p, "grab needs exactly one body%s", "");
        }
        if (!body_word(p, w->word[arg], &a.target)) {
            return false;
        }
    } else if (strcmp(verb, "throw") == 0) {
        a.verb = BODY_SYNC_VERB_THROW;
        a.has_target = true;
        if (arg < end && bare_number(w->word[arg], &number)) {
            if (arg + 2 >= end) {
                return refuse(p, "throw needs <x> <y> <z>%s", "");
            }
            for (int i = 0; i < 3; i++) {
                if (!position_word(p, w->word[arg + (uint32_t)i], &a.target.point[i])) {
                    return false;
                }
            }
            arg += 3;
        } else if (arg < end) {
            if (!body_word(p, w->word[arg], &a.target)) {
                return false;
            }
            arg += 1;
        } else {
            return refuse(p, "throw needs a point or a body%s", "");
        }
        if (!speed_tail(p, w, arg, end, &a.speed)) {
            return false;
        }
    } else if (strcmp(verb, "drop") == 0) {
        a.verb = BODY_SYNC_VERB_DROP;
        if (arg != end) {
            return refuse(p, "drop takes nothing, not '%s'", w->word[arg]);
        }
    } else if (strcmp(verb, "punch") == 0) {
        a.verb = BODY_SYNC_VERB_PUNCH;
        if (arg + 1 == end) {
            a.has_target = true;
            if (!body_word(p, w->word[arg], &a.target)) {
                return false;
            }
        } else if (arg != end) {
            return refuse(p, "punch takes at most one body%s", "");
        }
    } else if (strcmp(verb, "wait") == 0) {
        a.verb = BODY_SYNC_VERB_WAIT;
        if (arg + 1 != end) {
            return refuse(p, "wait needs one time%s", "");
        }
        if (!time_word(p, w->word[arg], &a.wait_ticks)) {
            return false;
        }
    } else if (strcmp(verb, "recall") == 0 || strcmp(verb, "remove") == 0) {
        bool recall = strcmp(verb, "recall") == 0;
        a.verb = recall ? BODY_SYNC_VERB_RECALL : BODY_SYNC_VERB_REMOVE;
        a.has_target = true;
        if (a.player != BODY_SYNC_ENDPOINT_HOST) {
            return refuse(p, "%s is the host's rule: only p0 may %s", verb, verb);
        }
        if (arg >= end) {
            return refuse(p, "%s needs a body", verb);
        }
        if (!body_word(p, w->word[arg], &a.target)) {
            return false;
        }
        arg += 1;
        if (recall && arg < end) {
            if (end - arg != 4 || strcmp(w->word[arg], "to") != 0) {
                return refuse(p, "recall takes a body and at most 'to <x> <y> <z>', not '%s'", w->word[arg]);
            }
            for (int i = 0; i < 3; i++) {
                if (!position_word(p, w->word[arg + 1u + (uint32_t)i], &a.home[i])) {
                    return false;
                }
            }
            a.has_home = true;
        } else if (arg != end) {
            return refuse(p, "remove takes exactly one body, not '%s'", w->word[arg]);
        }
    } else {
        return refuse(p, "unknown verb '%s'", verb);
    }
    s->actions[s->action_count++] = a;
    return true;
}

static bool add_net(parser_t *p, const body_sync_net_event_t *event) {
    body_sync_scenario_t *s = p->out;
    if (s->net_count >= BODY_SYNC_SCENARIO_NET_MAX) {
        return refuse(p, "more than 128 network items%s", "");
    }
    /* Kept sorted by tick, file order within a tick: the runner fires them in this order. */
    uint32_t at = s->net_count;
    while (at > 0 && s->net[at - 1].tick > event->tick) {
        s->net[at] = s->net[at - 1];
        at--;
    }
    s->net[at] = *event;
    s->net_count++;
    return true;
}

static bool net(parser_t *p, const line_words_t *w, uint32_t first, uint32_t tick) {
    body_sync_net_event_t e;
    memset(&e, 0, sizeof(e));
    e.line = p->line;
    e.tick = tick;
    e.kind = BODY_SYNC_NET_CONDITIONS;
    e.directions = BODY_SYNC_TO_HOST | BODY_SYNC_TO_GUEST;
    uint32_t i = first + 1;
    if (i >= w->count || !guest_word(w->word[i], &e.endpoint)) {
        return refuse(p, "net needs a guest g1..g3%s", "");
    }
    i++;
    bool directed = false;
    if (i < w->count && (strcmp(w->word[i], "to_host") == 0 || strcmp(w->word[i], "to_guest") == 0)) {
        e.directions = strcmp(w->word[i], "to_host") == 0 ? BODY_SYNC_TO_HOST : BODY_SYNC_TO_GUEST;
        directed = true;
        i++;
    }
    if (i >= w->count) {
        return refuse(p, "net needs at least one field%s", "");
    }
    for (; i < w->count; i += 2) {
        const char *field = w->word[i];
        if (i + 1 >= w->count) {
            return refuse(p, "'%s' needs a value", field);
        }
        const char *value = w->word[i + 1];
        float share = 0.0f;
        double rate = 0.0;
        uint32_t bit = 0;
        if (strcmp(field, "rtt") == 0 || strcmp(field, "delay") == 0 || strcmp(field, "jitter") == 0 || strcmp(field, "reorder_delay") == 0) {
            double seconds = 0.0;
            if (!seconds_word(p, value, &seconds)) {
                return false;
            }
            double ms = seconds * 1000.0;
            if (!(ms >= 0.0 && ms <= 10000.0)) {
                return refuse(p, "'%s' is outside 0..10000 ms", value);
            }
            if (strcmp(field, "rtt") == 0) {
                if (directed) {
                    return refuse(p, "rtt sets both directions; use delay with %s", w->word[first + 2]);
                }
                e.conditions.delay_ms = (float)(ms * 0.5);
                bit = BODY_SYNC_FIELD_DELAY;
            } else if (strcmp(field, "delay") == 0) {
                e.conditions.delay_ms = (float)ms;
                bit = BODY_SYNC_FIELD_DELAY;
            } else if (strcmp(field, "jitter") == 0) {
                e.conditions.jitter_ms = (float)ms;
                bit = BODY_SYNC_FIELD_JITTER;
            } else {
                e.conditions.reorder_ms = (float)ms;
                bit = BODY_SYNC_FIELD_REORDER_DELAY;
            }
        } else if (strcmp(field, "loss") == 0 || strcmp(field, "dup") == 0 || strcmp(field, "reorder") == 0) {
            if (!share_word(p, value, &share)) {
                return false;
            }
            if (strcmp(field, "loss") == 0) {
                e.conditions.loss = share;
                bit = BODY_SYNC_FIELD_LOSS;
            } else if (strcmp(field, "dup") == 0) {
                e.conditions.duplicate = share;
                bit = BODY_SYNC_FIELD_DUPLICATE;
            } else {
                e.conditions.reorder = share;
                bit = BODY_SYNC_FIELD_REORDER;
            }
        } else if (strcmp(field, "rate") == 0) {
            if (strcmp(value, "off") != 0) {
                if (!rate_word(p, value, &rate)) {
                    return false;
                }
                if (rate < 1.0) {
                    return refuse(p, "'%s': a rate is 1 B/s or more; 'rate off' removes the limit", value);
                }
            }
            e.conditions.rate_bytes_per_s = (uint32_t)rate;
            bit = BODY_SYNC_FIELD_RATE;
        } else {
            return refuse(p, "unknown net field '%s'", field);
        }
        if (e.fields & bit) {
            return refuse(p, "'%s' is set twice", field);
        }
        e.fields |= bit;
    }
    return add_net(p, &e);
}

static bool timed_condition(parser_t *p, const line_words_t *w, uint32_t first, uint32_t tick) {
    body_sync_net_event_t e;
    memset(&e, 0, sizeof(e));
    e.line = p->line;
    e.tick = tick;
    const char *kind = w->word[first];
    e.kind = strcmp(kind, "cut") == 0 ? BODY_SYNC_NET_CUT : strcmp(kind, "stall") == 0 ? BODY_SYNC_NET_STALL : BODY_SYNC_NET_DEAF;
    if (w->count != first + 4 || strcmp(w->word[first + 2], "for") != 0) {
        return refuse(p, "%s needs: <who> for <time>", kind);
    }
    bool who = e.kind == BODY_SYNC_NET_CUT ? guest_word(w->word[first + 1], &e.endpoint) : endpoint_word(w->word[first + 1], &e.endpoint);
    if (!who) {
        return refuse(p, e.kind == BODY_SYNC_NET_CUT ? "cut needs a guest g1..g3, not '%s'" : "'%s' is not host or g1..g3", w->word[first + 1]);
    }
    if (!time_word(p, w->word[first + 3], &e.duration_ticks)) {
        return false;
    }
    if (e.duration_ticks == 0) {
        return refuse(p, "%s needs a time above zero", kind);
    }
    return add_net(p, &e);
}

static bool expect(parser_t *p, const line_words_t *w) {
    body_sync_scenario_t *s = p->out;
    if (s->expect_count >= BODY_SYNC_SCENARIO_EXPECTS_MAX) {
        return refuse(p, "more than 32 checks%s", "");
    }
    body_sync_expect_t e;
    memset(&e, 0, sizeof(e));
    e.line = p->line;
    if (w->count < 2) {
        return refuse(p, "expect needs a check%s", "");
    }
    const char *what = w->word[1];
    if (strcmp(what, "agree_at_rest") == 0) {
        e.kind = BODY_SYNC_EXPECT_AGREE_AT_REST;
        if (w->count < 3 || w->count > 4) {
            return refuse(p, "agree_at_rest needs <length> [<angle>]%s", "");
        }
        e.tolerance_deg = -1.0f;
        if (!length_word(p, w->word[2], &e.tolerance_m) || (w->count == 4 && !angle_word(p, w->word[3], &e.tolerance_deg))) {
            return false;
        }
    } else if (strcmp(what, "no_stuck") == 0 || strcmp(what, "effects_once") == 0) {
        e.kind = strcmp(what, "no_stuck") == 0 ? BODY_SYNC_EXPECT_NO_STUCK : BODY_SYNC_EXPECT_EFFECTS_ONCE;
        if (w->count != 2) {
            return refuse(p, "%s takes nothing more", what);
        }
    } else if (strcmp(what, "moved") == 0) {
        e.kind = BODY_SYNC_EXPECT_MOVED;
        if ((w->count != 5 && (w->count != 7 || strcmp(w->word[5], "layer") != 0)) || !name_word(w->word[2], BODY_SYNC_NAME_MAX - 1) ||
            (strcmp(w->word[3], "<=") != 0 && strcmp(w->word[3], ">=") != 0)) {
            return refuse(p, "moved reads: expect moved <body> <=|>= <length> [layer <length>]%s", "");
        }
        float m, lm = -1.0f;
        if (!length_word(p, w->word[4], &m) || (w->count == 7 && !length_word(p, w->word[6], &lm))) {
            return false;
        }
        e.layer_limit = (double)lm;
        copy_text(e.body, sizeof(e.body), w->word[2]);
        e.at_least = w->word[3][0] == '>';
        e.limit = (double)m;
    } else if (strcmp(what, "effect") == 0) {
        e.kind = BODY_SYNC_EXPECT_EFFECT;
        if (w->count != 3 || !name_word(w->word[2], BODY_SYNC_NAME_MAX - 1)) {
            return refuse(p, "effect needs one body%s", "");
        }
        copy_text(e.body, sizeof(e.body), w->word[2]);
    } else if (strcmp(what, "left") == 0) {
        e.kind = BODY_SYNC_EXPECT_LEFT;
        if (w->count != 3 || !guest_word(w->word[2], &e.endpoint)) {
            return refuse(p, "left needs one guest g1..g3%s", "");
        }
    } else if (strcmp(what, "result") == 0) {
        e.kind = BODY_SYNC_EXPECT_RESULT;
        if (w->count != 4) {
            return refuse(p, "result needs <label> <outcome>%s", "");
        }
        if (!name_word(w->word[2], BODY_SYNC_LABEL_MAX - 1) || !outcome_word(w->word[3], &e.outcome)) {
            return refuse(p, "result needs a label and one of done granted refused hit missed rejected failed%s", "");
        }
        e.label_count = 1;
        copy_text(e.labels[0], sizeof(e.labels[0]), w->word[2]);
    } else if (strcmp(what, "pace") == 0) {
        e.kind = BODY_SYNC_EXPECT_PACE;
        if (w->count != 3 || !name_word(w->word[2], BODY_SYNC_LABEL_MAX - 1)) {
            return refuse(p, "pace needs one label%s", "");
        }
        e.label_count = 1;
        copy_text(e.labels[0], sizeof(e.labels[0]), w->word[2]);
    } else if (strcmp(what, "one_of") == 0) {
        e.kind = BODY_SYNC_EXPECT_ONE_OF;
        if (w->count < 5 || w->count > 3 + BODY_SYNC_EXPECT_LABELS_MAX) {
            return refuse(p, "one_of needs <outcome> and 2..4 labels%s", "");
        }
        if (!outcome_word(w->word[2], &e.outcome)) {
            return refuse(p, "'%s' is not an outcome", w->word[2]);
        }
        for (uint32_t i = 3; i < w->count; i++) {
            if (!name_word(w->word[i], BODY_SYNC_LABEL_MAX - 1)) {
                return refuse(p, "'%s' is not a label", w->word[i]);
            }
            for (uint32_t k = 0; k < e.label_count; k++) {
                if (strcmp(e.labels[k], w->word[i]) == 0) {
                    return refuse(p, "one_of names '%s' twice", w->word[i]);
                }
            }
            copy_text(e.labels[e.label_count++], sizeof(e.labels[0]), w->word[i]);
        }
    } else {
        stand_metric_unit_t unit;
        if (!stand_metric_unit(what, &unit)) {
            return refuse(p, "unknown check or metric '%s'", what);
        }
        if (w->count != 4 || (strcmp(w->word[2], "<=") != 0 && strcmp(w->word[2], ">=") != 0)) {
            return refuse(p, "a metric check reads: expect %s <=|>= <value>", what);
        }
        e.at_least = strcmp(w->word[2], ">=") == 0;
        e.kind = BODY_SYNC_EXPECT_METRIC_AT_MOST;
        copy_text(e.metric, sizeof(e.metric), what);
        const char *value = w->word[3];
        float f;
        uint32_t ticks;
        double v;
        const char *suffix;
        switch (unit) {
        case STAND_UNIT_LENGTH:
            if (!length_word(p, value, &f)) {
                return false;
            }
            e.limit = (double)f;
            break;
        case STAND_UNIT_ANGLE:
            if (!angle_word(p, value, &f)) {
                return false;
            }
            e.limit = (double)f;
            break;
        case STAND_UNIT_TICKS:
            if (!quantity(value, &v, &suffix) || strcmp(suffix, "t") != 0 || !time_word(p, value, &ticks)) {
                return refuse(p, "'%s' needs ticks, as 6t", value);
            }
            e.limit = (double)ticks;
            break;
        case STAND_UNIT_RATE:
            if (!rate_word(p, value, &e.limit)) {
                return false;
            }
            break;
        case STAND_UNIT_COUNT:
            if (!bare_number(value, &v) || v < 0.0 || v != floor(v) || v > 1.0e9) {
                return refuse(p, "'%s' needs a whole count", value);
            }
            e.limit = v;
            break;
        }
    }
    s->expects[s->expect_count++] = e;
    return true;
}

static bool header_once(parser_t *p, bool *seen, const char *what) {
    if (*seen) {
        return refuse(p, "'%s' is given twice", what);
    }
    *seen = true;
    return true;
}

static bool statement(parser_t *p, const line_words_t *w) {
    body_sync_scenario_t *s = p->out;
    const char *head = w->word[0];
    uint32_t first = 0;
    bool timed = false;
    uint32_t tick = 0;
    if (strcmp(head, "at") == 0) {
        if (w->count < 3) {
            return refuse(p, "at needs a time and a statement%s", "");
        }
        if (!time_word(p, w->word[1], &tick)) {
            return false;
        }
        timed = true;
        first = 2;
        head = w->word[2];
    }
    uint8_t player;
    if (player_word(head, &player)) {
        return action(p, w, first, timed, tick);
    }
    if (strcmp(head, "net") == 0) {
        return net(p, w, first, tick);
    }
    if (strcmp(head, "cut") == 0 || strcmp(head, "stall") == 0 || strcmp(head, "deaf") == 0) {
        if (!timed) {
            return refuse(p, "%s needs 'at <time>' before it", head);
        }
        return timed_condition(p, w, first, tick);
    }
    if (timed) {
        return refuse(p, "'%s' cannot follow at", head);
    }
    if (strcmp(head, "expect") == 0) {
        return expect(p, w);
    }
    if (strcmp(head, "scenario") == 0 || strcmp(head, "scene") == 0) {
        bool is_scene = strcmp(head, "scene") == 0;
        if (!header_once(p, is_scene ? &p->seen_scene : &p->seen_scenario, head)) {
            return false;
        }
        if (w->count != 2 || !name_word(w->word[1], BODY_SYNC_NAME_MAX - 1)) {
            return refuse(p, "%s needs one name of [a-z0-9_], 1..31 characters", head);
        }
        copy_text(is_scene ? s->scene : s->name, BODY_SYNC_NAME_MAX, w->word[1]);
        return true;
    }
    if (strcmp(head, "seed") == 0) {
        if (!header_once(p, &p->seen_seed, head)) {
            return false;
        }
        if (w->count != 2) {
            return refuse(p, "seed needs one number%s", "");
        }
        uint64_t seed = 0;
        const char *c = w->word[1];
        if (!*c) {
            return refuse(p, "seed needs one number%s", "");
        }
        for (; *c; c++) {
            if (*c < '0' || *c > '9') {
                return refuse(p, "'%s' is not a whole number", w->word[1]);
            }
            uint64_t digit = (uint64_t)(*c - '0');
            if (seed > (UINT64_MAX - digit) / 10u) {
                return refuse(p, "'%s' is beyond 2^64-1", w->word[1]);
            }
            seed = seed * 10u + digit;
        }
        s->seed = seed;
        return true;
    }
    if (strcmp(head, "light_mass") == 0) {
        double kg;
        const char *unit;
        if (w->count != 2 || !quantity(w->word[1], &kg, &unit) || strcmp(unit, "kg") != 0 || !(kg >= 0.1 && kg <= 1000.0)) {
            return refuse(p, "light_mass reads: light_mass <0.1..1000>kg%s", "");
        }
        if (s->light_mass_max > 0.0f) {
            return refuse(p, "light_mass is set twice%s", "");
        }
        s->light_mass_max = (float)kg;
        return true;
    }
    if (strcmp(head, "authority") == 0) {
        bool host = w->count == 2 && strcmp(w->word[1], "host") == 0, interaction = w->count == 2 && strcmp(w->word[1], "interaction") == 0;
        if (!host && !interaction) {
            return refuse(p, "authority reads: authority host|interaction%s", "");
        }
        if (s->authority_set) {
            return refuse(p, "authority is set twice%s", "");
        }
        s->authority = interaction ? BODY_SYNC_AUTHORITY_INTERACTION : BODY_SYNC_AUTHORITY_HOST;
        s->authority_set = true;
        return true;
    }
    if (strcmp(head, "rate") == 0) {
        uint8_t who;
        double hz;
        const char *unit;
        if (w->count != 3 || !endpoint_word(w->word[1], &who)) {
            return refuse(p, "rate reads: rate <host|g1..g3> <rate>hz%s", "");
        }
        if (!quantity(w->word[2], &hz, &unit) || strcmp(unit, "hz") != 0 || !(hz >= 20.0 && hz <= 240.0)) {
            return refuse(p, "'%s' is not a rate in 20hz..240hz (59.5hz)", w->word[2]);
        }
        if (s->rate_mhz[who] != 0) {
            return refuse(p, "the rate of '%s' is set twice", w->word[1]);
        }
        s->rate_mhz[who] = (uint32_t)llround(hz * 1000.0);
        p->rate_line[who] = p->line;
        return true;
    }
    if (strcmp(head, "guests") == 0) {
        if (!header_once(p, &p->seen_guests, head)) {
            return false;
        }
        if (w->count != 2 || strlen(w->word[1]) != 1 || w->word[1][0] < '0' || w->word[1][0] > '3') {
            return refuse(p, "guests needs 0..3%s", "");
        }
        s->guests = (uint8_t)(w->word[1][0] - '0');
        p->guests_line = p->line;
        return true;
    }
    if (strcmp(head, "settle") == 0 || strcmp(head, "end") == 0) {
        bool is_end = strcmp(head, "end") == 0;
        if (!header_once(p, is_end ? &p->seen_end : &p->seen_settle, head)) {
            return false;
        }
        if (w->count != 2) {
            return refuse(p, "%s needs one time", head);
        }
        uint32_t ticks;
        if (!time_word(p, w->word[1], &ticks)) {
            return false;
        }
        if (is_end) {
            if (ticks == 0) {
                return refuse(p, "end needs a time above zero%s", "");
            }
            s->has_end = true;
            s->end_tick = ticks;
        } else {
            s->settle_ticks = ticks;
            p->settle_line = p->line;
        }
        return true;
    }
    return refuse(p, "unknown statement '%s'", head);
}

static bool split(parser_t *p, const char *line, size_t length, line_words_t *w) {
    w->count = 0;
    size_t i = 0;
    while (i < length) {
        char c = line[i];
        if (c == '#') {
            break;
        }
        if (c == ' ' || c == '\t') {
            i++;
            continue;
        }
        size_t start = i;
        while (i < length && line[i] != ' ' && line[i] != '\t' && line[i] != '#') {
            unsigned char u = (unsigned char)line[i];
            if (u < 0x21 || u > 0x7E) {
                return refuse(p, "only printable ASCII outside comments%s", "");
            }
            i++;
        }
        if (w->count >= WORDS_MAX) {
            return refuse(p, "too many words on one line%s", "");
        }
        if (i - start >= WORD_MAX) {
            return refuse(p, "a word longer than 47 characters%s", "");
        }
        memcpy(w->word[w->count], line + start, i - start);
        w->word[w->count][i - start] = '\0';
        w->count++;
    }
    return true;
}

static bool finish(parser_t *p) {
    body_sync_scenario_t *s = p->out;
    p->line = 0;
    if (!p->seen_scenario || !p->seen_scene) {
        return refuse(p, "a scenario needs 'scenario <name>' and 'scene <name>'%s", "");
    }
    for (uint32_t i = 0; i < s->action_count; i++) {
        if (s->actions[i].player > s->guests) {
            p->line = s->actions[i].line;
            return refuse(p, "this player has no endpoint: raise 'guests'%s", "");
        }
    }
    for (uint32_t i = 0; i < 1u + BODY_SYNC_GUESTS_MAX; i++) {
        if (s->rate_mhz[i] != 0 && i > s->guests) {
            p->line = p->rate_line[i];
            return refuse(p, "this guest is not in the run: raise 'guests'%s", "");
        }
    }
    for (uint32_t i = 0; i < s->net_count; i++) {
        if (s->net[i].endpoint > s->guests) {
            p->line = s->net[i].line;
            return refuse(p, "this guest is not in the run: raise 'guests'%s", "");
        }
    }
    for (uint32_t i = 0; i < s->expect_count; i++) {
        const body_sync_expect_t *e = &s->expects[i];
        for (uint32_t l = 0; l < e->label_count; l++) {
            bool found = false;
            uint32_t a = 0;
            for (; a < s->action_count && !found; a++) {
                found = strcmp(s->actions[a].label, e->labels[l]) == 0;
            }
            if (!found) {
                p->line = e->line;
                return refuse(p, "no action is labelled '%s'", e->labels[l]);
            }
            /* Progress toward a point is the same measure on both runs; a body moves. */
            const body_sync_action_t *walk = &s->actions[a - 1];
            if (e->kind == BODY_SYNC_EXPECT_PACE && (walk->verb != BODY_SYNC_VERB_WALK || walk->target.is_body)) {
                p->line = e->line;
                return refuse(p, "pace needs a walk to a point, and '%s' is not one", e->labels[l]);
            }
        }
        if (e->kind == BODY_SYNC_EXPECT_LEFT && e->endpoint > s->guests) {
            p->line = e->line;
            return refuse(p, "this guest is not in the run: raise 'guests'%s", "");
        }
        /* Without a guest these compare nothing and would pass on nothing. */
        bool needs_guest = e->kind == BODY_SYNC_EXPECT_AGREE_AT_REST || e->kind == BODY_SYNC_EXPECT_LEFT ||
                           (e->kind == BODY_SYNC_EXPECT_METRIC_AT_MOST && strcmp(e->metric, "host_ticks.max") != 0 &&
                            strcmp(e->metric, "echo_ticks.max") != 0);
        if (needs_guest && s->guests == 0) {
            p->line = e->line;
            return refuse(p, "this check needs a guest, and 'guests' is 0%s", "");
        }
    }
    /* The earliest each player's schedule can reach: a timed action starts at its time (it
       cuts off what runs before), an untimed one no earlier than the one before it ends, and
       only waits have a known length. The stand runs ticks 0 to 600 s - 1 tick. */
    const uint32_t cap = (uint32_t)BODY_SYNC_TIME_MAX_S * BODY_SYNC_TICK_HZ;
    uint32_t earliest[BODY_SYNC_PLAYERS_MAX] = {0};
    uint32_t last = 0;
    for (uint32_t i = 0; i < s->action_count; i++) {
        const body_sync_action_t *a = &s->actions[i];
        if (a->trigger == BODY_SYNC_TRIGGER_CONTACT && a->trigger_player > s->guests) {
            p->line = a->line;
            return refuse(p, "this trigger's player is not in the run: raise 'guests'%s", "");
        }
        uint32_t *t = &earliest[a->player];
        *t = a->timed ? a->tick : *t;
        if (*t >= cap) {
            p->line = a->line;
            return refuse(p, "this action could only start at 600 s or later%s", "");
        }
        *t += a->verb == BODY_SYNC_VERB_WAIT ? a->wait_ticks : 0u;
        if (*t > cap) {
            p->line = a->line;
            return refuse(p, "this wait runs past 600 s%s", "");
        }
    }
    for (uint32_t q = 0; q < BODY_SYNC_PLAYERS_MAX; q++) {
        last = earliest[q] > last ? earliest[q] : last; /* each player's last item ends after the cut-offs */
    }
    for (uint32_t i = 0; i < s->net_count; i++) {
        uint32_t ends = s->net[i].tick + s->net[i].duration_ticks;
        if (s->net[i].tick >= cap || ends > cap) {
            p->line = s->net[i].line;
            return refuse(p, "this condition does not happen and end before 600 s%s", "");
        }
        last = ends > last ? ends : last;
    }
    if (!s->has_end && last + s->settle_ticks > cap) {
        p->line = p->settle_line;
        return refuse(p, "settle would run past 600 s after the last item%s", "");
    }
    return true;
}

bool body_sync_scenario_parse(const char *text, size_t size, body_sync_scenario_t *out, body_sync_error_t *error) {
    parser_t p;
    memset(&p, 0, sizeof(p));
    p.out = out;
    p.error = error;
    if (error) {
        memset(error, 0, sizeof(*error));
    }
    if (!out || (!text && size > 0)) {
        return refuse(&p, "no scenario or no text%s", "");
    }
    memset(out, 0, sizeof(*out));
    out->seed = 1;
    out->guests = 1;
    out->settle_ticks = 5u * BODY_SYNC_TICK_HZ;
    if (size > BODY_SYNC_SCENARIO_TEXT_MAX) {
        return refuse(&p, "a scenario is at most 65536 bytes%s", "");
    }
    line_words_t words;
    size_t at = 0;
    while (at < size) {
        p.line++;
        size_t end = at;
        while (end < size && text[end] != '\n') {
            if (text[end] == '\0') {
                return refuse(&p, "a NUL byte in the text%s", "");
            }
            end++;
        }
        size_t length = end - at;
        if (length > 0 && text[at + length - 1] == '\r') {
            length--;
        }
        if (length > BODY_SYNC_SCENARIO_LINE_MAX) {
            return refuse(&p, "a line longer than 256 characters%s", "");
        }
        if (!split(&p, text + at, length, &words)) {
            return false;
        }
        if (words.count > 0 && !statement(&p, &words)) {
            return false;
        }
        at = end + 1;
    }
    return finish(&p);
}

bool body_sync_scenario_load(const char *path, body_sync_scenario_t *out, body_sync_error_t *error) {
    if (error) {
        memset(error, 0, sizeof(*error));
    }
    FILE *file = path ? fopen(path, "rb") : NULL;
    if (!file) {
        if (error) {
            snprintf(error->text, sizeof(error->text), "cannot open %s", path ? path : "(null)");
        }
        return false;
    }
    char *text = malloc(BODY_SYNC_SCENARIO_TEXT_MAX + 1);
    size_t size = text ? fread(text, 1, BODY_SYNC_SCENARIO_TEXT_MAX + 1, file) : 0;
    bool failed = !text || ferror(file) != 0;
    fclose(file);
    if (failed || size > BODY_SYNC_SCENARIO_TEXT_MAX) {
        if (error) {
            snprintf(error->text, sizeof(error->text), failed ? "cannot read %s" : "%s is over 65536 bytes", path);
        }
        free(text);
        return false;
    }
    bool ok = body_sync_scenario_parse(text, size, out, error);
    free(text);
    return ok;
}
