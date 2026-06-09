#ifndef NT_DEVAPI_H
#define NT_DEVAPI_H

/* Temporary game-side debug command bus.
 *
 * This is intentionally shaped like the experimental engine devapi from the
 * jam project, but lives in the game so the engine submodule can stay clean.
 * Replace this target with the engine module when the feature lands upstream.
 *
 * Native protocol: one JSON request line over persistent TCP -> one JSON
 * response line. A line may also be a JSON array for ordered batch requests.
 * Frame waits are asynchronous barriers: submit may return no immediate
 * response, then poll_response returns it when the barrier is satisfied.
 *
 * Request:
 *   {"request_id":1,"method":"input.key","params":{"key":"D","mode":"tap"}}
 *
 * Response:
 *   {"request_id":1,"ok":true,"result":{}}
 */

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"
#include "input/nt_input.h"

#ifndef NT_DEVAPI_ENABLED
#define NT_DEVAPI_ENABLED 0
#endif

typedef bool (*nt_devapi_handler_fn)(const cJSON *params, cJSON **result, char *error, int error_cap, void *user);

#if NT_DEVAPI_ENABLED

void nt_devapi_init(void);
void nt_devapi_shutdown(void);

bool nt_devapi_register(const char *name, nt_devapi_handler_fn fn, void *user);
int nt_devapi_submit(const char *line, char *out, int out_cap);
int nt_devapi_poll_response(char *out, int out_cap);

void nt_devapi_set_frame(uint64_t frame);
void nt_devapi_set_view(float fb_w, float fb_h, float logical_w, float logical_h);
void nt_devapi_clear_ui_elements(void);
bool nt_devapi_register_ui_element(const char *id, const char *label, float x, float y, float w, float h);
void nt_devapi_register_builtins(void);

/* Call after nt_input_poll() and after nt_devapi_net_poll(). */
void nt_devapi_apply_pending(void);

bool nt_devapi_net_start(uint16_t port);
void nt_devapi_net_poll(void);
void nt_devapi_net_stop(void);

#else

static inline void nt_devapi_init(void) {}
static inline void nt_devapi_shutdown(void) {}
static inline bool nt_devapi_register(const char *name, nt_devapi_handler_fn fn, void *user) {
    (void)name;
    (void)fn;
    (void)user;
    return false;
}
static inline int nt_devapi_submit(const char *line, char *out, int out_cap) {
    (void)line;
    (void)out;
    (void)out_cap;
    return 0;
}
static inline int nt_devapi_poll_response(char *out, int out_cap) {
    (void)out;
    (void)out_cap;
    return 0;
}
static inline void nt_devapi_set_frame(uint64_t frame) { (void)frame; }
static inline void nt_devapi_set_view(float fb_w, float fb_h, float logical_w, float logical_h) {
    (void)fb_w;
    (void)fb_h;
    (void)logical_w;
    (void)logical_h;
}
static inline void nt_devapi_clear_ui_elements(void) {}
static inline bool nt_devapi_register_ui_element(const char *id, const char *label, float x, float y, float w, float h) {
    (void)id;
    (void)label;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    return false;
}
static inline void nt_devapi_register_builtins(void) {}
static inline void nt_devapi_apply_pending(void) {}
static inline bool nt_devapi_net_start(uint16_t port) {
    (void)port;
    return false;
}
static inline void nt_devapi_net_poll(void) {}
static inline void nt_devapi_net_stop(void) {}

#endif

#endif
