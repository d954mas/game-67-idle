#ifndef FEATURES_NET_LINK_CAPTURE_INTERNAL_H
#define FEATURES_NET_LINK_CAPTURE_INTERNAL_H

#include "features/net_link/net_link_capture.h"

void net_link_capture_record(net_link_capture_t *capture, uint8_t tag, net_link_capture_direction_t direction, uint64_t identity, uint8_t lane, bool reliable, const void *data, uint32_t size);

#endif
