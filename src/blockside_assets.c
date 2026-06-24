#include "blockside_assets.h"

#include "nt_pack_format.h"

#include <stdio.h>

nt_hash64_t rid(const char *s) {
    return nt_hash64_str(s);
}

nt_resource_t mesh_request(const char *name) {
    char buf[160];
    (void)snprintf(buf, sizeof(buf), "%s/0_0", name);
    return nt_resource_request(rid(buf), NT_ASSET_MESH);
}

nt_resource_t mesh_request_part(const char *name, uint32_t mesh_index, uint32_t prim_index) {
    char buf[160];
    (void)snprintf(buf, sizeof(buf), "%s/%u_%u", name, mesh_index, prim_index);
    return nt_resource_request(rid(buf), NT_ASSET_MESH);
}
