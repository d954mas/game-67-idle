#ifndef BLOCKSIDE_CAPTURE_H
#define BLOCKSIDE_CAPTURE_H

#include <stdbool.h>

bool blockside_capture_request(const char *output_path);
bool blockside_capture_write_pending(void);

#endif /* BLOCKSIDE_CAPTURE_H */
