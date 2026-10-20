#ifndef ONLINE_ROOM_CLOSE_H
#define ONLINE_ROOM_CLOSE_H

// The WebSocket close codes a room sends a player away with, and the client's
// connection ladder (online_room/entry.h) reads.
#define ONLINE_ROOM_CLOSE_REENTER 4100U        // full, draining, a refused ticket, or the profile not had in time
#define ONLINE_ROOM_CLOSE_SUPERSEDED 4101U     // the account's newer connection, or another room, has it
#define ONLINE_ROOM_CLOSE_PROFILE_NEWER 4102U  // a newer build wrote the profile
#define ONLINE_ROOM_CLOSE_PROFILE_BROKEN 4103U // the stored profile does not read

#endif
