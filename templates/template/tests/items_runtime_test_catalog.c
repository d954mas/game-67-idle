#include "items_runtime_test_catalog.h"

#include "features/items/items.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

bool items_runtime_test_catalog_bind(void) {
    FILE *file = fopen(ITEMS_RUNTIME_PACKAGE_PATH, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "items test catalog: cannot open %s\n", ITEMS_RUNTIME_PACKAGE_PATH);
        if (file != NULL) {
            fclose(file);
        }
        return false;
    }

    const long size = ftell(file);
    if (size <= 0 || (uint64_t)size > UINT32_MAX || fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "items test catalog: invalid package size\n");
        fclose(file);
        return false;
    }

    uint8_t *bytes = malloc((size_t)size);
    if (bytes == NULL || fread(bytes, 1, (size_t)size, file) != (size_t)size) {
        fprintf(stderr, "items test catalog: cannot read package\n");
        free(bytes);
        fclose(file);
        return false;
    }
    fclose(file);

    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
    const bool bound = items_catalog_try_bind(bytes, (uint32_t)size, &error);
    free(bytes);
    if (!bound) {
        fprintf(stderr, "items test catalog: bind failed (%d)\n", (int)error);
    }
    return bound;
}
