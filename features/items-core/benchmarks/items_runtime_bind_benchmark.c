#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "features/items/items.h"
#include "time/nt_time.h"


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

static uint8_t *load_blob(const char *path, uint32_t *out_size) {
    FILE *stream = fopen(path, "rb");
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

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: items_runtime_bind_benchmark <items.catalog>\n");
        return 2;
    }
    uint32_t blob_size = 0U;
    uint8_t *blob = load_blob(argv[1], &blob_size);
    if (blob == NULL) return 3;

    uint64_t samples[9];
    for (uint32_t index = 0U; index < 9U; ++index) {
        items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
        uint64_t started = nt_time_nanos();
        if (!items_catalog_try_bind(blob, blob_size, &error)) {
            free(blob);
            return 4;
        }
        samples[index] = nt_time_nanos() - started;
        items_catalog_shutdown();
    }
    sort_u64(samples, 9U);

    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
    if (!items_catalog_try_bind(blob, blob_size, &error)) {
        free(blob);
        return 5;
    }
    printf(
        "{\"schema\":\"items.runtime.bind.benchmark.v1\","
        "\"bind_samples\":9,\"startup_bind_ns\":%llu,"
        "\"steady_owned_bytes\":%u,\"transient_input_plus_owned_bytes\":%llu,"
        "\"content_fingerprint\":\"%016llx\"}\n",
        (unsigned long long)samples[4], blob_size, (unsigned long long)blob_size * 2ULL,
        (unsigned long long)items_catalog_content_fingerprint());
    items_catalog_shutdown();
    free(blob);
    return 0;
}
