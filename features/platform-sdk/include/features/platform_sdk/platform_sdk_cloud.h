#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_CLOUD_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_CLOUD_H

#include <stdbool.h>

/* Portal-side player storage: one text document per key, carried by the
   account rather than by the browser. The portal answers asynchronously and
   the game runs on one thread, so the read is started early and its result is
   polled at a barrier the game already waits on. */

typedef enum platform_sdk_cloud_status {
    PLATFORM_SDK_CLOUD_IDLE = 0,   /* nothing has been asked for yet */
    PLATFORM_SDK_CLOUD_PENDING,    /* the portal has not answered */
    PLATFORM_SDK_CLOUD_READY,      /* a document came back; take it */
    PLATFORM_SDK_CLOUD_EMPTY,      /* the portal has no document for this player */
    PLATFORM_SDK_CLOUD_UNAVAILABLE /* this build or this portal has no storage */
} platform_sdk_cloud_status_t;

/* False on every target whose backend cannot carry player data; the caller
   then stays on local storage alone and says nothing to the player. */
bool platform_sdk_cloud_supported(void);

/* Starts one read. Calling it again replaces the pending read. */
void platform_sdk_cloud_load(const char *key);

platform_sdk_cloud_status_t platform_sdk_cloud_status(void);

/* The document, transferred to the caller (free it), or NULL unless the status
   is READY. Taking it moves the status to EMPTY: a document is handed over
   once. */
char *platform_sdk_cloud_take(void);

/* Fire and forget. The bridge keeps at most one write in flight and sends only
   the newest text that arrived while one was running, because portals rate
   limit player data far below an autosave's cadence. */
void platform_sdk_cloud_store(const char *key, const char *text);

#endif /* FEATURES_PLATFORM_SDK_PLATFORM_SDK_CLOUD_H */
