#include "blockside_runtime_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void blockside_parse_args(int argc,
                          char **argv,
                          bool *devapi_enabled,
                          uint16_t *devapi_port,
                          int *window_width,
                          int *window_height) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--devapi") == 0) {
            *devapi_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                *devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
            }
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            int width = 0;
            int height = 0;
            if (sscanf(argv[++i], "%dx%d", &width, &height) == 2 && width > 0 && height > 0) {
                *window_width = width;
                *window_height = height;
            }
        }
    }
}
