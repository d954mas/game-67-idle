#include "net_codec.h"

#include <math.h>
#include <string.h>

#define NET_TAU 6.283185307179586F
#define ANGLE16_PER_RADIAN (65536.0F / NET_TAU)
#define ANGLE16_RADIANS (NET_TAU / 65536.0F)

static bool reader_take(net_reader_t *reader, size_t count) {
    if (!reader->ok || reader->size - reader->pos < count) {
        reader->ok = false;
        return false;
    }
    return true;
}

void net_reader_init(net_reader_t *reader, const uint8_t *data, size_t size) {
    reader->data = data;
    reader->size = size;
    reader->pos = 0U;
    reader->ok = data != NULL || size == 0U;
}

uint8_t net_read_u8(net_reader_t *reader) {
    if (!reader_take(reader, 1U)) { return 0U; }
    return reader->data[reader->pos++];
}

int8_t net_read_i8(net_reader_t *reader) {
    const uint8_t raw = net_read_u8(reader);
    return raw < 0x80U ? (int8_t)raw : (int8_t)((int)raw - 0x100);
}

uint16_t net_read_u16(net_reader_t *reader) {
    if (!reader_take(reader, 2U)) { return 0U; }
    const uint8_t *p = reader->data + reader->pos;
    reader->pos += 2U;
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

uint32_t net_read_u32(net_reader_t *reader) {
    if (!reader_take(reader, 4U)) { return 0U; }
    const uint8_t *p = reader->data + reader->pos;
    reader->pos += 4U;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint64_t net_read_u64(net_reader_t *reader) {
    if (!reader_take(reader, 8U)) { return 0U; }
    const uint32_t low = net_read_u32(reader);
    const uint32_t high = net_read_u32(reader);
    return (uint64_t)low | ((uint64_t)high << 32);
}

float net_read_f32(net_reader_t *reader) {
    const uint32_t bits = net_read_u32(reader);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

bool net_reader_complete(const net_reader_t *reader) {
    return reader->ok && reader->pos == reader->size;
}

static bool writer_take(net_writer_t *writer, size_t count) {
    if (!writer->ok || writer->capacity - writer->pos < count) {
        writer->ok = false;
        return false;
    }
    return true;
}

void net_writer_init(net_writer_t *writer, uint8_t *data, size_t capacity) {
    writer->data = data;
    writer->capacity = capacity;
    writer->pos = 0U;
    writer->ok = data != NULL || capacity == 0U;
}

void net_write_u8(net_writer_t *writer, uint8_t value) {
    if (!writer_take(writer, 1U)) { return; }
    writer->data[writer->pos++] = value;
}

void net_write_i8(net_writer_t *writer, int8_t value) {
    net_write_u8(writer, (uint8_t)value);
}

void net_write_u16(net_writer_t *writer, uint16_t value) {
    if (!writer_take(writer, 2U)) { return; }
    uint8_t *p = writer->data + writer->pos;
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)(value >> 8);
    writer->pos += 2U;
}

void net_write_u32(net_writer_t *writer, uint32_t value) {
    if (!writer_take(writer, 4U)) { return; }
    uint8_t *p = writer->data + writer->pos;
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8) & 0xFFU);
    p[2] = (uint8_t)((value >> 16) & 0xFFU);
    p[3] = (uint8_t)(value >> 24);
    writer->pos += 4U;
}

void net_write_u64(net_writer_t *writer, uint64_t value) {
    if (!writer_take(writer, 8U)) { return; }
    net_write_u32(writer, (uint32_t)value);
    net_write_u32(writer, (uint32_t)(value >> 32));
}

void net_write_f32(net_writer_t *writer, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    net_write_u32(writer, bits);
}

static uint32_t grid_last(net_grid_t grid) {
    return grid.bits >= 32U ? UINT32_MAX : (1U << grid.bits) - 1U;
}

static uint32_t grid_raw(net_grid_t grid, float value) {
    const float steps = (value - grid.min) / grid.step;
    const uint32_t last = grid_last(grid);
    if (!(steps > 0.0F)) { return 0U; }
    if (steps >= (float)last) { return last; }
    return (uint32_t)(steps + 0.5F);
}

static float grid_value(net_grid_t grid, uint32_t raw) {
    const uint32_t last = grid_last(grid);
    return grid.min + (float)(raw > last ? last : raw) * grid.step;
}

float net_quantize(net_grid_t grid, float value) {
    return grid_value(grid, grid_raw(grid, value));
}

void net_write_quantized(net_writer_t *writer, net_grid_t grid, float value) {
    const uint32_t raw = grid_raw(grid, value);
    if (grid.bits <= 8U) {
        net_write_u8(writer, (uint8_t)raw);
    } else if (grid.bits <= 16U) {
        net_write_u16(writer, (uint16_t)raw);
    } else {
        net_write_u32(writer, raw);
    }
}

float net_read_quantized(net_reader_t *reader, net_grid_t grid) {
    uint32_t raw;
    if (grid.bits <= 8U) {
        raw = net_read_u8(reader);
    } else if (grid.bits <= 16U) {
        raw = net_read_u16(reader);
    } else {
        raw = net_read_u32(reader);
    }
    return grid_value(grid, raw);
}

/* remainderf lands in [-pi, pi]; pi scales to 32768, which the 16-bit wrap
   folds onto -pi. */
static uint16_t angle16_raw(float radians) {
    return (uint16_t)lroundf(remainderf(radians, NET_TAU) * ANGLE16_PER_RADIAN);
}

static float angle16_radians(uint16_t raw) {
    const int32_t steps = raw < 0x8000U ? (int32_t)raw : (int32_t)raw - 0x10000;
    return (float)steps * ANGLE16_RADIANS;
}

float net_quantize_angle16(float radians) {
    return angle16_radians(angle16_raw(radians));
}

void net_write_angle16(net_writer_t *writer, float radians) {
    net_write_u16(writer, angle16_raw(radians));
}

float net_read_angle16(net_reader_t *reader) {
    return angle16_radians(net_read_u16(reader));
}

void net_hello_encode(net_writer_t *writer, uint32_t protocol_version, const uint8_t *ticket,
    size_t ticket_size) {
    net_write_u8(writer, NET_MSG_HELLO);
    net_write_u32(writer, NET_HELLO_MAGIC);
    net_write_u32(writer, protocol_version);
    if (ticket_size > NET_HELLO_TICKET_MAX) {
        writer->ok = false;
        return;
    }
    for (size_t index = 0U; index < ticket_size; ++index) { net_write_u8(writer, ticket[index]); }
}

bool net_hello_decode(const uint8_t *data, size_t size, uint32_t *protocol_version,
    const uint8_t **ticket, size_t *ticket_size) {
    net_reader_t reader;
    net_reader_init(&reader, data, size);
    const uint8_t type = net_read_u8(&reader);
    const uint32_t magic = net_read_u32(&reader);
    const uint32_t version = net_read_u32(&reader);
    if (!reader.ok || type != NET_MSG_HELLO || magic != NET_HELLO_MAGIC ||
        size - NET_HELLO_BASE_SIZE > NET_HELLO_TICKET_MAX) {
        return false;
    }
    *protocol_version = version;
    *ticket = data + NET_HELLO_BASE_SIZE;
    *ticket_size = size - NET_HELLO_BASE_SIZE;
    return true;
}
