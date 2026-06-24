#ifndef BLOCKSIDE_RUNTIME_CONFIG_H
#define BLOCKSIDE_RUNTIME_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

void blockside_parse_args(int argc,
                          char **argv,
                          bool *devapi_enabled,
                          uint16_t *devapi_port,
                          int *window_width,
                          int *window_height);

#endif /* BLOCKSIDE_RUNTIME_CONFIG_H */
