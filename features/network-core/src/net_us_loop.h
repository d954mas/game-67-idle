#ifndef NETWORK_CORE_NET_US_LOOP_H
#define NETWORK_CORE_NET_US_LOOP_H

/* Private to the uWebSockets server backend. */

#include "libusockets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Arms the loop's timeout sweep; once per loop, before its first pass. */
void net_us_loop_integrate(struct us_loop_t *loop);
/* Waits up to `timeout_ms` (0: not at all) and dispatches what is ready.
   Returns how many polls were ready. */
int net_us_loop_run_once(struct us_loop_t *loop, int timeout_ms);
int net_us_socket_fd(struct us_socket_t *socket);

#ifdef __cplusplus
}
#endif

#endif
