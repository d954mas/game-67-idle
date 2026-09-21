#include "net_ws_server.h"

#include "net_codec.h"
#include "net_queue.h"

#include <libwebsockets.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct session_t {
    uint32_t id;               /* 0 until a slot is assigned */
    uint32_t slot;
    bool hello_done;
    bool close_pending;
    bool close_started;
    uint16_t close_code;
    net_ws_close_reason_t close_reason;
    uint8_t *rx;               /* fragment reassembly, rx_capacity bytes */
    size_t rx_capacity;
    size_t rx_size;
    net_queue_t tx;
    double tokens;
    lws_usec_t last_us;
} session_t;

typedef struct slot_t {
    struct lws *wsi;
    uint32_t id;
} slot_t;

struct net_ws_server_t {
    net_ws_server_config_t config;
    struct lws_context *context;
    struct lws_vhost *vhost;
    struct lws_protocols protocols[2];
    lws_retry_bo_t idle_policy;
    lws_sorted_usec_list_t wake;
    slot_t *slots;
    uint32_t client_count;     /* sessions past HELLO */
    uint32_t next_id;
    int timeout_seconds;
    double accept_tokens;
    lws_usec_t accept_last_us;
    uint8_t *tx_scratch;       /* LWS_PRE + largest queued message */
    bool in_service;
    bool destroy_requested;
    bool destroying;
};

static void wake_noop(lws_sorted_usec_list_t *sul) { (void)sul; }

static session_t *session_of(struct lws *wsi) { return (session_t *)lws_wsi_user(wsi); }

static net_ws_server_t *server_of(struct lws *wsi) {
    return (net_ws_server_t *)lws_context_user(lws_get_context(wsi));
}

/* Accepts are budgeted before any TLS or HTTP work is done for a socket:
   every seat may reconnect at once, and a connect flood costs one
   handshake per seat per second on the service thread, never more. */
static bool accept_allowed(net_ws_server_t *server) {
    const lws_usec_t now = lws_now_usecs();
    const double rate = (double)server->config.max_clients;
    server->accept_tokens += (double)(now - server->accept_last_us) * rate / 1000000.0;
    server->accept_last_us = now;
    if (server->accept_tokens > 2.0 * rate) { server->accept_tokens = 2.0 * rate; }
    if (server->accept_tokens < 1.0) { return false; }
    server->accept_tokens -= 1.0;
    return true;
}

/* Closes go through the writeable callback so queued messages can drain
   first; the timeout guarantees a peer that never reads still goes away. */
static void request_close(net_ws_server_t *server, struct lws *wsi, session_t *session,
    uint16_t code, net_ws_close_reason_t reason) {
    if (session->close_pending) { return; }
    session->close_pending = true;
    session->close_code = code;
    session->close_reason = reason;
    lws_set_timeout(wsi, PENDING_TIMEOUT_USER_OK, server->timeout_seconds);
    lws_callback_on_writable(wsi);
}

static int on_established(net_ws_server_t *server, struct lws *wsi, session_t *session) {
    memset(session, 0, sizeof *session);
    session->close_reason = NET_WS_CLOSE_PEER;
    session->last_us = lws_now_usecs();
    session->tokens = (double)server->config.max_messages_per_second;
    /* HELLO may be longer than the smallest application message. */
    session->rx_capacity = server->config.max_message_bytes > NET_HELLO_MAX_SIZE
        ? server->config.max_message_bytes : NET_HELLO_MAX_SIZE;
    session->rx = (uint8_t *)malloc(session->rx_capacity);
    if (session->rx == NULL || !net_queue_init(&session->tx, server->config.send_queue_bytes)) {
        return -1;
    }
    for (uint32_t index = 0U; index < server->config.max_clients; ++index) {
        if (server->slots[index].wsi == NULL) {
            if (server->next_id == 0U) { server->next_id = 1U; }
            server->slots[index].wsi = wsi;
            server->slots[index].id = server->next_id;
            session->slot = index;
            session->id = server->next_id;
            server->next_id += 1U;
            /* A socket that never says HELLO must not hold the slot. */
            lws_set_timeout(wsi, PENDING_TIMEOUT_USER_OK, server->timeout_seconds);
            return 0;
        }
    }
    request_close(server, wsi, session, NET_CLOSE_FULL, NET_WS_CLOSE_PEER);
    return 0;
}

static void on_closed(net_ws_server_t *server, session_t *session) {
    if (session->id != 0U) {
        server->slots[session->slot].wsi = NULL;
        server->slots[session->slot].id = 0U;
        if (session->hello_done) {
            server->client_count -= 1U;
            if (!server->destroying && server->config.on_disconnect != NULL) {
                server->config.on_disconnect(server->config.user, session->id, session->close_reason);
            }
        }
    }
    free(session->rx);
    session->rx = NULL;
    net_queue_free(&session->tx);
}

static bool rate_allows(net_ws_server_t *server, session_t *session) {
    const double rate = (double)server->config.max_messages_per_second;
    if (rate <= 0.0) { return true; }
    const lws_usec_t now = lws_now_usecs();
    const double elapsed = (double)(now - session->last_us) / 1000000.0;
    session->last_us = now;
    session->tokens += elapsed * rate;
    if (session->tokens > rate) { session->tokens = rate; }
    if (session->tokens < 1.0) { return false; }
    session->tokens -= 1.0;
    return true;
}

static void on_complete_message(net_ws_server_t *server, struct lws *wsi, session_t *session) {
    if (!session->hello_done) {
        uint32_t version = 0U;
        const uint8_t *ticket = NULL;
        size_t ticket_size = 0U;
        if (!net_hello_decode(session->rx, session->rx_size, &version, &ticket, &ticket_size)) {
            request_close(server, wsi, session, NET_CLOSE_BAD_HELLO, NET_WS_CLOSE_PROTOCOL);
            return;
        }
        if (version != server->config.protocol_version) {
            request_close(server, wsi, session, NET_CLOSE_VERSION, NET_WS_CLOSE_PROTOCOL);
            return;
        }
        session->hello_done = true;
        server->client_count += 1U;
        lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);
        if (!server->destroying && server->config.on_connect != NULL) {
            server->config.on_connect(server->config.user, session->id, ticket, ticket_size);
        }
        return;
    }
    /* HELLO is the transport's own frame; the budget starts with the game's. */
    if (!rate_allows(server, session)) {
        request_close(server, wsi, session, NET_CLOSE_RATE, NET_WS_CLOSE_RATE);
        return;
    }
    if (session->rx_size == 0U || session->rx[0] < NET_MSG_APP_FIRST) {
        request_close(server, wsi, session, NET_CLOSE_FORMAT, NET_WS_CLOSE_PROTOCOL);
        return;
    }
    if (!server->destroying && server->config.on_message != NULL) {
        server->config.on_message(server->config.user, session->id, session->rx, session->rx_size);
    }
}

static int on_receive(net_ws_server_t *server, struct lws *wsi, session_t *session,
    const uint8_t *data, size_t size) {
    if (session->id == 0U || session->close_pending) { return 0; }
    if (lws_is_first_fragment(wsi)) {
        session->rx_size = 0U;
        if (!lws_frame_is_binary(wsi)) {
            request_close(server, wsi, session, NET_CLOSE_FORMAT, NET_WS_CLOSE_PROTOCOL);
            return 0;
        }
    }
    const size_t limit = session->hello_done ? server->config.max_message_bytes : session->rx_capacity;
    if (size > limit - session->rx_size) {
        request_close(server, wsi, session, session->hello_done ? NET_CLOSE_FORMAT : NET_CLOSE_BAD_HELLO,
            NET_WS_CLOSE_PROTOCOL);
        return 0;
    }
    memcpy(session->rx + session->rx_size, data, size);
    session->rx_size += size;
    if (!lws_is_final_fragment(wsi) || lws_remaining_packet_payload(wsi) > 0U) { return 0; }
    on_complete_message(server, wsi, session);
    return 0;
}

static void drain(net_ws_server_t *server, struct lws *wsi, session_t *session) {
    /* Drain while the socket accepts writes; lws buffers the tail of a
       partial write itself and reports choked until it is flushed. A refused
       write means lws is already closing the socket and will say so. */
    while (session->tx.count > 0U && !lws_send_pipe_choked(wsi)) {
        const size_t size = net_queue_front_size(&session->tx);
        net_queue_front_copy(&session->tx, server->tx_scratch + LWS_PRE, size);
        net_queue_pop(&session->tx);
        if (lws_write(wsi, server->tx_scratch + LWS_PRE, size, LWS_WRITE_BINARY) < 0) { return; }
    }
}

static int on_writeable(net_ws_server_t *server, struct lws *wsi, session_t *session) {
    if (session->close_pending) {
        /* One -1 starts the close handshake; a second one while lws awaits
           the peer's ack kills the socket outright and the peer sees a reset
           instead of the close code. */
        if (session->close_started) { return 0; }
        /* An application close still delivers what the application queued
           before it (a kick reason, a final state); protocol closes do not. */
        if (session->close_reason == NET_WS_CLOSE_APP) {
            drain(server, wsi, session);
            if (session->tx.count > 0U || lws_send_pipe_choked(wsi)) {
                lws_callback_on_writable(wsi);
                return 0;
            }
        }
        session->close_started = true;
        lws_close_reason(wsi, (enum lws_close_status)session->close_code, NULL, 0U);
        return -1;
    }
    drain(server, wsi, session);
    if (session->tx.count > 0U) { lws_callback_on_writable(wsi); }
    return 0;
}

static int protocol_callback(struct lws *wsi, enum lws_callback_reasons reason,
    void *user, void *in, size_t len) {
    session_t *session = (session_t *)user;
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        return on_established(server_of(wsi), wsi, session);
    case LWS_CALLBACK_RECEIVE:
        return on_receive(server_of(wsi), wsi, session, (const uint8_t *)in, len);
    case LWS_CALLBACK_SERVER_WRITEABLE:
        return on_writeable(server_of(wsi), wsi, session);
    case LWS_CALLBACK_CLOSED:
        on_closed(server_of(wsi), session);
        return 0;
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        return accept_allowed(server_of(wsi)) ? 0 : 1;
    case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
        /* lws binds a header-less upgrade to protocols[0] whatever its name;
           a configured subprotocol must actually be requested. */
        if (server_of(wsi)->config.subprotocol != NULL &&
            lws_hdr_total_length(wsi, WSI_TOKEN_PROTOCOL) == 0) {
            return 1;
        }
        return 0;
    case LWS_CALLBACK_HTTP:
        /* Plain HTTP has nothing to serve here. */
        return -1;
    default:
        return 0;
    }
}

static void free_server(net_ws_server_t *server) {
    if (server->context != NULL) {
        lws_sul_cancel(&server->wake);
        lws_context_destroy(server->context);
    }
    free(server->tx_scratch);
    free(server->slots);
    free(server);
}

net_ws_server_t *net_ws_server_create(const net_ws_server_config_t *config) {
    if (config == NULL || config->max_clients == 0U || config->max_message_bytes == 0U ||
        config->send_queue_bytes < 4U ||
        (config->ping_idle_s > 0U && config->hangup_idle_s <= config->ping_idle_s)) {
        return NULL;
    }
    net_ws_server_t *server = (net_ws_server_t *)calloc(1U, sizeof *server);
    if (server == NULL) { return NULL; }
    server->config = *config;
    server->next_id = 1U;
    server->timeout_seconds = config->handshake_timeout_ms == 0U
        ? 10 : (int)((config->handshake_timeout_ms + 999U) / 1000U);
    server->accept_tokens = 2.0 * (double)config->max_clients;
    server->accept_last_us = lws_now_usecs();
    server->slots = (slot_t *)calloc(config->max_clients, sizeof *server->slots);
    server->tx_scratch = (uint8_t *)malloc(LWS_PRE + (size_t)config->send_queue_bytes);
    if (server->slots == NULL || server->tx_scratch == NULL) {
        free_server(server);
        return NULL;
    }
    /* A browser that sends no Sec-WebSocket-Protocol binds protocols[0]. */
    server->protocols[0].name = config->subprotocol != NULL ? config->subprotocol : "";
    server->protocols[0].callback = protocol_callback;
    server->protocols[0].per_session_data_size = sizeof(session_t);
    server->protocols[0].rx_buffer_size = config->max_message_bytes > NET_HELLO_MAX_SIZE
        ? config->max_message_bytes : NET_HELLO_MAX_SIZE;

    const bool tls = config->tls_cert_path != NULL || config->tls_key_path != NULL;
    if (tls && (config->tls_cert_path == NULL || config->tls_key_path == NULL)) {
        free_server(server);
        return NULL;
    }
#if !NET_WS_TLS
    if (tls) {
        /* Built without TLS: a wss:// room would silently be a ws:// one. */
        free_server(server);
        return NULL;
    }
#endif
    /* Process-global in lws; every context in this process wants the same. */
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
#if NET_WS_TLS
    if (tls) { info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT; }
#endif
    info.user = server;
    info.gid = (gid_t)-1;
    info.uid = (uid_t)-1;
    /* Sockets short of a seat (a TLS handshake, an upgrade in flight) are
       bounded too: past this many the listener pauses until one closes,
       and one that stalls before HELLO is dropped on the same timeout as
       one that stalls after. Without a bound the process's fd limit is
       the only one, and every pending handshake holds a TLS buffer. */
    info.fd_limit_per_thread = config->max_clients * 4U + 8U;
    info.timeout_secs = (unsigned int)server->timeout_seconds;
    server->context = lws_create_context(&info);
    if (server->context == NULL) {
        free_server(server);
        return NULL;
    }
    memset(&info, 0, sizeof info);
    info.port = config->port;
    info.iface = config->bind_address;
    info.protocols = server->protocols;
    info.vhost_name = "default";
#if NET_WS_TLS
    if (tls) {
        /* Once more on the vhost: its own options decide whether it
           speaks TLS at all; the context's did the library-wide init. */
        info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        info.ssl_cert_filepath = config->tls_cert_path;
        info.ssl_private_key_filepath = config->tls_key_path;
        /* TLS 1.2 and 1.3 only, forward-secret AEAD suites: the library
           itself leaves the floor to whatever the runtime's OpenSSL was
           built with, and a Debian default is not a policy. */
        info.ssl_options_set = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
        info.ssl_cipher_list = "ECDHE+AESGCM:ECDHE+CHACHA20:!aNULL:!MD5";
    }
#endif
    /* Compression trades CPU and latency for bytes on every frame; messages
       are small and time-critical, so no extensions are offered. */
    info.extensions = NULL;
    info.gid = (gid_t)-1;
    info.uid = (uid_t)-1;
    /* Always ours: without a policy lws applies its own 40 s ping / 50 s
       hangup, and a zeroed policy is what turns validity checks off. */
    server->idle_policy.secs_since_valid_ping = config->ping_idle_s;
    server->idle_policy.secs_since_valid_hangup = config->ping_idle_s > 0U ? config->hangup_idle_s : 0U;
    info.retry_and_idle_policy = &server->idle_policy;
    server->vhost = lws_create_vhost(server->context, &info);
    if (server->vhost == NULL) {
        free_server(server);
        return NULL;
    }
    return server;
}

void net_ws_server_destroy(net_ws_server_t *server) {
    if (server == NULL) { return; }
    server->destroying = true;
    /* Inside a callback lws still walks its connections; the pump that is
       running finishes the pass and frees everything afterwards. */
    if (server->in_service) {
        server->destroy_requested = true;
        return;
    }
    free_server(server);
}

uint16_t net_ws_server_port(const net_ws_server_t *server) {
    return (uint16_t)lws_get_vhost_port(server->vhost);
}

uint32_t net_ws_server_client_count(const net_ws_server_t *server) {
    return server->client_count;
}

void net_ws_server_service(net_ws_server_t *server, uint32_t timeout_ms) {
    if (server->destroying) { return; }
    server->in_service = true;
    /* lws ignores a positive service timeout; a scheduled no-op is what
       bounds the poll wait to the caller's deadline. A zero-delay timer
       would fire before the poll and leave it unbounded, so zero maps to
       the one value lws does honour: a non-blocking pass. */
    if (timeout_ms == 0U) {
        lws_service(server->context, -1);
    } else {
        lws_sul_schedule(server->context, 0, &server->wake, wake_noop,
            (lws_usec_t)timeout_ms * LWS_US_PER_MS);
        lws_service(server->context, 0);
    }
    server->in_service = false;
    if (server->destroy_requested) { free_server(server); }
}

static struct lws *wsi_for(const net_ws_server_t *server, uint32_t client) {
    if (client == 0U) { return NULL; }
    for (uint32_t index = 0U; index < server->config.max_clients; ++index) {
        if (server->slots[index].id == client) { return server->slots[index].wsi; }
    }
    return NULL;
}

bool net_ws_server_send(net_ws_server_t *server, uint32_t client, const uint8_t *data, size_t size) {
    struct lws *wsi = wsi_for(server, client);
    if (wsi == NULL || size == 0U) { return false; }
    session_t *session = session_of(wsi);
    if (session->close_pending || !session->hello_done) { return false; }
    if (!net_queue_push(&session->tx, data, size)) {
        request_close(server, wsi, session, NET_CLOSE_SLOW, NET_WS_CLOSE_SLOW);
        return false;
    }
    lws_callback_on_writable(wsi);
    return true;
}

bool net_ws_server_send_latest(net_ws_server_t *server, uint32_t client, const uint8_t *data, size_t size,
    size_t *replaced) {
    if (replaced != NULL) { *replaced = 0U; }
    struct lws *wsi = wsi_for(server, client);
    if (wsi == NULL || size == 0U) { return false; }
    session_t *session = session_of(wsi);
    if (session->close_pending || !session->hello_done) { return false; }
    /* Whole messages only sit in the ring: a message being written lives
       in lws's own buffer once popped, so nothing here cuts a frame. */
    const size_t dropped = net_queue_drop_kind(&session->tx, data[0]);
    if (replaced != NULL) { *replaced = dropped; }
    if (!net_queue_push(&session->tx, data, size)) {
        request_close(server, wsi, session, NET_CLOSE_SLOW, NET_WS_CLOSE_SLOW);
        return false;
    }
    lws_callback_on_writable(wsi);
    return true;
}

size_t net_ws_server_queued_bytes(const net_ws_server_t *server, uint32_t client) {
    struct lws *wsi = wsi_for(server, client);
    return wsi == NULL ? 0U : session_of(wsi)->tx.used;
}

void net_ws_server_close(net_ws_server_t *server, uint32_t client, uint16_t code) {
    struct lws *wsi = wsi_for(server, client);
    if (wsi == NULL) { return; }
    request_close(server, wsi, session_of(wsi), code, NET_WS_CLOSE_APP);
}
