#ifndef ONLINE_ROOM_META_ENTRY_NT_HTTP_H
#define ONLINE_ROOM_META_ENTRY_NT_HTTP_H

#include "online_room/meta_entry.h"

// The metaserver entry's calls over the engine's HTTP client, which the host
// initializes and pumps (nt_resource_step pumps it in the client).
extern const online_meta_entry_http online_meta_entry_nt_http;

#endif
