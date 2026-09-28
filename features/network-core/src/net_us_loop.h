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
/* Lets go of what the passes hold on the loop; before the loop is freed. */
void net_us_loop_release(struct us_loop_t *loop);
/* Frees the sockets closed since the last pass; only outside a pass. */
void net_us_loop_free_closed(struct us_loop_t *loop);
/* Closes the handshakes of `context` waiting in the low-priority queue. */
void net_us_loop_close_waiting(struct us_loop_t *loop, struct us_socket_context_t *context, int ssl);
#if defined(__linux__)
int net_us_socket_fd(struct us_socket_t *socket);
#endif

#ifdef __cplusplus
}
#endif

#endif
