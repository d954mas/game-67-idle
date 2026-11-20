/* The Steam link in a build without the Steamworks SDK: there is no Steam to open a link on. */

#include "features/steam_platform/steam_net_link.h"

net_link_t *steam_net_link_host_ip(const steam_net_link_config_t *config, const char *address, uint16_t port) {
    (void)config;
    (void)address;
    (void)port;
    return NULL;
}

net_link_t *steam_net_link_join_ip(const steam_net_link_config_t *config, const char *address, uint16_t port) {
    (void)config;
    (void)address;
    (void)port;
    return NULL;
}

bool steam_net_link_lingering(void) { return false; }

void steam_net_link_set_fake(const steam_net_link_fake_t *send, const steam_net_link_fake_t *recv, float reorder_ms) {
    (void)send;
    (void)recv;
    (void)reorder_ms;
}
