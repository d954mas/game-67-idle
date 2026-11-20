#ifndef FEATURES_STEAM_PLATFORM_COMMON_H
#define FEATURES_STEAM_PLATFORM_COMMON_H

#include "features/steam_platform/steam_platform.h"

/* The parts of the module that need no Steam, compiled into both builds so the always-built
   test covers them. Internal to the feature. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct steam_platform_event_queue {
    steam_platform_event_t items[STEAM_PLATFORM_EVENT_CAPACITY];
    unsigned head;
    unsigned count;
    bool overlay_active; /* last state the game heard; the overlay starts closed */
    uint64_t dropped;
} steam_platform_event_queue_t;

/* Empties the queue and forgets the overlay state; `dropped` keeps counting. */
void steam_platform_queue_reset(steam_platform_event_queue_t *queue);

/* Queues an OVERLAY event only when `active` differs from the last one queued. */
void steam_platform_queue_overlay(steam_platform_event_queue_t *queue, bool active);

/* Oldest first. False when empty or `out` is NULL; then nothing is taken. */
bool steam_platform_queue_pop(steam_platform_event_queue_t *queue, steam_platform_event_t *out);

/* READY when `config` may start; otherwise FAILED with the reason in `message`. */
steam_platform_status_t steam_platform_check_config(const steam_platform_config_t *config, char *message, size_t message_size);

/* The dialog ActivateGameOverlay gets: `dialog` unchanged, or "friends" for NULL. */
const char *steam_platform_overlay_dialog(const char *dialog);

/* Copies `text` cut to fit and terminated; nothing when `message` is NULL or `size` zero. */
void steam_platform_write_message(char *message, size_t message_size, const char *text);

#ifdef __cplusplus
}
#endif

#endif
