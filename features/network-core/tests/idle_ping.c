#include "net_codec.h"
#include "net_ws_client.h"
#include "net_ws_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET raw_socket_t;
#define RAW_INVALID INVALID_SOCKET
#define raw_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int raw_socket_t;
#define RAW_INVALID (-1)
#define raw_close close
#endif

#define CHECK(condition) \
    do { if (!(condition)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

typedef struct peer_log_t {
    uint32_t connects, disconnects, messages;
    net_ws_close_reason_t reason;
} peer_log_t;

typedef struct wire_t {
    uint8_t bytes[64];
    size_t count;
    uint32_t pings, pongs;
    double last_pong;
    bool closed;
} wire_t;

static void connected(void *user, uint32_t client, const uint8_t *ticket, size_t size) {
    (void)client;
    (void)ticket;
    CHECK(size == 0U);
    ((peer_log_t *)user)->connects += 1U;
}

static void message(void *user, uint32_t client, const uint8_t *data, size_t size) {
    (void)client;
    (void)data;
    (void)size;
    ((peer_log_t *)user)->messages += 1U;
}

static void disconnected(void *user, uint32_t client, net_ws_close_reason_t reason) {
    (void)client;
    peer_log_t *log = user;
    log->disconnects += 1U;
    log->reason = reason;
}

static bool readable(raw_socket_t socket) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(socket, &reads);
    struct timeval wait = {0, 0};
    return select((int)socket + 1, &reads, NULL, NULL, &wait) > 0;
}

static void send_all(raw_socket_t socket, const void *bytes, size_t count) {
    const char *next = bytes;
    while (count > 0U) {
        const int sent = (int)send(socket, next, (int)count, 0);
        CHECK(sent > 0);
        next += sent;
        count -= (size_t)sent;
    }
}

static raw_socket_t upgrade(net_ws_server_t *server) {
    raw_socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sock != RAW_INVALID);
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(net_ws_server_port(server));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(connect(sock, (struct sockaddr *)&address, sizeof address) == 0);
    static const char request[] = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
    send_all(sock, request, sizeof request - 1U);
    char response[1024] = {0};
    size_t count = 0U;
    const double deadline = net_ws_client_clock() + 2.0;
    while (strstr(response, "\r\n\r\n") == NULL && net_ws_client_clock() < deadline) {
        net_ws_server_service(server, 10U);
        if (!readable(sock)) { continue; }
        const int part = (int)recv(sock, response + count, (int)(sizeof response - count - 1U), 0);
        CHECK(part > 0);
        count += (size_t)part;
        response[count] = '\0';
        CHECK(count + 1U < sizeof response);
    }
    CHECK(strstr(response, " 101 ") != NULL && strstr(response, "\r\n\r\n") != NULL);
    return sock;
}

static void hello(raw_socket_t socket, uint32_t version) {
    uint8_t payload[NET_HELLO_MAX_SIZE], frame[NET_HELLO_MAX_SIZE + 6U];
    net_writer_t writer;
    net_writer_init(&writer, payload, sizeof payload);
    net_hello_encode(&writer, version, NULL, 0U);
    CHECK(writer.ok && writer.pos < 126U);
    frame[0] = 0x82U;
    frame[1] = (uint8_t)(0x80U | writer.pos);
    const uint8_t mask[4] = {1U, 2U, 3U, 4U};
    memcpy(frame + 2U, mask, sizeof mask);
    for (size_t index = 0U; index < writer.pos; ++index) { frame[6U + index] = payload[index] ^ mask[index % 4U]; }
    send_all(socket, frame, writer.pos + 6U);
}

/* A raw peer exposes the control frames; library clients answer pings before
   application callbacks can verify their payload or deliberately stop ponging. */
static void read_pings(raw_socket_t socket, wire_t *wire, bool answer) {
    if (!readable(socket)) { return; }
    const int part = (int)recv(socket, (char *)wire->bytes + wire->count, (int)(sizeof wire->bytes - wire->count), 0);
    CHECK(part >= 0);
    if (part == 0) { wire->closed = true; return; }
    wire->count += (size_t)part;
    while (wire->count >= 2U) {
        CHECK(wire->bytes[0] == 0x89U && wire->bytes[1] == 0U);
        wire->pings += 1U;
        wire->count -= 2U;
        memmove(wire->bytes, wire->bytes + 2U, wire->count);
        if (answer) {
            const uint8_t pong[] = {0x8AU, 0x80U, 1U, 2U, 3U, 4U};
            send_all(socket, pong, sizeof pong);
            wire->pongs += 1U;
            wire->last_pong = net_ws_client_clock();
        }
    }
}

int main(void) {
#if defined(_WIN32)
    WSADATA data;
    CHECK(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    peer_log_t log = {0};
    const net_ws_server_config_t config = {.bind_address = "127.0.0.1", .port = 0U, .max_clients = 1U,
        .max_message_bytes = 64U, .send_queue_bytes = 256U, .handshake_timeout_ms = 1000U,
        .ping_idle_s = 1U, .hangup_idle_s = 3U, .protocol_version = 7U,
        .user = &log, .on_connect = connected, .on_message = message, .on_disconnect = disconnected};
    net_ws_server_t *server = net_ws_server_create(&config);
    CHECK(server != NULL);
    raw_socket_t socket = upgrade(server);
    hello(socket, config.protocol_version);
    const double hello_deadline = net_ws_client_clock() + 2.0;
    while (log.connects == 0U && net_ws_client_clock() < hello_deadline) { net_ws_server_service(server, 10U); }
    CHECK(log.connects == 1U && net_ws_server_client_count(server) == 1U);
    wire_t wire = {0};
    const double alive_until = net_ws_client_clock() + config.hangup_idle_s + config.ping_idle_s;
    while (net_ws_client_clock() < alive_until) {
        net_ws_server_service(server, 10U);
        read_pings(socket, &wire, true);
        CHECK(log.disconnects == 0U && !wire.closed);
    }
    CHECK(wire.pings >= 2U && wire.pongs == wire.pings);
    net_ws_server_stats_t stats;
    net_ws_server_take_stats(server, &stats);
    const double pong_deadline = net_ws_client_clock() + 1.0;
    while (stats.rx_pongs < wire.pongs && net_ws_client_clock() < pong_deadline) {
        net_ws_server_service(server, 10U);
        net_ws_server_take_stats(server, &stats);
    }
    CHECK(stats.rx_pongs == wire.pongs && log.messages == 0U);
    const uint32_t answered = wire.pongs;
    const double drop_deadline = wire.last_pong + config.hangup_idle_s + 2.0;
    while (log.disconnects == 0U && net_ws_client_clock() < drop_deadline) {
        net_ws_server_service(server, 10U);
        read_pings(socket, &wire, false);
    }
    CHECK(log.disconnects == 1U && log.reason == NET_WS_CLOSE_PEER);
    CHECK(net_ws_client_clock() - wire.last_pong >= (double)config.hangup_idle_s - 0.1);
    CHECK(net_ws_server_client_count(server) == 0U && wire.pongs == answered && wire.pings > wire.pongs);
    raw_close(socket);
    net_ws_server_destroy(server);
#if defined(_WIN32)
    WSACleanup();
#endif
    printf("idle ping wire ok: %u empty PING frames, %u PONGs, then peer timeout\n", wire.pings, wire.pongs);
    return 0;
}
