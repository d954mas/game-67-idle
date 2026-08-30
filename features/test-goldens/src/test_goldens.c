#include "features/test_goldens/test_goldens.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOLDEN_MAX_ENTRIES 128
#define GOLDEN_KEY_MAX 64
#define GOLDEN_VALUE_MAX 192
#define GOLDEN_PATH_MAX 512

typedef struct {
    char key[GOLDEN_KEY_MAX];
    char value[GOLDEN_VALUE_MAX];
} golden_entry_t;

// The bank is re-read on every call: a test asserts a handful of values, and a
// file read per assertion costs nothing next to the confusion a stale cache
// causes when a test records and then compares in the same process.
typedef struct {
    char bank[GOLDEN_KEY_MAX];
    golden_entry_t entries[GOLDEN_MAX_ENTRIES];
    int count;
} golden_bank_t;

static golden_bank_t g_bank;

static int golden_recording(void)
{
    const char *flag = getenv("GAME_UPDATE_GOLDENS");
    return flag != NULL && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static void golden_path(const char *bank, char *out, size_t out_size)
{
    const char *dir = getenv("GAME_GOLDENS_DIR");
    if (dir == NULL || dir[0] == '\0') dir = "tests/goldens";
    snprintf(out, out_size, "%s/%s.golden", dir, bank);
}

static void golden_fail(const char *bank, const char *key, const char *why)
{
    fprintf(stderr,
        "test-goldens: %s for '%s/%s'.\n"
        "Record it with: node tools/game.mjs test --update-goldens --only <test>\n",
        why, bank, key);
    exit(1);
}

static char *golden_trim(char *text)
{
    while (*text == ' ' || *text == '\t') text += 1;
    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t'
        || text[length - 1] == '\r' || text[length - 1] == '\n')) {
        text[length - 1] = '\0';
        length -= 1;
    }
    return text;
}

static void golden_load(const char *bank)
{
    memset(&g_bank, 0, sizeof(g_bank));
    snprintf(g_bank.bank, sizeof(g_bank.bank), "%s", bank);

    char path[GOLDEN_PATH_MAX];
    golden_path(bank, path, sizeof(path));
    FILE *file = fopen(path, "rb");
    if (file == NULL) return;

    char line[GOLDEN_KEY_MAX + GOLDEN_VALUE_MAX + 8];
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        char *separator = strchr(line, '=');
        if (separator == NULL) continue;
        *separator = '\0';
        char *key = golden_trim(line);
        char *value = golden_trim(separator + 1);
        if (key[0] == '\0' || key[0] == '#') continue;
        if (g_bank.count >= GOLDEN_MAX_ENTRIES) break;
        snprintf(g_bank.entries[g_bank.count].key, GOLDEN_KEY_MAX, "%s", key);
        snprintf(g_bank.entries[g_bank.count].value, GOLDEN_VALUE_MAX, "%s", value);
        g_bank.count += 1;
    }
    fclose(file);
}

static int golden_compare_entries(const void *left, const void *right)
{
    return strcmp(((const golden_entry_t *)left)->key, ((const golden_entry_t *)right)->key);
}

static void golden_store(const char *bank, const char *key, const char *value)
{
    golden_load(bank);
    int index = -1;
    for (int i = 0; i < g_bank.count; i += 1) {
        if (strcmp(g_bank.entries[i].key, key) == 0) index = i;
    }
    if (index < 0) {
        if (g_bank.count >= GOLDEN_MAX_ENTRIES) golden_fail(bank, key, "bank is full");
        index = g_bank.count;
        g_bank.count += 1;
        snprintf(g_bank.entries[index].key, GOLDEN_KEY_MAX, "%s", key);
    }
    snprintf(g_bank.entries[index].value, GOLDEN_VALUE_MAX, "%s", value);
    qsort(g_bank.entries, (size_t)g_bank.count, sizeof(golden_entry_t), golden_compare_entries);

    char path[GOLDEN_PATH_MAX];
    golden_path(bank, path, sizeof(path));
    FILE *file = fopen(path, "wb");
    if (file == NULL) golden_fail(bank, key, "bank directory is missing");
    for (int i = 0; i < g_bank.count; i += 1) {
        fprintf(file, "%s = %s\n", g_bank.entries[i].key, g_bank.entries[i].value);
    }
    fclose(file);
}

static const char *golden_recorded(const char *bank, const char *key)
{
    golden_load(bank);
    for (int i = 0; i < g_bank.count; i += 1) {
        if (strcmp(g_bank.entries[i].key, key) == 0) return g_bank.entries[i].value;
    }
    return NULL;
}

static const char *golden_resolve(const char *bank, const char *key, const char *actual)
{
    // A truncated value would record silently and compare wrong forever.
    if (strlen(actual) >= GOLDEN_VALUE_MAX) golden_fail(bank, key, "value is too long for a bank entry");
    if (strlen(key) >= GOLDEN_KEY_MAX) golden_fail(bank, key, "key is too long for a bank entry");
    if (golden_recording()) {
        golden_store(bank, key, actual);
        return golden_recorded(bank, key);
    }
    const char *recorded = golden_recorded(bank, key);
    if (recorded == NULL) golden_fail(bank, key, "no recorded value");
    return recorded;
}

uint64_t test_golden_u64(const char *bank, const char *key, uint64_t actual)
{
    char rendered[GOLDEN_VALUE_MAX];
    snprintf(rendered, sizeof(rendered), "%" PRIu64, actual);
    return strtoull(golden_resolve(bank, key, rendered), NULL, 10);
}

int64_t test_golden_i64(const char *bank, const char *key, int64_t actual)
{
    char rendered[GOLDEN_VALUE_MAX];
    snprintf(rendered, sizeof(rendered), "%" PRId64, actual);
    return strtoll(golden_resolve(bank, key, rendered), NULL, 10);
}

double test_golden_f64(const char *bank, const char *key, double actual)
{
    char rendered[GOLDEN_VALUE_MAX];
    snprintf(rendered, sizeof(rendered), "%.9g", actual);
    return strtod(golden_resolve(bank, key, rendered), NULL);
}

const char *test_golden_text(const char *bank, const char *key, const char *actual)
{
    if (actual != NULL && strpbrk(actual, "\r\n") != NULL) golden_fail(bank, key, "text goldens must be one line");
    return golden_resolve(bank, key, actual == NULL ? "" : actual);
}
