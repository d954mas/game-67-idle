/* What network-core needs from uSockets beyond its public API: one pass of
   the event loop bounded by the caller's deadline (us_loop_run only returns
   when the loop has nothing left) and a socket's descriptor. Both read the
   vendored uSockets' internals, so they move with its pin. */
#include "net_us_loop.h"

#include "internal/internal.h"

#include <stdlib.h>

#if !defined(LIBUS_USE_EPOLL) && !defined(LIBUS_USE_LIBUV)
#error "net_us_loop.c drives uSockets' epoll or libuv loop"
#endif

void net_us_loop_integrate(struct us_loop_t *loop) {
    /* The sweep timer that runs socket timeouts; us_loop_run arms it once. */
    us_loop_integrate(loop);
}

#if defined(LIBUS_USE_LIBUV)
/* The timer that bounds a pass: one per thread, as the loop is. */
static _Thread_local uv_timer_t *t_bound;

static void bound_elapsed(uv_timer_t *timer) { (void)timer; }

static void bound_closed(uv_handle_t *handle) { free(handle); }

int net_us_loop_run_once(struct us_loop_t *loop, int timeout_ms) {
    /* libuv runs the pre and post steps itself; the timer bounds the wait.
       It cannot say how much was ready, so a zero-timeout pass is reported
       as idle and not repeated. */
    if (timeout_ms <= 0) {
        uv_run(loop->uv_loop, UV_RUN_NOWAIT);
        return 0;
    }
    if (t_bound == NULL) {
        t_bound = (uv_timer_t *)malloc(sizeof *t_bound);
        if (t_bound == NULL) {
            uv_run(loop->uv_loop, UV_RUN_NOWAIT);
            return 0;
        }
        uv_timer_init(loop->uv_loop, t_bound);
    }
    /* The loop's clock is the last pass's; a timer set against it may
       already be due and the pass would not wait at all. */
    uv_update_time(loop->uv_loop);
    uv_timer_start(t_bound, bound_elapsed, (uint64_t)timeout_ms, 0U);
    uv_run(loop->uv_loop, UV_RUN_ONCE);
    uv_timer_stop(t_bound);
    return 0;
}

void net_us_loop_release(struct us_loop_t *loop) {
    /* An open handle keeps libuv from deleting the loop; the close
       completes in the pass uSockets runs before freeing it. */
    (void)loop;
    if (t_bound != NULL) {
        uv_close((uv_handle_t *)t_bound, bound_closed);
        t_bound = NULL;
    }
}
#else
int net_us_loop_run_once(struct us_loop_t *loop, int timeout_ms) {
    /* The body of us_loop_run's loop, with the wait bounded. */
    us_internal_loop_pre(loop);
    int ready = epoll_wait(loop->fd, loop->ready_polls, 1024, timeout_ms);
    loop->num_ready_polls = ready > 0 ? ready : 0;
    for (loop->current_ready_poll = 0; loop->current_ready_poll < loop->num_ready_polls; loop->current_ready_poll++) {
        struct us_poll_t *poll = (struct us_poll_t *)loop->ready_polls[loop->current_ready_poll].data.ptr;
        /* A poll closed earlier in this pass is marked NULL. */
        if (poll == NULL) { continue; }
        int events = (int)loop->ready_polls[loop->current_ready_poll].events;
        const int error = (int)(loop->ready_polls[loop->current_ready_poll].events & (EPOLLERR | EPOLLHUP));
        events &= us_poll_events(poll);
        if (events || error) { us_internal_dispatch_ready_poll(poll, error, events); }
    }
    us_internal_loop_post(loop);
    return loop->num_ready_polls;
}

void net_us_loop_release(struct us_loop_t *loop) { (void)loop; }
#endif

void net_us_loop_free_closed(struct us_loop_t *loop) {
    /* What a pass's post step does: closed sockets are only unlinked until
       then, and their contexts must outlive them. */
    us_internal_free_closed_sockets(loop);
}

void net_us_loop_close_waiting(struct us_loop_t *loop, struct us_socket_context_t *context, int ssl) {
    /* A TLS handshake past the per-pass budget waits in the loop's
       low-priority queue, linked to no context, so closing its context
       misses it; it would come back into a freed one. */
    for (;;) {
        struct us_socket_t *found = NULL;
        for (struct us_socket_t *s = loop->data.low_prio_head; s != NULL; s = s->next) {
            if (s->context == context) {
                found = s;
                break;
            }
        }
        if (found == NULL) { return; }
        us_socket_close(ssl, found, 0, NULL);
    }
}

#if defined(__linux__)
int net_us_socket_fd(struct us_socket_t *socket) {
    /* A TLS socket begins with its plain one, poll first. */
    return us_poll_fd((struct us_poll_t *)socket);
}
#endif
