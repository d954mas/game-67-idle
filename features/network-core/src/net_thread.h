#ifndef NETWORK_CORE_NET_THREAD_H
#define NETWORK_CORE_NET_THREAD_H

/* The one thread and one lock the native client needs; kept here so the
   feature builds with nothing but the C library on either platform. */

#include <stdbool.h>
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct net_mutex_t { CRITICAL_SECTION section; } net_mutex_t;
typedef struct net_thread_t { HANDLE handle; } net_thread_t;

static inline void net_mutex_init(net_mutex_t *mutex) { InitializeCriticalSection(&mutex->section); }
static inline void net_mutex_free(net_mutex_t *mutex) { DeleteCriticalSection(&mutex->section); }
static inline void net_mutex_lock(net_mutex_t *mutex) { EnterCriticalSection(&mutex->section); }
static inline void net_mutex_unlock(net_mutex_t *mutex) { LeaveCriticalSection(&mutex->section); }

typedef struct net_thread_start_t { void (*run)(void *); void *arg; } net_thread_start_t;

static DWORD WINAPI net_thread_trampoline(LPVOID raw) {
    net_thread_start_t *start = (net_thread_start_t *)raw;
    void (*run)(void *) = start->run;
    void *arg = start->arg;
    free(start);
    run(arg);
    return 0;
}

static inline bool net_thread_start(net_thread_t *thread, void (*run)(void *), void *arg) {
    net_thread_start_t *start = (net_thread_start_t *)malloc(sizeof *start);
    if (start == NULL) { return false; }
    start->run = run;
    start->arg = arg;
    thread->handle = CreateThread(NULL, 0, net_thread_trampoline, start, 0, NULL);
    if (thread->handle == NULL) { free(start); }
    return thread->handle != NULL;
}

static inline void net_thread_join(net_thread_t *thread) {
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    thread->handle = NULL;
}

#else
#include <pthread.h>

typedef struct net_mutex_t { pthread_mutex_t mutex; } net_mutex_t;
typedef struct net_thread_t { pthread_t handle; } net_thread_t;

static inline void net_mutex_init(net_mutex_t *mutex) { pthread_mutex_init(&mutex->mutex, NULL); }
static inline void net_mutex_free(net_mutex_t *mutex) { pthread_mutex_destroy(&mutex->mutex); }
static inline void net_mutex_lock(net_mutex_t *mutex) { pthread_mutex_lock(&mutex->mutex); }
static inline void net_mutex_unlock(net_mutex_t *mutex) { pthread_mutex_unlock(&mutex->mutex); }

typedef struct net_thread_start_t { void (*run)(void *); void *arg; } net_thread_start_t;

static void *net_thread_trampoline(void *raw) {
    net_thread_start_t *start = (net_thread_start_t *)raw;
    void (*run)(void *) = start->run;
    void *arg = start->arg;
    free(start);
    run(arg);
    return NULL;
}

static inline bool net_thread_start(net_thread_t *thread, void (*run)(void *), void *arg) {
    net_thread_start_t *start = (net_thread_start_t *)malloc(sizeof *start);
    if (start == NULL) { return false; }
    start->run = run;
    start->arg = arg;
    if (pthread_create(&thread->handle, NULL, net_thread_trampoline, start) != 0) {
        free(start);
        return false;
    }
    return true;
}

static inline void net_thread_join(net_thread_t *thread) { pthread_join(thread->handle, NULL); }

#endif

#endif
