#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#define MAX_PATH 4096
#endif

/* Per-session attribution (set in main, stamped into every record) so parallel
 * work in the same day -- different sessions, harnesses, or project cwds -- is
 * never mixed in one log. */
static char g_session_id[128] = "";
static char g_cwd[1024] = "";

typedef struct {
    size_t chars;
    size_t lines;
    int fields;
    bool previous_was_cr;
    bool ended_with_newline;
} OutputMetrics;

static char *read_stdin_all(void) {
    size_t cap = 8192;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 4096 + 1 > cap) {
            cap *= 2;
            char *next = (char *)realloc(buf, cap);
            if (!next) {
                free(buf);
                return NULL;
            }
            buf = next;
        }
        size_t n = fread(buf + len, 1, 4096, stdin);
        len += n;
        if (n < 4096) break;
    }
    buf[len] = '\0';
    return buf;
}

static const char *find_key(const char *json, const char *key) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ':') return NULL;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static bool json_string_value(const char *json, const char *key, char *out, size_t out_size) {
    const char *p = find_key(json, key);
    if (!p || *p != '"' || out_size == 0) return false;
    p++;
    size_t w = 0;
    while (*p && *p != '"' && w + 1 < out_size) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[w++] = '\n'; break;
                case 'r': out[w++] = '\r'; break;
                case 't': out[w++] = '\t'; break;
                default: out[w++] = *p; break;
            }
            p++;
            continue;
        }
        out[w++] = *p++;
    }
    out[w] = '\0';
    return true;
}

static int json_int_value(const char *json, const char *key, int fallback) {
    const char *p = find_key(json, key);
    if (!p) return fallback;
    char *end = NULL;
    long value = strtol(p, &end, 10);
    return end != p ? (int)value : fallback;
}

static bool json_error_value(const char *json) {
    const char *p = find_key(json, "is_error");
    if (!p) p = find_key(json, "isError");
    if (!p) p = find_key(json, "error");
    if (!p) return false;
    if (strncmp(p, "true", 4) == 0) return true;
    if (*p == '"') return p[1] != '"';
    return false;
}

static void output_metrics_add_char(OutputMetrics *metrics, char c) {
    metrics->chars++;
    if (c == '\r') {
        metrics->lines++;
        metrics->previous_was_cr = true;
        metrics->ended_with_newline = true;
    } else if (c == '\n') {
        if (!metrics->previous_was_cr) metrics->lines++;
        metrics->previous_was_cr = false;
        metrics->ended_with_newline = true;
    } else {
        metrics->previous_was_cr = false;
        metrics->ended_with_newline = false;
    }
}

static void output_metrics_start_field(OutputMetrics *metrics) {
    if (metrics->fields > 0) output_metrics_add_char(metrics, '\n');
    metrics->fields++;
}

static bool json_string_metrics(const char *json, const char *key, OutputMetrics *metrics) {
    const char *p = find_key(json, key);
    if (!p || *p != '"') return false;
    p++;
    output_metrics_start_field(metrics);
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': output_metrics_add_char(metrics, '\n'); break;
                case 'r': output_metrics_add_char(metrics, '\r'); break;
                case 't': output_metrics_add_char(metrics, '\t'); break;
                case 'u':
                    output_metrics_add_char(metrics, '?');
                    for (int i = 0; i < 4 && p[1] && isxdigit((unsigned char)p[1]); i++) p++;
                    break;
                default: output_metrics_add_char(metrics, *p); break;
            }
            p++;
            continue;
        }
        output_metrics_add_char(metrics, *p++);
    }
    return true;
}

static OutputMetrics output_metrics_from_payload(const char *payload) {
    OutputMetrics metrics = {0};
    json_string_metrics(payload, "output", &metrics);
    json_string_metrics(payload, "stdout", &metrics);
    json_string_metrics(payload, "stderr", &metrics);
    json_string_metrics(payload, "message", &metrics);
    json_string_metrics(payload, "error", &metrics);
    if (metrics.chars > 0 && !metrics.ended_with_newline) metrics.lines++;
    return metrics;
}

static void lower_copy(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;
    for (; src[i] && i + 1 < dst_size; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static bool starts_word(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || isspace((unsigned char)s[n]));
}

static const char *skip_git_options(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    while (s[0] == '-') {
        while (*s && !isspace((unsigned char)*s)) s++;
        while (*s && isspace((unsigned char)*s)) s++;
        if (*s && s[-2] != '-') {
            while (*s && !isspace((unsigned char)*s)) s++;
            while (*s && isspace((unsigned char)*s)) s++;
        }
    }
    return s;
}

static bool is_read_only_plumbing(const char *cmd) {
    char lower[512];
    lower_copy(lower, sizeof(lower), cmd);
    char *nl = strchr(lower, '\n');
    if (nl) *nl = '\0';
    char *s = lower;
    while (*s && isspace((unsigned char)*s)) s++;

    if (strncmp(s, "git ", 4) == 0 || strncmp(s, "git.exe ", 8) == 0) {
        s += strncmp(s, "git.exe ", 8) == 0 ? 8 : 4;
        while (strncmp(s, "-c ", 3) == 0) {
            s += 3;
            while (*s && !isspace((unsigned char)*s)) s++;
            while (*s && isspace((unsigned char)*s)) s++;
        }
        s = (char *)skip_git_options(s);
        return starts_word(s, "status") || starts_word(s, "diff") || starts_word(s, "log");
    }

    return starts_word(s, "get-content") || starts_word(s, "get-childitem") ||
           starts_word(s, "test-path") || starts_word(s, "select-string") ||
           starts_word(s, "where.exe") || starts_word(s, "ls") || starts_word(s, "cat");
}

/* Search tools exit 1 on "no match" -- a normal outcome, not a failure. */
static bool is_search_command(const char *cmd) {
    char lower[512];
    lower_copy(lower, sizeof(lower), cmd);
    char *nl = strchr(lower, '\n');
    if (nl) *nl = '\0';
    char *s = lower;
    while (*s && isspace((unsigned char)*s)) s++;
    return starts_word(s, "rg") || starts_word(s, "grep") || starts_word(s, "egrep") ||
           starts_word(s, "fgrep") || starts_word(s, "findstr") ||
           starts_word(s, "ack") || starts_word(s, "select-string");
}

static const char *category_for(const char *cmd) {
    char lower[512];
    lower_copy(lower, sizeof(lower), cmd);
    if (strstr(lower, "node --test") || strstr(lower, "--test ") || strstr(lower, "unittest") ||
        strstr(lower, "cmake --build") || strstr(lower, "pipeline_validate") ||
        strstr(lower, "pytest")) {
        return "validation";
    }
    if (strstr(lower, "git commit") || strstr(lower, "git add") || strstr(lower, "git push") ||
        strstr(lower, "git status") || strstr(lower, "git diff") || strstr(lower, "git log")) {
        return "task_status";
    }
    if (strstr(lower, "cmake") || strstr(lower, "ninja") || strstr(lower, "gcc") ||
        strstr(lower, "clang") || strstr(lower, "build")) {
        return "implementation";
    }
    if (strstr(lower, "grep") || strstr(lower, "find") || strstr(lower, "rg ")) {
        return "research";
    }
    return "tooling";
}

static void timestamp_now(char *out, size_t out_size) {
#ifdef _WIN32
    SYSTEMTIME local;
    TIME_ZONE_INFORMATION tz;
    GetLocalTime(&local);
    DWORD tz_result = GetTimeZoneInformation(&tz);
    LONG bias = tz.Bias;
    if (tz_result == TIME_ZONE_ID_DAYLIGHT) bias += tz.DaylightBias;
    else if (tz_result == TIME_ZONE_ID_STANDARD) bias += tz.StandardBias;
    int offset = (int)(-bias);
    char sign = offset >= 0 ? '+' : '-';
    if (offset < 0) offset = -offset;
    snprintf(out, out_size, "%04u-%02u-%02uT%02u:%02u:%02u.%03u%c%02d:%02d",
             local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute,
             local.wSecond, local.wMilliseconds, sign, offset / 60, offset % 60);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;
    struct tm local;
    localtime_r(&now, &local);
#if defined(__APPLE__) || defined(__linux__)
    long offset_seconds = local.tm_gmtoff;
#else
    struct tm utc;
    gmtime_r(&now, &utc);
    long offset_seconds = (long)difftime(mktime(&local), mktime(&utc));
#endif
    char sign = offset_seconds >= 0 ? '+' : '-';
    if (offset_seconds < 0) offset_seconds = -offset_seconds;
    snprintf(out, out_size, "%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld:%02ld",
             local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
             local.tm_min, local.tm_sec, tv.tv_usec / 1000, sign,
             offset_seconds / 3600, (offset_seconds / 60) % 60);
#endif
}

static void default_profile_path(char *out, size_t out_size) {
#ifdef _WIN32
    SYSTEMTIME local;
    GetLocalTime(&local);
    _mkdir("tmp");
    _mkdir("tmp\\session_profiles");
    snprintf(out, out_size, "tmp\\session_profiles\\session_profile_%04u-%02u-%02u.jsonl",
             local.wYear, local.wMonth, local.wDay);
#else
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    mkdir("tmp", 0755);
    mkdir("tmp/session_profiles", 0755);
    snprintf(out, out_size, "tmp/session_profiles/session_profile_%04d-%02d-%02d.jsonl",
             local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
#endif
}

static const char *base_name(const char *path) {
    const char *b = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

/* Find a UUID (8-4-4-4-12 hex) anywhere in src; copy the full uuid into full[]
 * and its first 8 hex into short8[]. Returns true on hit. Derives ONE stable
 * session id from either a Claude session_id ("3ab203ec-...") OR a Codex
 * rollout-<ts>-<uuid>.jsonl filename -- the leading timestamp groups (2/4 digits)
 * never match the 8-hex+dash test, so it locks onto the real uuid. */
static bool extract_uuid(const char *src, char *full, size_t full_size, char *short8, size_t short8_size) {
    if (!src) return false;
    for (const char *p = src; *p; p++) {
        int hex = 0;
        const char *q = p;
        while (isxdigit((unsigned char)*q)) { q++; hex++; }
        if (hex == 8 && *q == '-' && isxdigit((unsigned char)q[1])) {
            size_t n = 0;
            while (p[n] && n < 36 && (isxdigit((unsigned char)p[n]) || p[n] == '-') && n + 1 < full_size) {
                full[n] = p[n];
                n++;
            }
            full[n] = '\0';
            size_t s = 0;
            for (; s < 8 && p[s] && s + 1 < short8_size; s++) short8[s] = p[s];
            short8[s] = '\0';
            return true;
        }
    }
    return false;
}

#ifdef _WIN32
/* Codex best-effort: newest rollout-*.jsonl in ~/.codex/sessions/<Y>/<M>/<D>/
 * when CODEX_SESSION_FILE is not exported into the hook env. */
static bool latest_codex_session(char *out, size_t out_size) {
    const char *home = getenv("USERPROFILE");
    if (!home) home = getenv("HOME");
    if (!home) return false;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char pattern[MAX_PATH * 2];
    snprintf(pattern, sizeof(pattern), "%s\\.codex\\sessions\\%04u\\%02u\\%02u\\rollout-*.jsonl",
             home, t.wYear, t.wMonth, t.wDay);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    FILETIME best_time = {0, 0};
    do {
        if (CompareFileTime(&fd.ftLastWriteTime, &best_time) >= 0) {
            best_time = fd.ftLastWriteTime;
            snprintf(best, sizeof(best), "%s", fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    snprintf(out, out_size, "%s", best);
    return true;
}
#endif

/* Per-session log path: tmp/session_profiles/sessions/<date>__<harness>__<sid8>.jsonl */
static void session_profile_path(char *out, size_t out_size, const char *harness, const char *sid8) {
#ifdef _WIN32
    SYSTEMTIME local;
    GetLocalTime(&local);
    _mkdir("tmp");
    _mkdir("tmp\\session_profiles");
    _mkdir("tmp\\session_profiles\\sessions");
    snprintf(out, out_size, "tmp\\session_profiles\\sessions\\%04u-%02u-%02u__%s__%s.jsonl",
             local.wYear, local.wMonth, local.wDay, harness, sid8);
#else
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    mkdir("tmp", 0755);
    mkdir("tmp/session_profiles", 0755);
    mkdir("tmp/session_profiles/sessions", 0755);
    snprintf(out, out_size, "tmp/session_profiles/sessions/%04d-%02d-%02d__%s__%s.jsonl",
             local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, harness, sid8);
#endif
}

static void append_char(char *buf, size_t cap, size_t *len, char c) {
    if (*len + 1 >= cap) return;
    buf[(*len)++] = c;
    buf[*len] = '\0';
}

static void append_str(char *buf, size_t cap, size_t *len, const char *s) {
    while (*s && *len + 1 < cap) buf[(*len)++] = *s++;
    buf[*len] = '\0';
}

static void append_fmt(char *buf, size_t cap, size_t *len, const char *fmt, ...) {
    if (*len >= cap) return;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *len, cap - *len, fmt, args);
    va_end(args);
    if (n < 0) return;
    size_t add = (size_t)n;
    if (add >= cap - *len) {
        *len = cap - 1;
        buf[*len] = '\0';
    } else {
        *len += add;
    }
}

static void append_json_escape(char *buf, size_t cap, size_t *len, const char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') append_fmt(buf, cap, len, "\\%c", c);
        else if (c == '\n') append_str(buf, cap, len, "\\n");
        else if (c == '\r') append_str(buf, cap, len, "\\r");
        else if (c == '\t') append_str(buf, cap, len, "\\t");
        else if (c < 32) append_fmt(buf, cap, len, "\\u%04x", c);
        else append_char(buf, cap, len, (char)c);
    }
}

static bool lock_profile_file(FILE *f) {
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(f));
    if (h == INVALID_HANDLE_VALUE) return false;
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    return LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 0xffffffff, 0xffffffff, &ov) != 0;
#else
    return flock(fileno(f), LOCK_EX) == 0;
#endif
}

static void unlock_profile_file(FILE *f) {
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(f));
    if (h == INVALID_HANDLE_VALUE) return;
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    UnlockFileEx(h, 0, 0xffffffff, 0xffffffff, &ov);
#else
    flock(fileno(f), LOCK_UN);
#endif
}

static void trim_first_line(char *s) {
    char *nl = strpbrk(s, "\r\n");
    if (nl) *nl = '\0';
    if (strlen(s) > 200) s[200] = '\0';
}

static void append_record(const char *profile, const char *event_type, const char *harness,
                          const char *tool, const char *command, const char *result,
                          const char *value, const char *category, const char *intent,
                          size_t output_chars, size_t output_lines) {
    char path[MAX_PATH * 2];
    if (profile && *profile) {
        snprintf(path, sizeof(path), "%s", profile);
    } else {
        default_profile_path(path, sizeof(path));
    }
    char ts[64];
    timestamp_now(ts, sizeof(ts));

    char line[8192];
    size_t len = 0;
    line[0] = '\0';
    append_fmt(line, sizeof(line), &len,
               "{\"ts\":\"%s\",\"phase\":\"session\",\"category\":\"%s\",\"intent\":\"",
               ts, category);
    append_json_escape(line, sizeof(line), &len, intent);
    append_fmt(line, sizeof(line), &len,
               "\",\"result\":\"%s\",\"value\":\"%s\",\"event_type\":\"%s\",\"tools\":[\"",
               result, value, event_type);
    append_json_escape(line, sizeof(line), &len, harness);
    append_char(line, sizeof(line), &len, '/');
    append_json_escape(line, sizeof(line), &len, tool);
    append_str(line, sizeof(line), &len, "\"]");
    if (command && *command) {
        append_str(line, sizeof(line), &len, ",\"commands\":[\"");
        append_json_escape(line, sizeof(line), &len, command);
        append_str(line, sizeof(line), &len, "\"]");
    }
    if (output_chars > 0 || output_lines > 0) {
        append_fmt(line, sizeof(line), &len,
                   ",\"output_chars\":%llu,\"output_lines\":%llu",
                   (unsigned long long)output_chars, (unsigned long long)output_lines);
    }
    /* Per-session attribution so parallel work never mixes. */
    if (g_session_id[0]) {
        append_str(line, sizeof(line), &len, ",\"session_id\":\"");
        append_json_escape(line, sizeof(line), &len, g_session_id);
        append_char(line, sizeof(line), &len, '"');
    }
    if (harness && *harness) {
        append_str(line, sizeof(line), &len, ",\"harness\":\"");
        append_json_escape(line, sizeof(line), &len, harness);
        append_char(line, sizeof(line), &len, '"');
    }
    if (g_cwd[0]) {
        append_str(line, sizeof(line), &len, ",\"cwd\":\"");
        append_json_escape(line, sizeof(line), &len, g_cwd);
        append_char(line, sizeof(line), &len, '"');
    }
    append_str(line, sizeof(line), &len, "}\n");

    FILE *f = fopen(path, "ab");
    if (!f) return;
    if (!lock_profile_file(f)) {
        fclose(f);
        return;
    }
    fseek(f, 0, SEEK_END);
    fwrite(line, 1, len, f);
    fflush(f);
    unlock_profile_file(f);
    fclose(f);
}

int main(int argc, char **argv) {
    char *payload = read_stdin_all();
    if (!payload) return 0;

    const char *harness = argc > 1 ? argv[1] : "agent";
    char event[64] = "PostToolUse";
    char tool[64] = "shell";
    char command[1024] = "";
    json_string_value(payload, "hook_event_name", event, sizeof(event)) ||
        json_string_value(payload, "hookEventName", event, sizeof(event));
    json_string_value(payload, "tool_name", tool, sizeof(tool)) ||
        json_string_value(payload, "toolName", tool, sizeof(tool));

    /* ---- Resolve a stable per-session id (Claude AND Codex) + project cwd ---- */
    char sid_full[64] = "";
    char sid8[16] = "";
    char raw[256] = "";
    /* 1. payload session_id (Claude always; Codex if provided) */
    if (json_string_value(payload, "session_id", raw, sizeof(raw)) ||
        json_string_value(payload, "sessionId", raw, sizeof(raw)) ||
        json_string_value(payload, "conversation_id", raw, sizeof(raw))) {
        extract_uuid(raw, sid_full, sizeof(sid_full), sid8, sizeof(sid8));
    }
    /* 2. Codex: CODEX_SESSION_FILE env -> uuid in the rollout filename */
    if (!sid8[0]) {
        const char *csf = getenv("CODEX_SESSION_FILE");
        if (csf && *csf) extract_uuid(base_name(csf), sid_full, sizeof(sid_full), sid8, sizeof(sid8));
    }
#ifdef _WIN32
    /* 3. Codex fallback: newest rollout-*.jsonl for today */
    if (!sid8[0] && strcmp(harness, "codex") == 0) {
        char fn[MAX_PATH];
        if (latest_codex_session(fn, sizeof(fn))) extract_uuid(fn, sid_full, sizeof(sid_full), sid8, sizeof(sid8));
    }
#endif
    snprintf(g_session_id, sizeof(g_session_id), "%s", sid_full);
    /* project cwd = the hook's working dir (the harness runs hooks at the project
     * root), so work in different repos -- e.g. the engine vs the game -- is
     * attributable. Read it directly to avoid payload JSON-escaping edge cases. */
#ifdef _WIN32
    if (!_getcwd(g_cwd, sizeof(g_cwd))) g_cwd[0] = '\0';
#else
    if (!getcwd(g_cwd, sizeof(g_cwd))) g_cwd[0] = '\0';
#endif

    /* Pick the log file: explicit override -> per-session file -> daily fallback
     * (so a session with no resolvable id still records, never worse than before). */
    char profile_buf[MAX_PATH * 2];
    const char *env_profile = getenv("AI_PROFILE_FILE");
    const char *profile;
    if (env_profile && *env_profile) {
        profile = env_profile;
    } else if (sid8[0]) {
        session_profile_path(profile_buf, sizeof(profile_buf), harness, sid8);
        profile = profile_buf;
    } else {
        profile = "";
    }

    if (strcmp(event, "SessionStart") == 0) {
        char intent[128];
        snprintf(intent, sizeof(intent), "session start (%s)", harness);
        append_record(profile, "session_start", harness, "session", "", "pass", "unknown", "context", intent, 0, 0);
        free(payload);
        return 0;
    }

    if (!json_string_value(payload, "command", command, sizeof(command))) {
        free(payload);
        return 0;
    }
    trim_first_line(command);

    if (strcmp(event, "PreToolUse") == 0) {
        if (!is_read_only_plumbing(command)) {
            char intent[128];
            snprintf(intent, sizeof(intent), "auto:%s", tool[0] ? tool : "shell");
            append_record(profile, "tool_call_start", harness, tool[0] ? tool : "shell", command,
                          "unknown", "necessary_overhead", category_for(command), intent, 0, 0);
        }
        free(payload);
        return 0;
    }

    int exit_code = json_int_value(payload, "exit_code", json_int_value(payload, "exitCode", 0));
    bool failed = exit_code != 0 || json_error_value(payload);
    /* A search tool returning exit 1 = "no match", a normal outcome -- counting
     * it as a failure inflates the failure rate and hides real failures (retro
     * 2026-06-17). exit 2 (a real search error) still counts as failed. */
    if (failed && exit_code == 1 && !json_error_value(payload) && is_search_command(command)) {
        failed = false;
    }
    if (!failed && is_read_only_plumbing(command)) {
        free(payload);
        return 0;
    }

    char intent[128];
    snprintf(intent, sizeof(intent), "auto:%s", tool[0] ? tool : "shell");
    OutputMetrics output_metrics = output_metrics_from_payload(payload);
    append_record(profile, "tool_call_result", harness, tool[0] ? tool : "shell", command,
                  failed ? "fail" : "pass", failed ? "rework" : "unknown",
                  category_for(command), intent, output_metrics.chars, output_metrics.lines);
    free(payload);
    return 0;
}
