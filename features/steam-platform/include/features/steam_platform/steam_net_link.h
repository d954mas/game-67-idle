#ifndef FEATURES_STEAM_PLATFORM_STEAM_NET_LINK_H
#define FEATURES_STEAM_PLATFORM_STEAM_NET_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "features/net_link/net_link.h"

/* net_link on Steam's networking sockets. A link opened here is used through net_link.h like
   any other; this header only opens links and sets what is Steam's own.

   It needs a started steam_platform. Every open answers NULL in a build without the SDK and
   while the platform is not ready, so a game calls it the same way in every build.

   steam_platform_run_frame is what moves connections: who came, who left, keepalives and
   deadlines. net_link_poll hands out what it collected and the messages that arrived, so a
   game calls run_frame before it polls.

   Shutdown, in this order: destroy every link; run frames while steam_net_link_lingering();
   then steam_platform_stop. A stop cuts every connection at once, closes still in hand too:
   what a net_link_close had queued and its reason are then lost, and the other side reads
   REMOTE_CLOSE. A link that outlives the stop stays safe: its next poll reports every peer
   still connected DISCONNECTED with TRANSPORT (a peer already leaving keeps its reason), sends
   answer NO_PEER and destroy only frees memory; the bytes of messages its last poll returned
   are gone with the stop.

   What net_link.h leaves to the transport:
   - identity is the SteamID the peer presents, checked by Steam's certificate; with
     local_without_proof it may be unchecked. Two windows of one account see the same SteamID.
     Zero when the other side never answered, and also for REFUSED: a host that refuses tells
     the guest nothing about itself;
   - a reason in the app range crosses unchanged; any other reason given to net_link_close
     crosses as Steam's generic code and reads as REMOTE_CLOSE on the other side, as does a
     peer whose Steam shut down. A host refuses with a code of this transport outside the app
     range; a peer that breaks net_link.h's rules is closed with another and reads TRANSPORT.
     A host (not this one) that closes a knocking guest with an app code before accepting it
     reaches the guest as that code, with no CONNECTED before it;
   - net_link_max_unreliable is the largest message Steam sends in one packet when it is sent
     alone (Steam's MTU_DataSize). Steam packs messages of one flush together and may split a
     message that follows other data across two packets; it is then lost when either packet is.
     A larger unreliable message is refused at send;
   - a reliable send is FULL and an unreliable one DROPPED when the peer's send buffer, shared
     by all lanes, has no room. A peer that never reads stays connected and sends to it go on
     answering FULL: closing it is the session's decision;
   - net_link_stats reads Steam's own estimates: they are negative until Steam has measured;
   - net_link_close hands the connection over and closes it when Steam has everything sent
     before it acknowledged, or after close_wait_s (see steam_net_link_lingering). Either way
     the peer reads the reason; what a large send buffer or a bad path could not deliver within
     close_wait_s is lost. Steam's own linger is never used: it dies without a word with its
     listen socket or the platform, and leaves the peer waiting for a timeout.

   Steam notices silence only while reliable data waits for an acknowledgement, so each end
   keeps an empty reliable message in flight, on its lane that goes first, when it has sent
   nothing reliable for a quarter of timeout_s. The receiving link consumes it on any of
   net-link's lanes, also one it does not have itself; the owner never sees it. A peer that
   sends them faster than the shortest timeout allows (16 a second, after a burst of 1024)
   breaks the rules like any other and reads TRANSPORT. The burst is what may wait in Steam
   while this side does not poll, so a link left unpolled for more than 1024 of its peer's
   keepalive periods (64 s with the shortest timeout, 43 minutes with the default) cuts that
   peer too. */

#ifdef __cplusplus
extern "C" {
#endif

enum {
    STEAM_NET_LINK_PEERS_MAX = 16,
    /* Links open at once in one process, hosts and guests together. */
    STEAM_NET_LINK_LINKS_MAX = 8,
};

typedef struct steam_net_link_config {
    net_link_lanes_t lanes;
    /* Host: peers connected at once, 1..STEAM_NET_LINK_PEERS_MAX; the next one is REFUSED.
       A peer whose DISCONNECTED was not polled yet still holds its place. Ignored by a guest. */
    uint32_t max_peers;
    /* The largest reliable message this link sends or accepts, 1..524288 (Steam's own limit);
       a peer that sends a larger one is disconnected with TRANSPORT. Zero: 16384. */
    uint32_t max_reliable_bytes;
    /* Bytes one peer may have waiting to be sent or acknowledged, all lanes together; at least
       max_reliable_bytes. Zero: Steam's 524288. */
    uint32_t send_buffer_bytes;
    /* A connected peer silent for this long is DISCONNECTED with TIMEOUT; it is read within
       about a second after. 0.25..600; zero: 10. */
    float timeout_s;
    /* A guest whose host has not answered for this long is DISCONNECTED with UNREACHABLE.
       The time also covers this process's wait for its own Steam certificate, which a fresh
       start needs before its first connect (one to two seconds were seen): a deadline that
       passes during that wait reads TRANSPORT, as this side could not even try; a certificate
       that comes in time is followed by a dial with what is left of it.
       0.25..600; zero: 10. */
    float connect_timeout_s;
    /* net_link_close waits this long for Steam to report everything sent before it
       acknowledged, then closes with the reason and drops the rest. It is the most a bad path
       adds to a shutdown loop on steam_net_link_lingering. 0.25..60; zero: 5. */
    float close_wait_s;
    /* IP mode, development only: a peer on this machine may connect while Steam cannot
       prove who it is (no certificate yet, Steam offline). Set it on both ends: with only one,
       the connect succeeds or fails by whether the certificates happen to be ready. Such a
       peer's identity is the SteamID it claims, unchecked. */
    bool local_without_proof;
} steam_net_link_config_t;

/* IP mode: plain UDP to an address, with no relay and no lobby. It is how two windows on one
   PC meet, and two machines on one LAN.

   `address` is the local interface to listen on, IPv4 or IPv6 text without a port: NULL
   listens on every interface (LAN play); "127.0.0.1" listens on the loopback only: guests
   from this machine, and nothing a system firewall filters.

   NULL when the config is out of range, the port is zero or taken, the address does not
   parse or names a port, STEAM_NET_LINK_LINKS_MAX links are open, or Steam is not ready.

   A host that closed peers with net_link_close keeps its listen socket after net_link_destroy
   while those closes are in hand (see steam_net_link_lingering): closing it would cut them.
   A guest that knocks there meanwhile reads UNREACHABLE at once. Hosting again on the same
   address and port ends the wait: those closes go out at once with their codes and what they
   still held is lost. Another address on that port does not end the wait, and whether both
   may hold the port is the system's call.

   Steam frees a port a moment after its listen socket closes. For 250 ms after a host of this
   process left (its destroy, or the end of its closes in hand) an open that cannot bind tries
   again instead of answering NULL at once, on whatever port: one on a port somebody else
   holds then answers NULL that much later. */
net_link_t *steam_net_link_host_ip(const steam_net_link_config_t *config, const char *address, uint16_t port);

/* `address` is IPv4 or IPv6 text without a port ("127.0.0.1"). Starts connecting; CONNECTED
   or DISCONNECTED follows. A disconnected guest link stays dead: open a new one to return.
   NULL also when the address does not parse or names a port. */
net_link_t *steam_net_link_join_ip(const steam_net_link_config_t *config, const char *address, uint16_t port);

/* True while a close made through net_link_close is in hand, on a host or a guest, whether its
   link is destroyed or not. A close is in hand until Steam reports everything sent before it
   acknowledged: under loss that takes what the path needs, close_wait_s at most, and then it
   is closed anyway. A short hold follows so the close can be answered: Steam sends it once
   and only answers the peer's next packet, so the hold is two keepalives of this link's
   timeout plus four round trips, at most 3 s. A peer whose timeout is longer than this link's
   may send nothing within the hold, and reads TIMEOUT when the close packet was lost.

   So it turns false by itself within close_wait_s plus 3 s of the last close (8 s with the
   defaults), and a shutdown loop on it always ends. A game that will not wait that long ends
   the loop sooner and takes the same loss. At most 32 closes are in hand; one more closes the
   one due first at once. False in a build without the SDK and while the platform is not
   ready. */
bool steam_net_link_lingering(void);

/* Steam's built-in bad network, for tests and stands. Steam applies it to every packet of
   the process, so it is not a link's setting: with two windows, each window sets its own.
   Out-of-range values are clamped. */
typedef struct steam_net_link_fake {
    float loss;       /* share of packets dropped, 0..1 */
    float lag_ms;     /* every packet waits this long, whole milliseconds */
    float reorder;    /* share of packets held back by `reorder_ms`, so later ones pass */
    float duplicate;  /* share of packets that go twice */
} steam_net_link_fake_t;

/* `send` shapes what this process sends, `recv` what it receives; NULL leaves that direction
   clean. `reorder_ms` is one value for both directions, as Steam has it. Does nothing while
   the platform is not ready; steam_platform_stop forgets it. */
void steam_net_link_set_fake(const steam_net_link_fake_t *send, const steam_net_link_fake_t *recv, float reorder_ms);

#ifdef __cplusplus
}
#endif

#endif
