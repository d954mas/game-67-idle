#include "net_ws_client.h"

#include "net_codec.h"
#include "net_queue.h"
#include "net_thread.h"

#include <libwebsockets.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The socket lives on its own thread so a frame arrives the moment the
   network delivers it, whatever the game is doing: its arrival time is
   exact, pings are answered during a loading stall, and sends never wait
   for the next frame. The game thread sees only the queues; callbacks
   still fire from net_ws_client_service() alone. The lws thread sleeps in
   lws_service and is woken by lws_cancel_service, the one lws call that is
   safe from another thread. */

#define SERVICE_SLICE_MS 100U
#define TIMESTAMP_BYTES 8U

struct net_ws_client_t {
    net_ws_client_config_t config;
    uint8_t ticket[NET_HELLO_TICKET_MAX];
    struct lws_context *context;
    struct lws_protocols protocols[2];
    lws_sorted_usec_list_t wake;
    net_thread_t thread;
    bool thread_started;

    /* lws thread only; the connect itself is its first act, so a slow DNS
       lookup never stalls the game and lws state has one owner. */
    struct lws *wsi;
    char url[512];
    uint8_t *assembly;              /* fragment reassembly, max_message_bytes */
    size_t assembly_size;
    uint8_t *tx_scratch;            /* LWS_PRE + max message */
    uint16_t peer_close_code;

    /* shared, under lock */
    net_mutex_t lock;
    net_ws_client_state_t state;
    net_queue_t rx;                 /* [arrival seconds][payload] per message */
    net_queue_t tx;
    bool open_event;
    bool close_event;
    uint16_t close_code;
    bool stop;

    /* game thread only */
    uint8_t *scratch;               /* TIMESTAMP_BYTES + max_message_bytes */
    bool in_service;
    bool destroy_requested;
    bool destroying;
};

double net_ws_client_clock(void) { return net_clock_seconds(); }

static void wake_noop(lws_sorted_usec_list_t *sul) { (void)sul; }

/* lws thread: the connection is over; the game hears about it from service(). */
static void finish(net_ws_client_t *client, uint16_t code) {
    net_mutex_lock(&client->lock);
    const bool already = client->state == NET_WS_CLIENT_CLOSED;
    if (!already) {
        client->state = NET_WS_CLIENT_CLOSED;
        client->close_event = true;
        client->close_code = code;
    }
    net_mutex_unlock(&client->lock);
    client->wsi = NULL;
}

static int on_established(net_ws_client_t *client, struct lws *wsi) {
    uint8_t hello[NET_HELLO_MAX_SIZE];
    net_writer_t writer;
    net_writer_init(&writer, hello, sizeof hello);
    net_hello_encode(&writer, client->config.protocol_version, client->ticket, client->config.ticket_size);
    net_mutex_lock(&client->lock);
    net_queue_push(&client->tx, hello, writer.pos);
    client->state = NET_WS_CLIENT_OPEN;
    client->open_event = true;
    net_mutex_unlock(&client->lock);
    lws_callback_on_writable(wsi);
    return 0;
}

/* The client's own protocol close is reported to the game with the same
   code the peer receives. */
static int refuse(net_ws_client_t *client, struct lws *wsi, uint16_t code) {
    client->peer_close_code = code;
    lws_close_reason(wsi, (enum lws_close_status)code, NULL, 0U);
    return -1;
}

static void write_timestamp(uint8_t *out, double seconds) {
    memcpy(out, &seconds, sizeof seconds);
}

static int on_receive(net_ws_client_t *client, struct lws *wsi, const uint8_t *data, size_t size) {
    if (lws_is_first_fragment(wsi)) {
        client->assembly_size = TIMESTAMP_BYTES;
        if (!lws_frame_is_binary(wsi)) { return refuse(client, wsi, NET_CLOSE_FORMAT); }
    }
    if (size > client->config.max_message_bytes - (client->assembly_size - TIMESTAMP_BYTES)) {
        return refuse(client, wsi, NET_CLOSE_FORMAT);
    }
    memcpy(client->assembly + client->assembly_size, data, size);
    client->assembly_size += size;
    if (!lws_is_final_fragment(wsi) || lws_remaining_packet_payload(wsi) > 0U) { return 0; }
    if (client->assembly_size == TIMESTAMP_BYTES) { return refuse(client, wsi, NET_CLOSE_FORMAT); }
    write_timestamp(client->assembly, net_ws_client_clock());
    net_mutex_lock(&client->lock);
    bool queued = net_queue_push(&client->rx, client->assembly, client->assembly_size);
    while (!queued && client->config.overflow_policy == NET_WS_OVERFLOW_DROP_OLDEST && client->rx.count > 0U) {
        /* The queue holds at least one message, so a pop always makes room. */
        net_queue_pop(&client->rx);
        queued = net_queue_push(&client->rx, client->assembly, client->assembly_size);
    }
    net_mutex_unlock(&client->lock);
    return queued ? 0 : refuse(client, wsi, NET_CLOSE_SLOW);
}

static int on_writeable(net_ws_client_t *client, struct lws *wsi) {
    for (;;) {
        if (lws_send_pipe_choked(wsi)) { break; }
        net_mutex_lock(&client->lock);
        const size_t size = net_queue_front_size(&client->tx);
        if (size > 0U) {
            net_queue_front_copy(&client->tx, client->tx_scratch + LWS_PRE, size);
            net_queue_pop(&client->tx);
        }
        net_mutex_unlock(&client->lock);
        if (size == 0U) { return 0; }
        /* A refused write means lws is already closing this socket (a peer
           close frame is being answered); killing it here would turn that
           clean close into a reset and lose the peer's code. */
        if (lws_write(wsi, client->tx_scratch + LWS_PRE, size, LWS_WRITE_BINARY) < 0) { return 0; }
    }
    lws_callback_on_writable(wsi);
    return 0;
}

static int protocol_callback(struct lws *wsi, enum lws_callback_reasons reason,
    void *user, void *in, size_t len) {
    net_ws_client_t *client = (net_ws_client_t *)user;
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        return on_established(client, wsi);
    case LWS_CALLBACK_CLIENT_RECEIVE:
        return on_receive(client, wsi, (const uint8_t *)in, len);
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        return on_writeable(client, wsi);
    case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
        if (len >= 2U) {
            const uint8_t *code = (const uint8_t *)in;
            client->peer_close_code = (uint16_t)(((uint16_t)code[0] << 8) | code[1]);
        }
        return 0;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        finish(client, 0U);
        return -1;
    case LWS_CALLBACK_CLIENT_CLOSED:
        finish(client, client->peer_close_code);
        return 0;
    default:
        return 0;
    }
}

static void start_connect(net_ws_client_t *client) {
    const char *scheme = NULL;
    const char *address = NULL;
    const char *path = NULL;
    int port = 0;
    if (lws_parse_uri(client->url, &scheme, &address, &port, &path) != 0 || strcmp(scheme, "ws") != 0) {
        finish(client, 0U);
        return;
    }
    /* lws_parse_uri hands back "/" for an empty path and the bare remainder
       otherwise; the request line needs exactly one leading slash. */
    char full_path[512];
    snprintf(full_path, sizeof full_path, "%s%s", path[0] == '/' ? "" : "/", path);
    struct lws_client_connect_info connect;
    memset(&connect, 0, sizeof connect);
    connect.context = client->context;
    connect.address = address;
    connect.port = port;
    connect.path = full_path;
    connect.host = address;
    connect.origin = address;
    connect.protocol = client->config.subprotocol;
    connect.userdata = client;
    connect.pwsi = &client->wsi;
    /* A synchronous failure (DNS, refused) lands in finish() during this call. */
    if (lws_client_connect_via_info(&connect) == NULL) { finish(client, 0U); }
}

static void service_thread(void *arg) {
    net_ws_client_t *client = (net_ws_client_t *)arg;
    start_connect(client);
    for (;;) {
        net_mutex_lock(&client->lock);
        const bool stop = client->stop || client->state == NET_WS_CLIENT_CLOSED;
        const bool pending = client->tx.count > 0U && client->state == NET_WS_CLIENT_OPEN;
        net_mutex_unlock(&client->lock);
        /* Nothing more can happen on a closed socket; the thread idles out
           and destroy joins it at once. */
        if (stop) { break; }
        if (pending && client->wsi != NULL) { lws_callback_on_writable(client->wsi); }
        /* lws ignores a positive timeout; the no-op wake bounds the sleep. */
        lws_sul_schedule(client->context, 0, &client->wake, wake_noop, SERVICE_SLICE_MS * LWS_US_PER_MS);
        lws_service(client->context, 0);
    }
}

static void free_client(net_ws_client_t *client) {
    if (client->thread_started) {
        net_mutex_lock(&client->lock);
        client->stop = true;
        net_mutex_unlock(&client->lock);
        lws_cancel_service(client->context);
        net_thread_join(&client->thread);
    }
    if (client->context != NULL) {
        lws_sul_cancel(&client->wake);
        lws_context_destroy(client->context);
    }
    net_mutex_free(&client->lock);
    free(client->assembly);
    free(client->tx_scratch);
    free(client->scratch);
    net_queue_free(&client->rx);
    net_queue_free(&client->tx);
    free(client);
}

net_ws_client_t *net_ws_client_create(const net_ws_client_config_t *config) {
    if (config == NULL || config->url == NULL || config->max_message_bytes == 0U ||
        config->receive_queue_bytes < config->max_message_bytes + TIMESTAMP_BYTES + 4U ||
        config->ticket_size > NET_HELLO_TICKET_MAX || (config->ticket_size > 0U && config->ticket == NULL) ||
        config->send_queue_bytes < NET_HELLO_BASE_SIZE + config->ticket_size + 4U) {
        return NULL;
    }
    net_ws_client_t *client = (net_ws_client_t *)calloc(1U, sizeof *client);
    if (client == NULL) { return NULL; }
    client->config = *config;
    if (config->ticket_size > 0U) { memcpy(client->ticket, config->ticket, config->ticket_size); }
    client->config.ticket = client->ticket;
    client->state = NET_WS_CLIENT_CONNECTING;
    net_mutex_init(&client->lock);
    client->assembly = (uint8_t *)malloc(TIMESTAMP_BYTES + (size_t)config->max_message_bytes);
    client->scratch = (uint8_t *)malloc(TIMESTAMP_BYTES + (size_t)config->max_message_bytes);
    client->tx_scratch = (uint8_t *)malloc(LWS_PRE + (size_t)config->send_queue_bytes);
    if (client->assembly == NULL || client->scratch == NULL || client->tx_scratch == NULL ||
        !net_queue_init(&client->rx, config->receive_queue_bytes) ||
        !net_queue_init(&client->tx, config->send_queue_bytes)) {
        free_client(client);
        return NULL;
    }
    client->protocols[0].name = config->subprotocol != NULL ? config->subprotocol : "";
    client->protocols[0].callback = protocol_callback;
    client->protocols[0].rx_buffer_size = config->max_message_bytes;
    /* At 0 lws would cap a send at rx_buffer_size and fragment the rest
       into further writes; a message never exceeds its queue. */
    client->protocols[0].tx_packet_size = config->send_queue_bytes;

    /* Process-global in lws; every context in this process wants the same,
       and one store keeps other clients' threads from reading a write. */
    static bool log_level_set;
    if (!log_level_set) {
        log_level_set = true;
        lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
    }
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = client->protocols;
    info.user = client;
    info.gid = (gid_t)-1;
    info.uid = (uid_t)-1;
    client->context = lws_create_context(&info);
    if (client->context == NULL) {
        free_client(client);
        return NULL;
    }

    /* An unusable URL is the caller's mistake and refused here; a resolver
       or connect failure is the network's and arrives as on_close. */
    if (strlen(config->url) >= sizeof client->url) {
        free_client(client);
        return NULL;
    }
    memcpy(client->url, config->url, strlen(config->url) + 1U);
    {
        char probe[512];
        memcpy(probe, client->url, strlen(client->url) + 1U);
        const char *scheme = NULL;
        const char *address = NULL;
        const char *path = NULL;
        int port = 0;
        if (lws_parse_uri(probe, &scheme, &address, &port, &path) != 0 || strcmp(scheme, "ws") != 0) {
            free_client(client);
            return NULL;
        }
    }
    if (!net_thread_start(&client->thread, service_thread, client)) {
        free_client(client);
        return NULL;
    }
    client->thread_started = true;
    return client;
}

void net_ws_client_destroy(net_ws_client_t *client) {
    if (client == NULL) { return; }
    client->destroying = true;
    /* Inside a callback the pump is walking the queues; it finishes the
       pass and frees everything afterwards. */
    if (client->in_service) {
        client->destroy_requested = true;
        return;
    }
    free_client(client);
}

net_ws_client_state_t net_ws_client_state(const net_ws_client_t *client) {
    net_ws_client_t *mutable = (net_ws_client_t *)client;
    net_mutex_lock(&mutable->lock);
    const net_ws_client_state_t state = client->state;
    net_mutex_unlock(&mutable->lock);
    return state;
}

void net_ws_client_service(net_ws_client_t *client, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (client->destroying) { return; }
    client->in_service = true;
    /* One critical section decides each delivery, so what the socket did in
       the order open, messages, close is replayed in that order. */
    while (!client->destroying) {
        net_mutex_lock(&client->lock);
        if (client->open_event) {
            client->open_event = false;
            net_mutex_unlock(&client->lock);
            if (client->config.on_open != NULL) { client->config.on_open(client->config.user); }
            continue;
        }
        const size_t size = net_queue_front_size(&client->rx);
        if (size > 0U) {
            net_queue_front_copy(&client->rx, client->scratch, size);
            net_queue_pop(&client->rx);
            net_mutex_unlock(&client->lock);
            double received_at = 0.0;
            memcpy(&received_at, client->scratch, sizeof received_at);
            if (client->config.on_message != NULL) {
                client->config.on_message(client->config.user, client->scratch + TIMESTAMP_BYTES,
                    size - TIMESTAMP_BYTES, received_at);
            }
            continue;
        }
        const bool closed = client->close_event;
        client->close_event = false;
        const uint16_t code = client->close_code;
        net_mutex_unlock(&client->lock);
        if (closed && client->config.on_close != NULL) { client->config.on_close(client->config.user, code); }
        break;
    }
    client->in_service = false;
    if (client->destroy_requested) { free_client(client); }
}

bool net_ws_client_send(net_ws_client_t *client, const uint8_t *data, size_t size) {
    if (size == 0U) { return false; }
    net_mutex_lock(&client->lock);
    const bool queued = client->state == NET_WS_CLIENT_OPEN && net_queue_push(&client->tx, data, size);
    net_mutex_unlock(&client->lock);
    if (queued) { lws_cancel_service(client->context); }
    return queued;
}
