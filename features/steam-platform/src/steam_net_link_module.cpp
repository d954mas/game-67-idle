/* What the process has once for every Steam link: the registry that routes Steam's connection
   callbacks to a link or to a close in hand, the per-frame hook (dial after the certificate,
   keepalives, closes), and the fake network. All of it is fixed memory and is forgotten when
   the platform stops. */

#include "features/steam_platform/steam_net_link.h"

#include "steam_net_link_internal.hpp"
#include "steam_platform_internal.h"

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>


namespace steam_net_link {
namespace {

struct module_state {
    bool hooked; /* listener and hook are in place for this start of the platform */
    uint32_t next_serial;
    link *links[STEAM_NET_LINK_LINKS_MAX];
};

module_state s_module;

ISteamNetworkingSockets *sockets() { return SteamNetworkingSockets(); }

link *registered(uint32_t index, uint32_t serial) {
    link *l = index < STEAM_NET_LINK_LINKS_MAX ? s_module.links[index] : nullptr;
    return l != nullptr && l->serial == serial ? l : nullptr;
}

void on_status(void *, int, const void *data, size_t size) {
    if (size < sizeof(SteamNetConnectionStatusChangedCallback_t)) {
        return;
    }
    const auto *change = static_cast<const SteamNetConnectionStatusChangedCallback_t *>(data);
    const SteamNetConnectionInfo_t &info = change->m_info;
    if (closing_status(change->m_hConn, info)) {
        return;
    }
    /* The user data now, not the one copied into the callback when it was queued. */
    uint32_t index = 0, serial = 0;
    net_link_peer_t handle = NET_LINK_PEER_NONE;
    link *l = nullptr;
    if (steam_net_link_user_data_split(sockets()->GetConnectionUserData(change->m_hConn), &index, &serial, &handle)) {
        l = registered(index, serial);
    }
    const bool knocking = info.m_eState == k_ESteamNetworkingConnectionState_Connecting && info.m_hListenSocket != k_HSteamListenSocket_Invalid;
    if (l != nullptr && handle == NET_LINK_PEER_NONE) {
        if (knocking && l->host && info.m_hListenSocket == l->listen) {
            link_incoming(l, change->m_hConn, info);
        }
    } else if (l != nullptr) {
        link_status(l, handle, change->m_hConn, info);
    } else if (knocking && closing_turns_away(info.m_hListenSocket)) {
        /* The host that listened here is gone; the guest reads UNREACHABLE. */
        sockets()->CloseConnection(change->m_hConn, STEAM_NET_LINK_END_GENERIC, "host left", false);
    }
}

void on_moment(void *, steam_platform_moment_t moment) {
    if (moment == STEAM_PLATFORM_MOMENT_STOPPING) {
        for (link *&l : s_module.links) {
            if (l != nullptr) {
                link_orphan(l);
                l = nullptr;
            }
        }
        /* Steam's shutdown closes the kept sockets and cuts every close, and clears the fake
           network too (the probe records it). */
        closing_forget();
        s_module.hooked = false;
        return;
    }
    const SteamNetworkingMicroseconds now = SteamNetworkingUtils()->GetLocalTimestamp();
    for (link *l : s_module.links) {
        if (l != nullptr) {
            link_frame(l, now);
        }
    }
    closing_frame(now);
}

bool hook_up() {
    if (s_module.hooked) {
        return true;
    }
    const int id = SteamNetConnectionStatusChangedCallback_t::k_iCallback;
    if (!steam_platform_internal_listen(id, on_status, nullptr)) {
        return false;
    }
    if (!steam_platform_internal_hook(on_moment, nullptr)) {
        steam_platform_internal_unlisten(id, on_status, nullptr);
        return false;
    }
    s_module.hooked = true;
    return true;
}

void set_direction(const steam_net_link_fake_t *fake, ESteamNetworkingConfigValue loss, ESteamNetworkingConfigValue lag, ESteamNetworkingConfigValue reorder, ESteamNetworkingConfigValue duplicate) {
    const steam_net_link_fake_values_t values = steam_net_link_fake_values(fake);
    ISteamNetworkingUtils *utils = SteamNetworkingUtils();
    utils->SetGlobalConfigValueFloat(loss, values.loss_pct);
    utils->SetGlobalConfigValueInt32(lag, values.lag_ms);
    utils->SetGlobalConfigValueFloat(reorder, values.reorder_pct);
    utils->SetGlobalConfigValueFloat(duplicate, values.duplicate_pct);
}

void set_fake(const steam_net_link_fake_t *send, const steam_net_link_fake_t *recv, float reorder_ms) {
    set_direction(send, k_ESteamNetworkingConfig_FakePacketLoss_Send, k_ESteamNetworkingConfig_FakePacketLag_Send, k_ESteamNetworkingConfig_FakePacketReorder_Send,
                  k_ESteamNetworkingConfig_FakePacketDup_Send);
    set_direction(recv, k_ESteamNetworkingConfig_FakePacketLoss_Recv, k_ESteamNetworkingConfig_FakePacketLag_Recv, k_ESteamNetworkingConfig_FakePacketReorder_Recv,
                  k_ESteamNetworkingConfig_FakePacketDup_Recv);
    SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketReorder_Time, steam_net_link_reorder_ms(reorder_ms));
}

} // namespace

bool module_attach(link *l) {
    if (!hook_up()) {
        return false;
    }
    for (uint32_t i = 0; i < STEAM_NET_LINK_LINKS_MAX; i++) {
        if (s_module.links[i] == nullptr) {
            s_module.next_serial = s_module.next_serial >= STEAM_NET_LINK_SERIAL_MAX ? 1 : s_module.next_serial + 1;
            l->index = i;
            l->serial = s_module.next_serial;
            s_module.links[i] = l;
            return true;
        }
    }
    return false;
}

void module_detach(link *l) {
    if (l->index < STEAM_NET_LINK_LINKS_MAX && s_module.links[l->index] == l) {
        s_module.links[l->index] = nullptr;
    }
}

} // namespace steam_net_link

extern "C" bool steam_net_link_lingering(void) { return steam_platform_ready() && steam_net_link::closing_any(); }

extern "C" void steam_net_link_set_fake(const steam_net_link_fake_t *send, const steam_net_link_fake_t *recv, float reorder_ms) {
    if (!steam_platform_ready()) {
        return;
    }
    steam_net_link::set_fake(send, recv, reorder_ms);
}
