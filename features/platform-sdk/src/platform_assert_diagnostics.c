#include "core/nt_assert.h"
#include "log/nt_log.h"

#include <string.h>

static void report_assert(const char *expr, const char *file, int line) {
    // Keep the source suffix useful for diagnosis without sending a host path to the portal.
    const char *short_file = file;
    int separators = 0;
    for (const char *p = file + strlen(file); p > file;) {
        --p;
        if (*p == '/' || *p == '\\') {
            if (++separators == 1) short_file = p + 1;
            if (separators == 3) {
                short_file = p + 1;
                break;
            }
        }
    }
    // The crash sink copies ERROR lines into its ring before the trap reaches JavaScript.
    nt_log_error("NT_ASSERT failed: %s:%d: %s", short_file, line, expr);
    __builtin_trap();
}

__attribute__((constructor)) static void install_assert_reporter(void) {
    nt_assert_handler = report_assert;
}
