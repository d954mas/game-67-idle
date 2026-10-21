/* Minimal second consumer of network-core: an echo server and a native client
   in one process, pumped alternately from one thread. Proves the handshake,
   echo, and every rejection path the transport promises. */
#include "net_codec.h"
#include "net_hmac.h"
#include "net_ws_client.h"
#include "net_ws_server.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET raw_socket_t;
#define RAW_INVALID INVALID_SOCKET
#define raw_close closesocket
#define sleep_ms(ms) Sleep(ms)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int raw_socket_t;
#define RAW_INVALID (-1)
#define raw_close close
#define sleep_ms(ms) usleep((ms) * 1000U)
#endif

#define CHECK(condition) \
    do { if (!(condition)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

#define VERSION 7U
#define MAX_MESSAGE 64U

/* The native backend owns this deterministic failure seam; it is not a
   transport API and keeps write-error coverage off socket timing. */
void net_ws_server_test_fail_next_write(net_ws_server_t *server);
void net_ws_server_test_hold_writes(net_ws_server_t *server, bool hold);
void net_ws_server_test_partial_buffered(net_ws_server_t *server, bool buffered);
uint32_t net_ws_client_test_queued_messages(net_ws_client_t *client);
typedef struct server_log_t {
    net_ws_server_t *server;
    uint32_t connects;
    uint32_t disconnects;
    uint32_t last_client;
    uint8_t last_ticket[NET_HELLO_TICKET_MAX];
    size_t last_ticket_size;
    uint32_t messages;
    net_ws_close_reason_t last_reason;
    bool echo;
} server_log_t;

typedef struct client_log_t {
    uint32_t opens;
    uint32_t closes;
    uint16_t close_code;
    uint32_t messages;
    uint8_t last[MAX_MESSAGE];
    size_t last_size;
    double last_received_at;
    net_ws_client_t *destroy_on_close; /* the natural game pattern: free in on_close */
} client_log_t;

static void server_connect(void *user, uint32_t client, const uint8_t *ticket, size_t ticket_size) {
    server_log_t *log = (server_log_t *)user;
    log->connects += 1U;
    log->last_client = client;
    log->last_ticket_size = ticket_size;
    if (ticket_size > 0U && ticket_size <= sizeof log->last_ticket) {
        memcpy(log->last_ticket, ticket, ticket_size);
    }
}

static void server_message(void *user, uint32_t client, const uint8_t *data, size_t size) {
    server_log_t *log = (server_log_t *)user;
    log->messages += 1U;
    if (log->echo) { net_ws_server_send(log->server, client, data, size); }
}

static void server_disconnect(void *user, uint32_t client, net_ws_close_reason_t reason) {
    server_log_t *log = (server_log_t *)user;
    (void)client;
    log->disconnects += 1U;
    log->last_reason = reason;
}

static void client_open(void *user) { ((client_log_t *)user)->opens += 1U; }

static void client_message(void *user, const uint8_t *data, size_t size, double received_at) {
    client_log_t *log = (client_log_t *)user;
    log->messages += 1U;
    log->last_received_at = received_at;
    log->last_size = size;
    memcpy(log->last, data, size);
}

static void client_close(void *user, uint16_t code) {
    client_log_t *log = (client_log_t *)user;
    log->closes += 1U;
    log->close_code = code;
    if (log->destroy_on_close != NULL) {
        net_ws_client_destroy(log->destroy_on_close);
        log->destroy_on_close = NULL;
    }
}


/* A peer made of a bare socket: it completes the upgrade and HELLO like a
   real client and then answers nothing, which no library client will do
   for us since every one of them pongs by itself. */
static raw_socket_t raw_peer_connect_from(uint16_t port, uint32_t source) {
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    raw_socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == RAW_INVALID) { return RAW_INVALID; }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(source);
    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
        raw_close(sock);
        return RAW_INVALID;
    }
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
        raw_close(sock);
        return RAW_INVALID;
    }
    char request[256];
    const int length = snprintf(request, sizeof request,
        "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n");
    if (send(sock, request, length, 0) != length) {
        raw_close(sock);
        return RAW_INVALID;
    }
    return sock;
}

static raw_socket_t raw_peer_connect(uint16_t port) { return raw_peer_connect_from(port, INADDR_LOOPBACK); }

/* True once the 101 has arrived, however it was split; the server must be
   serviced between calls. `response`/`got` accumulate across calls. */
static bool raw_peer_upgraded(raw_socket_t sock, char *response, size_t capacity, int *got) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(sock, &readable);
    struct timeval zero = {0, 0};
    if (select((int)sock + 1, &readable, NULL, NULL, &zero) <= 0) { return false; }
    const int part = (int)recv(sock, response + *got, (int)capacity - 1 - *got, 0);
    if (part <= 0) { return false; }
    *got += part;
    response[*got] = '\0';
    return strstr(response, " 101 ") != NULL;
}

/* The code of the first frame after the 101 if it is a close, else 0: a
   refused socket hears nothing else before it, and both may share a read. */
static uint16_t raw_peer_close_code(const char *response, int got) {
    const char *end = strstr(response, "\r\n\r\n");
    if (end == NULL) { return 0U; }
    const uint8_t *frame = (const uint8_t *)end + 4;
    /* An unmasked close with at least the two-byte code. */
    if ((const char *)frame + 4 > response + got || frame[0] != 0x88U || frame[1] < 2U || frame[1] > 125U) { return 0U; }
    return (uint16_t)((frame[2] << 8) | frame[3]);
}

typedef struct raw_peer_t {
    raw_socket_t sock;
    char response[512];
    int got;
    bool upgraded;
} raw_peer_t;

static void raw_peers_open(raw_peer_t *peers, uint32_t count, uint16_t port, uint32_t source) {
    for (uint32_t index = 0U; index < count; ++index) {
        memset(&peers[index], 0, sizeof peers[index]);
        peers[index].sock = raw_peer_connect_from(port, source);
        CHECK(peers[index].sock != RAW_INVALID);
    }
}

/* Services the server and reads every peer for `seconds`, or until all are upgraded. */
static bool raw_peers_pump(net_ws_server_t *server, raw_peer_t *peers, uint32_t count, double seconds,
    bool until_upgraded) {
    const double end = net_ws_client_clock() + seconds;
    bool all = false;
    while (net_ws_client_clock() < end && !(until_upgraded && all)) {
        net_ws_server_service(server, 1U);
        all = true;
        for (uint32_t index = 0U; index < count; ++index) {
            raw_peer_t *peer = &peers[index];
            peer->upgraded = raw_peer_upgraded(peer->sock, peer->response, sizeof peer->response, &peer->got) ||
                peer->upgraded;
            all = all && peer->upgraded;
        }
    }
    return all;
}

static void raw_peers_close(raw_peer_t *peers, uint32_t count) {
    for (uint32_t index = 0U; index < count; ++index) { raw_close(peers[index].sock); }
}

/* False only when the host has no such loopback alias (127.0.0.2 is not
   local on every OS); any other failure is a real one. */
static bool loopback_alias_available(uint32_t host) {
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    raw_socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sock != RAW_INVALID);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(host);
    const int bound = bind(sock, (struct sockaddr *)&addr, sizeof addr);
#if defined(_WIN32)
    const bool missing = bound != 0 && WSAGetLastError() == WSAEADDRNOTAVAIL;
#else
    const bool missing = bound != 0 && errno == EADDRNOTAVAIL;
#endif
    raw_close(sock);
    CHECK(bound == 0 || missing);
    return !missing;
}

static bool raw_peer_send_hello(raw_socket_t sock, uint32_t version) {
    /* One masked binary frame: the HELLO. */
    uint8_t hello[NET_HELLO_MAX_SIZE];
    net_writer_t writer;
    net_writer_init(&writer, hello, sizeof hello);
    net_hello_encode(&writer, version, NULL, 0U);
    uint8_t frame[2U + 4U + NET_HELLO_MAX_SIZE];
    const uint8_t mask[4] = {1U, 2U, 3U, 4U};
    frame[0] = 0x82U;
    frame[1] = (uint8_t)(0x80U | writer.pos);
    memcpy(frame + 2U, mask, 4U);
    for (size_t index = 0U; index < writer.pos; ++index) { frame[6U + index] = hello[index] ^ mask[index % 4U]; }
    const int frame_size = (int)(6U + writer.pos);
    return send(sock, (const char *)frame, frame_size, 0) == frame_size;
}

static void pump(net_ws_server_t *server, net_ws_client_t *client) {
    net_ws_server_service(server, 1U);
    if (client != NULL) { net_ws_client_service(client, 0U); }
}

/* Waits on the outcome against the monotonic clock, never a round count: a
   service call lasts as long as the OS timer granularity makes it (1 to
   16 ms on Windows), and the client's socket thread runs on wall time. */
#define WAIT_S 3.0
#define wait_until(condition, step) do { const double wait_end_ = net_ws_client_clock() + WAIT_S; while (!(condition) && net_ws_client_clock() < wait_end_) { step; } } while (0)
#define pump_until(server, client, condition) wait_until((condition), pump((server), (client)))

/* A window for checks that something did NOT happen; no event can end it. */
static void pump_for(net_ws_server_t *server, net_ws_client_t *client, double seconds) {
    const double end = net_ws_client_clock() + seconds;
    while (net_ws_client_clock() < end) { pump(server, client); }
}

/* Until the client is open and the server has run on_connect for it, so
   slog->last_client names this client. */
static void pump_open(net_ws_server_t *server, const server_log_t *slog, net_ws_client_t *client,
    const client_log_t *clog) {
    const uint32_t connects = slog->connects;
    pump_until(server, client, clog->opens == 1U && slog->connects == connects + 1U);
    CHECK(clog->opens == 1U && slog->connects == connects + 1U);
}

/* Every server here admits max_clients (2) new sockets a second and closes
   the rest at accept, so dials are spaced to stay inside that budget. */
static void pace_dial(void) {
    static double last_dial = 0.0;
    const double wait = last_dial + 0.5 - net_ws_client_clock();
    if (wait > 0.0) { sleep_ms((unsigned)(wait * 1000.0) + 1U); }
    last_dial = net_ws_client_clock();
}

static net_ws_client_t *connect_client_sized(uint16_t port, uint32_t version, client_log_t *log,
    uint32_t send_queue, uint32_t max_message) {
    char url[64];
    snprintf(url, sizeof url, "ws://127.0.0.1:%u/", (unsigned)port);
    static const uint8_t ticket[] = {'s', 'e', 'a', 't'};
    const net_ws_client_config_t config = {
        .url = url, .protocol_version = version, .max_message_bytes = max_message,
        .ticket = ticket, .ticket_size = sizeof ticket,
        .receive_queue_bytes = 1024U, .send_queue_bytes = send_queue, .user = log,
        .on_open = client_open, .on_message = client_message, .on_close = client_close,
    };
    pace_dial();
    net_ws_client_t *client = net_ws_client_create(&config);
    CHECK(client != NULL);
    return client;
}

static net_ws_client_t *connect_client(uint16_t port, uint32_t version, client_log_t *log,
    uint32_t send_queue) {
    return connect_client_sized(port, version, log, send_queue, MAX_MESSAGE);
}

static net_ws_client_t *connect_client_limits(uint16_t port, uint32_t version, client_log_t *log,
    uint32_t receive_messages, uint32_t service_limit) {
    char url[64];
    snprintf(url, sizeof url, "ws://127.0.0.1:%u/", (unsigned)port);
    const net_ws_client_config_t config = {
        .url = url, .protocol_version = version, .max_message_bytes = MAX_MESSAGE,
        .receive_queue_bytes = 1024U, .receive_queue_messages = receive_messages,
        .service_message_limit = service_limit, .send_queue_bytes = 256U, .user = log,
        .on_open = client_open, .on_message = client_message, .on_close = client_close,
    };
    pace_dial();
    net_ws_client_t *client = net_ws_client_create(&config);
    CHECK(client != NULL);
    return client;
}

static void send_numbered(net_ws_server_t *server, uint32_t client, uint32_t count) {
    for (uint32_t index = 0U; index < count; ++index) {
        const uint8_t message[] = {NET_MSG_APP_FIRST, (uint8_t)index};
        CHECK(net_ws_server_send(server, client, message, sizeof message));
    }
}

static void test_codec(void) {
    uint8_t buffer[16];
    net_writer_t writer;
    net_writer_init(&writer, buffer, sizeof buffer);
    net_write_u8(&writer, 0xABU);
    net_write_i8(&writer, -5);
    net_write_u16(&writer, 0x1234U);
    net_write_u32(&writer, 0xDEADBEEFU);
    net_write_f32(&writer, 1.5F);
    CHECK(writer.ok && writer.pos == 12U);
    CHECK(buffer[0] == 0xABU && buffer[2] == 0x34U && buffer[3] == 0x12U && buffer[4] == 0xEFU);
    net_write_u32(&writer, 1U);
    net_write_u8(&writer, 1U);
    CHECK(!writer.ok);

    net_reader_t reader;
    net_reader_init(&reader, buffer, 12U);
    CHECK(net_read_u8(&reader) == 0xABU);
    CHECK(net_read_i8(&reader) == -5);
    CHECK(net_read_u16(&reader) == 0x1234U);
    CHECK(net_read_u32(&reader) == 0xDEADBEEFU);
    CHECK(net_read_f32(&reader) == 1.5F);
    CHECK(net_reader_complete(&reader));
    CHECK(net_read_u8(&reader) == 0U && !reader.ok && !net_reader_complete(&reader));

    net_reader_init(&reader, buffer, 3U);
    (void)net_read_u32(&reader);
    CHECK(!reader.ok);

    uint8_t hello[NET_HELLO_MAX_SIZE];
    const uint8_t stub[3] = {7U, 8U, 9U};
    net_writer_init(&writer, hello, sizeof hello);
    net_hello_encode(&writer, 42U, stub, sizeof stub);
    uint32_t version = 0U;
    const uint8_t *ticket = NULL;
    size_t ticket_size = 0U;
    CHECK(writer.ok && writer.pos == NET_HELLO_BASE_SIZE + sizeof stub);
    CHECK(net_hello_decode(hello, writer.pos, &version, &ticket, &ticket_size) && version == 42U);
    CHECK(ticket_size == sizeof stub && memcmp(ticket, stub, sizeof stub) == 0);
    CHECK(net_hello_decode(hello, NET_HELLO_BASE_SIZE, &version, &ticket, &ticket_size) && ticket_size == 0U);
    CHECK(!net_hello_decode(hello, NET_HELLO_BASE_SIZE - 1U, &version, &ticket, &ticket_size));
    hello[1] ^= 0xFFU;
    CHECK(!net_hello_decode(hello, writer.pos, &version, &ticket, &ticket_size));
    net_writer_init(&writer, hello, sizeof hello);
    net_hello_encode(&writer, 42U, hello, NET_HELLO_TICKET_MAX + 1U);
    CHECK(!writer.ok);
}

static void test_quantized(void) {
    /* A power-of-two step over a range that starts on it: the grid's
       points, its ends included, come back exact. */
    const net_grid_t metres = {.min = -19.5F, .step = 1.0F / 1024.0F, .bits = 16U};
    CHECK(net_quantize(metres, -19.5F) == -19.5F);
    CHECK(net_quantize(metres, 0.0F) == 0.0F);
    CHECK(net_quantize(metres, 19.5F) == 19.5F);
    CHECK(net_quantize(metres, 1.75F) == 1.75F);
    /* Nearest point; past the ends clamps; a snap is idempotent. */
    const float snapped = net_quantize(metres, 3.14159F);
    CHECK(fabsf(snapped - 3.14159F) <= 0.5F / 1024.0F);
    CHECK(net_quantize(metres, snapped) == snapped);
    CHECK(net_quantize(metres, -1e9F) == -19.5F);
    CHECK(net_quantize(metres, 1e9F) == -19.5F + 65535.0F / 1024.0F);
    CHECK(net_quantize(metres, NAN) == -19.5F);

    uint8_t buffer[16];
    net_writer_t writer;
    net_writer_init(&writer, buffer, sizeof buffer);
    net_write_quantized(&writer, metres, 1.75F);
    const net_grid_t bytes = {.min = 0.0F, .step = 0.25F, .bits = 8U};
    net_write_quantized(&writer, bytes, 63.75F);
    const net_grid_t wide = {.min = -1.0F, .step = 1e-6F, .bits = 32U};
    net_write_quantized(&writer, wide, 0.5F);
    net_write_angle16(&writer, 3.0F);
    const net_grid_t city = {.min = -256.0F, .step = 1.0F / 1024.0F, .bits = 19U};
    net_write_quantized(&writer, city, 200.25F);
    CHECK(writer.ok && writer.pos == 2U + 1U + 4U + 2U + 3U);
    /* -19.5 + 21760 / 1024 == 1.75: two bytes, little-endian. */
    CHECK(buffer[0] == 0x00U && buffer[1] == 0x55U && buffer[2] == 0xFFU);

    net_reader_t reader;
    net_reader_init(&reader, buffer, writer.pos);
    CHECK(net_read_quantized(&reader, metres) == 1.75F);
    CHECK(net_read_quantized(&reader, bytes) == 63.75F);
    CHECK(fabsf(net_read_quantized(&reader, wide) - 0.5F) <= 1e-6F);
    CHECK(fabsf(net_read_angle16(&reader) - 3.0F) <= 3.14159F / 65536.0F);
    CHECK(net_read_quantized(&reader, city) == 200.25F);
    CHECK(net_reader_complete(&reader));

    /* Angles wrap: a turn and a half is half a turn, pi lands on -pi, and
       the quarter turns are exact. */
    CHECK(net_quantize_angle16(0.0F) == 0.0F);
    CHECK(net_quantize_angle16(3.14159265F) == net_quantize_angle16(-3.14159265F));
    CHECK(net_quantize_angle16(3.14159265F) < 0.0F);
    CHECK(net_quantize_angle16(3.0F * 3.14159265F) == net_quantize_angle16(3.14159265F));
    CHECK(net_quantize_angle16(1.57079633F) == 1.57079633F);
    CHECK(net_quantize_angle16(-0.78539816F) == -0.78539816F);
    CHECK(net_quantize_angle16(1e30F) >= -3.14159274F && net_quantize_angle16(1e30F) < 3.14159274F);
    const float turned = net_quantize_angle16(2.5F);
    CHECK(net_quantize_angle16(turned) == turned);
}

static bool hex_equals(const uint8_t *bytes, size_t size, const char *hex) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0U; i < size; ++i) {
        if (hex[i * 2U] != digits[bytes[i] >> 4] || hex[i * 2U + 1U] != digits[bytes[i] & 0xFU]) { return false; }
    }
    return hex[size * 2U] == '\0';
}

/* FIPS 180-4 / NIST CAVP vectors for SHA-256, RFC 4231 for HMAC. */
static void test_hmac(void) {
    uint8_t digest[NET_SHA256_SIZE];
    net_sha256("abc", 3U, digest);
    CHECK(hex_equals(digest, sizeof digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    net_sha256("", 0U, digest);
    CHECK(hex_equals(digest, sizeof digest, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    net_sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56U, digest);
    CHECK(hex_equals(digest, sizeof digest, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    net_sha256_t ctx;
    net_sha256_init(&ctx);
    uint8_t chunk[1000];
    memset(chunk, 'a', sizeof chunk);
    for (size_t i = 0U; i < 1000U; ++i) { net_sha256_update(&ctx, chunk, sizeof chunk); }
    net_sha256_final(&ctx, digest);
    CHECK(hex_equals(digest, sizeof digest, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    /* Streaming in odd pieces must equal one shot. */
    net_sha256_init(&ctx);
    net_sha256_update(&ctx, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 3U);
    net_sha256_update(&ctx, "dbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 53U);
    net_sha256_final(&ctx, digest);
    CHECK(hex_equals(digest, sizeof digest, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));

    uint8_t mac[NET_SHA256_SIZE];
    uint8_t key[131];
    memset(key, 0x0bU, 20U);
    net_hmac_sha256(key, 20U, "Hi There", 8U, mac);
    CHECK(hex_equals(mac, sizeof mac, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
    net_hmac_sha256("Jefe", 4U, "what do ya want for nothing?", 28U, mac);
    CHECK(hex_equals(mac, sizeof mac, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
    memset(key, 0xaaU, sizeof key);
    net_hmac_sha256(key, sizeof key, "Test Using Larger Than Block-Size Key - Hash Key First", 54U, mac);
    CHECK(hex_equals(mac, sizeof mac, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));

    uint8_t other[NET_SHA256_SIZE];
    memcpy(other, mac, sizeof other);
    CHECK(net_equal_constant_time(mac, other, sizeof mac));
    other[31] ^= 1U;
    CHECK(!net_equal_constant_time(mac, other, sizeof mac));
    CHECK(net_equal_constant_time(mac, other, 0U));

    uint8_t buffer[8];
    net_writer_t writer;
    net_writer_init(&writer, buffer, sizeof buffer);
    net_write_u64(&writer, 0x0102030405060708ULL);
    CHECK(writer.ok && writer.pos == 8U && buffer[0] == 8U && buffer[7] == 1U);
    net_reader_t reader;
    net_reader_init(&reader, buffer, sizeof buffer);
    CHECK(net_read_u64(&reader) == 0x0102030405060708ULL && net_reader_complete(&reader));
    net_reader_init(&reader, buffer, 7U);
    CHECK(net_read_u64(&reader) == 0U && !reader.ok);
}

/* A venue behind one NAT reconnects every seat at once (a room restart
   sends them all back together): sockets from one address that have not
   yet said HELLO wait their turn instead of being refused as full. */
static void test_one_address_reconnects(const net_ws_server_config_t *base) {
    server_log_t log = {0};
    net_ws_server_config_t config = *base;
    config.max_clients = 8U;
    config.handshake_timeout_ms = 5000U;
    config.user = &log;
    net_ws_server_t *server = net_ws_server_create(&config);
    CHECK(server != NULL);
    log.server = server;
    raw_peer_t peers[5];
    raw_peers_open(peers, 5U, net_ws_server_port(server), INADDR_LOOPBACK);
    CHECK(raw_peers_pump(server, peers, 5U, WAIT_S, true));
    raw_peers_pump(server, peers, 5U, 0.3, false);
    for (uint32_t index = 0U; index < 5U; ++index) {
        CHECK(raw_peer_close_code(peers[index].response, peers[index].got) == 0U);
        CHECK(raw_peer_send_hello(peers[index].sock, VERSION));
    }
    pump_until(server, NULL, log.connects == 5U);
    CHECK(log.connects == 5U && net_ws_server_client_count(server) == 5U);
    raw_peers_close(peers, 5U);
    net_ws_server_destroy(server);
}

/* One address holds at most max_clients of the 2 x max_clients waiting
   room: its next socket is refused while the room still has space, a
   second address fills the rest, a third is refused, and silent sockets
   leave at the handshake timeout. */
static void test_waiting_room_bound(const net_ws_server_config_t *base) {
    if (!loopback_alias_available(INADDR_LOOPBACK + 1U) || !loopback_alias_available(INADDR_LOOPBACK + 2U)) {
        printf("waiting-room bound skipped: no 127.0.0.2/127.0.0.3 on this host\n");
        return;
    }
    enum { TIMEOUT_MS = 6000 };
    server_log_t log = {0};
    net_ws_server_config_t config = *base;
    config.max_clients = 4U;
    /* Longer than every setup wait below, so no silent socket expires
       before the last refusal is checked. */
    config.handshake_timeout_ms = TIMEOUT_MS;
    config.user = &log;
    net_ws_server_t *server = net_ws_server_create(&config);
    CHECK(server != NULL);
    log.server = server;
    const uint16_t port = net_ws_server_port(server);
    /* A's five and B's three spend the accept burst (2 x max_clients); B's
       fourth and C each wait for a token, which refills every 1 / max_clients
       seconds. */
    raw_peer_t peers[10];
    raw_peers_open(peers, 5U, port, INADDR_LOOPBACK);
    raw_peers_open(peers + 5, 3U, port, INADDR_LOOPBACK + 1U);
    CHECK(raw_peers_pump(server, peers, 8U, WAIT_S, true));
    raw_peers_pump(server, peers, 8U, 0.4, false);
    raw_peers_open(peers + 8, 1U, port, INADDR_LOOPBACK + 1U);
    CHECK(raw_peers_pump(server, peers, 9U, WAIT_S, true));
    raw_peers_pump(server, peers, 9U, 0.4, false);
    raw_peers_open(peers + 9, 1U, port, INADDR_LOOPBACK + 2U);
    CHECK(raw_peers_pump(server, peers, 10U, WAIT_S, true));
    raw_peers_pump(server, peers, 10U, 0.3, false);
    for (uint32_t index = 0U; index < 9U; ++index) {
        const uint16_t want = index == 4U ? NET_CLOSE_FULL : 0U;
        CHECK(raw_peer_close_code(peers[index].response, peers[index].got) == want);
    }
    CHECK(raw_peer_close_code(peers[9].response, peers[9].got) == NET_CLOSE_FULL);
    /* The handshake timeout, one sweep, and slack. */
    pump_for(server, NULL, TIMEOUT_MS / 1000.0 + 0.25 + 0.5);
    client_log_t clog = {0};
    net_ws_client_t *client = connect_client(port, VERSION, &clog, 256U);
    pump_open(server, &log, client, &clog);
    CHECK(clog.closes == 0U);
    net_ws_client_destroy(client);
    raw_peers_close(peers, 10U);
    net_ws_server_destroy(server);
}

/* A full room of real clients dialing from one address at the same
   instant all get in. */
static void test_burst_enters(const net_ws_server_config_t *base) {
    enum { BURST = 32 };
    server_log_t log = {0};
    net_ws_server_config_t config = *base;
    config.max_clients = BURST;
    config.user = &log;
    net_ws_server_t *server = net_ws_server_create(&config);
    CHECK(server != NULL);
    log.server = server;
    char url[64];
    snprintf(url, sizeof url, "ws://127.0.0.1:%u/", (unsigned)net_ws_server_port(server));
    client_log_t logs[BURST];
    net_ws_client_t *clients[BURST];
    memset(logs, 0, sizeof logs);
    for (uint32_t index = 0U; index < BURST; ++index) {
        const net_ws_client_config_t cconfig = {
            .url = url, .protocol_version = VERSION, .max_message_bytes = MAX_MESSAGE,
            .receive_queue_bytes = 1024U, .send_queue_bytes = 256U, .user = &logs[index],
            .on_open = client_open, .on_message = client_message, .on_close = client_close,
        };
        clients[index] = net_ws_client_create(&cconfig);
        CHECK(clients[index] != NULL);
    }
    uint32_t opens = 0U;
    uint32_t closes = 0U;
    const double end = net_ws_client_clock() + 10.0;
    while ((opens < BURST || log.connects < BURST) && closes == 0U && net_ws_client_clock() < end) {
        net_ws_server_service(server, 1U);
        opens = 0U;
        closes = 0U;
        for (uint32_t index = 0U; index < BURST; ++index) {
            net_ws_client_service(clients[index], 0U);
            opens += logs[index].opens;
            closes += logs[index].closes;
        }
    }
    CHECK(opens == BURST && closes == 0U && log.connects == BURST);
    CHECK(net_ws_server_client_count(server) == BURST);
    for (uint32_t index = 0U; index < BURST; ++index) { net_ws_client_destroy(clients[index]); }
    net_ws_server_destroy(server);
}

int main(void) {
    test_codec();
    test_quantized();
    test_hmac();

    server_log_t slog = {0};
    const net_ws_server_config_t sconfig = {
        .bind_address = "127.0.0.1", .port = 0U, .max_clients = 2U, .handshake_timeout_ms = 1000U,
        .ping_idle_s = 1U, .hangup_idle_s = 3U,
        .max_message_bytes = MAX_MESSAGE, .send_queue_bytes = 256U,
        .max_messages_per_second = 200U, .protocol_version = VERSION, .user = &slog,
        .on_connect = server_connect, .on_message = server_message,
        .on_disconnect = server_disconnect,
    };
    net_ws_server_t *server = net_ws_server_create(&sconfig);
    CHECK(server != NULL);
    slog.server = server;
    slog.echo = true;
    const uint16_t port = net_ws_server_port(server);
    CHECK(port != 0U);

    /* Echo round trip. */
    client_log_t clog = {0};
    net_ws_client_t *client = connect_client(port, VERSION, &clog, 256U);
    pump_until(server, client, (clog.opens == 1U && slog.connects == 1U && net_ws_server_client_count(server) == 1U));
    CHECK(clog.opens == 1U && slog.connects == 1U && net_ws_server_client_count(server) == 1U);
    /* The HELLO ticket reaches the application with the connect. */
    CHECK(slog.last_ticket_size == 4U && memcmp(slog.last_ticket, "seat", 4U) == 0);
    const uint8_t ping[] = {NET_MSG_APP_FIRST, 1U, 2U, 3U};
    CHECK(net_ws_client_send(client, ping, sizeof ping));
    pump_until(server, client, (slog.messages == 1U && clog.messages == 1U) && (clog.last_size == sizeof ping && memcmp(clog.last, ping, sizeof ping) == 0));
    CHECK(slog.messages == 1U && clog.messages == 1U);
    CHECK(clog.last_size == sizeof ping && memcmp(clog.last, ping, sizeof ping) == 0);
    /* The echo is one message each way: HELLO is a frame, not a message. */
    net_ws_server_stats_t stats;
    net_ws_server_take_stats(server, &stats);
    CHECK(stats.rx_messages == 1U && stats.rx_bytes == sizeof ping && stats.rx_frames == 2U);
    CHECK(stats.tx_messages == 1U && stats.tx_bytes == sizeof ping && stats.tx_writes == 1U && stats.tx_refused == 0U);
    CHECK(stats.queue_peak_bytes == sizeof ping + 4U && stats.services > 0U && stats.service_passes >= stats.services);
    net_ws_server_take_stats(server, &stats);
    CHECK(stats.queue_peak_bytes == 0U && stats.rx_messages == 1U);
    CHECK(strlen(net_ws_library_version()) > 0U && strlen(net_ws_tls_library_version()) > 0U);
    /* Stamped on arrival: after the send, before this service pass. */
    CHECK(clog.last_received_at > 0.0 && clog.last_received_at <= net_ws_client_clock());

    /* A settled connection lets the caller defer its pass; output the socket
       has not taken, held by it or still queued, does not. */
    CHECK(!net_ws_server_needs_events(server));
    net_ws_server_test_partial_buffered(server, true);
    CHECK(net_ws_server_needs_events(server));
    net_ws_server_test_partial_buffered(server, false);
    net_ws_server_test_hold_writes(server, true);
    CHECK(net_ws_server_send(server, slog.last_client, ping, sizeof ping));
    CHECK(net_ws_server_needs_events(server));
    net_ws_server_test_hold_writes(server, false);
    CHECK(net_ws_server_send(server, slog.last_client, ping, sizeof ping));
    pump_until(server, client, (clog.messages == 3U && !net_ws_server_needs_events(server)));
    CHECK(clog.messages == 3U && !net_ws_server_needs_events(server));

    /* An application close delivers what was queued before it, then the
       application's own code. */
    const uint8_t farewell[] = {NET_MSG_APP_FIRST, 9U};
    CHECK(net_ws_server_send(server, slog.last_client, farewell, sizeof farewell));
    net_ws_server_close(server, slog.last_client, 4123U);
    pump_until(server, client, (clog.closes == 1U && clog.close_code == 4123U) && (clog.messages == 4U && clog.last_size == sizeof farewell && clog.last[1] == 9U) && (slog.disconnects == 1U && slog.last_reason == NET_WS_CLOSE_APP) && (net_ws_server_client_count(server) == 0U));
    CHECK(clog.closes == 1U && clog.close_code == 4123U);
    CHECK(clog.messages == 4U && clog.last_size == sizeof farewell && clog.last[1] == 9U);
    CHECK(slog.disconnects == 1U && slog.last_reason == NET_WS_CLOSE_APP);
    CHECK(net_ws_server_client_count(server) == 0U);
    CHECK(!net_ws_server_send(server, slog.last_client, ping, 0U));
    net_ws_client_destroy(client);

    /* A zero message cap and service cap preserve the former byte-only,
       all-queued-callback behavior. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_limits(port, VERSION, &clog, 0U, 0U);
    pump_open(server, &slog, client, &clog);
    send_numbered(server, slog.last_client, 17U);
    pump_until(server, NULL, net_ws_client_test_queued_messages(client) == 17U);
    net_ws_client_service(client, 0U);
    CHECK(clog.messages == 17U && clog.last[1] == 16U && clog.closes == 0U);
    net_ws_client_destroy(client);
    pump_until(server, NULL, net_ws_server_client_count(server) == 0U);

    /* A bounded service pass leaves the FIFO tail for later frames. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_limits(port, VERSION, &clog, 64U, 16U);
    pump_open(server, &slog, client, &clog);
    send_numbered(server, slog.last_client, 64U);
    pump_until(server, NULL, net_ws_client_test_queued_messages(client) == 64U);
    for (uint32_t batch = 1U; batch <= 4U; ++batch) {
        net_ws_client_service(client, 0U);
        CHECK(clog.messages == batch * 16U && clog.last[1] == batch * 16U - 1U);
        CHECK(clog.closes == 0U);
    }
    net_ws_client_destroy(client);
    pump_until(server, NULL, net_ws_server_client_count(server) == 0U);

    /* The 65th tiny message reaches the independent record cap. The valid
       64-message prefix still arrives before close, including a destroy in
       that delayed close callback. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_limits(port, VERSION, &clog, 64U, 16U);
    pump_open(server, &slog, client, &clog);
    send_numbered(server, slog.last_client, 65U);
    pump_until(server, NULL, net_ws_client_test_queued_messages(client) == 64U &&
        net_ws_client_state(client) == NET_WS_CLIENT_CLOSED);
    for (uint32_t batch = 1U; batch <= 4U; ++batch) {
        net_ws_client_service(client, 0U);
        CHECK(clog.messages == batch * 16U && clog.last[1] == batch * 16U - 1U);
        CHECK(clog.closes == 0U);
    }
    clog.destroy_on_close = client;
    net_ws_client_service(client, 0U);
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW && clog.destroy_on_close == NULL);
    pump_until(server, NULL, net_ws_server_client_count(server) == 0U);

    /* A terminal slow close overrides an application close before its
       queued output can become visible. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump_open(server, &slog, client, &clog);
    const uint32_t slow_client = slog.last_client;
    const uint32_t slow_disconnects = slog.disconnects;
    CHECK(net_ws_server_send_ready(server, slow_client));
    net_ws_server_test_hold_writes(server, true);
    CHECK(net_ws_server_send(server, slow_client, farewell, sizeof farewell));
    CHECK(net_ws_server_queued_bytes(server, slow_client) > 0U);
    net_ws_server_close(server, slow_client, 4123U);
    CHECK(net_ws_server_try_close_slow(server, slow_client));
    CHECK(!net_ws_server_try_close_slow(server, slow_client));
    net_ws_server_test_hold_writes(server, false);
    CHECK(!net_ws_server_send_ready(server, slow_client));
    CHECK(!net_ws_server_send(server, slow_client, ping, sizeof ping));
    CHECK(!net_ws_server_send_latest(server, slow_client, ping, sizeof ping, NULL));
    pump_until(server, client, (clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW) && (clog.messages == 0U) && (slog.disconnects == slow_disconnects + 1U && slog.last_reason == NET_WS_CLOSE_SLOW));
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW);
    CHECK(clog.messages == 0U);
    CHECK(slog.disconnects == slow_disconnects + 1U && slog.last_reason == NET_WS_CLOSE_SLOW);
    net_ws_client_destroy(client);

    /* A write error is synchronous admission failure and leaves the stream
       terminal before the later close lifecycle callback. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump_open(server, &slog, client, &clog);
    const uint32_t failed_client = slog.last_client;
    const uint32_t failed_disconnects = slog.disconnects;
    CHECK(net_ws_server_send_ready(server, failed_client));
    net_ws_server_test_partial_buffered(server, true);
    CHECK(!net_ws_server_send_ready(server, failed_client));
    net_ws_server_test_partial_buffered(server, false);
    CHECK(net_ws_server_send_ready(server, failed_client));
    net_ws_server_test_fail_next_write(server);
    CHECK(!net_ws_server_send(server, failed_client, ping, sizeof ping));
    CHECK(!net_ws_server_try_close_slow(server, failed_client));
    CHECK(!net_ws_server_send_ready(server, failed_client));
    CHECK(!net_ws_server_send(server, failed_client, ping, sizeof ping));
    CHECK(!net_ws_server_send_latest(server, failed_client, ping, sizeof ping, NULL));
    pump_until(server, client, (clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW) && (slog.disconnects == failed_disconnects + 1U && slog.last_reason == NET_WS_CLOSE_SLOW));
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW);
    CHECK(slog.disconnects == failed_disconnects + 1U && slog.last_reason == NET_WS_CLOSE_SLOW);
    net_ws_client_destroy(client);

    /* Admission can succeed before a later writable callback fails. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump_open(server, &slog, client, &clog);
    const uint32_t delayed_client = slog.last_client;
    net_ws_server_test_hold_writes(server, true);
    CHECK(net_ws_server_send(server, delayed_client, ping, sizeof ping));
    net_ws_server_test_fail_next_write(server);
    net_ws_server_test_hold_writes(server, false);
    pump_until(server, client, (clog.messages == 0U && clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW) && (!net_ws_server_send_ready(server, delayed_client)));
    CHECK(clog.messages == 0U && clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW);
    CHECK(!net_ws_server_send_ready(server, delayed_client));
    CHECK(!net_ws_server_send(server, delayed_client, ping, sizeof ping));
    net_ws_client_destroy(client);

    /* A failing drain escalates a pending application close to SLOW. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump_open(server, &slog, client, &clog);
    const uint32_t app_failed_client = slog.last_client;
    net_ws_server_test_hold_writes(server, true);
    CHECK(net_ws_server_send(server, app_failed_client, farewell, sizeof farewell));
    net_ws_server_close(server, app_failed_client, 4123U);
    net_ws_server_test_fail_next_write(server);
    net_ws_server_test_hold_writes(server, false);
    pump_until(server, client, (clog.messages == 0U && clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW));
    CHECK(clog.messages == 0U && clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW);
    net_ws_client_destroy(client);

    /* Destroying the client from inside on_close is safe. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    clog.destroy_on_close = client;
    pump_open(server, &slog, client, &clog);
    net_ws_server_close(server, slog.last_client, NET_CLOSE_APP);
    pump_until(server, client, clog.closes != 0U);
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_APP && clog.destroy_on_close == NULL);
    pump_until(server, NULL, net_ws_server_client_count(server) == 0U);

    /* A refused connection reports through on_close, never from create. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_sized(1U, VERSION, &clog, 256U, MAX_MESSAGE);
    CHECK(clog.closes == 0U);
    {
        /* The client's service returns at once; the refusal comes from its thread. */
        const double started = net_ws_client_clock();
        while (clog.closes == 0U && net_ws_client_clock() - started < 5.0) { net_ws_client_service(client, 0U); }
    }
    CHECK(clog.closes == 1U && clog.close_code == 0U);
    CHECK(net_ws_client_state(client) == NET_WS_CLIENT_CLOSED);
    net_ws_client_destroy(client);

    /* A message above the client's own limit closes with FORMAT. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_sized(port, VERSION, &clog, 256U, 8U);
    pump_open(server, &slog, client, &clog);
    const uint32_t format_disconnects = slog.disconnects;
    uint8_t nine[9] = {NET_MSG_APP_FIRST};
    CHECK(net_ws_server_send(server, slog.last_client, nine, sizeof nine));
    pump_until(server, client, (clog.closes == 1U && clog.close_code == NET_CLOSE_FORMAT) && (slog.disconnects == format_disconnects + 1U && slog.last_reason == NET_WS_CLOSE_PEER));
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_FORMAT);
    CHECK(slog.last_reason == NET_WS_CLOSE_PEER);
    net_ws_client_destroy(client);

    /* A ticketless HELLO reaches on_connect with an empty ticket. */
    memset(&clog, 0, sizeof clog);
    {
        char url[64];
        snprintf(url, sizeof url, "ws://127.0.0.1:%u/", (unsigned)port);
        const net_ws_client_config_t bare = {
            .url = url, .protocol_version = VERSION, .max_message_bytes = MAX_MESSAGE,
            .receive_queue_bytes = 1024U, .send_queue_bytes = NET_HELLO_BASE_SIZE + 4U, .user = &clog,
            .on_open = client_open, .on_message = client_message, .on_close = client_close,
        };
        pace_dial();
        client = net_ws_client_create(&bare);
        CHECK(client != NULL);
        slog.last_ticket_size = 99U;
        pump_until(server, client, (clog.opens == 1U && slog.last_ticket_size == 0U));
        CHECK(clog.opens == 1U && slog.last_ticket_size == 0U);
        net_ws_client_destroy(client);
        pump_until(server, NULL, net_ws_server_client_count(server) == 0U);
    }

    /* Version mismatch is refused before on_connect. */
    memset(&clog, 0, sizeof clog);
    const uint32_t connects_before = slog.connects;
    const uint32_t disconnects_before = slog.disconnects;
    client = connect_client(port, VERSION + 1U, &clog, 256U);
    pump_until(server, client, (clog.closes == 1U && clog.close_code == NET_CLOSE_VERSION) && (slog.connects == connects_before && slog.disconnects == disconnects_before));
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_VERSION);
    CHECK(slog.connects == connects_before && slog.disconnects == disconnects_before);
    net_ws_client_destroy(client);

    /* Oversize and reserved-type messages close the session, not the room. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump_open(server, &slog, client, &clog);
    uint8_t big[MAX_MESSAGE + 1U];
    memset(big, NET_MSG_APP_FIRST, sizeof big);
    CHECK(net_ws_client_send(client, big, sizeof big));
    pump_until(server, client, (clog.closes == 1U && clog.close_code == NET_CLOSE_FORMAT) && (slog.last_reason == NET_WS_CLOSE_PROTOCOL));
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_FORMAT);
    CHECK(slog.last_reason == NET_WS_CLOSE_PROTOCOL);
    net_ws_client_destroy(client);

    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump_open(server, &slog, client, &clog);
    const uint8_t reserved[] = {NET_MSG_HELLO, 0U};
    CHECK(net_ws_client_send(client, reserved, sizeof reserved));
    pump_until(server, client, (clog.closes == 1U && clog.close_code == NET_CLOSE_FORMAT));
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_FORMAT);
    net_ws_client_destroy(client);

    /* Flooding past the per-second budget closes the session: a budget of
       one message per second makes the second message the overdraw. */
    server_log_t rlog = {0};
    net_ws_server_config_t rconfig = sconfig;
    rconfig.max_messages_per_second = 1U;
    rconfig.user = &rlog;
    net_ws_server_t *rated = net_ws_server_create(&rconfig);
    CHECK(rated != NULL);
    rlog.server = rated;
    memset(&clog, 0, sizeof clog);
    client = connect_client(net_ws_server_port(rated), VERSION, &clog, 256U);
    pump_until(rated, client, (clog.opens == 1U));
    CHECK(clog.opens == 1U);
    CHECK(net_ws_client_send(client, ping, sizeof ping));
    CHECK(net_ws_client_send(client, ping, sizeof ping));
    pump_until(rated, client, (clog.closes == 1U && clog.close_code == NET_CLOSE_RATE) && (rlog.last_reason == NET_WS_CLOSE_RATE && rlog.messages == 1U));
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_RATE);
    CHECK(rlog.last_reason == NET_WS_CLOSE_RATE && rlog.messages == 1U);
    net_ws_client_destroy(client);
    net_ws_server_destroy(rated);

    /* A client that cannot drain its queue is dropped as slow: its own
       receive queue is the first to fill when its game thread never
       services it, and the server, once it has heard the close, refuses
       to send to it. (A send writes straight to the socket, so the
       server's own queue only fills once the socket is choked, which a
       loopback peer that reads never lets happen.) */
    slog.echo = false;
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump_open(server, &slog, client, &clog);
    bool refused = false;
    for (int index = 0; index < 50000 && !refused; ++index) {
        refused = !net_ws_server_send(server, slog.last_client, big, MAX_MESSAGE);
        if (index % 100 == 99) { net_ws_server_service(server, 1U); }
    }
    CHECK(refused);
    /* Servicing the client here would drain the receive queue whose
       overflow this case is about; the close replays afterwards. */
    pump_until(server, NULL, net_ws_client_state(client) == NET_WS_CLIENT_CLOSED &&
        net_ws_server_client_count(server) == 0U);
    net_ws_client_service(client, 0U);
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW);
    CHECK(slog.last_reason == NET_WS_CLOSE_PEER || slog.last_reason == NET_WS_CLOSE_SLOW);
    net_ws_client_destroy(client);

    /* A peer that never answers pings is dropped. */
    {
        net_ws_server_config_t lconfig = sconfig;
        lconfig.ping_idle_s = 1U;
        lconfig.hangup_idle_s = 2U;
        server_log_t llog = {0};
        lconfig.user = &llog;
        net_ws_server_t *live = net_ws_server_create(&lconfig);
        CHECK(live != NULL);
        llog.server = live;
        raw_socket_t quiet = raw_peer_connect(net_ws_server_port(live));
        CHECK(quiet != RAW_INVALID);
        bool upgraded = false;
        char response[512];
        int got = 0;
        wait_until(upgraded, {
            net_ws_server_service(live, 1U);
            upgraded = raw_peer_upgraded(quiet, response, sizeof response, &got);
        });
        CHECK(upgraded && raw_peer_send_hello(quiet, VERSION));
        pump_until(live, NULL, (llog.connects == 1U));
        CHECK(llog.connects == 1U);
        /* Service calls return early on any event, so count time, not rounds. */
        const double started = net_ws_client_clock();
        while (llog.disconnects == 0U && net_ws_client_clock() - started < 6.0) { net_ws_server_service(live, 50U); }
        CHECK(llog.disconnects == 1U && llog.last_reason == NET_WS_CLOSE_PEER);
        raw_close(quiet);
        net_ws_server_destroy(live);
        /* A pong window of zero is refused, not silently widened. */
        lconfig.hangup_idle_s = 1U;
        CHECK(net_ws_server_create(&lconfig) == NULL);
    }

    /* Sockets that close before their upgrade give back their address's
       share: more of them than one address may hold at once still leave it
       free to enter. */
    {
        server_log_t plog = {0};
        net_ws_server_config_t pconfig = sconfig;
        pconfig.user = &plog;
        net_ws_server_t *plain = net_ws_server_create(&pconfig);
        CHECK(plain != NULL);
        plog.server = plain;
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(net_ws_server_port(plain));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        /* Past the per-address cap (2 x max_clients), paced under the
           accept budget (max_clients a second). */
        for (uint32_t index = 0U; index < 2U * pconfig.max_clients + 2U; ++index) {
            raw_socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
            CHECK(sock != RAW_INVALID && connect(sock, (struct sockaddr *)&addr, sizeof addr) == 0);
            pump_for(plain, NULL, 0.1);
            raw_close(sock);
            const double closed_at = net_ws_client_clock();
            while (net_ws_client_clock() - closed_at < 0.6) { net_ws_server_service(plain, 50U); }
        }
        /* The accept budget refills; only a leaked share could still refuse. */
        const double settled_at = net_ws_client_clock();
        while (net_ws_client_clock() - settled_at < 2.5) { net_ws_server_service(plain, 50U); }
        client_log_t elog = {0};
        net_ws_client_t *entrant = connect_client(net_ws_server_port(plain), VERSION, &elog, 256U);
        const double dialed_at = net_ws_client_clock();
        while (elog.opens == 0U && elog.closes == 0U && net_ws_client_clock() - dialed_at < 8.0) { pump(plain, entrant); }
        pump_for(plain, entrant, 0.2);
        CHECK(elog.opens == 1U && elog.closes == 0U && plog.connects == 1U);
        net_ws_client_destroy(entrant);
        pump_until(plain, NULL, net_ws_server_client_count(plain) == 0U);
        net_ws_server_destroy(plain);
    }

    test_one_address_reconnects(&sconfig);
    test_waiting_room_bound(&sconfig);
    test_burst_enters(&sconfig);

    /* Room capacity is enforced per connection. */
    client_log_t alog = {0};
    client_log_t blog = {0};
    client_log_t clog2 = {0};
    net_ws_client_t *a = connect_client(port, VERSION, &alog, 256U);
    net_ws_client_t *b = connect_client(port, VERSION, &blog, 256U);
    wait_until(alog.opens == 1U && blog.opens == 1U && net_ws_server_client_count(server) == 2U, {
        pump(server, a);
        net_ws_client_service(b, 0U);
    });
    CHECK(net_ws_server_client_count(server) == 2U);
    net_ws_client_t *c = connect_client(port, VERSION, &clog2, 256U);
    wait_until(clog2.closes == 1U, {
        pump(server, a);
        net_ws_client_service(b, 0U);
        net_ws_client_service(c, 0U);
    });
    CHECK(clog2.closes == 1U && clog2.close_code == NET_CLOSE_FULL);
    /* The refusal must not have closed the seated clients: watch them a while. */
    const double watch_until = net_ws_client_clock() + 0.2;
    while (net_ws_client_clock() < watch_until) {
        pump(server, a);
        net_ws_client_service(b, 0U);
    }
    CHECK(alog.closes == 0U && blog.closes == 0U);
    net_ws_client_destroy(c);
    net_ws_client_destroy(a);
    net_ws_client_destroy(b);
    pump_until(server, NULL, net_ws_server_client_count(server) == 0U);
    CHECK(net_ws_server_client_count(server) == 0U);

    net_ws_server_destroy(server);
    printf("network-core roundtrip ok\n");
    return 0;
}
