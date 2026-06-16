#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#define MAX_PATH 4096
#endif

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

static const char *category_for(const char *cmd) {
    char lower[512];
    lower_copy(lower, sizeof(lower), cmd);
    if (strstr(lower, "node --test") || strstr(lower, "--test ") || strstr(lower, "unittest") ||
        strstr(lower, "cmake --build") || strstr(lower, "pipeline_validate") ||
        strstr(lower, "skills_eval") || strstr(lower, "pytest")) {
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

static void json_escape(FILE *f, const char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') fprintf(f, "\\%c", c);
        else if (c == '\n') fputs("\\n", f);
        else if (c == '\r') fputs("\\r", f);
        else if (c == '\t') fputs("\\t", f);
        else if (c < 32) fprintf(f, "\\u%04x", c);
        else fputc(c, f);
    }
}

static void trim_first_line(char *s) {
    char *nl = strpbrk(s, "\r\n");
    if (nl) *nl = '\0';
    if (strlen(s) > 200) s[200] = '\0';
}

static void append_record(const char *profile, const char *event_type, const char *harness,
                          const char *tool, const char *command, const char *result,
                          const char *value, const char *category, const char *intent) {
    char path[MAX_PATH * 2];
    if (profile && *profile) {
        snprintf(path, sizeof(path), "%s", profile);
    } else {
        default_profile_path(path, sizeof(path));
    }
    FILE *f = fopen(path, "ab");
    if (!f) return;
    char ts[64];
    timestamp_now(ts, sizeof(ts));
    fprintf(f, "{\"ts\":\"%s\",\"phase\":\"session\",\"category\":\"%s\",\"intent\":\"", ts, category);
    json_escape(f, intent);
    fprintf(f, "\",\"result\":\"%s\",\"value\":\"%s\",\"event_type\":\"%s\",\"tools\":[\"", result, value, event_type);
    json_escape(f, harness);
    fputc('/', f);
    json_escape(f, tool);
    fputs("\"]", f);
    if (command && *command) {
        fputs(",\"commands\":[\"", f);
        json_escape(f, command);
        fputs("\"]", f);
    }
    fputs("}\n", f);
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

    const char *profile = getenv("AI_PROFILE_FILE");

    if (strcmp(event, "SessionStart") == 0) {
        char intent[128];
        snprintf(intent, sizeof(intent), "session start (%s)", harness);
        append_record(profile, "session_start", harness, "session", "", "pass", "unknown", "context", intent);
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
                          "unknown", "necessary_overhead", category_for(command), intent);
        }
        free(payload);
        return 0;
    }

    int exit_code = json_int_value(payload, "exit_code", json_int_value(payload, "exitCode", 0));
    bool failed = exit_code != 0 || json_error_value(payload);
    if (!failed && is_read_only_plumbing(command)) {
        free(payload);
        return 0;
    }

    char intent[128];
    snprintf(intent, sizeof(intent), "auto:%s", tool[0] ? tool : "shell");
    append_record(profile, "tool_call_result", harness, tool[0] ? tool : "shell", command,
                  failed ? "fail" : "pass", failed ? "rework" : "unknown",
                  category_for(command), intent);
    free(payload);
    return 0;
}
