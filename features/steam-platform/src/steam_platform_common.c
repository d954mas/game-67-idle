#include "steam_platform_common.h"

#include <stdio.h>

void steam_platform_queue_reset(steam_platform_event_queue_t *queue) {
    queue->head = 0;
    queue->count = 0;
    queue->overlay_active = false;
}

static void push(steam_platform_event_queue_t *queue, steam_platform_event_t event) {
    if (queue->count == STEAM_PLATFORM_EVENT_CAPACITY) {
        queue->head = (queue->head + 1U) % STEAM_PLATFORM_EVENT_CAPACITY;
        queue->count--;
        queue->dropped++;
    }
    queue->items[(queue->head + queue->count) % STEAM_PLATFORM_EVENT_CAPACITY] = event;
    queue->count++;
}

void steam_platform_queue_overlay(steam_platform_event_queue_t *queue, bool active) {
    /* Steam may report the same state twice in a row; the game hears changes only. */
    if (active == queue->overlay_active) {
        return;
    }
    queue->overlay_active = active;
    push(queue, (steam_platform_event_t){.type = STEAM_PLATFORM_EVENT_OVERLAY, .active = active});
}

bool steam_platform_queue_pop(steam_platform_event_queue_t *queue, steam_platform_event_t *out) {
    if (out == NULL || queue->count == 0) {
        return false;
    }
    *out = queue->items[queue->head];
    queue->head = (queue->head + 1U) % STEAM_PLATFORM_EVENT_CAPACITY;
    queue->count--;
    return true;
}

steam_platform_status_t steam_platform_check_config(const steam_platform_config_t *config, char *message, size_t message_size) {
    if (config == NULL) {
        steam_platform_write_message(message, message_size, "config is NULL");
        return STEAM_PLATFORM_FAILED;
    }
    if (config->restart_through_steam && config->app_id == 0) {
        steam_platform_write_message(message, message_size, "restart_through_steam needs a non-zero app_id");
        return STEAM_PLATFORM_FAILED;
    }
    return STEAM_PLATFORM_READY;
}

const char *steam_platform_overlay_dialog(const char *dialog) { return dialog != NULL ? dialog : "friends"; }

void steam_platform_write_message(char *message, size_t message_size, const char *text) {
    if (message != NULL && message_size != 0) {
        snprintf(message, message_size, "%s", text);
    }
}
