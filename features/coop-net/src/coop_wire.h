#ifndef FEATURES_COOP_NET_COOP_WIRE_H
#define FEATURES_COOP_NET_COOP_WIRE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The session's packets on the wire, written and read with net-bits. Parsing takes bytes from
   another player and is the one place that decides whether a packet is well formed: kind,
   lane, reliability, direction, field ranges, sizes, zero padding, no trailing bytes. What a
   well-formed packet means in the session's current phase is the session's call.
   README.md, "Wire format", is the layout a second implementation follows. */

typedef enum coop_wire_kind {
    COOP_WIRE_HELLO = 1,          /* guest to host, records lane */
    COOP_WIRE_WELCOME = 2,        /* host to guest, records lane */
    COOP_WIRE_JOINED = 3,         /* host to guest, records lane */
    COOP_WIRE_LEFT = 4,           /* host to guest, records lane */
    COOP_WIRE_RECORD = 5,         /* both ways, records lane */
    COOP_WIRE_STATE = 6,          /* both ways, state lane, unreliable */
    COOP_WIRE_BASELINE_FIRST = 7, /* host to guest, baseline lane */
    COOP_WIRE_BASELINE_PIECE = 8, /* host to guest, baseline lane */
} coop_wire_kind_t;

enum {
    COOP_WIRE_KIND_BITS = 4,
    COOP_WIRE_MAGIC = 0x504F4F43, /* "COOP" as little-endian bytes */
    /* Bytes before the payload of each kind; HELLO and WELCOME continue with the world key. */
    COOP_WIRE_HELLO_BYTES = 28,
    COOP_WIRE_WELCOME_BYTES = 9,
    COOP_WIRE_JOINED_BYTES = 9,
    COOP_WIRE_LEFT_BYTES = 5,
    COOP_WIRE_RECORD_BYTES = 5,
    COOP_WIRE_STATE_BYTES = 15,
    COOP_WIRE_BASELINE_FIRST_BYTES = 13,
    COOP_WIRE_BASELINE_PIECE_BYTES = 1,
    COOP_WIRE_ACK_WAIT_UNITS_PER_S = 4000, /* quarter milliseconds */
    COOP_WIRE_ACK_WAIT_TOO_LONG = 0xFFFF,  /* the wait did not fit: no round-trip sample */
    COOP_WIRE_TICK_HZ_MAX = 0xFFFF,
};

/* One parsed packet. Fields a kind does not carry are zero; `bytes` points into the parsed
   buffer. */
typedef struct coop_wire_msg {
    coop_wire_kind_t kind;
    uint32_t protocol;   /* HELLO; when it is not COOP_SESSION_PROTOCOL nothing after it is read */
    uint64_t build_key;  /* HELLO */
    uint64_t player_key; /* HELLO */
    uint32_t baseline_max; /* HELLO: the largest baseline the guest accepts */
    uint64_t identity;   /* JOINED */
    uint8_t seat;        /* WELCOME, JOINED, LEFT */
    uint8_t seats;       /* WELCOME */
    uint32_t tick_hz;    /* WELCOME */
    uint32_t tick;       /* WELCOME, RECORD, STATE, BASELINE_FIRST */
    uint32_t reason;     /* LEFT */
    uint32_t total;      /* BASELINE_FIRST: the whole baseline, in bytes */
    uint32_t records_before; /* BASELINE_FIRST: records the host sent to this seat before the cut */
    uint16_t number;     /* STATE */
    bool has_ack;        /* STATE: false when the sender has received no state packet yet */
    uint16_t ack;        /* STATE: the newest packet received */
    uint32_t ack_mask;   /* STATE: bit i set when packet ack - 1 - i was received */
    uint16_t ack_wait;   /* STATE: how long `ack` waited at the sender, COOP_WIRE_ACK_WAIT units */
    const uint8_t *bytes; /* HELLO, WELCOME: world key; RECORD, STATE: payload (STATE: may be none); BASELINE_*: piece */
    uint32_t size;
} coop_wire_msg_t;

/* True when `data` is a well-formed packet of a kind allowed on `lane` with that reliability
   and in that direction. Never reads outside [data, data + size). */
bool coop_wire_parse(const uint8_t *data, uint32_t size, uint8_t lane, bool reliable, bool from_host, coop_wire_msg_t *out);

/* Each writer returns the packet size, or zero when a field is out of range or the packet does
   not fit `capacity`. Kinds with a payload copy `msg->bytes`; the rest ignore it. */
uint32_t coop_wire_write(const coop_wire_msg_t *msg, uint8_t *out, uint32_t capacity);

/* Bytes in front of the payload of `kind`, world keys excluded; zero for an unknown kind. */
uint32_t coop_wire_header_bytes(coop_wire_kind_t kind);

/* Lane and reliability of a kind; false for an unknown kind. */
bool coop_wire_route(coop_wire_kind_t kind, uint8_t *lane, bool *reliable);

#endif
