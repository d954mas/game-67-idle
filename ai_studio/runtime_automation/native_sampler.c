#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_T 256
#define MAX_S 4000000
#define DEPTH 40
static DWORD64 (*stacks)[DEPTH];
static unsigned char *depth;
static long nstack;
static HANDLE g_proc;

typedef struct { DWORD tid; HANDLE h; long samples; } Th;
static Th th[MAX_T];
static int nth;
static DWORD64 *rips;
static unsigned short *who;
static long ns;
static int main_idx = -1;
static volatile LONG g_stop;

// Only sets a flag: the loop exits between samples, so no thread is left suspended.
static BOOL WINAPI on_ctrl(DWORD type) {
    (void)type;
    g_stop = 1;
    return TRUE;
}

static unsigned long long created_at(HANDLE h) {
    FILETIME c, e, k, u;
    if (!GetThreadTimes(h, &c, &e, &k, &u)) return ~0ull;
    return ((unsigned long long)c.dwHighDateTime << 32) | c.dwLowDateTime;
}

// The earliest-created thread is the process main thread; a forced tid overrides it.
static int pick_main(DWORD forced_tid) {
    int best = -1;
    unsigned long long best_t = ~0ull;
    for (int i = 0; i < nth; ++i) {
        if (forced_tid) {
            if (th[i].tid == forced_tid) return i;
            continue;
        }
        unsigned long long t = created_at(th[i].h);
        if (best < 0 || t < best_t) { best = i; best_t = t; }
    }
    return best;
}

static void refresh(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = {.dwSize = sizeof te};
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        int known = 0;
        for (int i = 0; i < nth; ++i) if (th[i].tid == te.th32ThreadID) known = 1;
        if (known || nth >= MAX_T) continue;
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
        if (h) th[nth++] = (Th){te.th32ThreadID, h, 0};
    }
    CloseHandle(snap);
}

typedef struct { char name[256]; long self; } Fn;
static int cmp_fn(const void *a, const void *b) {
    const Fn *x = a, *y = b;
    return (y->self > x->self) - (y->self < x->self);
}

static void report(HANDLE proc, int only_thread, const char *title) {
    Fn *fns = calloc(65536, sizeof(Fn));
    int nf = 0; long total = 0;
    for (long i = 0; i < ns; ++i) {
        if (only_thread >= 0 && who[i] != only_thread) continue;
        ++total;
        char buf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO *si = (SYMBOL_INFO *)buf;
        si->SizeOfStruct = sizeof(SYMBOL_INFO);
        si->MaxNameLen = 255;
        DWORD64 disp = 0;
        char name[256];
        if (SymFromAddr(proc, rips[i], &disp, si)) {
            IMAGEHLP_MODULE64 mi = {.SizeOfStruct = sizeof mi};
            const char *mod = SymGetModuleInfo64(proc, rips[i], &mi) ? mi.ModuleName : "?";
            snprintf(name, sizeof name, "%s!%s", mod, si->Name);
        } else {
            IMAGEHLP_MODULE64 mi = {.SizeOfStruct = sizeof mi};
            snprintf(name, sizeof name, "%s!?", SymGetModuleInfo64(proc, rips[i], &mi) ? mi.ModuleName : "unknown");
        }
        int f = 0;
        for (; f < nf; ++f) if (strcmp(fns[f].name, name) == 0) break;
        if (f == nf && nf < 65535) { strcpy(fns[nf].name, name); ++nf; }
        fns[f].self++;
    }
    qsort(fns, (size_t)nf, sizeof(Fn), cmp_fn);
    printf("== %s: %ld samples\n", title, total);
    for (int f = 0; f < nf && f < 45; ++f)
        printf("%6.2f%%  %s\n", 100.0 * fns[f].self / (total ? total : 1), fns[f].name);
    free(fns);
}

static void name_of(HANDLE proc, DWORD64 a, char *name) {
    char buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO *si = (SYMBOL_INFO *)buf;
    si->SizeOfStruct = sizeof(SYMBOL_INFO);
    si->MaxNameLen = 255;
    DWORD64 disp = 0;
    if (SymFromAddr(proc, a, &disp, si)) snprintf(name, 256, "%s", si->Name);
    else snprintf(name, 256, "?%llx", (unsigned long long)(a >> 12));
}

static void inclusive(HANDLE proc) {
    Fn *fns = calloc(65536, sizeof(Fn));
    int nf = 0; long busy = 0;
    for (long i = 0; i < nstack; ++i) {
        char top[256];
        if (depth[i] == 0) continue;
        name_of(proc, stacks[i][0], top);
        if (strstr(top, "Wait") || strstr(top, "DelayExecution") || strstr(top, "QueryPerformanceCounter") || strstr(top, "nt_time_now")) continue;
        ++busy;
        char seen[DEPTH][256]; int nseen = 0;
        for (int d = 0; d < depth[i]; ++d) {
            char name[256];
            name_of(proc, stacks[i][d], name);
            int dup = 0;
            for (int k = 0; k < nseen; ++k) if (!strcmp(seen[k], name)) dup = 1;
            if (dup) continue;
            strcpy(seen[nseen++], name);
            int f = 0;
            for (; f < nf; ++f) if (strcmp(fns[f].name, name) == 0) break;
            if (f == nf && nf < 65535) { strcpy(fns[nf].name, name); ++nf; }
            fns[f].self++;
        }
    }
    qsort(fns, (size_t)nf, sizeof(Fn), cmp_fn);
    FILE *fold = getenv("SAMPLER_FOLDED") ? fopen(getenv("SAMPLER_FOLDED"), "w") : NULL;
    for (long i = 0; fold && i < nstack; ++i) {
        for (int d = depth[i] - 1; d >= 0; --d) {
            char name[256];
            name_of(proc, stacks[i][d], name);
            fprintf(fold, "%s%s", name, d ? ";" : "\n");
        }
    }
    if (fold) fclose(fold);
    printf("== main thread inclusive, busy samples %ld of %ld\n", busy, nstack);
    for (int f = 0; f < nf && f < 90; ++f)
        printf("%6.2f%%  %s\n", 100.0 * fns[f].self / (busy ? busy : 1), fns[f].name);
    free(fns);
}

static void lines_report(HANDLE proc) {
    SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES);
    Fn *fns = calloc(65536, sizeof(Fn));
    int nf = 0; long busy = 0;
    for (long i = 0; i < nstack; ++i) {
        if (depth[i] == 0) continue;
        char top[256];
        name_of(proc, stacks[i][0], top);
        if (strstr(top, "Wait") || strstr(top, "DelayExecution") || strstr(top, "QueryPerformanceCounter") || strstr(top, "nt_time_now")) continue;
        ++busy;
        IMAGEHLP_LINE64 line = {.SizeOfStruct = sizeof line};
        DWORD disp = 0;
        char name[256];
        if (SymGetLineFromAddr64(proc, stacks[i][0], &disp, &line)) {
            const char *file = strrchr(line.FileName, 92);
            snprintf(name, sizeof name, "%s %s:%lu", top, file ? file + 1 : line.FileName, line.LineNumber);
        } else snprintf(name, sizeof name, "%s ?", top);
        int f = 0;
        for (; f < nf; ++f) if (strcmp(fns[f].name, name) == 0) break;
        if (f == nf && nf < 65535) { strcpy(fns[nf].name, name); ++nf; }
        fns[f].self++;
    }
    qsort(fns, (size_t)nf, sizeof(Fn), cmp_fn);
    printf("== main thread self by line, busy %ld\n", busy);
    for (int f = 0; f < nf && f < 120; ++f)
        printf("%6.2f%%  %s\n", 100.0 * fns[f].self / (busy ? busy : 1), fns[f].name);
    free(fns);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: native_sampler <pid> <seconds> [interval_us] [tid]\n"); return 2; }
    DWORD pid = (DWORD)atoi(argv[1]);
    double seconds = atof(argv[2]);
    int interval_us = argc > 3 ? atoi(argv[3]) : 1000;
    DWORD forced_tid = argc > 4 ? (DWORD)atoi(argv[4]) : 0;
    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!proc) { printf("open failed %lu\n", GetLastError()); return 1; }
    rips = malloc(sizeof(DWORD64) * MAX_S);
    stacks = malloc(sizeof(*stacks) * (MAX_S / 4));
    depth = malloc(MAX_S / 4);
    g_proc = proc;
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    // No symbol path while sampling: a multi-GB PDB load inside the loop stalls it.
    SymInitialize(proc, ".", TRUE);
    who = malloc(sizeof(unsigned short) * MAX_S);
    refresh(pid);
    main_idx = pick_main(forced_tid);
    if (main_idx < 0) { fprintf(stderr, "no matching thread in pid %lu\n", pid); return 1; }
    SetConsoleCtrlHandler(on_ctrl, TRUE);
    timeBeginPeriod(1);
    LARGE_INTEGER f, t0, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    long tick = 0;
    for (;;) {
        QueryPerformanceCounter(&t);
        if (g_stop || (double)(t.QuadPart - t0.QuadPart) / f.QuadPart > seconds) break;
        if (tick++ % 200 == 0) refresh(pid);
        for (int i = 0; i < nth && ns < MAX_S; ++i) {
            if (g_stop) break;
            if (SuspendThread(th[i].h) == (DWORD)-1) continue;
            CONTEXT c = {.ContextFlags = CONTEXT_CONTROL};
            if (GetThreadContext(th[i].h, &c)) {
                // Stack walks are costly while the thread is suspended, so only the main thread is walked.
                if (i == main_idx && nstack < MAX_S / 4) {
                    CONTEXT full = {.ContextFlags = CONTEXT_FULL};
                    if (GetThreadContext(th[i].h, &full)) {
                        STACKFRAME64 sf = {0};
                        sf.AddrPC.Offset = full.Rip; sf.AddrPC.Mode = AddrModeFlat;
                        sf.AddrFrame.Offset = full.Rbp; sf.AddrFrame.Mode = AddrModeFlat;
                        sf.AddrStack.Offset = full.Rsp; sf.AddrStack.Mode = AddrModeFlat;
                        int d = 0;
                        while (d < DEPTH && StackWalk64(IMAGE_FILE_MACHINE_AMD64, g_proc, th[i].h, &sf, &full, NULL,
                                   SymFunctionTableAccess64, SymGetModuleBase64, NULL) && sf.AddrPC.Offset) {
                            stacks[nstack][d++] = sf.AddrPC.Offset;
                        }
                        depth[nstack++] = (unsigned char)d;
                    }
                }
                rips[ns] = c.Rip;
                who[ns] = (unsigned short)i;
                ++ns;
                th[i].samples++;
            }
            ResumeThread(th[i].h);
        }
        LARGE_INTEGER w;
        do { QueryPerformanceCounter(&w); } while ((double)(w.QuadPart - t.QuadPart) * 1e6 / f.QuadPart < interval_us);
    }
    SymCleanup(proc);
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, NULL, TRUE);
    SymRefreshModuleList(proc);
    // Symbols load only now: see the SymInitialize note above.
    report(proc, -1, "all threads (incl. waits)");
    for (int i = 0; i < nth; ++i) printf("thread %d tid %lu samples %ld%s\n", i, th[i].tid, th[i].samples, i == main_idx ? " (main)" : "");
    report(proc, main_idx, "main thread");
    inclusive(proc);
    lines_report(proc);
    FILE *rf = getenv("SAMPLER_RIPS") ? fopen(getenv("SAMPLER_RIPS"), "w") : NULL;
    for (long i = 0; rf && i < nstack; ++i) {
        if (depth[i] == 0) continue;
        IMAGEHLP_MODULE64 mi = {.SizeOfStruct = sizeof mi};
        if (!SymGetModuleInfo64(proc, stacks[i][0], &mi)) continue;
        fprintf(rf, "%s %llx\n", mi.ModuleName, (unsigned long long)(stacks[i][0] - mi.BaseOfImage));
    }
    if (rf) fclose(rf);
    return 0;
}
