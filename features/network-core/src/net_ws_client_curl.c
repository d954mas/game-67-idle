#include "net_ws_client.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "net_codec.h"
#include "net_queue.h"
#include "net_thread.h"

#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The native client on libcurl's WebSocket: the same library the engine's
   HTTP already links, so wss:// comes with the box's own trust store
   (Schannel on Windows, OpenSSL elsewhere) and no second TLS library. The
   socket lives on its own thread so a frame arrives the moment the network
   delivers it, whatever the game is doing: its arrival time is exact,
   pings are answered during a loading stall, and sends never wait for
   the next frame. The game thread sees only the queues; callbacks still
   fire from net_ws_client_service() alone. The thread sleeps in poll()
   on the socket and a loopback wake socket, which a send from the game
   thread kicks. A destroy while the connect is still in flight does not
   wait for it: the thread is handed the client and frees it on its way
   out. */

#define SERVICE_SLICE_MS 100
#define CONNECT_TIMEOUT_MS 15000L
#define TIMESTAMP_BYTES 8U
#define PONG_MAX 125U               /* a control frame's payload, RFC 6455 */
#define CLOSE_FRAME_WAITS 5         /* slices given to a half-sent frame before a close */

#if defined(_WIN32)
typedef SOCKET wake_socket_t;
#define WAKE_INVALID INVALID_SOCKET
#define wake_close closesocket
#else
typedef int wake_socket_t;
#define WAKE_INVALID (-1)
#define wake_close close
#endif

struct net_ws_client_t {
    net_ws_client_config_t config;
    uint8_t ticket[NET_HELLO_TICKET_MAX];
    net_thread_t thread;
    bool thread_started;
    wake_socket_t wake;

    /* service thread only; the connect itself is its first act, so a slow
       DNS lookup never stalls the game and the handle has one owner. */
    CURL *easy;
    struct curl_slist *headers;
    curl_socket_t socket;
    char url[512];
    char error[CURL_ERROR_SIZE];
    uint8_t *assembly;              /* fragment reassembly, max_message_bytes */
    size_t assembly_size;
    bool assembling;
    uint8_t *tx_scratch;            /* the message being sent */
    size_t tx_size;
    size_t tx_offset;               /* sent so far; a frame goes out whole across calls */
    uint8_t *rx_chunk;              /* one curl_ws_recv worth */
    uint16_t peer_close_code;
    /* The peer's last ping, answered between our own frames: libcurl
       would answer it by itself, but only on the next send or the next
       frame received, which a quiet room may not give within its
       hangup. */
    uint8_t pong[PONG_MAX];
    size_t pong_size;
    bool pong_pending;

    /* shared, under lock */
    net_mutex_t lock;
    net_ws_client_state_t state;
    net_queue_t rx;                 /* [arrival seconds][payload] per message */
    net_queue_t tx;
    bool open_event;
    bool close_event;
    uint16_t close_code;
    bool stop;
    bool thread_done;               /* the service thread is past its last touch */
    bool orphaned;                  /* destroyed mid-connect: the thread frees the client */

    /* game thread only */
    uint8_t *scratch;               /* TIMESTAMP_BYTES + max_message_bytes */
    bool in_service;
    bool destroy_requested;
    bool destroying;
};

double net_ws_client_clock(void) { return net_clock_seconds(); }

/* A UDP socket connected to itself: one byte sent to it wakes a poll()
   sleeping on it, from any thread, on either platform. */
static wake_socket_t wake_open(void) {
    wake_socket_t fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == WAKE_INVALID) { return WAKE_INVALID; }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof addr;
    bool ok = bind(fd, (struct sockaddr *)&addr, sizeof addr) == 0 &&
        getsockname(fd, (struct sockaddr *)&addr, &len) == 0 &&
        connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0;
#if defined(_WIN32)
    u_long nonblocking = 1;
    ok = ok && ioctlsocket(fd, (long)FIONBIO, &nonblocking) == 0;
#else
    ok = ok && fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == 0;
#endif
    if (!ok) {
        wake_close(fd);
        return WAKE_INVALID;
    }
    return fd;
}

static void wake_kick(wake_socket_t fd) {
    const char byte = 0;
    (void)send(fd, &byte, 1, 0);
}

static void wake_drain(wake_socket_t fd) {
    char bytes[64];
    while (recv(fd, bytes, sizeof bytes, 0) > 0) {}
}

/* Sleeps until the socket is readable (writable too when asked), the wake
   socket is kicked, or the slice ends. */
static void wait_for(curl_socket_t socket, bool writable, wake_socket_t wake) {
#if defined(_WIN32)
    WSAPOLLFD fds[2];
#else
    struct pollfd fds[2];
#endif
    memset(fds, 0, sizeof fds);
    fds[0].fd = socket;
    fds[0].events = (short)(POLLIN | (writable ? POLLOUT : 0));
    fds[1].fd = wake;
    fds[1].events = POLLIN;
#if defined(_WIN32)
    WSAPoll(fds, 2, SERVICE_SLICE_MS);
#else
    poll(fds, 2, SERVICE_SLICE_MS);
#endif
    wake_drain(wake);
}

/* service thread: the connection is over; the game hears about it from service(). */
static void finish(net_ws_client_t *client, uint16_t code) {
    net_mutex_lock(&client->lock);
    if (client->state != NET_WS_CLIENT_CLOSED) {
        client->state = NET_WS_CLIENT_CLOSED;
        client->close_event = true;
        client->close_code = code;
    }
    net_mutex_unlock(&client->lock);
}

static bool flush_tx(net_ws_client_t *client);
static void wait_for(curl_socket_t socket, bool writable, wake_socket_t wake);

/* A close frame cannot cut into a frame that went out in part: libcurl
   would take the close bytes as that frame's payload. The frame gets a
   few slices to finish; a socket that stays choked gets no close frame,
   and the TCP close that follows tells the peer the same. */
static void send_close(net_ws_client_t *client, uint16_t code) {
    for (int slice = 0; slice < CLOSE_FRAME_WAITS && client->tx_size != 0U; ++slice) {
        if (!flush_tx(client)) { return; }
        if (client->tx_size != 0U) { wait_for(client->socket, true, client->wake); }
    }
    if (client->tx_size != 0U) { return; }
    const uint8_t payload[2] = {(uint8_t)(code >> 8), (uint8_t)(code & 0xFFU)};
    size_t sent = 0U;
    (void)curl_ws_send(client->easy, payload, sizeof payload, &sent, 0, CURLWS_CLOSE);
}

/* The client's own protocol close is reported to the game with the same
   code the peer receives. */
static void refuse(net_ws_client_t *client, uint16_t code) {
    send_close(client, code);
    finish(client, code);
}

static bool connect_ws(net_ws_client_t *client) {
    client->easy = curl_easy_init();
    if (client->easy == NULL) { return false; }
    CURL *easy = client->easy;
    curl_easy_setopt(easy, CURLOPT_URL, client->url);
    curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 2L);
    /* The whole handshake is bounded, not only the TCP connect: a peer
       that accepts and never answers would otherwise hold the thread. */
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, CONNECT_TIMEOUT_MS);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, CONNECT_TIMEOUT_MS);
    /* A resolver still working is left to itself at cleanup. */
    curl_easy_setopt(easy, CURLOPT_QUICK_EXIT, 1L);
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(easy, CURLOPT_WS_OPTIONS, (long)CURLWS_NOAUTOPONG);
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, client->error);
    if (client->config.subprotocol != NULL) {
        char header[128];
        snprintf(header, sizeof header, "Sec-WebSocket-Protocol: %s", client->config.subprotocol);
        client->headers = curl_slist_append(NULL, header);
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, client->headers);
    }
    if (client->config.tls_insecure) {
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    const CURLcode code = curl_easy_perform(easy);
    if (code != CURLE_OK) {
        fprintf(stderr, "net: %s: %s\n", client->url, client->error[0] != '\0' ? client->error : curl_easy_strerror(code));
        return false;
    }
    if (curl_easy_getinfo(easy, CURLINFO_ACTIVESOCKET, &client->socket) != CURLE_OK ||
        client->socket == CURL_SOCKET_BAD) {
        return false;
    }
    return true;
}

/* HELLO is the first frame: the game cannot queue anything before the
   state turns OPEN, which happens here, after the push. */
static void on_established(net_ws_client_t *client) {
    uint8_t hello[NET_HELLO_MAX_SIZE];
    net_writer_t writer;
    net_writer_init(&writer, hello, sizeof hello);
    net_hello_encode(&writer, client->config.protocol_version, client->ticket, client->config.ticket_size);
    net_mutex_lock(&client->lock);
    net_queue_push(&client->tx, hello, writer.pos);
    client->state = NET_WS_CLIENT_OPEN;
    client->open_event = true;
    net_mutex_unlock(&client->lock);
}

/* Sends queued messages until the socket blocks or the queue is empty;
   false when the connection failed. A frame that went out in part
   continues from its offset on the next call: libcurl reports the payload
   bytes it flushed and takes the remainder, exactly, next time. */
static bool flush_tx(net_ws_client_t *client) {
    for (;;) {
        if (client->tx_size == 0U && client->pong_pending) {
            size_t sent = 0U;
            const CURLcode code = curl_ws_send(client->easy, client->pong, client->pong_size, &sent, 0, CURLWS_PONG);
            if (code == CURLE_AGAIN) { return true; }
            if (code != CURLE_OK) {
                finish(client, 0U);
                return false;
            }
            client->pong_pending = false;
        }
        if (client->tx_size == 0U) {
            net_mutex_lock(&client->lock);
            const size_t size = net_queue_front_size(&client->tx);
            if (size > 0U) {
                net_queue_front_copy(&client->tx, client->tx_scratch, size);
                net_queue_pop(&client->tx);
            }
            net_mutex_unlock(&client->lock);
            if (size == 0U) { return true; }
            client->tx_size = size;
            client->tx_offset = 0U;
        }
        size_t sent = 0U;
        const CURLcode code = curl_ws_send(client->easy, client->tx_scratch + client->tx_offset,
            client->tx_size - client->tx_offset, &sent, 0, CURLWS_BINARY);
        if (code == CURLE_AGAIN) { return true; }
        if (code != CURLE_OK) {
            finish(client, 0U);
            return false;
        }
        client->tx_offset += sent;
        if (client->tx_offset < client->tx_size) { return true; }
        client->tx_size = 0U;
    }
}

static bool queue_message(net_ws_client_t *client) {
    const double now = net_ws_client_clock();
    memcpy(client->assembly, &now, sizeof now);
    net_mutex_lock(&client->lock);
    bool queued = net_queue_push(&client->rx, client->assembly, client->assembly_size);
    while (!queued && client->config.overflow_policy == NET_WS_OVERFLOW_DROP_OLDEST && client->rx.count > 0U) {
        /* The queue holds at least one message, so a pop always makes room. */
        net_queue_pop(&client->rx);
        queued = net_queue_push(&client->rx, client->assembly, client->assembly_size);
    }
    net_mutex_unlock(&client->lock);
    return queued;
}

/* Reads until the socket is drained; false when the connection ended.
   libcurl hands frames in chunks with the frame's offset and what is left
   after the chunk; a message is whole when its final fragment's last chunk
   arrives. Control frames may land between the fragments of a message and
   do not touch the assembly. */
static bool drain_rx(net_ws_client_t *client) {
    for (;;) {
        size_t nread = 0U;
        const struct curl_ws_frame *meta = NULL;
        const CURLcode code = curl_ws_recv(client->easy, client->rx_chunk, client->config.max_message_bytes,
            &nread, &meta);
        if (code == CURLE_AGAIN) { return true; }
        if (code != CURLE_OK || meta == NULL) {
            finish(client, 0U);
            return false;
        }
        if (meta->flags & CURLWS_CLOSE) {
            if (meta->offset == 0 && nread >= 2U) {
                client->peer_close_code = (uint16_t)(((uint16_t)client->rx_chunk[0] << 8) | client->rx_chunk[1]);
            }
            /* The close handshake: the peer's frame is answered with its
               own code, and the socket goes down with the thread. */
            send_close(client, client->peer_close_code != 0U ? client->peer_close_code : 1000U);
            finish(client, client->peer_close_code);
            return false;
        }
        if (meta->flags & CURLWS_PING) {
            /* The newest ping is the one worth answering. */
            if (meta->offset == 0 && nread <= PONG_MAX) {
                memcpy(client->pong, client->rx_chunk, nread);
                client->pong_size = nread;
                client->pong_pending = true;
            }
            continue;
        }
        if (meta->flags & CURLWS_PONG) { continue; }
        if (meta->flags & CURLWS_TEXT) {
            refuse(client, NET_CLOSE_FORMAT);
            return false;
        }
        /* Every fragment of a message carries its type; CONT marks the
           ones before the last. */
        if (!client->assembling) {
            if (!(meta->flags & CURLWS_BINARY)) {
                refuse(client, NET_CLOSE_FORMAT);
                return false;
            }
            client->assembling = true;
            client->assembly_size = TIMESTAMP_BYTES;
        }
        if (nread > client->config.max_message_bytes - (client->assembly_size - TIMESTAMP_BYTES)) {
            refuse(client, NET_CLOSE_FORMAT);
            return false;
        }
        memcpy(client->assembly + client->assembly_size, client->rx_chunk, nread);
        client->assembly_size += nread;
        const bool frame_done = meta->bytesleft == 0;
        const bool message_done = frame_done && !(meta->flags & CURLWS_CONT);
        if (!message_done) { continue; }
        client->assembling = false;
        if (client->assembly_size == TIMESTAMP_BYTES) {
            refuse(client, NET_CLOSE_FORMAT);
            return false;
        }
        if (!queue_message(client)) {
            refuse(client, NET_CLOSE_SLOW);
            return false;
        }
    }
}

static void release(net_ws_client_t *client);

static void service_thread(void *arg) {
    net_ws_client_t *client = (net_ws_client_t *)arg;
    if (connect_ws(client)) {
        on_established(client);
        for (;;) {
            net_mutex_lock(&client->lock);
            const bool stop = client->stop || client->state == NET_WS_CLIENT_CLOSED;
            net_mutex_unlock(&client->lock);
            /* Nothing more can happen on a closed socket; the thread idles
               out and destroy joins it at once. */
            if (stop) { break; }
            if (!flush_tx(client)) { break; }
            net_mutex_lock(&client->lock);
            const bool pending = client->tx_size > 0U || client->tx.count > 0U || client->pong_pending;
            net_mutex_unlock(&client->lock);
            wait_for(client->socket, pending, client->wake);
            if (!drain_rx(client)) { break; }
        }
        net_mutex_lock(&client->lock);
        const bool open = client->state == NET_WS_CLIENT_OPEN;
        net_mutex_unlock(&client->lock);
        /* Going away on the game's word is a normal close for the peer. */
        if (open) { send_close(client, 1000U); }
    } else {
        finish(client, 0U);
    }
    /* The socket closes with the thread, not with the game's destroy: a
       peer that closed us sees the connection end at once. */
    if (client->easy != NULL) {
        curl_easy_cleanup(client->easy);
        client->easy = NULL;
    }
    net_mutex_lock(&client->lock);
    const bool orphaned = client->orphaned;
    client->thread_done = true;
    net_mutex_unlock(&client->lock);
    if (orphaned) { release(client); }
}

/* Frees what the client holds; the thread is gone or detached by now. */
static void release(net_ws_client_t *client) {
    if (client->headers != NULL) { curl_slist_free_all(client->headers); }
    if (client->wake != WAKE_INVALID) { wake_close(client->wake); }
    net_mutex_free(&client->lock);
    free(client->assembly);
    free(client->tx_scratch);
    free(client->rx_chunk);
    free(client->scratch);
    net_queue_free(&client->rx);
    net_queue_free(&client->tx);
    free(client);
}

/* A thread still connecting is not waited for: it takes the client with
   it and frees it when the connect returns. */
static void free_client(net_ws_client_t *client) {
    if (client->thread_started) {
        net_mutex_lock(&client->lock);
        client->stop = true;
        const bool done = client->thread_done;
        const bool open = client->state != NET_WS_CLIENT_CONNECTING;
        if (!done && !open) { client->orphaned = true; }
        net_mutex_unlock(&client->lock);
        wake_kick(client->wake);
        if (!done && !open) {
            net_thread_detach(&client->thread);
            return;
        }
        net_thread_join(&client->thread);
    }
    release(client);
}

net_ws_client_t *net_ws_client_create(const net_ws_client_config_t *config) {
    if (config == NULL || config->url == NULL || config->max_message_bytes == 0U ||
        config->receive_queue_bytes < config->max_message_bytes + TIMESTAMP_BYTES + 4U ||
        config->ticket_size > NET_HELLO_TICKET_MAX || (config->ticket_size > 0U && config->ticket == NULL) ||
        config->send_queue_bytes < NET_HELLO_BASE_SIZE + config->ticket_size + 4U) {
        return NULL;
    }
    /* An unusable URL is the caller's mistake and refused here; a resolver
       or connect failure is the network's and arrives as on_close. */
    if (strlen(config->url) >= sizeof ((net_ws_client_t *)0)->url ||
        (strncmp(config->url, "ws://", 5U) != 0 && strncmp(config->url, "wss://", 6U) != 0)) {
        return NULL;
    }
    /* Process-wide and reference counted in libcurl; the engine's HTTP
       may have done it already, one more count is harmless. */
    static bool curl_ready;
    if (!curl_ready) {
        curl_ready = true;
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    net_ws_client_t *client = (net_ws_client_t *)calloc(1U, sizeof *client);
    if (client == NULL) { return NULL; }
    client->config = *config;
    if (config->ticket_size > 0U) { memcpy(client->ticket, config->ticket, config->ticket_size); }
    client->config.ticket = client->ticket;
    client->state = NET_WS_CLIENT_CONNECTING;
    client->socket = CURL_SOCKET_BAD;
    client->wake = WAKE_INVALID;
    net_mutex_init(&client->lock);
    memcpy(client->url, config->url, strlen(config->url) + 1U);
    client->assembly = (uint8_t *)malloc(TIMESTAMP_BYTES + (size_t)config->max_message_bytes);
    client->scratch = (uint8_t *)malloc(TIMESTAMP_BYTES + (size_t)config->max_message_bytes);
    client->rx_chunk = (uint8_t *)malloc((size_t)config->max_message_bytes);
    client->tx_scratch = (uint8_t *)malloc((size_t)config->send_queue_bytes);
    client->wake = wake_open();
    if (client->assembly == NULL || client->scratch == NULL || client->rx_chunk == NULL ||
        client->tx_scratch == NULL || client->wake == WAKE_INVALID ||
        !net_queue_init(&client->rx, config->receive_queue_bytes) ||
        !net_queue_init(&client->tx, config->send_queue_bytes)) {
        free_client(client);
        return NULL;
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
    if (queued) { wake_kick(client->wake); }
    return queued;
}
