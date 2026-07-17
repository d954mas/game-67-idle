#define ITEMS_RUNTIME_PACKAGE_IMPLEMENTATION 1
#include "features/items/items.h"

#include "core/nt_assert.h"
#include "hash/nt_hash.h"

#include <stdlib.h>
#include <string.h>


#define ITEMS_HEADER_SIZE UINT32_C(144)
#define ITEMS_SECTION_COUNT UINT32_C(6)
#define ITEMS_LEVEL_FREE UINT32_C(1)
#define ITEMS_ITEM_ACQUIRE_FREE UINT32_C(1)
#define ITEMS_ITEM_HAS_CURRENCY UINT32_C(2)
#define ITEMS_COST_LEVEL_FLAG UINT32_C(0x80000000)

typedef enum items_section_index_t {
    ITEMS_SECTION_STRINGS = 0,
    ITEMS_SECTION_ITEMS,
    ITEMS_SECTION_FIELDS,
    ITEMS_SECTION_LEVELS,
    ITEMS_SECTION_VALUES,
    ITEMS_SECTION_COSTS,
} items_section_index_t;

typedef struct items_section_t {
    uint32_t offset;
    uint32_t count;
    uint32_t stride;
} items_section_t;

static uint8_t *s_catalog;
static uint32_t s_catalog_size;
static uint32_t s_item_count;
static uint64_t s_schema_abi;
static uint64_t s_content_fingerprint;
static items_section_t s_sections[ITEMS_SECTION_COUNT];

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes) {
    uint64_t value = 0;
    for (uint32_t index = 0; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static int64_t read_i64(const uint8_t *bytes) {
    uint64_t bits = read_u64(bytes);
    int64_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void write_u64(uint8_t *bytes, uint64_t value) {
    for (uint32_t index = 0; index < 8U; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint32_t align8(uint32_t value) { return (value + 7U) & ~UINT32_C(7); }

static bool span_end(uint32_t offset, uint32_t count, uint32_t stride, uint32_t size, uint32_t *out_end) {
    if (offset > size || stride == 0U || count > (size - offset) / stride) {
        return false;
    }
    *out_end = offset + count * stride;
    return true;
}

static bool valid_utf8(const uint8_t *bytes, uint32_t count) {
    uint32_t index = 0U;
    while (index < count) {
        uint8_t first = bytes[index++];
        if (first <= 0x7FU) {
            continue;
        }
        uint32_t continuation = 0U;
        uint8_t second_min = 0x80U;
        uint8_t second_max = 0xBFU;
        if (first >= 0xC2U && first <= 0xDFU) {
            continuation = 1U;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            continuation = 2U;
            if (first == 0xE0U) second_min = 0xA0U;
            if (first == 0xEDU) second_max = 0x9FU;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            continuation = 3U;
            if (first == 0xF0U) second_min = 0x90U;
            if (first == 0xF4U) second_max = 0x8FU;
        } else {
            return false;
        }
        if (continuation > count - index || bytes[index] < second_min || bytes[index] > second_max) {
            return false;
        }
        ++index;
        for (uint32_t tail = 1U; tail < continuation; ++tail, ++index) {
            if (bytes[index] < 0x80U || bytes[index] > 0xBFU) {
                return false;
            }
        }
    }
    return true;
}

static const char *checked_string(
    const uint8_t *bytes, const items_section_t *strings, uint32_t relative_offset) {
    if (relative_offset == 0U || relative_offset >= strings->count) {
        return NULL;
    }
    const uint8_t *start = bytes + strings->offset + relative_offset;
    uint32_t remaining = strings->count - relative_offset;
    const uint8_t *end = (const uint8_t *)memchr(start, 0, remaining);
    uint32_t length = end == NULL ? 0U : (uint32_t)(end - start);
    return end != NULL && length > 0U && valid_utf8(start, length) ? (const char *)start : NULL;
}

static bool zero_padding(const uint8_t *bytes, uint32_t start, uint32_t end) {
    for (uint32_t index = start; index < end; ++index) {
        if (bytes[index] != 0U) {
            return false;
        }
    }
    return true;
}

static items_catalog_bind_error_t validate_package(
    uint8_t *bytes, uint32_t size, uint32_t *out_item_count,
    uint64_t *out_schema_abi, uint64_t *out_content_fingerprint,
    items_section_t out_sections[ITEMS_SECTION_COUNT]) {
    static const uint8_t magic[8] = {'N', 'T', 'I', 'T', 'E', 'M', 'S', 0};
    static const uint32_t strides[ITEMS_SECTION_COUNT] = {1U, 56U, 48U, 32U, 24U, 16U};
    if (size < ITEMS_HEADER_SIZE || read_u32(bytes + 12U) != ITEMS_HEADER_SIZE
            || read_u32(bytes + 16U) != size || read_u32(bytes + 20U) != 0U) {
        return ITEMS_CATALOG_BIND_BAD_HEADER;
    }
    if (memcmp(bytes, magic, sizeof(magic)) != 0) {
        return ITEMS_CATALOG_BIND_BAD_MAGIC;
    }
    if (read_u32(bytes + 8U) != ITEMS_CATALOG_FORMAT_VERSION) {
        return ITEMS_CATALOG_BIND_BAD_VERSION;
    }
    uint64_t schema_abi = read_u64(bytes + 24U);
    if (schema_abi != ITEMS_CATALOG_SCHEMA_ABI) {
        return ITEMS_CATALOG_BIND_ABI_MISMATCH;
    }
    uint64_t content_fingerprint = read_u64(bytes + 32U);
    write_u64(bytes + 32U, 0U);
    uint64_t actual_fingerprint = nt_hash64(bytes, size).value;
    write_u64(bytes + 32U, content_fingerprint);
    if (content_fingerprint != actual_fingerprint) {
        return ITEMS_CATALOG_BIND_CONTENT_MISMATCH;
    }

    items_section_t sections[ITEMS_SECTION_COUNT];
    uint32_t previous_end = ITEMS_HEADER_SIZE;
    for (uint32_t index = 0; index < ITEMS_SECTION_COUNT; ++index) {
        uint32_t base = 40U + index * 12U;
        sections[index].offset = read_u32(bytes + base);
        sections[index].count = read_u32(bytes + base + 4U);
        sections[index].stride = read_u32(bytes + base + 8U);
        uint32_t expected_offset = align8(previous_end);
        uint32_t end = 0U;
        if (sections[index].offset != expected_offset || sections[index].stride != strides[index]
                || !zero_padding(bytes, previous_end, expected_offset)
                || !span_end(sections[index].offset, sections[index].count,
                             sections[index].stride, size, &end)) {
            return ITEMS_CATALOG_BIND_BAD_LAYOUT;
        }
        previous_end = end;
    }
    if (previous_end != size || sections[0].count == 0U || bytes[sections[0].offset] != 0U) {
        return ITEMS_CATALOG_BIND_BAD_LAYOUT;
    }

    const items_section_t *strings = &sections[0];
    const items_section_t *items = &sections[1];
    const items_section_t *fields = &sections[2];
    const items_section_t *levels = &sections[3];
    const items_section_t *values = &sections[4];
    const items_section_t *costs = &sections[5];
    if (items->count != ITEMS_CATALOG_ITEM_COUNT || fields->count != ITEMS_CATALOG_FIELD_COUNT) {
        return ITEMS_CATALOG_BIND_ABI_MISMATCH;
    }
    const char *previous_name = NULL;
    for (uint32_t index = 0; index < fields->count; ++index) {
        const uint8_t *row = bytes + fields->offset + index * fields->stride;
        const char *id = checked_string(bytes, strings, read_u32(row + 8U));
        const char *member = checked_string(bytes, strings, read_u32(row + 12U));
        const char *unit = checked_string(bytes, strings, read_u32(row + 16U));
        const items_catalog_expected_field_t *expected = &s_items_catalog_expected_fields[index];
        if (id == NULL || member == NULL || unit == NULL
                || nt_hash64(id, (uint32_t)strlen(id)).value != read_u64(row)
                || read_u32(row + 20U) != expected->type
                || read_i64(row + 24U) != expected->minimum || read_i64(row + 32U) != expected->maximum
                || read_u32(row + 40U) != expected->rounding || read_u32(row + 44U) != 0U
                || (previous_name != NULL && strcmp(previous_name, id) >= 0)) {
            return ITEMS_CATALOG_BIND_BAD_LAYOUT;
        }
        if (strcmp(id, expected->id) != 0 || strcmp(member, expected->member) != 0
                || strcmp(unit, expected->unit) != 0) {
            return ITEMS_CATALOG_BIND_ABI_MISMATCH;
        }
        previous_name = id;
    }

    previous_name = NULL;
    for (uint32_t index = 0; index < items->count; ++index) {
        const uint8_t *row = bytes + items->offset + index * items->stride;
        const char *id = checked_string(bytes, strings, read_u32(row + 8U));
        const char *kind = checked_string(bytes, strings, read_u32(row + 12U));
        uint32_t storage = read_u32(row + 16U);
        uint32_t stack = read_u32(row + 20U);
        uint32_t flags = read_u32(row + 40U);
        if (id == NULL || kind == NULL || nt_hash64(id, (uint32_t)strlen(id)).value != read_u64(row)
                || storage > 1U || ((storage == 1U) != (stack == 1U))
                || (flags & ~(ITEMS_ITEM_ACQUIRE_FREE | ITEMS_ITEM_HAS_CURRENCY)) != 0U
                || ((flags & ITEMS_ITEM_ACQUIRE_FREE) != 0U && read_u32(row + 36U) != 0U)
                || read_u32(row + 44U) != 0U
                || read_i64(row + 48U) < 0
                || ((flags & ITEMS_ITEM_HAS_CURRENCY) == 0U && read_i64(row + 48U) != 0)
                || ((flags & ITEMS_ITEM_HAS_CURRENCY) != 0U && strcmp(kind, "currency") != 0)
                || (previous_name != NULL && strcmp(previous_name, id) >= 0)) {
            return ITEMS_CATALOG_BIND_BAD_LAYOUT;
        }
        if (strcmp(id, s_items_catalog_expected_item_ids[index]) != 0) {
            return ITEMS_CATALOG_BIND_ABI_MISMATCH;
        }
        previous_name = id;
    }

    for (uint32_t index = 0; index < costs->count; ++index) {
        const uint8_t *row = bytes + costs->offset + index * costs->stride;
        if (read_u32(row) >= items->count || read_u32(row + 4U) != 0U || read_i64(row + 8U) <= 0) {
            return ITEMS_CATALOG_BIND_BAD_LAYOUT;
        }
    }
    for (uint32_t index = 0; index < values->count; ++index) {
        const uint8_t *row = bytes + values->offset + index * values->stride;
        if (read_u32(row) >= items->count || read_u32(row + 4U) == 0U
                || read_u32(row + 8U) >= fields->count || read_u32(row + 12U) != 0U) {
            return ITEMS_CATALOG_BIND_BAD_LAYOUT;
        }
        uint32_t field_index = read_u32(row + 8U);
        int64_t value = read_i64(row + 16U);
        if (value < s_items_catalog_expected_fields[field_index].minimum
                || value > s_items_catalog_expected_fields[field_index].maximum) {
            return ITEMS_CATALOG_BIND_BAD_LAYOUT;
        }
    }
    for (uint32_t index = 0; index < levels->count; ++index) {
        const uint8_t *row = bytes + levels->offset + index * levels->stride;
        uint32_t value_end = 0U;
        uint32_t cost_end = 0U;
        uint32_t flags = read_u32(row + 24U);
        if (read_u32(row) >= items->count || read_u32(row + 4U) == 0U
                || !span_end(read_u32(row + 8U), read_u32(row + 12U), 1U, values->count, &value_end)
                || !span_end(read_u32(row + 16U), read_u32(row + 20U), 1U, costs->count, &cost_end)
                || (flags & ~ITEMS_LEVEL_FREE) != 0U
                || ((flags & ITEMS_LEVEL_FREE) != 0U && read_u32(row + 20U) != 0U)
                || read_u32(row + 28U) != 0U) {
            return ITEMS_CATALOG_BIND_BAD_LAYOUT;
        }
    }

    uint32_t level_cursor = 0U;
    uint32_t value_cursor = 0U;
    uint32_t cost_cursor = 0U;
    for (uint32_t item_index = 0; item_index < items->count; ++item_index) {
        const uint8_t *item = bytes + items->offset + item_index * items->stride;
        uint32_t level_count = read_u32(item + 28U);
        uint32_t acquire_count = read_u32(item + 36U);
        uint32_t acquire_end = 0U;
        if (read_u32(item + 24U) != level_cursor || read_u32(item + 32U) != cost_cursor
                || !span_end(level_cursor, level_count, 1U, levels->count, &acquire_end)
                || !span_end(cost_cursor, acquire_count, 1U, costs->count, &acquire_end)
                || (read_u32(item + 16U) == 0U && level_count != 0U)) {
            return ITEMS_CATALOG_BIND_BAD_LAYOUT;
        }
        cost_cursor += acquire_count;
        for (uint32_t expected_level = 1U; expected_level <= level_count; ++expected_level) {
            const uint8_t *level = bytes + levels->offset + level_cursor * levels->stride;
            uint32_t value_count = read_u32(level + 12U);
            uint32_t cost_count = read_u32(level + 20U);
            if (read_u32(level) != item_index || read_u32(level + 4U) != expected_level
                    || read_u32(level + 8U) != value_cursor || read_u32(level + 16U) != cost_cursor) {
                return ITEMS_CATALOG_BIND_BAD_LAYOUT;
            }
            uint32_t previous_field = 0U;
            for (uint32_t value_index = 0; value_index < value_count; ++value_index) {
                const uint8_t *value = bytes + values->offset + (value_cursor + value_index) * values->stride;
                uint32_t field_index = read_u32(value + 8U);
                if (read_u32(value) != item_index || read_u32(value + 4U) != expected_level
                        || (value_index > 0U && field_index <= previous_field)) {
                    return ITEMS_CATALOG_BIND_BAD_LAYOUT;
                }
                previous_field = field_index;
            }
            value_cursor += value_count;
            cost_cursor += cost_count;
            ++level_cursor;
        }
    }
    if (level_cursor != levels->count || value_cursor != values->count || cost_cursor != costs->count) {
        return ITEMS_CATALOG_BIND_BAD_LAYOUT;
    }
    *out_item_count = items->count;
    *out_schema_abi = schema_abi;
    *out_content_fingerprint = content_fingerprint;
    memcpy(out_sections, sections, sizeof(sections));
    return ITEMS_CATALOG_BIND_OK;
}

bool items_catalog_try_bind(
    const uint8_t *bytes, uint32_t byte_count,
    items_catalog_bind_error_t *out_error) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
    if (s_catalog != NULL) {
        error = ITEMS_CATALOG_BIND_ALREADY_BOUND;
    } else if (bytes == NULL || byte_count < ITEMS_HEADER_SIZE) {
        error = ITEMS_CATALOG_BIND_BAD_HEADER;
    } else if (byte_count > ITEMS_RUNTIME_PACKAGE_MAX_BYTES) {
        error = ITEMS_CATALOG_BIND_BAD_LAYOUT;
    } else {
        uint8_t *owned = (uint8_t *)malloc(byte_count);
        if (owned == NULL) {
            error = ITEMS_CATALOG_BIND_NO_MEMORY;
        } else {
            memcpy(owned, bytes, byte_count);
            uint32_t item_count = 0U;
            uint64_t schema_abi = 0U;
            uint64_t content_fingerprint = 0U;
            items_section_t sections[ITEMS_SECTION_COUNT] = {{0U, 0U, 0U}};
            error = validate_package(
                owned, byte_count, &item_count, &schema_abi, &content_fingerprint, sections);
            if (error == ITEMS_CATALOG_BIND_OK) {
                s_catalog = owned;
                s_catalog_size = byte_count;
                s_item_count = item_count;
                s_schema_abi = schema_abi;
                s_content_fingerprint = content_fingerprint;
                memcpy(s_sections, sections, sizeof(s_sections));
                owned = NULL;
            }
            free(owned);
        }
    }
    if (out_error != NULL) {
        *out_error = error;
    }
    return error == ITEMS_CATALOG_BIND_OK;
}

void items_catalog_shutdown(void) {
    free(s_catalog);
    s_catalog = NULL;
    s_catalog_size = 0U;
    s_item_count = 0U;
    s_schema_abi = 0U;
    s_content_fingerprint = 0U;
    memset(s_sections, 0, sizeof(s_sections));
}

bool items_catalog_is_bound(void) { return s_catalog != NULL; }

uint32_t items_catalog_item_count(void) {
    NT_ASSERT(s_catalog != NULL && s_catalog_size >= ITEMS_HEADER_SIZE);
    return s_item_count;
}

uint64_t items_catalog_schema_abi(void) {
    NT_ASSERT(s_catalog != NULL && s_catalog_size >= ITEMS_HEADER_SIZE);
    return s_schema_abi;
}

uint64_t items_catalog_content_fingerprint(void) {
    NT_ASSERT(s_catalog != NULL && s_catalog_size >= ITEMS_HEADER_SIZE);
    return s_content_fingerprint;
}

static const uint8_t *catalog_row(items_section_index_t section, uint32_t index) {
    const items_section_t *span = &s_sections[(uint32_t)section];
    NT_ASSERT(s_catalog != NULL && index < span->count);
    return s_catalog + span->offset + index * span->stride;
}

static const char *catalog_item_string(uint32_t item_index, uint32_t row_offset) {
    const uint8_t *item = catalog_row(ITEMS_SECTION_ITEMS, item_index);
    const items_section_t *strings = &s_sections[ITEMS_SECTION_STRINGS];
    const char *value = checked_string(s_catalog, strings, read_u32(item + row_offset));
    NT_ASSERT(value != NULL);
    return value;
}

#if ITEMS_CATALOG_CAPABILITY_COUNT > 0U
static uint32_t catalog_level_index(item_def_ref_t ref, uint32_t level) {
    const uint8_t *item = catalog_row(ITEMS_SECTION_ITEMS, ref._index);
    uint32_t level_count = read_u32(item + 28U);
    NT_ASSERT(level > 0U && level <= level_count);
    return read_u32(item + 24U) + level - 1U;
}

static bool items_catalog_internal_is_kind(item_def_ref_t ref, const char *kind) {
    NT_ASSERT(kind != NULL);
    NT_ASSERT(s_catalog != NULL && s_catalog_size >= ITEMS_HEADER_SIZE);
    if (ref._index >= s_item_count) {
        return false;
    }
    return strcmp(catalog_item_string(ref._index, 12U), kind) == 0;
}

static uint32_t items_catalog_internal_level_count(item_def_ref_t ref) {
    return read_u32(catalog_row(ITEMS_SECTION_ITEMS, ref._index) + 28U);
}

static bool items_catalog_internal_level_exists(item_def_ref_t ref, uint32_t level) {
    return level > 0U && level <= items_catalog_internal_level_count(ref);
}

static int64_t items_catalog_internal_level_i64(
    item_def_ref_t ref, uint32_t level, uint32_t field_index) {
    NT_ASSERT(field_index < s_sections[ITEMS_SECTION_FIELDS].count);
    const uint8_t *row = catalog_row(
        ITEMS_SECTION_LEVELS, catalog_level_index(ref, level));
    uint32_t value_start = read_u32(row + 8U);
    uint32_t value_count = read_u32(row + 12U);
    for (uint32_t index = 0; index < value_count; ++index) {
        const uint8_t *value = catalog_row(ITEMS_SECTION_VALUES, value_start + index);
        if (read_u32(value + 8U) == field_index) {
            return read_i64(value + 16U);
        }
    }
    NT_ASSERT(false && "generated required field is absent from runtime level");
    return 0;
}

static item_transition_t items_catalog_internal_level_transition(
    item_def_ref_t ref, uint32_t level) {
    uint32_t level_index = catalog_level_index(ref, level);
    const uint8_t *row = catalog_row(ITEMS_SECTION_LEVELS, level_index);
    if ((read_u32(row + 24U) & ITEMS_LEVEL_FREE) != 0U) {
        return (item_transition_t){ITEM_TRANSITION_FREE, {0U}};
    }
    if (read_u32(row + 20U) == 0U) {
        return (item_transition_t){ITEM_TRANSITION_UNAVAILABLE, {0U}};
    }
    return (item_transition_t){
        ITEM_TRANSITION_COST, {ITEMS_COST_LEVEL_FLAG | (level_index + 1U)}};
}
#endif

bool items_try_get(item_id_t id, item_def_ref_t *out) {
    NT_ASSERT(s_catalog != NULL && s_catalog_size >= ITEMS_HEADER_SIZE);
    if (out == NULL) {
        return false;
    }
    for (uint32_t index = 0; index < s_item_count; ++index) {
        if (read_u64(catalog_row(ITEMS_SECTION_ITEMS, index)) == id.value) {
            out->_index = index;
            return true;
        }
    }
    return false;
}

bool items_exists(item_id_t id) {
    item_def_ref_t ignored;
    return items_try_get(id, &ignored);
}

item_def_ref_t items_get(item_id_t id) {
    item_def_ref_t ref = {UINT32_MAX};
    const bool found = items_try_get(id, &ref);
    NT_ASSERT(found && "items_get: unknown item id; use items_exists/items_try_get for expected absence");
    return ref;
}

bool items_try_get_string(const char *def_id, item_def_ref_t *out) {
    NT_ASSERT(s_catalog != NULL && s_catalog_size >= ITEMS_HEADER_SIZE);
    if (def_id == NULL || out == NULL) {
        return false;
    }
    item_def_ref_t ref;
    const item_id_t id = {nt_hash64_str(def_id).value};
    if (!items_try_get(id, &ref) || strcmp(catalog_item_string(ref._index, 8U), def_id) != 0) {
        return false;
    }
    *out = ref;
    return true;
}

item_core_t items_core(item_def_ref_t ref) {
    const uint8_t *item = catalog_row(ITEMS_SECTION_ITEMS, ref._index);
    item_core_t core = {{read_u64(item)}, (int64_t)read_u32(item + 20U)};
    return core;
}

const char *items_def_id(item_def_ref_t ref) {
    NT_ASSERT(s_catalog != NULL && ref._index < s_item_count && "items_def_id: invalid item ref");
    return catalog_item_string(ref._index, 8U);
}

uint32_t items_level_count(item_def_ref_t ref) {
    NT_ASSERT(s_catalog != NULL && ref._index < s_item_count && "items_level_count: invalid item ref");
    return read_u32(catalog_row(ITEMS_SECTION_ITEMS, ref._index) + 28U);
}

bool items_level_exists(item_def_ref_t ref, uint32_t level) {
    return s_catalog != NULL && ref._index < s_item_count && level > 0U &&
        level <= read_u32(catalog_row(ITEMS_SECTION_ITEMS, ref._index) + 28U);
}

item_transition_t items_level_transition(item_def_ref_t ref, uint32_t level) {
    NT_ASSERT(items_level_exists(ref, level) && "items_level_transition: invalid item or level");
    const uint8_t *item = catalog_row(ITEMS_SECTION_ITEMS, ref._index);
    uint32_t level_index = read_u32(item + 24U) + level - 1U;
    const uint8_t *row = catalog_row(ITEMS_SECTION_LEVELS, level_index);
    if ((read_u32(row + 24U) & ITEMS_LEVEL_FREE) != 0U) {
        return (item_transition_t){ITEM_TRANSITION_FREE, {0U}};
    }
    if (read_u32(row + 20U) == 0U) {
        return (item_transition_t){ITEM_TRANSITION_UNAVAILABLE, {0U}};
    }
    return (item_transition_t){
        ITEM_TRANSITION_COST, {ITEMS_COST_LEVEL_FLAG | (level_index + 1U)}};
}

bool items_has_currency(item_def_ref_t ref) {
    if (s_catalog == NULL || ref._index >= s_item_count) {
        return false;
    }
    return (read_u32(catalog_row(ITEMS_SECTION_ITEMS, ref._index) + 40U)
        & ITEMS_ITEM_HAS_CURRENCY) != 0U;
}

int64_t items_currency_cap(item_def_ref_t ref) {
    NT_ASSERT(items_has_currency(ref) && "items_currency_cap: item has no currency block");
    return read_i64(catalog_row(ITEMS_SECTION_ITEMS, ref._index) + 48U);
}

item_transition_t items_acquire_transition(item_def_ref_t ref) {
    const uint8_t *item = catalog_row(ITEMS_SECTION_ITEMS, ref._index);
    if ((read_u32(item + 40U) & ITEMS_ITEM_ACQUIRE_FREE) != 0U) {
        return (item_transition_t){ITEM_TRANSITION_FREE, {0U}};
    }
    if (read_u32(item + 36U) == 0U) {
        return (item_transition_t){ITEM_TRANSITION_UNAVAILABLE, {0U}};
    }
    return (item_transition_t){ITEM_TRANSITION_COST, {ref._index + 1U}};
}

static void catalog_cost_span(
    item_cost_ref_t cost, uint32_t *out_start, uint32_t *out_count) {
    NT_ASSERT(out_start != NULL && out_count != NULL);
    uint32_t owner = cost._opaque & ~ITEMS_COST_LEVEL_FLAG;
    NT_ASSERT(owner > 0U && "items cost: invalid runtime cost ref");
    if ((cost._opaque & ITEMS_COST_LEVEL_FLAG) != 0U) {
        const uint8_t *level = catalog_row(ITEMS_SECTION_LEVELS, owner - 1U);
        *out_start = read_u32(level + 16U);
        *out_count = read_u32(level + 20U);
    } else {
        const uint8_t *item = catalog_row(ITEMS_SECTION_ITEMS, owner - 1U);
        *out_start = read_u32(item + 32U);
        *out_count = read_u32(item + 36U);
    }
    NT_ASSERT(*out_count > 0U && "items cost: empty runtime cost ref");
}

uint32_t items_cost_count(item_cost_ref_t cost) {
    uint32_t start = 0U;
    uint32_t count = 0U;
    catalog_cost_span(cost, &start, &count);
    (void)start;
    return count;
}

item_cost_entry_t items_cost_at(item_cost_ref_t cost, uint32_t index) {
    uint32_t start = 0U;
    uint32_t count = 0U;
    catalog_cost_span(cost, &start, &count);
    NT_ASSERT(index < count && "items_cost_at: index out of range");
    const uint8_t *entry = catalog_row(ITEMS_SECTION_COSTS, start + index);
    const uint8_t *target = catalog_row(ITEMS_SECTION_ITEMS, read_u32(entry));
    return (item_cost_entry_t){{read_u64(target)}, read_i64(entry + 8U)};
}

void items_register_debug_labels(void) {
    NT_ASSERT(s_catalog != NULL && s_catalog_size >= ITEMS_HEADER_SIZE);
    for (uint32_t index = 0; index < s_item_count; ++index) {
        const uint8_t *item = catalog_row(ITEMS_SECTION_ITEMS, index);
        nt_hash_register_label64(
            (nt_hash64_t){read_u64(item)}, catalog_item_string(index, 8U));
    }
}
