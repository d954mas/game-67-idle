#ifndef FEATURES_SCENES_INTERNAL_SCENE_ID_H
#define FEATURES_SCENES_INTERNAL_SCENE_ID_H

#include "features/scenes/scene_manager.h"

#include <stdbool.h>
#include <stddef.h>

static inline bool scene_id_is_valid(const char *id) {
    size_t length = 0;
    unsigned char ch;

    if (id == NULL || id[0] == '\0') {
        return false;
    }
    ch = (unsigned char)id[0];
    if (!((ch >= (unsigned char)'a' && ch <= (unsigned char)'z') ||
          ch == (unsigned char)'_')) {
        return false;
    }
    for (; id[length] != '\0'; ++length) {
        ch = (unsigned char)id[length];
        if (length >= SCENE_ID_MAX_LENGTH ||
            !((ch >= (unsigned char)'a' && ch <= (unsigned char)'z') ||
              (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') ||
              ch == (unsigned char)'.' || ch == (unsigned char)'_' ||
              ch == (unsigned char)'-')) {
            return false;
        }
    }
    return length > 0;
}

#endif
