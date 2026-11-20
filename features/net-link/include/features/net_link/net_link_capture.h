#ifndef FEATURES_NET_LINK_NET_LINK_CAPTURE_H
#define FEATURES_NET_LINK_NET_LINK_CAPTURE_H

#include "features/net_link/net_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A capture is a file with one record per message a link sent or polled: when, which way,
   to or from whom, the lane, the kind and the size, and the bytes themselves when asked.
   It is for measuring traffic and for collecting a corpus on a developer's machine. It works
   on any transport, because the calls of net_link.h write it, not the transport.

   File: little-endian. Header: "NLCP", u16 version, u16 flags. Record: u64 time in
   microseconds, u64 peer identity, u32 size, u8 tag, u8 direction, u8 lane, u8 reliable,
   then `size` payload bytes when the header flag says so. */

typedef struct net_link_capture net_link_capture_t;

enum {
    NET_LINK_CAPTURE_VERSION = 1,
    NET_LINK_CAPTURE_PAYLOAD = 1U << 0, /* keep the bytes, not only the sizes */
};

typedef enum net_link_capture_direction {
    NET_LINK_CAPTURE_OUT = 0, /* accepted by net_link_send */
    NET_LINK_CAPTURE_IN = 1,  /* returned by net_link_poll */
} net_link_capture_direction_t;

/* NULL when the file cannot be created. */
net_link_capture_t *net_link_capture_open(const char *path, uint32_t flags);

/* Flushes and closes the file. Detach every link first. */
void net_link_capture_close(net_link_capture_t *capture);

/* Records carry the time last set here: a link has no clock, its owner does. */
void net_link_capture_set_time(net_link_capture_t *capture, double seconds);

/* False once a write has failed; the capture then records nothing more. */
bool net_link_capture_ok(const net_link_capture_t *capture);

/* From now on the link's traffic goes to `capture`, marked with `tag` so that several links
   can share a file. NULL detaches. */
void net_link_capture_attach(net_link_t *link, net_link_capture_t *capture, uint8_t tag);

typedef struct net_link_capture_record {
    double time_s;
    uint64_t identity;
    uint32_t size;
    uint8_t tag;
    uint8_t direction; /* net_link_capture_direction_t */
    uint8_t lane;
    bool reliable;
    const uint8_t *payload; /* NULL without NET_LINK_CAPTURE_PAYLOAD; else readable until the next read */
} net_link_capture_record_t;

typedef struct net_link_capture_reader net_link_capture_reader_t;

/* NULL when the file is missing, is not a capture, or has a version this code does not read. */
net_link_capture_reader_t *net_link_capture_reader_open(const char *path);
void net_link_capture_reader_close(net_link_capture_reader_t *reader);

/* False at the end of the file and on a record that is cut short or out of range; a capture
   is a file like any other, so a bad one stops the read, it does not break the reader. */
bool net_link_capture_reader_next(net_link_capture_reader_t *reader, net_link_capture_record_t *out);

/* True when the last `next` stopped on a bad record, not on the end of the file. */
bool net_link_capture_reader_failed(const net_link_capture_reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif
