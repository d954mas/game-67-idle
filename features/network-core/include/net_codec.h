#ifndef NETWORK_CORE_NET_CODEC_H
#define NETWORK_CORE_NET_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bounded little-endian readers and writers. A read past the end or a write
   past capacity clears `ok` and returns zero; the caller checks once at the
   end instead of after every field, and a truncated message can never read
   outside its buffer. */

typedef struct net_reader_t {
    const uint8_t *data;
    size_t size;
    size_t pos;
    bool ok;
} net_reader_t;

void net_reader_init(net_reader_t *reader, const uint8_t *data, size_t size);
uint8_t net_read_u8(net_reader_t *reader);
int8_t net_read_i8(net_reader_t *reader);
uint16_t net_read_u16(net_reader_t *reader);
uint32_t net_read_u32(net_reader_t *reader);
uint64_t net_read_u64(net_reader_t *reader);
float net_read_f32(net_reader_t *reader);
/* Whole message consumed without a bounds failure: trailing bytes are a
   protocol error, not padding. */
bool net_reader_complete(const net_reader_t *reader);

typedef struct net_writer_t {
    uint8_t *data;
    size_t capacity;
    size_t pos;
    bool ok;
} net_writer_t;

void net_writer_init(net_writer_t *writer, uint8_t *data, size_t capacity);
void net_write_u8(net_writer_t *writer, uint8_t value);
void net_write_i8(net_writer_t *writer, int8_t value);
void net_write_u16(net_writer_t *writer, uint16_t value);
void net_write_u32(net_writer_t *writer, uint32_t value);
void net_write_u64(net_writer_t *writer, uint64_t value);
void net_write_f32(net_writer_t *writer, float value);

/* Fixed point on the wire. A value travels as a whole count of `step`s
   above `min`, in the fewest whole bytes that hold `bits` (1 to 4);
   anything past the grid's ends
   clamps to them, and a value that is not a number lands on `min`.
   `net_quantize` is what the other side reads: a simulation that snaps its
   state to the grid after every step holds the wire's numbers exactly, so a
   snapshot of a correct prediction is never a correction. A power-of-two
   step with `min` on it keeps every grid point exact in float. */
typedef struct net_grid_t {
    float min;
    float step;
    unsigned bits;
} net_grid_t;

float net_quantize(net_grid_t grid, float value);
void net_write_quantized(net_writer_t *writer, net_grid_t grid, float value);
float net_read_quantized(net_reader_t *reader, net_grid_t grid);

/* A heading in radians as 16 bits of a turn: any angle lands in [-pi, pi),
   pi itself on -pi. */
float net_quantize_angle16(float radians);
void net_write_angle16(net_writer_t *writer, float radians);
float net_read_angle16(net_reader_t *reader);

/* Message type 0 is owned by network-core: the first frame of every
   connection carries the magic, the protocol version and an optional
   ticket, opaque bytes the application handed the client (a session to
   resume, a join code). The transport rejects the session before the
   application sees anything on a version mismatch and passes the ticket to
   on_connect otherwise. Application message types start at NET_MSG_APP_FIRST. */
#define NET_MSG_HELLO 0U
#define NET_MSG_APP_FIRST 1U
#define NET_HELLO_MAGIC 0x5357544EU /* "NTWS" */
#define NET_HELLO_BASE_SIZE 9U
#define NET_HELLO_TICKET_MAX 64U
#define NET_HELLO_MAX_SIZE (NET_HELLO_BASE_SIZE + NET_HELLO_TICKET_MAX)

void net_hello_encode(net_writer_t *writer, uint32_t protocol_version, const uint8_t *ticket,
    size_t ticket_size);
/* False when the bytes are not a well-formed HELLO; the outputs are only
   valid when true is returned. `ticket` points into `data`. */
bool net_hello_decode(const uint8_t *data, size_t size, uint32_t *protocol_version,
    const uint8_t **ticket, size_t *ticket_size);

/* Close codes in the application range of RFC 6455 (4000-4999). */
#define NET_CLOSE_BAD_HELLO 4001U
#define NET_CLOSE_VERSION 4002U
#define NET_CLOSE_RATE 4003U
#define NET_CLOSE_FORMAT 4004U
#define NET_CLOSE_SLOW 4005U
#define NET_CLOSE_FULL 4006U
#define NET_CLOSE_APP 4007U

#endif
