#include "features/items/items.h"

#include "nt_pack_format.h"
#include "resource/nt_resource.h"

static bool bind_resource_error(
    items_catalog_bind_error_t error,
    items_catalog_bind_error_t *out_error) {
    if (out_error != NULL) *out_error = error;
    return false;
}

bool items_catalog_try_bind_resource(
    uint64_t asset_id, items_catalog_bind_error_t *out_error) {
    if (asset_id == 0) {
        return bind_resource_error(ITEMS_CATALOG_BIND_RESOURCE_MISSING, out_error);
    }

    const nt_resource_t resource = nt_resource_find((nt_hash64_t){asset_id});
    if (resource.id == NT_RESOURCE_INVALID.id) {
        return bind_resource_error(ITEMS_CATALOG_BIND_RESOURCE_MISSING, out_error);
    }
    if (nt_resource_get_asset_type(resource) != NT_ASSET_BLOB) {
        return bind_resource_error(ITEMS_CATALOG_BIND_RESOURCE_WRONG_TYPE, out_error);
    }
    if (!nt_resource_is_ready(resource)) {
        return bind_resource_error(ITEMS_CATALOG_BIND_RESOURCE_NOT_READY, out_error);
    }

    uint32_t byte_count = 0;
    const uint8_t *bytes = nt_resource_get_blob(resource, &byte_count);
    if (bytes == NULL || byte_count == 0) {
        return bind_resource_error(ITEMS_CATALOG_BIND_BAD_HEADER, out_error);
    }
    return items_catalog_try_bind(bytes, byte_count, out_error);
}
