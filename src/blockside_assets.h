#ifndef BLOCKSIDE_ASSETS_H
#define BLOCKSIDE_ASSETS_H

#include "hash/nt_hash.h"
#include "resource/nt_resource.h"

#include <stdint.h>

nt_hash64_t rid(const char *s);
nt_resource_t mesh_request(const char *name);
nt_resource_t mesh_request_part(const char *name, uint32_t mesh_index, uint32_t prim_index);

#endif /* BLOCKSIDE_ASSETS_H */
