#include "game_storage_backend.h"

#include "log/nt_log.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define GAME_STORAGE_PATH_MAX 512
#define GAME_STORAGE_QUARANTINE_MAX_FILES 3

#ifndef GAME_STORAGE_APP_ID
#error "GAME_STORAGE_APP_ID must be defined via CMake"
#endif

typedef struct game_storage_native_paths_t {
    char directory[GAME_STORAGE_PATH_MAX];
    char primary[GAME_STORAGE_PATH_MAX];
    char primary_tmp[GAME_STORAGE_PATH_MAX];
    char backup[GAME_STORAGE_PATH_MAX];
    char backup_tmp[GAME_STORAGE_PATH_MAX];
    char quarantine_base[GAME_STORAGE_PATH_MAX];
} game_storage_native_paths_t;

static void set_error(char *error, int error_cap, const char *message) {
    if (error != NULL && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

static bool is_safe_segment(const char *value) {
    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (const char *p = value; *p != '\0'; ++p) {
        const char c = *p;
        const bool safe =
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-';
        if (!safe) {
            return false;
        }
    }
    return true;
}

static bool is_absolute_path(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }
#ifdef _WIN32
    if (path[0] == '\\' && path[1] == '\\') {
        return true;
    }
    const bool has_drive =
        (path[0] >= 'A' && path[0] <= 'Z') ||
        (path[0] >= 'a' && path[0] <= 'z');
    return has_drive && path[1] == ':' &&
           (path[2] == '/' || path[2] == '\\');
#else
    return path[0] == '/';
#endif
}

static bool resolve_storage_root(
    char *out, size_t out_cap, char *error, int error_cap) {
#ifdef GAME_STORAGE_NATIVE_ROOT
    const char *override_root = GAME_STORAGE_NATIVE_ROOT;
#else
    const char *override_root = getenv("GAME_STORAGE_ROOT");
#endif
    if (override_root != NULL && is_absolute_path(override_root)) {
        if (snprintf(out, out_cap, "%s", override_root) >= (int)out_cap) {
            set_error(error, error_cap, "native storage root is too long");
            return false;
        }
        return true;
    }

    const char *root = NULL;
#ifdef _WIN32
    root = getenv("LOCALAPPDATA");
    if (root == NULL || !is_absolute_path(root)) {
        static char temp_path[GAME_STORAGE_PATH_MAX];
        const DWORD length = GetTempPathA((DWORD)sizeof temp_path, temp_path);
        if (length == 0 || length >= sizeof temp_path) {
            set_error(error, error_cap, "failed to resolve native storage root");
            return false;
        }
        root = temp_path;
    }
#else
    root = getenv("XDG_STATE_HOME");
    char home_state[GAME_STORAGE_PATH_MAX];
    if (root == NULL || !is_absolute_path(root)) {
        const char *home = getenv("HOME");
        if (home != NULL && is_absolute_path(home)) {
            if (snprintf(home_state, sizeof home_state, "%s/.local/state", home) >=
                (int)sizeof home_state) {
                set_error(error, error_cap, "native storage root is too long");
                return false;
            }
            root = home_state;
        } else {
            root = "/tmp";
        }
    }
#endif
    if (snprintf(out, out_cap, "%s/neotolis/%s/saves", root,
                 GAME_STORAGE_APP_ID) >= (int)out_cap) {
        set_error(error, error_cap, "native storage root is too long");
        return false;
    }
    return true;
}

static bool make_dir_if_needed(const char *path) {
#ifdef _WIN32
    if (_mkdir(path) == 0) {
        return true;
    }
#else
    if (mkdir(path, 0755) == 0) {
        return true;
    }
#endif
    return errno == EEXIST;
}

static bool ensure_parent_dirs(
    const char *path, char *error, int error_cap) {
    char temp[GAME_STORAGE_PATH_MAX];
    if (path == NULL ||
        snprintf(temp, sizeof temp, "%s", path) >= (int)sizeof temp) {
        set_error(error, error_cap, "storage path is too long");
        return false;
    }
    for (char *p = temp; *p != '\0'; ++p) {
        if (*p != '/' && *p != '\\') {
            continue;
        }
        if (p == temp || *(p - 1) == ':') {
            continue;
        }
        const char saved = *p;
        *p = '\0';
        if (!make_dir_if_needed(temp)) {
            if (error != NULL && error_cap > 0) {
                (void)snprintf(
                    error, (size_t)error_cap,
                    "failed to create storage directory: %s", temp);
            }
            return false;
        }
        *p = saved;
    }
    return true;
}

/* `<message> (os error N)`. Every failure path below used to produce a sentence
   with no CODE in it -- "failed to replace storage file" and nothing else --
   which is how a real intermittent write failure survived being seen twice and
   diagnosed zero times (T0055). A flag without a reason is not a report. */
/* The channel is part of the report, not decoration: on Windows `5` is
   ERROR_ACCESS_DENIED in one namespace and EIO in the other, `32` is
   ERROR_SHARING_VIOLATION or EPIPE. A bare "os error 5" tells the next reader
   the number and hides which table to look it up in. */
#ifdef _WIN32
#define STORAGE_OS_CHANNEL "win32 error"
#else
#define STORAGE_OS_CHANNEL "errno"
#endif

static void set_error_coded(
    char *error, int error_cap, const char *message, const char *channel, long code) {
    if (error == NULL || error_cap <= 0) {
        return;
    }
    (void)snprintf(error, (size_t)error_cap, "%s (%s %ld)", message, channel, code);
}

static void set_error_os(char *error, int error_cap, const char *message, long code) {
    set_error_coded(error, error_cap, message, STORAGE_OS_CHANNEL, code);
}

static void set_error_crt(char *error, int error_cap, const char *message, long code) {
    set_error_coded(error, error_cap, message, "errno", code);
}

/* Two error channels, because they carry different truths: the Win32 API calls
   (MoveFileEx) report through GetLastError, while the C stream calls (fopen,
   fwrite, fclose) report through errno. Reading the wrong one prints a stale or
   unrelated code, which is the failure mode T0055 was already suffering from
   one level up. On POSIX both are errno. */
static long last_os_error(void) {
#ifdef _WIN32
    return (long)GetLastError();
#else
    return (long)errno;
#endif
}

static long last_crt_error(void) {
    return (long)errno;
}

#ifdef _WIN32
/* T0055, CONFIRMED 2026-08-06 (run 41 of 400): MoveFileExA returned
   ERROR_ACCESS_DENIED replacing a plain file it had replaced successfully on the
   40 runs before it, with byte-identical inputs. A permanent condition -- ACL,
   read-only attribute, a directory in the way -- cannot fail 1 run in 30, so the
   refusal comes from OUTSIDE this process: on Windows another process holding a
   handle without FILE_SHARE_DELETE, or a destination in delete-pending state,
   makes the move fail rather than wait. An on-access virus scanner opening the
   temp file we just closed is the textbook producer of exactly that window;
   which process it is does not change the fix, because the condition clears by
   itself within milliseconds.

   Measured on this box: a held DESTINATION yields code 5, a held SOURCE yields
   code 32. Both are retried; both have a test.

   Retrying a PERMANENT failure would be a lie dressed as robustness, so the
   guards are explicit: only the two codes that mean "someone else is holding
   it", and never when the destination is a directory or read-only -- both of
   which also produce code 5 and never clear. Without that guard
   test_replace_failure_preserves_primary would sleep the entire budget to reach
   the verdict it reaches in one syscall today. */

/* Milliseconds to wait BEFORE each retry. The first two are 0 on purpose: the
   observed window is sub-millisecond, and a yield costs nothing. The rest are
   multiples of the ~15.6ms default timer tick, because Sleep(1) does not sleep
   1ms -- it sleeps a whole tick, so a 1/2/4/8 ramp is four indistinguishable
   15ms waits pretending to be precision. (The engine raises the timer
   resolution via timeBeginPeriod, but storage is linked into tools and tests
   that never initialise it, so the budget must not depend on that.) Worst case
   ~110ms of blocking -- and this runs on the frame thread, which is why it is
   this short: a failed autosave is retried again by game_save_tick a debounce
   later without blocking anything. */
static const DWORD kStorageRetryBackoffMs[] = {0, 0, 15, 30, 60};
#define STORAGE_RETRY_STEPS \
    ((int)(sizeof kStorageRetryBackoffMs / sizeof kStorageRetryBackoffMs[0]))

static bool refusal_can_clear(DWORD code) {
    return code == ERROR_ACCESS_DENIED || code == ERROR_SHARING_VIOLATION;
}

/* Conditions that produce the SAME codes and never clear. One probe, both
   attributes: a directory in the way, or a read-only destination (a save
   restored by a backup tool or pinned by a cloud-sync client). */
static bool destination_refuses_permanently(const char *path) {
    const DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false; /* absent: nothing permanent about it */
    }
    return (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY)) != 0;
}

/* true = caller should try again; this function has already waited. */
static bool wait_before_retry(DWORD code, int attempt, const char *destination) {
    if (attempt >= STORAGE_RETRY_STEPS || !refusal_can_clear(code) ||
        destination_refuses_permanently(destination)) {
        return false;
    }
    Sleep(kStorageRetryBackoffMs[attempt]);
    return true;
}

/* The only evidence that the transient refusal is real and self-clearing. If
   this line never appears in the wild, the retry is dead code and should be
   deleted. Throttled: an always-on scanner would otherwise print it on every
   autosave. */
static void note_refusal_absorbed(const char *what, const char *path, int refusals) {
    static long s_recoveries;
    ++s_recoveries;
    if (s_recoveries == 1 || (s_recoveries % 100) == 0) {
        nt_log_warn(
            "game_storage: %s of '%s' was refused %d time(s) and then succeeded "
            "(%ld such recoveries so far); another process was holding the path",
            what, path, refusals, s_recoveries);
    }
}
#endif

/* Every rename in this file goes through here. The first version of the T0055
   fix guarded only the replace, and 150 verification runs promptly caught the
   SAME refusal on the quarantine rename, which had its own bare rename() and
   did not even report a code. One door is not a fix. */
static bool move_file(const char *from, const char *to, bool replace_existing) {
#ifdef _WIN32
    const DWORD flags =
        MOVEFILE_WRITE_THROUGH | (replace_existing ? MOVEFILE_REPLACE_EXISTING : 0u);
    for (int attempt = 0;; ++attempt) {
        if (MoveFileExA(from, to, flags) != 0) {
            if (attempt > 0) {
                note_refusal_absorbed(replace_existing ? "replace" : "move", to, attempt);
            }
            return true;
        }
        const DWORD code = GetLastError();
        if (!wait_before_retry(code, attempt, to)) {
            SetLastError(code); /* the guards clobber it; the caller reports it */
            return false;
        }
    }
#else
    /* POSIX rename() is atomic and has no equivalent refusal -- no evidence of
       the T0055 failure here, so no retry here either. It replaces
       unconditionally; the quarantine caller passes a path it has just proven
       free, so the flag is not needed to get the same behaviour. */
    (void)replace_existing;
    return rename(from, to) == 0;
#endif
}

static bool replace_file(const char *temporary, const char *primary) {
    return move_file(temporary, primary, true);
}

static bool sync_parent_directory(const char *path) {
#ifdef _WIN32
    (void)path;
    /* MoveFileEx(...WRITE_THROUGH) is the durable replacement primitive. */
    return true;
#else
    /* The caller reports errno on failure, so the two failures that are not
       syscalls have to set one themselves -- otherwise the message carries a
       stale number from whatever ran last, which is the same disease this
       change exists to cure. */
    char directory[GAME_STORAGE_PATH_MAX];
    if (snprintf(directory, sizeof directory, "%s", path) >=
        (int)sizeof directory) {
        errno = ENAMETOOLONG;
        return false;
    }
    char *separator = strrchr(directory, '/');
    if (separator == NULL || separator == directory) {
        errno = EINVAL;
        return false;
    }
    *separator = '\0';
    const int descriptor = open(directory, O_RDONLY);
    if (descriptor < 0) {
        return false;
    }
    const int result = fsync(descriptor);
    const int fsync_errno = errno;
    (void)close(descriptor); /* must not overwrite fsync's errno */
    if (result != 0) {
        errno = fsync_errno;
        return false;
    }
    return true;
#endif
}

static bool flush_file_to_disk(FILE *file) {
    if (fflush(file) != 0) {
        return false;
    }
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}

static bool path_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat info;
    return stat(path, &info) == 0;
#endif
}

static bool readable_regular_file_exists(const char *path) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return false;
    }
#else
    struct stat info;
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return false;
    }
#endif
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    (void)fclose(file);
    return true;
}

static bool write_file_atomic(
    const char *temporary, const char *primary, const char *text,
    char *error, int error_cap) {
    if (!ensure_parent_dirs(primary, error, error_cap)) {
        return false;
    }
    /* NOTE: this fopen can in principle be refused by the same held handle that
       refuses the move. It has never been observed -- the three captures were
       all on a rename -- so it is deliberately NOT retried; guessing at
       failures nobody has seen is how the retry above would turn into
       decoration. If it ever fires, the message below now names it and carries
       the errno, which is exactly what made T0055 solvable. */
    FILE *file = fopen(temporary, "wb");
    if (file == NULL) {
        set_error_crt(error, error_cap, "failed to open storage temp file for write", last_crt_error());
        return false;
    }
    const size_t length = strlen(text);
    bool ok = fwrite(text, 1, length, file) == length;
    if (ok) {
        ok = flush_file_to_disk(file);
    }
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        /* Capture BEFORE the cleanup: remove() issues its own syscall and
           overwrites both errno and the thread's last-error value, so reading
           the code afterwards can report the cleanup's outcome as the cause. */
        const long code = last_crt_error();
        (void)remove(temporary);
        set_error_crt(error, error_cap, "failed to write storage temp file", code);
        return false;
    }
    if (!replace_file(temporary, primary)) {
        const long code = last_os_error(); /* see above: before remove() */
        (void)remove(temporary);
        set_error_os(error, error_cap, "failed to replace storage file", code);
        return false;
    }
    if (!sync_parent_directory(primary)) {
        set_error_os(error, error_cap, "failed to sync storage directory", last_os_error());
        return false;
    }
    return true;
}

static bool read_file_bytes(
    const char *path, char **out, game_storage_read_status_t *status,
    char *error, int error_cap) {
    if (status != NULL) {
        *status = GAME_STORAGE_READ_ERROR;
    }
    errno = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            if (status != NULL) {
                *status = GAME_STORAGE_READ_ABSENT;
            }
            set_error(error, error_cap, "no storage file for slot");
        } else {
            set_error(error, error_cap, "failed to open storage file for read");
        }
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        (void)fclose(file);
        set_error(error, error_cap, "failed to seek storage file");
        return false;
    }
    const long size = ftell(file);
    if (size < 0 || (unsigned long)size > (unsigned long)GAME_STORAGE_MAX_BYTES) {
        (void)fclose(file);
        set_error(error, error_cap, "storage file is too large");
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        set_error(error, error_cap, "failed to rewind storage file");
        return false;
    }
    char *data = malloc((size_t)size + 1U);
    if (data == NULL) {
        (void)fclose(file);
        set_error(error, error_cap, "failed to allocate storage buffer");
        return false;
    }
    const size_t bytes_read = fread(data, 1, (size_t)size, file);
    (void)fclose(file);
    if (bytes_read != (size_t)size) {
        free(data);
        set_error(error, error_cap, "failed to read storage file");
        return false;
    }
    data[size] = '\0';
    *out = data;
    if (status != NULL) {
        *status = GAME_STORAGE_READ_OK;
    }
    return true;
}

static bool resolve_native_paths(
    const char *slot, game_storage_native_paths_t *paths,
    char *error, int error_cap) {
    if (!is_safe_segment(slot)) {
        set_error(error, error_cap, "storage slot must be a simple name");
        return false;
    }
    if (!is_safe_segment(GAME_STORAGE_APP_ID)) {
        set_error(error, error_cap, "GAME_STORAGE_APP_ID must be a simple name");
        return false;
    }
    char root[GAME_STORAGE_PATH_MAX];
    if (!resolve_storage_root(root, sizeof root, error, error_cap) ||
        snprintf(paths->directory, sizeof paths->directory, "%s", root) >=
            (int)sizeof paths->directory ||
        snprintf(paths->primary, sizeof paths->primary,
                 "%s/%s.json", paths->directory, slot) >=
            (int)sizeof paths->primary ||
        snprintf(paths->primary_tmp, sizeof paths->primary_tmp,
                 "%s.tmp", paths->primary) >= (int)sizeof paths->primary_tmp ||
        snprintf(paths->backup, sizeof paths->backup,
                 "%s/%s.bak", paths->directory, slot) >=
            (int)sizeof paths->backup ||
        snprintf(paths->backup_tmp, sizeof paths->backup_tmp,
                 "%s.tmp", paths->backup) >= (int)sizeof paths->backup_tmp ||
        snprintf(paths->quarantine_base, sizeof paths->quarantine_base,
                 "%s/%s.corrupt", paths->directory, slot) >=
            (int)sizeof paths->quarantine_base) {
        set_error(error, error_cap, "resolved storage path is too long");
        return false;
    }
    return true;
}

static bool resolve_quarantine_path(
    const game_storage_native_paths_t *paths, char *out, size_t out_cap,
    char *error, int error_cap) {
    for (int attempt = 0; attempt < GAME_STORAGE_QUARANTINE_MAX_FILES; ++attempt) {
        const int written =
            attempt == 0
                ? snprintf(
                      out, out_cap, "%s", paths->quarantine_base)
                : snprintf(
                      out, out_cap, "%s-%d", paths->quarantine_base, attempt);
        if (written < 0 || written >= (int)out_cap) {
            set_error(error, error_cap, "resolved quarantine path is too long");
            return false;
        }
        if (!path_exists(out)) {
            return true;
        }
    }
    set_error(
        error, error_cap, "quarantine retention limit reached for this slot");
    return false;
}

static bool quarantine_unreadable_copy(const char *slot) {
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, NULL, 0)) {
        return false;
    }
    FILE *source = fopen(paths.primary, "rb");
    if (source == NULL) {
        return false;
    }
    char quarantine_path[GAME_STORAGE_PATH_MAX];
    if (!resolve_quarantine_path(
            &paths, quarantine_path, sizeof quarantine_path, NULL, 0)) {
        (void)fclose(source);
        return false;
    }
    FILE *destination = fopen(quarantine_path, "wb");
    if (destination == NULL) {
        (void)fclose(source);
        return false;
    }
    char buffer[8192];
    bool failed = false;
    size_t count;
    while ((count = fread(buffer, 1, sizeof buffer, source)) > 0) {
        if (fwrite(buffer, 1, count, destination) != count) {
            failed = true;
            break;
        }
    }
    const bool source_ok = ferror(source) == 0;
    (void)fclose(source);
    const bool destination_stream_ok =
        ferror(destination) == 0 && flush_file_to_disk(destination);
    const bool destination_closed = fclose(destination) == 0;
    const bool destination_ok =
        destination_stream_ok && destination_closed;
    if (failed || !source_ok || !destination_ok) {
        (void)remove(quarantine_path);
        return false;
    }
    if (!sync_parent_directory(quarantine_path)) {
        (void)remove(quarantine_path);
        return false;
    }
    return true;
}

bool game_storage_backend_write(
    const char *slot, const char *text, char *error, int error_cap) {
    if (text == NULL || strlen(text) > (size_t)GAME_STORAGE_MAX_BYTES) {
        set_error(error, error_cap, "storage text is too large");
        return false;
    }
    game_storage_native_paths_t paths;
    return resolve_native_paths(slot, &paths, error, error_cap) &&
           write_file_atomic(
               paths.primary_tmp, paths.primary, text, error, error_cap);
}

bool game_storage_backend_read(
    const char *slot, char **out, game_storage_read_status_t *status,
    char *error, int error_cap) {
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    game_storage_read_status_t read_status = GAME_STORAGE_READ_ERROR;
    const bool ok = read_file_bytes(
        paths.primary, out, &read_status, error, error_cap);
    if (!ok && read_status == GAME_STORAGE_READ_ERROR) {
        const bool copied = quarantine_unreadable_copy(slot);
        read_status = copied
            ? GAME_STORAGE_READ_ERROR
            : GAME_STORAGE_READ_ERROR_PRESERVED;
        nt_log_warn(
            copied
                ? "game_storage: read slot '%s' failed (%s); quarantine copy verified"
                : "game_storage: read slot '%s' failed (%s); primary preserved, quarantine unavailable",
            slot, error != NULL ? error : "");
    }
    if (status != NULL) {
        *status = read_status;
    }
    return ok;
}

bool game_storage_backend_exists(const char *slot) {
    game_storage_native_paths_t paths;
    return resolve_native_paths(slot, &paths, NULL, 0) &&
           readable_regular_file_exists(paths.primary);
}

bool game_storage_backend_write_backup(
    const char *slot, char *error, int error_cap) {
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    if (!readable_regular_file_exists(paths.primary)) {
        return true;
    }
    char *data = NULL;
    if (!read_file_bytes(paths.primary, &data, NULL, error, error_cap)) {
        return false;
    }
    const bool ok = write_file_atomic(
        paths.backup_tmp, paths.backup, data, error, error_cap);
    free(data);
    return ok;
}

bool game_storage_backend_read_backup(
    const char *slot, char **out, char *error, int error_cap) {
    game_storage_native_paths_t paths;
    return resolve_native_paths(slot, &paths, error, error_cap) &&
           read_file_bytes(paths.backup, out, NULL, error, error_cap);
}

bool game_storage_backend_quarantine(
    const char *slot, char *error, int error_cap) {
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    if (!readable_regular_file_exists(paths.primary)) {
        set_error(error, error_cap, "no primary to quarantine");
        return false;
    }
    char quarantine_path[GAME_STORAGE_PATH_MAX];
    if (!resolve_quarantine_path(
            &paths, quarantine_path, sizeof quarantine_path,
            error, error_cap)) {
        return false;
    }
    /* Was a bare rename() with a codeless message -- which is precisely how the
       T0055 verification run caught this path and could say nothing about it
       beyond "false". */
    if (!move_file(paths.primary, quarantine_path, false)) {
        set_error_os(error, error_cap, "failed to quarantine storage file", last_os_error());
        return false;
    }
    if (!sync_parent_directory(paths.primary)) {
        set_error_os(error, error_cap, "failed to sync storage directory", last_os_error());
        return false;
    }
    return true;
}

bool game_storage_backend_probe(char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    return true;
}
