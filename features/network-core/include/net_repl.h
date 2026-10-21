#ifndef NETWORK_CORE_NET_REPL_H
#define NETWORK_CORE_NET_REPL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The room's side of replication: which events each viewer receives and the
   packet that carries them each snapshot tick. The game owns the event
   encodings, the row section and what a district means.

   Delivery model. The connection is one ordered stream and a packet is
   written whole or not at all.
   - When the transport cannot take a packet (send not ready) the tick is
     skipped for that viewer: its baseline stays, the reliable events of that
     tick wait in its outbox, and its cosmetic events are counted as dropped.
   - Reliable events wait in a bounded outbox until a sent packet carries
     them; an outbox that overflows closes the connection for a resync.
   - A lost connection loses the reliable events in flight. A reconnect
     resets the viewer (net_repl_reset) and the full state restores the
     truth; old events are not resent. So a reliable event never carries
     truth that state does not also carry: it is a moment (a sound, a
     banner), not a fact.
   - Cosmetic events ride only in the room the byte budget leaves. The row
     section rides outside the budget, whole every packet.

   Packet: tick, last input seq, input wait, baseline (0 = none, else
   tick + 1), the game's row section, event count, then each event as its
   origin tick's offset back from the packet tick and its bytes; every
   number a net_varint. A client applies the packet's state first and then
   the events in order, each at its origin tick. */

enum {
    NET_REPL_AUDIENCE_OWNER = 1,
    NET_REPL_AUDIENCE_NEARBY = 2,
    NET_REPL_AUDIENCE_ALL = 3,
};

/* A viewer not yet placed in a district, or an event that happened in none:
   no nearby event passes between them. */
#define NET_REPL_DISTRICT_NONE 0xFFU

#define NET_REPL_EVENT_MAX_BYTES 64U
#define NET_REPL_OUTBOX_EVENTS 64U
/* The header's varints and a full outbox of the largest events with their
   tick offsets: with this much room the reliable events always fit, so a
   packet is never cut or refused for them. */
#define NET_REPL_PACKET_CAP_MIN (5U * 5U + NET_REPL_OUTBOX_EVENTS * (5U + NET_REPL_EVENT_MAX_BYTES))
/* The header's input wait: how long the move it acknowledges sat in the
   room, ms plus one, capped so it never takes more than two varint bytes;
   0 when unknown (a stand-in tick, a room in the same process). */
#define NET_REPL_WAIT_UNKNOWN 0U
#define NET_REPL_WAIT_MAX_MS 16382U

typedef struct net_repl_event_t {
    uint32_t tick; /* the room tick it happened on */
    uint8_t audience;
    bool reliable;
    uint32_t owner;   /* the player an owner event is for */
    uint8_t district; /* where a nearby event happens */
    float x, z;
    uint8_t size;
    uint8_t bytes[NET_REPL_EVENT_MAX_BYTES];
} net_repl_event_t;

typedef struct net_repl_viewer_t {
    uint32_t player;
    uint8_t district;
    float x, z;
    bool has_baseline;
    uint32_t baseline_tick;
    uint32_t outbox_count;
    net_repl_event_t outbox[NET_REPL_OUTBOX_EVENTS];
    uint32_t cosmetic_dropped;
} net_repl_viewer_t;

typedef struct net_repl_knobs_t {
    uint32_t bytes_per_tick; /* the per-player byte budget of one packet */
    float interest_radius;   /* metres: nearby events farther away are not sent */
} net_repl_knobs_t;

/* A viewer for `player`, placed nowhere, with no baseline and an empty outbox. */
void net_repl_viewer_init(net_repl_viewer_t *viewer, uint32_t player);

/* After a reconnect or a resync: no baseline (the next packet carries full
   state) and an empty outbox. Placement and the drop count stay. */
void net_repl_reset(net_repl_viewer_t *viewer);

/* Whether the viewer may receive the event: owner events only by their
   owner, nearby ones inside its district and interest radius, all ones
   always. */
bool net_repl_admits(const net_repl_viewer_t *viewer, const net_repl_event_t *event, const net_repl_knobs_t *knobs);

/* Queues the tick's reliable events the viewer may receive into its outbox.
   Called every tick, sent or skipped. False: the outbox overflowed, and the
   connection must close for a resync. */
bool net_repl_queue(net_repl_viewer_t *viewer, const net_repl_event_t *events, uint32_t count, const net_repl_knobs_t *knobs);

typedef struct net_repl_built_t {
    uint32_t reliable_sent;
    uint32_t cosmetic_sent;
    size_t rows_at; /* where the row section starts in the packet */
} net_repl_built_t;

/* Builds the viewer's packet for `tick` into `out` (cap at least
   NET_REPL_PACKET_CAP_MIN + rows_size): the row section whole, every queued
   reliable event, then the tick's cosmetic events it may receive while the
   budget lasts; the others are dropped and counted. Every event's tick is at
   most `tick`. `input_wait` is how long the move `input_seq` sat in the
   room (NET_REPL_WAIT_*). Returns the size. */
size_t net_repl_build(net_repl_viewer_t *viewer, uint32_t tick, uint32_t input_seq, uint32_t input_wait, const uint8_t *rows,
    size_t rows_size, const net_repl_event_t *events, uint32_t count, const net_repl_knobs_t *knobs, uint8_t *out, size_t cap,
    net_repl_built_t *built);

/* The transport took the packet built for `tick`: the baseline moves to it
   and the reliable events it carried leave the outbox. */
void net_repl_sent(net_repl_viewer_t *viewer, uint32_t tick, const net_repl_built_t *built);

/* The transport refused the packet: the baseline and the outbox stay, and
   its cosmetic events count as dropped. */
void net_repl_refused(net_repl_viewer_t *viewer, const net_repl_built_t *built);

#endif
