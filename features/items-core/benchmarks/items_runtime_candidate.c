#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "features/items/items.h"
#include "time/nt_time.h"

#ifndef ITEMS_BENCHMARK_RUNTIME
#define ITEMS_BENCHMARK_RUNTIME 0
#endif

#ifndef ITEMS_BENCHMARK_ACCESS_RUNS
#define ITEMS_BENCHMARK_ACCESS_RUNS UINT32_C(1000000)
#endif

#if ITEMS_BENCHMARK_RUNTIME
static void sort_u64(uint64_t *values, uint32_t count) {
    for (uint32_t index = 1U; index < count; ++index) {
        uint64_t value = values[index];
        uint32_t position = index;
        while (position > 0U && values[position - 1U] > value) {
            values[position] = values[position - 1U];
            --position;
        }
        values[position] = value;
    }
}

static uint8_t *load_blob(uint32_t *out_size) {
    FILE *stream = fopen(ITEMS_RUNTIME_PACKAGE_PATH, "rb");
    if (stream == NULL || fseek(stream, 0, SEEK_END) != 0) return NULL;
    long size = ftell(stream);
    if (size <= 0 || (uint64_t)size > UINT32_MAX || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return NULL;
    }
    uint8_t *bytes = (uint8_t *)malloc((size_t)size);
    if (bytes == NULL || fread(bytes, 1U, (size_t)size, stream) != (size_t)size) {
        free(bytes);
        fclose(stream);
        return NULL;
    }
    fclose(stream);
    *out_size = (uint32_t)size;
    return bytes;
}
#endif

int main(void) {
    uint64_t bind_ns = 0U;
    uint32_t steady_bytes = 0U;
    uint32_t transient_bytes = 0U;
#if ITEMS_BENCHMARK_RUNTIME
    uint32_t blob_size = 0U;
    uint8_t *blob = load_blob(&blob_size);
    if (blob == NULL) return 2;
    uint64_t samples[9];
    for (uint32_t index = 0U; index < 9U; ++index) {
        items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
        uint64_t start = nt_time_nanos();
        if (!items_catalog_try_bind(blob, blob_size, &error)) return 3;
        samples[index] = nt_time_nanos() - start;
        items_catalog_shutdown();
    }
    sort_u64(samples, 9U);
    bind_ns = samples[4];
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
    if (!items_catalog_try_bind(blob, blob_size, &error)) return 4;
    steady_bytes = blob_size;
    transient_bytes = blob_size * 2U;
    free(blob);
#endif

    item_def_ref_t sword = items_get(ITEM_GAME_SWORD);
    volatile int64_t checksum = 0;
    for (uint32_t index = 0U; index < UINT32_C(10000); ++index) {
        item_weapon_level_t level = items_weapon_level(sword, index % 3U + 1U);
        checksum += level.attack + (int64_t)level.cost_to_reach.kind;
    }
    uint64_t start = nt_time_nanos();
    for (uint32_t index = 0U; index < ITEMS_BENCHMARK_ACCESS_RUNS; ++index) {
        item_weapon_level_t level = items_weapon_level(sword, index % 3U + 1U);
        checksum += level.attack + (int64_t)level.cost_to_reach.kind;
        if (level.cost_to_reach.kind == ITEM_TRANSITION_COST) {
            checksum += items_cost_at(level.cost_to_reach.cost, 0U).count;
        }
    }
    uint64_t elapsed = nt_time_nanos() - start;
    printf(
        "{\"schema\":\"items.runtime.candidate.v1\",\"candidate\":\"%s\","
        "\"startup_bind_ns\":%llu,\"access_ns_per_op\":%.3f,"
        "\"steady_owned_bytes\":%u,\"transient_input_plus_owned_bytes\":%u,"
        "\"access_runs\":%u,\"checksum\":%lld}\n",
        ITEMS_BENCHMARK_RUNTIME ? "blob" : "c_arrays",
        (unsigned long long)bind_ns,
        (double)elapsed / (double)ITEMS_BENCHMARK_ACCESS_RUNS,
        steady_bytes,
        transient_bytes,
        ITEMS_BENCHMARK_ACCESS_RUNS,
        (long long)checksum);

#if ITEMS_BENCHMARK_RUNTIME
    items_catalog_shutdown();
#endif
    return 0;
}
