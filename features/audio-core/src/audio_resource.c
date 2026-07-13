#include "audio_backend.h"

#include "nt_pack_format.h"
#include "resource/nt_resource.h"

enum {
    AUDIO_RESOURCE_MISSING = 0,
    AUDIO_RESOURCE_NOT_READY = 1,
    AUDIO_RESOURCE_READY_BLOB = 2,
    AUDIO_RESOURCE_WRONG_TYPE = 3,
};

uint32_t audio_core_resource_state(uint64_t asset_id) {
    if (asset_id == 0) return AUDIO_RESOURCE_MISSING;

    const nt_resource_t resource = nt_resource_find((nt_hash64_t){asset_id});
    if (resource.id == NT_RESOURCE_INVALID.id) return AUDIO_RESOURCE_MISSING;
    if (!nt_resource_is_ready(resource)) return AUDIO_RESOURCE_NOT_READY;
    if (nt_resource_get_asset_type(resource) != NT_ASSET_BLOB) return AUDIO_RESOURCE_WRONG_TYPE;
    return AUDIO_RESOURCE_READY_BLOB;
}

bool audio_core_resource_blob_view(uint64_t asset_id, const void **bytes, uint32_t *size) {
    if (bytes == NULL || size == NULL) return false;
    *bytes = NULL;
    *size = 0;
    if (audio_core_resource_state(asset_id) != AUDIO_RESOURCE_READY_BLOB) return false;

    const nt_resource_t resource = nt_resource_find((nt_hash64_t){asset_id});
    const uint8_t *blob = nt_resource_get_blob(resource, size);
    if (blob == NULL || *size == 0) {
        *size = 0;
        return false;
    }
    *bytes = blob;
    return true;
}
