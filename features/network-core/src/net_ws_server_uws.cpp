/* net_ws_server.h on uWebSockets and uSockets: an epoll loop that wakes for
   ready sockets only, TLS through OpenSSL in memory. The session rules are
   network-core's own and sit above it: HELLO and the version gate, seats,
   the waiting room, message and inbound rate limits, the bounded outbound
   queue, drain-first application closes and the liveness pings. Every
   server of a thread shares that thread's uWebSockets loop (it keeps one
   per thread), so servicing one server also runs the others' sockets. */
extern "C" {
#include "net_codec.h"
#include "net_queue.h"
#include "net_ws_server.h"
}

#include "net_us_loop.h"

#include "App.h"

#if defined(__linux__)
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif
#if NET_WS_TLS
#include <openssl/crypto.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct Peer {
    net_ws_server_t *server = nullptr;
    void *ws = nullptr;
    uint32_t id = 0U;          /* 0 until HELLO earns a slot */
    uint32_t slot = 0U;
    bool hello_done = false;
    bool listed = false;       /* in the server's closing list */
    bool close_pending = false;
    bool close_started = false;
    bool terminal = false;
    uint16_t close_code = 0U;
    net_ws_close_reason_t close_reason = NET_WS_CLOSE_PEER;
    net_queue_t tx{};
    double tokens = 0.0;
    double rate_at = 0.0;
    double opened_at = 0.0;
    double close_at = 0.0;
    double heard_at = 0.0;     /* the last frame of any kind from the peer */
    double pinged_at = 0.0;
    std::string address;
};

template <bool SSL> using Socket = uWS::WebSocket<SSL, true, Peer>;
template <bool SSL> using App = uWS::TemplatedApp<SSL>;

double now_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* Sockets one address may hold in the waiting room before HELLO. */
constexpr uint32_t PENDING_PER_PEER = 4U;
/* The sweep that runs handshake, close and liveness deadlines. */
constexpr int SWEEP_MS = 250;

thread_local int t_in_service = 0;
thread_local std::vector<net_ws_server_t *> t_doomed;
thread_local bool t_integrated = false;

struct us_loop_t *thread_loop() {
    auto *loop = reinterpret_cast<struct us_loop_t *>(uWS::Loop::get());
    if (!t_integrated) {
        net_us_loop_integrate(loop);
        t_integrated = true;
    }
    return loop;
}

} // namespace

struct net_ws_server_t {
    net_ws_server_config_t config{};
    bool tls = false;
    void *app = nullptr;
    us_listen_socket_t *listen = nullptr;
    us_timer_t *sweep = nullptr;
    uint16_t port = 0U;
    struct Slot {
        void *ws;
        Peer *peer;
        uint32_t id;
    };
    std::vector<Slot> slots;
    /* Sockets upgraded but not yet past HELLO: they hold no slot, so a crowd
       of them cannot take the seats, and no more than a few come from one
       address, so one machine cannot fill the waiting room. */
    std::vector<Peer *> pending;
    std::vector<Peer *> closing;
    uint32_t client_count = 0U;
    uint32_t next_id = 1U;
    double timeout_seconds = 10.0;
    double accept_tokens = 0.0;
    double accept_at = 0.0;
    /* Every socket from accept on, the handshake included, and per address. */
    uint32_t sockets = 0U;
    std::unordered_map<std::string, uint32_t> per_address;
    std::vector<uint8_t> scratch;
    net_ws_server_stats_t stats{};
    bool destroy_requested = false;
    bool destroying = false;
    bool test_fail_next_write = false;
    bool test_hold_writes = false;
    bool test_partial_buffered = false;
};

namespace {

uint32_t socket_limit(const net_ws_server_t *server) { return server->config.max_clients * 4U + 8U; }

uint32_t address_limit(const net_ws_server_t *server) {
    return server->config.max_clients > 32767U ? 65535U : 2U * server->config.max_clients;
}

/* Accepts are budgeted before any TLS or HTTP work is done for a socket:
   every seat may reconnect at once, and a connect flood costs one handshake
   per seat per second on the service thread, never more. */
bool accept_allowed(net_ws_server_t *server) {
    const double now = now_seconds();
    const double rate = static_cast<double>(server->config.max_clients);
    server->accept_tokens = std::min(2.0 * rate, server->accept_tokens + (now - server->accept_at) * rate);
    server->accept_at = now;
    if (server->accept_tokens < 1.0) { return false; }
    server->accept_tokens -= 1.0;
    return true;
}

void count_socket(net_ws_server_t *server, const std::string &address, int delta) {
    if (delta > 0) {
        server->sockets += 1U;
        server->per_address[address] += 1U;
        return;
    }
    server->sockets -= 1U;
    auto found = server->per_address.find(address);
    if (found != server->per_address.end() && --found->second == 0U) { server->per_address.erase(found); }
}

void pending_remove(net_ws_server_t *server, const Peer *peer) {
    auto &list = server->pending;
    list.erase(std::remove(list.begin(), list.end(), peer), list.end());
}

bool pending_admit(net_ws_server_t *server, Peer *peer) {
    if (server->pending.size() >= server->config.max_clients) { return false; }
    uint32_t same = 0U;
    for (const Peer *other : server->pending) { same += other->address == peer->address ? 1U : 0U; }
    if (same >= PENDING_PER_PEER) { return false; }
    server->pending.push_back(peer);
    return true;
}

/* The slot, and with it the id, is earned by HELLO. */
bool assign_slot(net_ws_server_t *server, Peer *peer) {
    for (uint32_t index = 0U; index < server->config.max_clients; ++index) {
        auto &slot = server->slots[index];
        if (slot.ws != nullptr) { continue; }
        if (server->next_id == 0U) { server->next_id = 1U; }
        slot = {peer->ws, peer, server->next_id};
        peer->slot = index;
        peer->id = server->next_id;
        server->next_id += 1U;
        return true;
    }
    return false;
}

Peer *peer_for(const net_ws_server_t *server, uint32_t client) {
    if (client == 0U) { return nullptr; }
    for (const auto &slot : server->slots) {
        if (slot.id == client) { return slot.peer; }
    }
    return nullptr;
}

bool live(const Peer *peer) { return peer->hello_done && !peer->close_pending && !peer->terminal; }

/* Closes are carried out by the service pass: an application's close
   reports its disconnect from a later service, as the contract says, and
   one raised inside a callback does not end a socket under uWebSockets'
   feet. */
void list_closing(net_ws_server_t *server, Peer *peer) {
    if (peer->listed) { return; }
    peer->listed = true;
    server->closing.push_back(peer);
}

void request_close(net_ws_server_t *server, Peer *peer, uint16_t code, net_ws_close_reason_t reason) {
    if (peer->close_pending) { return; }
    peer->close_pending = true;
    peer->close_code = code;
    peer->close_reason = reason;
    peer->close_at = now_seconds();
    list_closing(server, peer);
}

bool terminal_close(net_ws_server_t *server, Peer *peer) {
    peer->terminal = true;
    if (peer->close_started || (peer->close_pending && peer->close_reason != NET_WS_CLOSE_APP)) { return false; }
    peer->close_pending = true;
    peer->close_code = NET_CLOSE_SLOW;
    peer->close_reason = NET_WS_CLOSE_SLOW;
    peer->close_at = now_seconds();
    list_closing(server, peer);
    return true;
}

bool admit(net_ws_server_t *server, Peer *peer, const uint8_t *data, size_t size) {
    if (!net_queue_push(&peer->tx, data, size)) {
        server->stats.tx_refused += 1U;
        return false;
    }
    server->stats.tx_messages += 1U;
    server->stats.tx_bytes += size;
    if (peer->tx.used > server->stats.queue_peak_bytes) { server->stats.queue_peak_bytes = static_cast<uint32_t>(peer->tx.used); }
    return true;
}

template <bool SSL> bool buffered(const net_ws_server_t *server, const Peer *peer) {
    return server->test_partial_buffered || static_cast<Socket<SSL> *>(peer->ws)->getBufferedAmount() > 0U;
}

/* Write until the socket holds a tail: uWebSockets keeps the rest of a
   partial write itself and says so with a drain, and no further message is
   handed over until that tail is out, so the queue stays ours to replace
   from. */
template <bool SSL> bool drain(net_ws_server_t *server, Peer *peer) {
    auto *ws = static_cast<Socket<SSL> *>(peer->ws);
    while (peer->tx.count > 0U && !buffered<SSL>(server, peer)) {
        const size_t size = net_queue_front_size(&peer->tx);
        net_queue_front_copy(&peer->tx, server->scratch.data(), size);
        net_queue_pop(&peer->tx);
        if (server->test_fail_next_write) {
            server->test_fail_next_write = false;
            terminal_close(server, peer);
            return false;
        }
        server->stats.tx_writes += 1U;
        const auto status = ws->send(std::string_view(reinterpret_cast<const char *>(server->scratch.data()), size),
            uWS::OpCode::BINARY);
        if (status != Socket<SSL>::SendStatus::SUCCESS) { server->stats.tx_partial += 1U; }
    }
    return !peer->terminal;
}

template <bool SSL> bool flush(net_ws_server_t *server, Peer *peer) {
    if (server->test_hold_writes) { return true; }
    return drain<SSL>(server, peer);
}

template <bool SSL> void end(Peer *peer) {
    peer->close_started = true;
    static_cast<Socket<SSL> *>(peer->ws)->end(peer->close_code);
}

/* One step of every close the service has to carry out. An application
   close still delivers what was queued before it (a kick reason, a final
   state); protocol and slow closes do not. */
template <bool SSL> void progress_closes(net_ws_server_t *server) {
    std::vector<Peer *> due;
    due.swap(server->closing);
    for (Peer *peer : due) { peer->listed = false; }
    for (size_t index = 0U; index < due.size(); ++index) {
        Peer *peer = due[index];
        if (peer == nullptr || peer->close_started) { continue; }
        if (peer->close_reason == NET_WS_CLOSE_APP && !peer->terminal) {
            if (drain<SSL>(server, peer) && (peer->tx.count > 0U || buffered<SSL>(server, peer))) {
                list_closing(server, peer);
                continue;
            }
        }
        /* The close handler below may run for this peer and clear it from
           the rest of `due`. */
        for (size_t later = index + 1U; later < due.size(); ++later) {
            if (due[later] == peer) { due[later] = nullptr; }
        }
        end<SSL>(peer);
    }
}

template <bool SSL> void on_filter(net_ws_server_t *server, uWS::HttpResponse<SSL> *res, int delta) {
    const std::string address(res->getRemoteAddressAsText());
    count_socket(server, address, delta);
    if (delta < 0 || server->destroying) { return; }
    if (!accept_allowed(server) || server->sockets > socket_limit(server) ||
        server->per_address[address] > address_limit(server)) {
        us_socket_close(SSL, reinterpret_cast<us_socket_t *>(res), 0, nullptr);
    }
}

template <bool SSL> void on_open(net_ws_server_t *server, Socket<SSL> *ws) {
    Peer *peer = ws->getUserData();
    const double now = now_seconds();
    peer->server = server;
    peer->ws = ws;
    peer->opened_at = now;
    peer->heard_at = now;
    peer->pinged_at = now;
    peer->rate_at = now;
    peer->tokens = static_cast<double>(server->config.max_messages_per_second);
    peer->address = std::string(ws->getRemoteAddressAsText());
    if (!net_queue_init(&peer->tx, server->config.send_queue_bytes)) {
        request_close(server, peer, NET_CLOSE_FULL, NET_WS_CLOSE_PEER);
        return;
    }
    if (!pending_admit(server, peer)) { request_close(server, peer, NET_CLOSE_FULL, NET_WS_CLOSE_PEER); }
}

bool rate_allows(net_ws_server_t *server, Peer *peer, double now) {
    const double rate = static_cast<double>(server->config.max_messages_per_second);
    if (rate <= 0.0) { return true; }
    peer->tokens = std::min(rate, peer->tokens + (now - peer->rate_at) * rate);
    peer->rate_at = now;
    if (peer->tokens < 1.0) { return false; }
    peer->tokens -= 1.0;
    return true;
}

template <bool SSL> void on_message(net_ws_server_t *server, Socket<SSL> *ws, std::string_view message, uWS::OpCode op) {
    Peer *peer = ws->getUserData();
    server->stats.rx_frames += 1U;
    const double now = now_seconds();
    peer->heard_at = now;
    if (peer->close_pending) { return; }
    const auto *data = reinterpret_cast<const uint8_t *>(message.data());
    const size_t size = message.size();
    if (op != uWS::OpCode::BINARY) {
        request_close(server, peer, NET_CLOSE_FORMAT, NET_WS_CLOSE_PROTOCOL);
        return;
    }
    if (!peer->hello_done) {
        uint32_t version = 0U;
        const uint8_t *ticket = nullptr;
        size_t ticket_size = 0U;
        if (!net_hello_decode(data, size, &version, &ticket, &ticket_size)) {
            request_close(server, peer, NET_CLOSE_BAD_HELLO, NET_WS_CLOSE_PROTOCOL);
            return;
        }
        if (version != server->config.protocol_version) {
            request_close(server, peer, NET_CLOSE_VERSION, NET_WS_CLOSE_PROTOCOL);
            return;
        }
        if (!assign_slot(server, peer)) {
            request_close(server, peer, NET_CLOSE_FULL, NET_WS_CLOSE_PEER);
            return;
        }
        pending_remove(server, peer);
        peer->hello_done = true;
        server->client_count += 1U;
        if (!server->destroying && server->config.on_connect != nullptr) {
            server->config.on_connect(server->config.user, peer->id, ticket, ticket_size);
        }
        return;
    }
    /* HELLO is the transport's own frame; the budget starts with the game's. */
    if (!rate_allows(server, peer, now)) {
        request_close(server, peer, NET_CLOSE_RATE, NET_WS_CLOSE_RATE);
        return;
    }
    if (size == 0U || size > server->config.max_message_bytes || data[0] < NET_MSG_APP_FIRST) {
        request_close(server, peer, NET_CLOSE_FORMAT, NET_WS_CLOSE_PROTOCOL);
        return;
    }
    server->stats.rx_messages += 1U;
    server->stats.rx_bytes += size;
    if (!server->destroying && server->config.on_message != nullptr) {
        server->config.on_message(server->config.user, peer->id, data, size);
    }
}

template <bool SSL> void on_drain(net_ws_server_t *server, Socket<SSL> *ws) {
    Peer *peer = ws->getUserData();
    server->stats.writable += 1U;
    if (peer->close_pending) {
        list_closing(server, peer);
        return;
    }
    if (!drain<SSL>(server, peer)) { list_closing(server, peer); }
}

template <bool SSL> void on_close(net_ws_server_t *server, Socket<SSL> *ws) {
    Peer *peer = ws->getUserData();
    peer->close_started = true;
    if (peer->listed) {
        auto &list = server->closing;
        list.erase(std::remove(list.begin(), list.end(), peer), list.end());
        peer->listed = false;
    }
    if (!peer->hello_done) { pending_remove(server, peer); }
    if (peer->id != 0U) {
        server->slots[peer->slot] = {nullptr, nullptr, 0U};
        if (peer->hello_done) {
            server->client_count -= 1U;
            if (!server->destroying && server->config.on_disconnect != nullptr) {
                server->config.on_disconnect(server->config.user, peer->id, peer->close_reason);
            }
        }
    }
    net_queue_free(&peer->tx);
    count_socket(server, peer->address, -1);
}

/* Deadlines: a socket that never says HELLO, a close the peer never
   acknowledges or drains, and liveness. Any frame from the peer counts as
   life; after ping_idle_s of silence a ping goes out, after hangup_idle_s
   the socket is dropped as NET_WS_CLOSE_PEER. */
template <bool SSL> void sweep(net_ws_server_t *server) {
    const double now = now_seconds();
    std::vector<Peer *> expired;
    for (Peer *peer : server->pending) {
        if (!peer->close_pending && now - peer->opened_at > server->timeout_seconds) { expired.push_back(peer); }
    }
    for (const auto &slot : server->slots) {
        Peer *peer = slot.peer;
        if (peer == nullptr) { continue; }
        if (peer->close_pending) {
            if (now - peer->close_at > server->timeout_seconds) { expired.push_back(peer); }
            continue;
        }
        if (server->config.ping_idle_s == 0U) { continue; }
        const double silent = now - peer->heard_at;
        if (silent >= static_cast<double>(server->config.hangup_idle_s)) {
            peer->close_reason = NET_WS_CLOSE_PEER;
            expired.push_back(peer);
        } else if (silent >= static_cast<double>(server->config.ping_idle_s) &&
                   now - peer->pinged_at >= static_cast<double>(server->config.ping_idle_s)) {
            peer->pinged_at = now;
            static_cast<Socket<SSL> *>(peer->ws)->send(std::string_view(), uWS::OpCode::PING);
        }
    }
    for (Peer *peer : server->pending) {
        if (peer->close_pending && now - peer->close_at > server->timeout_seconds) { expired.push_back(peer); }
    }
    /* A hard close runs the close handler at once, which edits the lists
       walked above; hence the copy. */
    for (Peer *peer : expired) { static_cast<Socket<SSL> *>(peer->ws)->close(); }
}

template <bool SSL> bool build(net_ws_server_t *server) {
    uWS::SocketContextOptions options{};
    if constexpr (SSL) {
        options.cert_file_name = server->config.tls_cert_path;
        options.key_file_name = server->config.tls_key_path;
        /* TLS 1.2 and 1.3 only (uSockets' floor), forward-secret AEAD suites. */
        options.ssl_ciphers = "ECDHE+AESGCM:ECDHE+CHACHA20:!aNULL:!MD5";
    }
    auto *app = new App<SSL>(options);
    if (app->constructorFailed()) {
        delete app;
        return false;
    }
    server->app = app;
    app->filter([server](uWS::HttpResponse<SSL> *res, int delta) { on_filter<SSL>(server, res, delta); });
    typename App<SSL>::template WebSocketBehavior<Peer> behavior;
    /* Compression trades CPU and latency for bytes on every frame; messages
       are small and time-critical, so none is offered. */
    behavior.compression = uWS::DISABLED;
    behavior.maxPayloadLength = std::max<uint32_t>(server->config.max_message_bytes, NET_HELLO_MAX_SIZE);
    /* Deadlines and pings are the sweep's; the outbound bound is the queue's. */
    behavior.idleTimeout = 0U;
    behavior.sendPingsAutomatically = false;
    behavior.maxBackpressure = 0U;
    behavior.closeOnBackpressureLimit = false;
    behavior.upgrade = [server](uWS::HttpResponse<SSL> *res, uWS::HttpRequest *req, us_socket_context_t *context) {
        const std::string_view key = req->getHeader("sec-websocket-key");
        std::string_view protocol;
        if (server->config.subprotocol != nullptr) {
            /* A configured subprotocol must actually be requested. */
            const std::string_view wanted(server->config.subprotocol);
            if (req->getHeader("sec-websocket-protocol").find(wanted) == std::string_view::npos) {
                res->close();
                return;
            }
            protocol = wanted;
        }
        res->template upgrade<Peer>(Peer{}, key, protocol, std::string_view(), context);
    };
    behavior.open = [server](Socket<SSL> *ws) { on_open<SSL>(server, ws); };
    behavior.message = [server](Socket<SSL> *ws, std::string_view message, uWS::OpCode op) {
        on_message<SSL>(server, ws, message, op);
    };
    behavior.drain = [server](Socket<SSL> *ws) { on_drain<SSL>(server, ws); };
    behavior.ping = [](Socket<SSL> *ws, std::string_view) { ws->getUserData()->heard_at = now_seconds(); };
    behavior.pong = [server](Socket<SSL> *ws, std::string_view) {
        ws->getUserData()->heard_at = now_seconds();
        server->stats.rx_pongs += 1U;
    };
    behavior.close = [server](Socket<SSL> *ws, int, std::string_view) { on_close<SSL>(server, ws); };
    app->template ws<Peer>("/*", std::move(behavior));
    auto on_listen = [server](us_listen_socket_t *socket) { server->listen = socket; };
    if (server->config.bind_address != nullptr) {
        app->listen(std::string(server->config.bind_address), server->config.port, std::move(on_listen));
    } else {
        app->listen(server->config.port, std::move(on_listen));
    }
    if (server->listen == nullptr) { return false; }
    server->port = static_cast<uint16_t>(us_socket_local_port(SSL, reinterpret_cast<us_socket_t *>(server->listen)));
    return true;
}

template <bool SSL> void close_app(net_ws_server_t *server) {
    auto *app = static_cast<App<SSL> *>(server->app);
    if (app == nullptr) { return; }
    app->close();
    /* Closed sockets are freed at the end of a loop pass; the app's
       contexts may go only after that. */
    net_us_loop_run_once(thread_loop(), 0);
    delete app;
    server->app = nullptr;
}

void sweep_timer(us_timer_t *timer) {
    net_ws_server_t *server = *static_cast<net_ws_server_t **>(us_timer_ext(timer));
    if (server->destroying) { return; }
    if (server->tls) {
        sweep<true>(server);
    } else {
        sweep<false>(server);
    }
}

void free_server(net_ws_server_t *server) {
    server->destroying = true;
    if (server->sweep != nullptr) { us_timer_close(server->sweep); }
    if (server->tls) {
        close_app<true>(server);
    } else {
        close_app<false>(server);
    }
    delete server;
}

} // namespace

net_ws_server_t *net_ws_server_create(const net_ws_server_config_t *config) {
    if (config == nullptr || config->max_clients == 0U || config->max_message_bytes == 0U ||
        config->send_queue_bytes < 4U ||
        (config->ping_idle_s > 0U && config->hangup_idle_s <= config->ping_idle_s)) {
        return nullptr;
    }
    const bool tls = config->tls_cert_path != nullptr || config->tls_key_path != nullptr;
    if (tls && (config->tls_cert_path == nullptr || config->tls_key_path == nullptr)) { return nullptr; }
#if !NET_WS_TLS
    /* Built without TLS: a wss:// room would silently be a ws:// one. */
    if (tls) { return nullptr; }
#endif
    auto *server = new net_ws_server_t;
    server->config = *config;
    server->tls = tls;
    server->timeout_seconds = config->handshake_timeout_ms == 0U
        ? 10.0 : static_cast<double>((config->handshake_timeout_ms + 999U) / 1000U);
    server->accept_tokens = 2.0 * static_cast<double>(config->max_clients);
    server->accept_at = now_seconds();
    server->slots.assign(config->max_clients, {nullptr, nullptr, 0U});
    server->pending.reserve(config->max_clients);
    server->scratch.resize(config->send_queue_bytes);
    struct us_loop_t *loop = thread_loop();
    const bool built = tls ? build<true>(server) : build<false>(server);
    if (!built) {
        free_server(server);
        return nullptr;
    }
    server->sweep = us_create_timer(loop, 1, sizeof(net_ws_server_t *));
    *static_cast<net_ws_server_t **>(us_timer_ext(server->sweep)) = server;
    us_timer_set(server->sweep, sweep_timer, SWEEP_MS, SWEEP_MS);
    return server;
}

void net_ws_server_destroy(net_ws_server_t *server) {
    if (server == nullptr) { return; }
    server->destroying = true;
    /* Inside a callback the loop still walks its sockets; the service that
       is running frees the server after its pass. */
    if (t_in_service > 0) {
        if (!server->destroy_requested) {
            server->destroy_requested = true;
            t_doomed.push_back(server);
        }
        return;
    }
    free_server(server);
}

uint16_t net_ws_server_port(const net_ws_server_t *server) { return server->port; }

uint32_t net_ws_server_client_count(const net_ws_server_t *server) { return server->client_count; }

void net_ws_server_service(net_ws_server_t *server, uint32_t timeout_ms) {
    if (server->destroying) { return; }
    t_in_service += 1;
    server->stats.services += 1U;
    struct us_loop_t *loop = thread_loop();
    const bool tls = server->tls;
    auto step = [server, tls]() {
        if (server->destroying) { return; }
        for (const auto &slot : server->slots) {
            if (slot.peer != nullptr && slot.peer->tx.count > 0U && live(slot.peer)) {
                if (!(tls ? flush<true>(server, slot.peer) : flush<false>(server, slot.peer))) { list_closing(server, slot.peer); }
            }
        }
        if (tls) {
            progress_closes<true>(server);
        } else {
            progress_closes<false>(server);
        }
    };
    step();
    if (timeout_ms == 0U) {
        /* Passes repeat while something was ready, so a caller that services
           on its own schedule gets everything that has arrived. */
        for (uint32_t pass = 0U; pass < 8U && !server->destroying; ++pass) {
            server->stats.service_passes += 1U;
            if (net_us_loop_run_once(loop, 0) == 0) { break; }
        }
    } else {
        server->stats.service_passes += 1U;
        net_us_loop_run_once(loop, static_cast<int>(timeout_ms));
    }
    step();
    t_in_service -= 1;
    if (t_in_service == 0 && !t_doomed.empty()) {
        std::vector<net_ws_server_t *> doomed;
        doomed.swap(t_doomed);
        for (net_ws_server_t *gone : doomed) { free_server(gone); }
    }
}

bool net_ws_server_send(net_ws_server_t *server, uint32_t client, const uint8_t *data, size_t size) {
    Peer *peer = peer_for(server, client);
    if (peer == nullptr || size == 0U || !live(peer)) { return false; }
    if (!admit(server, peer, data, size)) {
        terminal_close(server, peer);
        return false;
    }
    return server->tls ? flush<true>(server, peer) : flush<false>(server, peer);
}

bool net_ws_server_send_ready(const net_ws_server_t *server, uint32_t client) {
    const Peer *peer = peer_for(server, client);
    if (peer == nullptr || !live(peer) || peer->tx.count > 0U) { return false; }
    return !(server->tls ? buffered<true>(server, peer) : buffered<false>(server, peer));
}

bool net_ws_server_send_latest(net_ws_server_t *server, uint32_t client, const uint8_t *data, size_t size,
    size_t *replaced) {
    if (replaced != nullptr) { *replaced = 0U; }
    Peer *peer = peer_for(server, client);
    if (peer == nullptr || size == 0U || !live(peer)) { return false; }
    /* Whole messages only sit in the queue: one being written lives in
       uWebSockets' buffer once handed over, so nothing here cuts a frame. */
    const size_t dropped = net_queue_drop_kind(&peer->tx, data[0]);
    if (replaced != nullptr) { *replaced = dropped; }
    if (!admit(server, peer, data, size)) {
        terminal_close(server, peer);
        return false;
    }
    return server->tls ? flush<true>(server, peer) : flush<false>(server, peer);
}

bool net_ws_server_reports_arrival(void) {
#if defined(__linux__)
    return true;
#else
    return false;
#endif
}

double net_ws_server_receive_age(const net_ws_server_t *server, uint32_t client) {
    const Peer *peer = peer_for(server, client);
    if (peer == nullptr) { return 0.0; }
#if defined(__linux__)
    struct tcp_info info;
    socklen_t length = sizeof info;
    if (getsockopt(net_us_socket_fd(static_cast<us_socket_t *>(peer->ws)), IPPROTO_TCP, TCP_INFO, &info, &length) != 0) {
        return 0.0;
    }
    return static_cast<double>(info.tcpi_last_data_recv) / 1000.0;
#else
    return -1.0;
#endif
}

size_t net_ws_server_queued_bytes(const net_ws_server_t *server, uint32_t client) {
    const Peer *peer = peer_for(server, client);
    return peer == nullptr ? 0U : peer->tx.used;
}

void net_ws_server_take_stats(net_ws_server_t *server, net_ws_server_stats_t *out) {
    *out = server->stats;
    server->stats.queue_peak_bytes = 0U;
}

const char *net_ws_library_version(void) { return "uWebSockets " NETWORK_CORE_UWS_VERSION; }

const char *net_ws_tls_library_version(void) {
#if NET_WS_TLS
    return OpenSSL_version(OPENSSL_VERSION);
#else
    return "none";
#endif
}

void net_ws_server_close(net_ws_server_t *server, uint32_t client, uint16_t code) {
    Peer *peer = peer_for(server, client);
    if (peer != nullptr) { request_close(server, peer, code, NET_WS_CLOSE_APP); }
}

bool net_ws_server_try_close_slow(net_ws_server_t *server, uint32_t client) {
    Peer *peer = peer_for(server, client);
    return peer != nullptr && terminal_close(server, peer);
}

void net_ws_server_close_slow(net_ws_server_t *server, uint32_t client) {
    (void)net_ws_server_try_close_slow(server, client);
}

extern "C" void net_ws_server_test_fail_next_write(net_ws_server_t *server) {
    if (server != nullptr) { server->test_fail_next_write = true; }
}

extern "C" void net_ws_server_test_hold_writes(net_ws_server_t *server, bool hold) {
    if (server != nullptr) { server->test_hold_writes = hold; }
}

extern "C" void net_ws_server_test_partial_buffered(net_ws_server_t *server, bool buffered) {
    if (server != nullptr) { server->test_partial_buffered = buffered; }
}
