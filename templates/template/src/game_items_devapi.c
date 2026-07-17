#if NT_DEVAPI_ENABLED

#include "game_items.h"

#include "cJSON.h"
#include "devapi/nt_devapi.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static char s_items_error[256];

enum { FILTER_PARSE_ERROR = -2 };

static bool items_fail(nt_devapi_error *error, const char *code, const char *message) {
    (void)snprintf(s_items_error, sizeof(s_items_error), "%s", message);
    error->code = code;
    error->message = s_items_error;
    return false;
}

static bool exact_u32(const cJSON *value, uint32_t *out) {
    if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble) ||
        value->valuedouble < 0.0 || value->valuedouble > (double)UINT32_MAX ||
        floor(value->valuedouble) != value->valuedouble) {
        return false;
    }
    *out = (uint32_t)value->valuedouble;
    return true;
}

static bool optional_u32(
    const cJSON *params, const char *name, uint32_t fallback,
    uint32_t *out, nt_devapi_error *error) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, name);
    if (value == NULL) {
        *out = fallback;
        return true;
    }
    if (!exact_u32(value, out)) {
        (void)snprintf(s_items_error, sizeof(s_items_error), "%s must be an exact uint32", name);
        error->code = "bad_params";
        error->message = s_items_error;
        return false;
    }
    return true;
}

static bool required_u32(
    const cJSON *params, const char *name, uint32_t *out,
    nt_devapi_error *error) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, name);
    if (value == NULL || !exact_u32(value, out)) {
        (void)snprintf(s_items_error, sizeof(s_items_error), "%s is required as an exact uint32", name);
        error->code = "bad_params";
        error->message = s_items_error;
        return false;
    }
    return true;
}

static const char *policy_name(items_container_policy_t policy) {
    switch (policy) {
        case ITEMS_CONTAINER_POLICY_GENERIC: return "generic";
        case ITEMS_CONTAINER_POLICY_CURRENCY_ONLY: return "currency_only";
        case ITEMS_CONTAINER_POLICY_EQUIPMENT: return "equipment";
    }
    return "unknown";
}

static const char *lifetime_name(items_lifetime_t lifetime) {
    return lifetime == ITEMS_LIFETIME_EPHEMERAL ? "ephemeral" : "persistent";
}

static int policy_filter(const cJSON *params, nt_devapi_error *error) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, "policy");
    if (value == NULL) { return ITEMS_INSPECTION_FILTER_ANY; }
    if (!cJSON_IsString(value)) {
        items_fail(error, "bad_params", "policy must be generic, currency_only, or equipment");
        return FILTER_PARSE_ERROR;
    }
    if (strcmp(value->valuestring, "generic") == 0) { return ITEMS_CONTAINER_POLICY_GENERIC; }
    if (strcmp(value->valuestring, "currency_only") == 0) { return ITEMS_CONTAINER_POLICY_CURRENCY_ONLY; }
    if (strcmp(value->valuestring, "equipment") == 0) { return ITEMS_CONTAINER_POLICY_EQUIPMENT; }
    items_fail(error, "bad_params", "policy must be generic, currency_only, or equipment");
    return FILTER_PARSE_ERROR;
}

static int lifetime_filter(const cJSON *params, nt_devapi_error *error) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, "lifetime");
    if (value == NULL) { return ITEMS_INSPECTION_FILTER_ANY; }
    if (!cJSON_IsString(value)) {
        items_fail(error, "bad_params", "lifetime must be persistent or ephemeral");
        return FILTER_PARSE_ERROR;
    }
    if (strcmp(value->valuestring, "persistent") == 0) { return ITEMS_LIFETIME_PERSISTENT; }
    if (strcmp(value->valuestring, "ephemeral") == 0) { return ITEMS_LIFETIME_EPHEMERAL; }
    items_fail(error, "bad_params", "lifetime must be persistent or ephemeral");
    return FILTER_PARSE_ERROR;
}

static void add_ref(cJSON *object, items_container_ref_t ref) {
    cJSON *value = cJSON_AddObjectToObject(object, "ref");
    cJSON_AddNumberToObject(value, "index", (double)ref.index);
    cJSON_AddNumberToObject(value, "generation", (double)ref.generation);
}

static void add_entry_ref(cJSON *object, item_entry_ref_t ref) {
    cJSON *value = cJSON_AddObjectToObject(object, "ref");
    cJSON_AddNumberToObject(value, "index", (double)ref.index);
    cJSON_AddNumberToObject(value, "generation", (double)ref.generation);
}

static bool result_fits(cJSON *result, nt_devapi_error *error) {
    char *json = cJSON_PrintUnformatted(result);
    if (json == NULL) { return items_fail(error, "internal", "failed to measure inspection output"); }
    size_t bytes = strlen(json);
    cJSON_free(json);
    return bytes <= ITEMS_INSPECTION_MAX_BYTES ||
        items_fail(error, "internal", "inspection output exceeds the hard byte budget");
}

static bool inspection_result(
    items_inspection_result_t result, nt_devapi_error *error) {
    switch (result) {
        case ITEMS_INSPECTION_OK: return true;
        case ITEMS_INSPECTION_BAD_QUERY:
            return items_fail(error, "bad_params", "invalid inspection query");
        case ITEMS_INSPECTION_NOT_FOUND:
            return items_fail(error, "bad_params", "container not found");
        case ITEMS_INSPECTION_ROW_LIMIT:
        case ITEMS_INSPECTION_BYTE_LIMIT:
        case ITEMS_INSPECTION_CONTEXT_LIMIT:
            return items_fail(error, "internal", "inspection hard budget was exceeded");
    }
    return items_fail(error, "internal", "unknown inspection result");
}

static bool container_list(
    const cJSON *params, cJSON *result_obj, nt_devapi_error *error, void *user) {
    (void)user;
    uint32_t offset = 0;
    uint32_t limit = 32;
    if (!optional_u32(params, "offset", 0, &offset, error) ||
        !optional_u32(params, "limit", 32, &limit, error)) {
        return false;
    }
    if (limit == 0 || limit > ITEMS_INSPECTION_MAX_ROWS) {
        return items_fail(error, "bad_params", "limit out of range [1,64]");
    }
    int policy = policy_filter(params, error);
    int lifetime = lifetime_filter(params, error);
    if (policy == FILTER_PARSE_ERROR || lifetime == FILTER_PARSE_ERROR) { return false; }
    bool include_empty = true;
    const cJSON *empty = cJSON_GetObjectItemCaseSensitive(params, "include_empty");
    if (empty != NULL) {
        if (!cJSON_IsBool(empty)) {
            return items_fail(error, "bad_params", "include_empty must be boolean");
        }
        include_empty = cJSON_IsTrue(empty);
    }

    items_container_inspection_t rows[ITEMS_INSPECTION_MAX_ROWS];
    items_inspection_page_t page = {0};
    items_container_list_query_t query = {
        .offset = offset,
        .policy = policy,
        .lifetime = lifetime,
        .include_empty = include_empty,
        .budget = {
            .max_rows = limit,
            .max_bytes = ITEMS_INSPECTION_MAX_BYTES,
            .max_context_rows = ITEMS_INSPECTION_MAX_CONTEXT_ROWS,
        },
    };
    items_inspection_result_t status = items_inspect_container_list(
        &query, rows, ITEMS_INSPECTION_MAX_ROWS, &page);
    if (!inspection_result(status, error)) { return false; }

    cJSON *containers = cJSON_AddArrayToObject(result_obj, "containers");
    for (uint32_t i = 0; i < page.count; i++) {
        const items_container_inspection_t *row = &rows[i];
        cJSON *object = cJSON_CreateObject();
        add_ref(object, row->ref);
        if (row->lifetime == ITEMS_LIFETIME_PERSISTENT) {
            cJSON_AddNumberToObject(object, "container_id", (double)row->container_id);
        } else {
            cJSON_AddNullToObject(object, "container_id");
        }
        cJSON_AddNumberToObject(object, "capacity", (double)row->capacity);
        cJSON_AddStringToObject(object, "policy", policy_name(row->policy));
        cJSON_AddStringToObject(object, "lifetime", lifetime_name(row->lifetime));
        cJSON_AddNumberToObject(object, "entry_count", (double)row->entry_count);
        cJSON_AddNumberToObject(object, "quarantined_count", (double)row->quarantined_count);
        cJSON_AddItemToArray(containers, object);
    }
    cJSON_AddNumberToObject(result_obj, "next_offset", (double)page.next_offset);
    cJSON_AddBoolToObject(result_obj, "has_more", page.has_more);
    cJSON_AddNumberToObject(result_obj, "projected_bytes", (double)page.projected_bytes);
    cJSON_AddNumberToObject(result_obj, "context_rows", (double)page.context_rows);
    return result_fits(result_obj, error);
}

static bool resolve_container(
    const cJSON *params, items_container_ref_t *out, nt_devapi_error *error) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, "container_id");
    const cJSON *ref = cJSON_GetObjectItemCaseSensitive(params, "ref");
    if ((id == NULL) == (ref == NULL)) {
        return items_fail(error, "bad_params", "provide exactly one of container_id or ref");
    }
    if (id != NULL) {
        uint32_t value = 0;
        if (!exact_u32(id, &value) || value == ITEMS_ID_NONE || value == ITEMS_ID_RESERVED ||
            !items_container_try_from_id(value, out)) {
            return items_fail(error, "bad_params", "container_id was not found");
        }
        return true;
    }
    if (!cJSON_IsObject(ref)) {
        return items_fail(error, "bad_params", "ref must contain exact uint32 index and generation");
    }
    const cJSON *index = cJSON_GetObjectItemCaseSensitive(ref, "index");
    const cJSON *generation = cJSON_GetObjectItemCaseSensitive(ref, "generation");
    if (!exact_u32(index, &out->index) || !exact_u32(generation, &out->generation) ||
        out->generation == 0) {
        return items_fail(error, "bad_params", "ref must contain exact uint32 index and generation");
    }
    return true;
}

static bool container_inspect(
    const cJSON *params, cJSON *result_obj, nt_devapi_error *error, void *user) {
    (void)user;
    items_container_ref_t container = ITEMS_CONTAINER_REF_NONE;
    uint32_t slot_begin = 0;
    uint32_t slot_end = 0;
    uint32_t offset = 0;
    uint32_t limit = 32;
    if (!resolve_container(params, &container, error) ||
        !required_u32(params, "slot_begin", &slot_begin, error) ||
        !required_u32(params, "slot_end", &slot_end, error) ||
        !optional_u32(params, "offset", 0, &offset, error) ||
        !optional_u32(params, "limit", 32, &limit, error)) {
        return false;
    }
    if (limit == 0 || limit > ITEMS_INSPECTION_MAX_ROWS) {
        return items_fail(error, "bad_params", "limit out of range [1,64]");
    }
    const cJSON *definition = cJSON_GetObjectItemCaseSensitive(params, "def_id");
    if (definition != NULL && (!cJSON_IsString(definition) || definition->valuestring[0] == '\0')) {
        return items_fail(error, "bad_params", "def_id must be a non-empty string");
    }
    int quarantined = ITEMS_INSPECTION_FILTER_ANY;
    const cJSON *quarantine = cJSON_GetObjectItemCaseSensitive(params, "quarantined");
    if (quarantine != NULL) {
        if (!cJSON_IsBool(quarantine)) {
            return items_fail(error, "bad_params", "quarantined must be boolean");
        }
        quarantined = cJSON_IsTrue(quarantine) ? 1 : 0;
    }

    items_entry_inspection_t rows[ITEMS_INSPECTION_MAX_ROWS];
    items_inspection_page_t page = {0};
    items_entry_list_query_t query = {
        .offset = offset,
        .slot_begin = slot_begin,
        .slot_end = slot_end,
        .def_id = definition != NULL ? definition->valuestring : NULL,
        .quarantined = quarantined,
        .budget = {
            .max_rows = limit,
            .max_bytes = ITEMS_INSPECTION_MAX_BYTES,
            .max_context_rows = ITEMS_INSPECTION_MAX_CONTEXT_ROWS,
        },
    };
    items_inspection_result_t status = items_inspect_container_entries(
        container, &query, rows, ITEMS_INSPECTION_MAX_ROWS, &page);
    if (!inspection_result(status, error)) { return false; }

    items_lifetime_t lifetime = items_container_lifetime(container);
    cJSON *summary = cJSON_AddObjectToObject(result_obj, "container");
    add_ref(summary, container);
    if (lifetime == ITEMS_LIFETIME_PERSISTENT) {
        cJSON_AddNumberToObject(summary, "container_id", (double)items_container_id(container));
    } else {
        cJSON_AddNullToObject(summary, "container_id");
    }
    cJSON_AddNumberToObject(summary, "capacity", (double)items_container_capacity(container));
    cJSON_AddStringToObject(summary, "policy", policy_name(items_container_policy(container)));
    cJSON_AddStringToObject(summary, "lifetime", lifetime_name(lifetime));

    cJSON *entries = cJSON_AddArrayToObject(result_obj, "entries");
    for (uint32_t i = 0; i < page.count; i++) {
        const items_entry_inspection_t *row = &rows[i];
        cJSON *object = cJSON_CreateObject();
        add_entry_ref(object, row->ref);
        if (row->view.lifetime == ITEMS_LIFETIME_PERSISTENT) {
            cJSON_AddNumberToObject(object, "entry_id", (double)row->view.entry_id);
        } else {
            cJSON_AddNullToObject(object, "entry_id");
        }
        cJSON_AddNumberToObject(object, "slot", (double)row->view.slot);
        cJSON_AddStringToObject(object, "def_id", row->view.def_id);
        char count[32];
        (void)snprintf(count, sizeof(count), "%" PRId64, row->view.count);
        cJSON_AddStringToObject(object, "count", count);
        cJSON_AddNumberToObject(object, "level", (double)row->view.level);
        cJSON_AddNumberToObject(object, "durability", (double)row->view.durability);
        cJSON_AddBoolToObject(object, "quarantined", row->view.quarantined);
        cJSON_AddStringToObject(object, "lifetime", lifetime_name(row->view.lifetime));
        cJSON_AddItemToArray(entries, object);
    }
    cJSON_AddNumberToObject(result_obj, "next_offset", (double)page.next_offset);
    cJSON_AddBoolToObject(result_obj, "has_more", page.has_more);
    cJSON_AddNumberToObject(result_obj, "projected_bytes", (double)page.projected_bytes);
    cJSON_AddNumberToObject(result_obj, "context_rows", (double)page.context_rows);
    return result_fits(result_obj, error);
}

void game_items_register_devapi(void) {
    static const nt_devapi_command_desc list_desc = {
        "game.items.container.list", "game",
        "List Items containers with bounded filtered pagination.",
        "offset?, limit?, policy?, lifetime?, include_empty?",
        "containers, next_offset, has_more, projected_bytes, context_rows",
        "immediate", "none"};
    static const nt_devapi_command_desc inspect_desc = {
        "game.items.container.inspect", "game",
        "Inspect one Items container over an explicit bounded slot range.",
        "container_id|ref, slot_begin, slot_end, offset?, limit?, def_id?, quarantined?",
        "container, entries, next_offset, has_more, projected_bytes, context_rows",
        "immediate", "none"};
    (void)nt_devapi_register(&list_desc, container_list, NULL);
    (void)nt_devapi_register(&inspect_desc, container_inspect, NULL);
}

#endif /* NT_DEVAPI_ENABLED */
