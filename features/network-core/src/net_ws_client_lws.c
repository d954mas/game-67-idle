#include "net_ws_client.h"

#include "net_codec.h"
#include "net_queue.h"

#include <libwebsockets.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct net_ws_client_t {
    net_ws_client_config_t config;
    struct lws_context *context;
    struct lws_protocols protocols[2];
    struct lws *wsi;
    lws_sorted_usec_list_t wake;
    net_ws_client_state_t state;
    uint16_t peer_close_code;
    uint8_t *rx;
    size_t rx_size;
    net_queue_t tx;
    uint8_t *tx_scratch;
    /* A close that happened while the game had no handle yet (inside create)
       is replayed from the first service call. */
    bool close_pending_replay;
    bool in_create;
    bool in_service;
    bool destroy_requested;
    bool destroying;
};

static void wake_noop(lws_sorted_usec_list_t *sul) { (void)sul; }

static void finish(net_ws_client_t *client, uint16_t code) {
    if (client->state == NET_WS_CLIENT_CLOSED) { return; }
    client->state = NET_WS_CLIENT_CLOSED;
    client->wsi = NULL;
    client->peer_close_code = code;
    if (client->destroying) { return; }
    if (client->in_create) {
        client->close_pending_replay = true;
        return;
    }
    if (client->config.on_close != NULL) { client->config.on_close(client->config.user, code); }
}

static int on_established(net_ws_client_t *client) {
    client->state = NET_WS_CLIENT_OPEN;
    uint8_t hello[NET_HELLO_SIZE];
    net_writer_t writer;
    net_writer_init(&writer, hello, sizeof hello);
    net_hello_encode(&writer, client->config.protocol_version);
    net_queue_push(&client->tx, hello, sizeof hello);
    lws_callback_on_writable(client->wsi);
    if (!client->destroying && client->config.on_open != NULL) { client->config.on_open(client->config.user); }
    return 0;
}

/* The client's own protocol close is reported to the game with the same
   code the peer receives. */
static int refuse(net_ws_client_t *client, struct lws *wsi) {
    client->peer_close_code = NET_CLOSE_FORMAT;
    lws_close_reason(wsi, (enum lws_close_status)NET_CLOSE_FORMAT, NULL, 0U);
    return -1;
}

static int on_receive(net_ws_client_t *client, struct lws *wsi, const uint8_t *data, size_t size) {
    if (lws_is_first_fragment(wsi)) {
        client->rx_size = 0U;
        if (!lws_frame_is_binary(wsi)) { return refuse(client, wsi); }
    }
    if (size > client->config.max_message_bytes - client->rx_size) { return refuse(client, wsi); }
    memcpy(client->rx + client->rx_size, data, size);
    client->rx_size += size;
    if (!lws_is_final_fragment(wsi) || lws_remaining_packet_payload(wsi) > 0U) { return 0; }
    if (!client->destroying && client->config.on_message != NULL) {
        client->config.on_message(client->config.user, client->rx, client->rx_size);
    }
    return 0;
}

static int on_writeable(net_ws_client_t *client, struct lws *wsi) {
    while (client->tx.count > 0U && !lws_send_pipe_choked(wsi)) {
        const size_t size = net_queue_front_size(&client->tx);
        net_queue_front_copy(&client->tx, client->tx_scratch + LWS_PRE, size);
        net_queue_pop(&client->tx);
        /* A refused write means lws is already closing this socket (a peer
           close frame is being answered); killing it here would turn that
           clean close into a reset and lose the peer's code. */
        if (lws_write(wsi, client->tx_scratch + LWS_PRE, size, LWS_WRITE_BINARY) < 0) { return 0; }
    }
    if (client->tx.count > 0U) { lws_callback_on_writable(wsi); }
    return 0;
}

static int protocol_callback(struct lws *wsi, enum lws_callback_reasons reason,
    void *user, void *in, size_t len) {
    net_ws_client_t *client = (net_ws_client_t *)user;
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        return on_established(client);
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

static void free_client(net_ws_client_t *client) {
    if (client->context != NULL) {
        lws_sul_cancel(&client->wake);
        lws_context_destroy(client->context);
    }
    free(client->rx);
    free(client->tx_scratch);
    net_queue_free(&client->tx);
    free(client);
}

net_ws_client_t *net_ws_client_create(const net_ws_client_config_t *config) {
    if (config == NULL || config->url == NULL || config->max_message_bytes == 0U ||
        config->send_queue_bytes < NET_HELLO_SIZE + 4U) {
        return NULL;
    }
    net_ws_client_t *client = (net_ws_client_t *)calloc(1U, sizeof *client);
    if (client == NULL) { return NULL; }
    client->config = *config;
    client->state = NET_WS_CLIENT_CONNECTING;
    client->in_create = true;
    client->rx = (uint8_t *)malloc(config->max_message_bytes);
    client->tx_scratch = (uint8_t *)malloc(LWS_PRE + (size_t)config->send_queue_bytes);
    if (client->rx == NULL || client->tx_scratch == NULL ||
        !net_queue_init(&client->tx, config->send_queue_bytes)) {
        free_client(client);
        return NULL;
    }
    client->protocols[0].name = config->subprotocol != NULL ? config->subprotocol : "";
    client->protocols[0].callback = protocol_callback;
    client->protocols[0].rx_buffer_size = config->max_message_bytes;

    /* Process-global in lws; every context in this process wants the same. */
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
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

    char url[512];
    if (strlen(config->url) >= sizeof url) {
        free_client(client);
        return NULL;
    }
    memcpy(url, config->url, strlen(config->url) + 1U);
    const char *scheme = NULL;
    const char *address = NULL;
    const char *path = NULL;
    int port = 0;
    if (lws_parse_uri(url, &scheme, &address, &port, &path) != 0 || strcmp(scheme, "ws") != 0) {
        free_client(client);
        return NULL;
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
    connect.protocol = config->subprotocol;
    connect.userdata = client;
    connect.pwsi = &client->wsi;
    /* A synchronous failure (DNS, refused) lands in finish() during this
       call and is replayed as on_close from the first service(). */
    if (lws_client_connect_via_info(&connect) == NULL) {
        client->state = NET_WS_CLIENT_CLOSED;
        client->wsi = NULL;
        client->close_pending_replay = true;
    }
    client->in_create = false;
    return client;
}

void net_ws_client_destroy(net_ws_client_t *client) {
    if (client == NULL) { return; }
    client->destroying = true;
    /* Inside a callback lws still walks this connection; the pump that is
       running finishes the pass and frees everything afterwards. */
    if (client->in_service) {
        client->destroy_requested = true;
        return;
    }
    free_client(client);
}

net_ws_client_state_t net_ws_client_state(const net_ws_client_t *client) {
    return client->state;
}

void net_ws_client_service(net_ws_client_t *client, uint32_t timeout_ms) {
    if (client->destroying) { return; }
    client->in_service = true;
    if (client->close_pending_replay) {
        client->close_pending_replay = false;
        if (client->config.on_close != NULL) {
            client->config.on_close(client->config.user, client->peer_close_code);
        }
    } else if (timeout_ms == 0U) {
        /* Same timeout contract as the server: zero is a non-blocking pass. */
        lws_service(client->context, -1);
    } else {
        lws_sul_schedule(client->context, 0, &client->wake, wake_noop,
            (lws_usec_t)timeout_ms * LWS_US_PER_MS);
        lws_service(client->context, 0);
    }
    client->in_service = false;
    if (client->destroy_requested) { free_client(client); }
}

bool net_ws_client_send(net_ws_client_t *client, const uint8_t *data, size_t size) {
    if (client->state != NET_WS_CLIENT_OPEN || client->wsi == NULL || size == 0U) { return false; }
    if (!net_queue_push(&client->tx, data, size)) { return false; }
    lws_callback_on_writable(client->wsi);
    return true;
}
