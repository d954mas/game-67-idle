#include "features/net_bits/net_bits_record.h"

#include <string.h>

#define FNV_BASIS 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL

/* kind, wire bits, int_min, int_max, range min, max, step: see README.md, "Table hash". The
   rules byte leads every hash, so a change in how values are encoded renames every table. */
enum { CANONICAL_FIELD_BYTES = 22, WIRE_RULES = 1 };

static uint64_t fnv_bytes(uint64_t hash, const uint8_t *bytes, size_t count) {
    for (size_t i = 0U; i < count; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

static void put_u32(uint8_t *out, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        out[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint32_t float_pattern(float value) {
    uint32_t pattern;
    memcpy(&pattern, &value, sizeof pattern);
    return pattern;
}

/* Bits the line puts on the wire; false for a malformed line. An INT with min == max is
   legal and takes no bits. */
static bool field_bits(const net_bits_field_t *field, unsigned *bits) {
    switch (field->kind) {
    case NET_BITS_FIELD_BOOL:
        *bits = 1U;
        return true;
    case NET_BITS_FIELD_UINT:
        *bits = field->bits;
        return field->bits >= 1U && field->bits <= 32U;
    case NET_BITS_FIELD_INT: {
        if (field->int_min > field->int_max) {
            return false;
        }
        const uint32_t span = (uint32_t)((int64_t)field->int_max - (int64_t)field->int_min);
        *bits = span == UINT32_MAX ? 32U : net_bits_width(span + 1U);
        return true;
    }
    case NET_BITS_FIELD_RANGED:
        *bits = net_bits_range_width(field->range);
        return *bits != 0U;
    case NET_BITS_FIELD_QUAT:
        *bits = 2U + 3U * field->bits;
        return field->bits >= NET_BITS_QUAT_BITS_MIN && field->bits <= NET_BITS_QUAT_BITS_MAX;
    default:
        return false;
    }
}

static size_t member_size(uint8_t kind) {
    switch (kind) {
    case NET_BITS_FIELD_BOOL:
        return sizeof(bool);
    case NET_BITS_FIELD_QUAT:
        return 4U * sizeof(float);
    default:
        return 4U;
    }
}

static uint64_t hash_field(uint64_t hash, const net_bits_field_t *field, unsigned bits) {
    uint8_t bytes[CANONICAL_FIELD_BYTES] = {0};
    bytes[0] = field->kind;
    bytes[1] = (uint8_t)bits;
    if (field->kind == NET_BITS_FIELD_INT) {
        put_u32(&bytes[2], (uint32_t)field->int_min);
        put_u32(&bytes[6], (uint32_t)field->int_max);
    } else if (field->kind == NET_BITS_FIELD_RANGED) {
        put_u32(&bytes[10], float_pattern(field->range.min));
        put_u32(&bytes[14], float_pattern(field->range.max));
        put_u32(&bytes[18], float_pattern(field->range.step));
    }
    return fnv_bytes(hash, bytes, sizeof bytes);
}

uint64_t net_bits_hash_mix(uint64_t hash, uint64_t value) {
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (uint8_t)(value >> (8 * i));
    }
    return fnv_bytes(hash, bytes, sizeof bytes);
}

bool net_bits_layout_init(net_bits_layout_t *layout, const net_bits_field_t *fields, uint32_t count, size_t record_size) {
    *layout = (net_bits_layout_t){0};
    if (fields == NULL) {
        return false;
    }
    uint64_t total = 0U;
    const uint8_t rules = WIRE_RULES;
    uint64_t hash = fnv_bytes(FNV_BASIS, &rules, 1U);
    for (uint32_t i = 0U; i < count; i++) {
        unsigned bits = 0U;
        if (!field_bits(&fields[i], &bits) || member_size(fields[i].kind) > record_size ||
            fields[i].offset > record_size - member_size(fields[i].kind)) {
            return false;
        }
        total += bits;
        hash = hash_field(hash, &fields[i], bits);
    }
    if (total == 0U || total > UINT32_MAX) {
        return false;
    }
    *layout = (net_bits_layout_t){.fields = fields, .count = count, .bits = (uint32_t)total, .hash = hash};
    return true;
}

void net_bits_write_record(net_bits_writer_t *writer, const net_bits_layout_t *layout, const void *record) {
    if (!writer->ok) {
        return;
    }
    if (layout->bits == 0U || net_bits_writer_room(writer) < layout->bits) {
        writer->ok = false;
        return;
    }
    const uint8_t *base = record;
    for (uint32_t i = 0U; i < layout->count && writer->ok; i++) {
        const net_bits_field_t *field = &layout->fields[i];
        const uint8_t *member = base + field->offset;
        switch (field->kind) {
        case NET_BITS_FIELD_BOOL: {
            /* Read as bytes: a bool member holding a stray value is not a load of a bool. */
            uint8_t raw[sizeof(bool)];
            memcpy(raw, member, sizeof raw);
            uint32_t set = 0U;
            for (size_t b = 0U; b < sizeof raw; b++) {
                set |= raw[b];
            }
            net_bits_write(writer, set != 0U ? 1U : 0U, 1U);
            break;
        }
        case NET_BITS_FIELD_UINT: {
            uint32_t value;
            memcpy(&value, member, sizeof value);
            net_bits_write(writer, value, field->bits);
            break;
        }
        case NET_BITS_FIELD_INT: {
            int32_t value;
            memcpy(&value, member, sizeof value);
            net_bits_write_int(writer, value, field->int_min, field->int_max);
            break;
        }
        case NET_BITS_FIELD_RANGED: {
            float value;
            memcpy(&value, member, sizeof value);
            net_bits_write_ranged(writer, field->range, value);
            break;
        }
        case NET_BITS_FIELD_QUAT: {
            float quat[4];
            memcpy(quat, member, sizeof quat);
            net_bits_write_quat(writer, quat, field->bits);
            break;
        }
        default:
            writer->ok = false;
            break;
        }
    }
}

bool net_bits_read_record(net_bits_reader_t *reader, const net_bits_layout_t *layout, void *record) {
    if (!reader->ok) {
        return false;
    }
    if (layout->bits == 0U || net_bits_reader_left(reader) < layout->bits) {
        reader->ok = false;
        return false;
    }
    uint8_t *base = record;
    for (uint32_t i = 0U; i < layout->count; i++) {
        const net_bits_field_t *field = &layout->fields[i];
        /* Each member is read into a local and stored only once its code is known good, so
           the failing member and those after it keep their values. */
        union {
            bool flag;
            uint32_t uint;
            int32_t sint;
            float real;
            float quat[4];
        } value;
        size_t size = 0U;
        switch (field->kind) {
        case NET_BITS_FIELD_BOOL:
            value.flag = net_bits_read(reader, 1U) != 0U;
            size = sizeof value.flag;
            break;
        case NET_BITS_FIELD_UINT:
            value.uint = net_bits_read(reader, field->bits);
            size = sizeof value.uint;
            break;
        case NET_BITS_FIELD_INT:
            value.sint = net_bits_read_int(reader, field->int_min, field->int_max);
            size = sizeof value.sint;
            break;
        case NET_BITS_FIELD_RANGED:
            value.real = net_bits_read_ranged(reader, field->range);
            size = sizeof value.real;
            break;
        case NET_BITS_FIELD_QUAT:
            net_bits_read_quat(reader, value.quat, field->bits);
            size = sizeof value.quat;
            break;
        default:
            reader->ok = false;
            break;
        }
        if (!reader->ok) {
            return false;
        }
        memcpy(base + field->offset, &value, size);
    }
    return true;
}
