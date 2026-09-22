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
#include <time.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET raw_socket_t;
#define RAW_INVALID INVALID_SOCKET
#define raw_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int raw_socket_t;
#define RAW_INVALID (-1)
#define raw_close close
#endif

#define CHECK(condition) \
    do { if (!(condition)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

#define VERSION 7U
#define MAX_MESSAGE 64U

/* The native backend owns this deterministic failure seam; it is not a
   transport API and keeps lws write-error coverage off socket timing. */
void net_ws_server_test_fail_next_write(net_ws_server_t *server);
void net_ws_server_test_hold_writes(net_ws_server_t *server, bool hold);
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
static raw_socket_t raw_peer_connect(uint16_t port) {
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    raw_socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == RAW_INVALID) { return RAW_INVALID; }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
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

static void pump(net_ws_server_t *server, net_ws_client_t *client, int rounds) {
    for (int round = 0; round < rounds; ++round) {
        net_ws_server_service(server, 5U);
        if (client != NULL) { net_ws_client_service(client, 5U); }
    }
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
    CHECK(writer.ok && writer.pos == 2U + 1U + 4U + 2U);
    /* -19.5 + 21760 / 1024 == 1.75: two bytes, little-endian. */
    CHECK(buffer[0] == 0x00U && buffer[1] == 0x55U && buffer[2] == 0xFFU);

    net_reader_t reader;
    net_reader_init(&reader, buffer, writer.pos);
    CHECK(net_read_quantized(&reader, metres) == 1.75F);
    CHECK(net_read_quantized(&reader, bytes) == 63.75F);
    CHECK(fabsf(net_read_quantized(&reader, wide) - 0.5F) <= 1e-6F);
    CHECK(fabsf(net_read_angle16(&reader) - 3.0F) <= 3.14159F / 65536.0F);
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
    pump(server, client, 50);
    CHECK(clog.opens == 1U && slog.connects == 1U && net_ws_server_client_count(server) == 1U);
    /* The HELLO ticket reaches the application with the connect. */
    CHECK(slog.last_ticket_size == 4U && memcmp(slog.last_ticket, "seat", 4U) == 0);
    const uint8_t ping[] = {NET_MSG_APP_FIRST, 1U, 2U, 3U};
    CHECK(net_ws_client_send(client, ping, sizeof ping));
    pump(server, client, 50);
    CHECK(slog.messages == 1U && clog.messages == 1U);
    CHECK(clog.last_size == sizeof ping && memcmp(clog.last, ping, sizeof ping) == 0);
    /* Stamped on arrival: after the send, before this service pass. */
    CHECK(clog.last_received_at > 0.0 && clog.last_received_at <= net_ws_client_clock());

    /* An application close delivers what was queued before it, then the
       application's own code. */
    const uint8_t farewell[] = {NET_MSG_APP_FIRST, 9U};
    CHECK(net_ws_server_send(server, slog.last_client, farewell, sizeof farewell));
    net_ws_server_close(server, slog.last_client, 4123U);
    pump(server, client, 50);
    CHECK(clog.closes == 1U && clog.close_code == 4123U);
    CHECK(clog.messages == 2U && clog.last_size == sizeof farewell && clog.last[1] == 9U);
    CHECK(slog.disconnects == 1U && slog.last_reason == NET_WS_CLOSE_APP);
    CHECK(net_ws_server_client_count(server) == 0U);
    CHECK(!net_ws_server_send(server, slog.last_client, ping, 0U));
    net_ws_client_destroy(client);

    /* A zero message cap and service cap preserve the former byte-only,
       all-queued-callback behavior. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_limits(port, VERSION, &clog, 0U, 0U);
    pump(server, client, 50);
    send_numbered(server, slog.last_client, 17U);
    pump(server, NULL, 50);
    net_ws_client_service(client, 0U);
    CHECK(clog.messages == 17U && clog.last[1] == 16U && clog.closes == 0U);
    net_ws_client_destroy(client);
    pump(server, NULL, 20);

    /* A bounded service pass leaves the FIFO tail for later frames. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_limits(port, VERSION, &clog, 64U, 16U);
    pump(server, client, 50);
    send_numbered(server, slog.last_client, 64U);
    pump(server, NULL, 50);
    for (uint32_t batch = 1U; batch <= 4U; ++batch) {
        net_ws_client_service(client, 0U);
        CHECK(clog.messages == batch * 16U && clog.last[1] == batch * 16U - 1U);
        CHECK(clog.closes == 0U);
    }
    net_ws_client_destroy(client);
    pump(server, NULL, 20);

    /* The 65th tiny message reaches the independent record cap. The valid
       64-message prefix still arrives before close, including a destroy in
       that delayed close callback. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_limits(port, VERSION, &clog, 64U, 16U);
    pump(server, client, 50);
    send_numbered(server, slog.last_client, 65U);
    pump(server, NULL, 50);
    for (uint32_t batch = 1U; batch <= 4U; ++batch) {
        net_ws_client_service(client, 0U);
        CHECK(clog.messages == batch * 16U && clog.last[1] == batch * 16U - 1U);
        CHECK(clog.closes == 0U);
    }
    clog.destroy_on_close = client;
    net_ws_client_service(client, 0U);
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW && clog.destroy_on_close == NULL);
    pump(server, NULL, 20);

    /* A terminal slow close overrides an application close before its
       queued output can become visible. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump(server, client, 50);
    const uint32_t slow_client = slog.last_client;
    const uint32_t slow_disconnects = slog.disconnects;
    CHECK(net_ws_server_send_ready(server, slow_client));
    net_ws_server_test_hold_writes(server, true);
    CHECK(net_ws_server_send(server, slow_client, farewell, sizeof farewell));
    CHECK(net_ws_server_queued_bytes(server, slow_client) > 0U);
    net_ws_server_close(server, slow_client, 4123U);
    net_ws_server_close_slow(server, slow_client);
    net_ws_server_test_hold_writes(server, false);
    CHECK(!net_ws_server_send_ready(server, slow_client));
    CHECK(!net_ws_server_send(server, slow_client, ping, sizeof ping));
    CHECK(!net_ws_server_send_latest(server, slow_client, ping, sizeof ping, NULL));
    pump(server, client, 50);
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW);
    CHECK(clog.messages == 0U);
    CHECK(slog.disconnects == slow_disconnects + 1U && slog.last_reason == NET_WS_CLOSE_SLOW);
    net_ws_client_destroy(client);

    /* A write error is synchronous admission failure and leaves the stream
       terminal before the later close lifecycle callback. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump(server, client, 50);
    const uint32_t failed_client = slog.last_client;
    const uint32_t failed_disconnects = slog.disconnects;
    CHECK(net_ws_server_send_ready(server, failed_client));
    net_ws_server_test_fail_next_write(server);
    CHECK(!net_ws_server_send(server, failed_client, ping, sizeof ping));
    CHECK(!net_ws_server_send_ready(server, failed_client));
    CHECK(!net_ws_server_send(server, failed_client, ping, sizeof ping));
    CHECK(!net_ws_server_send_latest(server, failed_client, ping, sizeof ping, NULL));
    pump(server, client, 50);
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_SLOW);
    CHECK(slog.disconnects == failed_disconnects + 1U && slog.last_reason == NET_WS_CLOSE_SLOW);
    net_ws_client_destroy(client);

    /* Destroying the client from inside on_close is safe. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    clog.destroy_on_close = client;
    pump(server, client, 50);
    net_ws_server_close(server, slog.last_client, NET_CLOSE_APP);
    for (int round = 0; round < 50 && clog.closes == 0U; ++round) {
        net_ws_server_service(server, 5U);
        net_ws_client_service(client, 5U);
    }
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_APP && clog.destroy_on_close == NULL);
    pump(server, NULL, 10);

    /* A refused connection reports through on_close, never from create. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_sized(1U, VERSION, &clog, 256U, MAX_MESSAGE);
    CHECK(clog.closes == 0U);
    {
        /* The client's service returns at once; the refusal comes from its thread. */
        const time_t started = time(NULL);
        while (clog.closes == 0U && time(NULL) - started < 5) { net_ws_client_service(client, 5U); }
    }
    CHECK(clog.closes == 1U && clog.close_code == 0U);
    CHECK(net_ws_client_state(client) == NET_WS_CLIENT_CLOSED);
    net_ws_client_destroy(client);

    /* A message above the client's own limit closes with FORMAT. */
    memset(&clog, 0, sizeof clog);
    client = connect_client_sized(port, VERSION, &clog, 256U, 8U);
    pump(server, client, 50);
    uint8_t nine[9] = {NET_MSG_APP_FIRST};
    CHECK(net_ws_server_send(server, slog.last_client, nine, sizeof nine));
    pump(server, client, 50);
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
        client = net_ws_client_create(&bare);
        CHECK(client != NULL);
        slog.last_ticket_size = 99U;
        pump(server, client, 50);
        CHECK(clog.opens == 1U && slog.last_ticket_size == 0U);
        net_ws_client_destroy(client);
        pump(server, NULL, 20);
    }

    /* Version mismatch is refused before on_connect. */
    memset(&clog, 0, sizeof clog);
    const uint32_t connects_before = slog.connects;
    const uint32_t disconnects_before = slog.disconnects;
    client = connect_client(port, VERSION + 1U, &clog, 256U);
    pump(server, client, 50);
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_VERSION);
    CHECK(slog.connects == connects_before && slog.disconnects == disconnects_before);
    net_ws_client_destroy(client);

    /* Oversize and reserved-type messages close the session, not the room. */
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump(server, client, 50);
    uint8_t big[MAX_MESSAGE + 1U];
    memset(big, NET_MSG_APP_FIRST, sizeof big);
    CHECK(net_ws_client_send(client, big, sizeof big));
    pump(server, client, 50);
    CHECK(clog.closes == 1U && clog.close_code == NET_CLOSE_FORMAT);
    CHECK(slog.last_reason == NET_WS_CLOSE_PROTOCOL);
    net_ws_client_destroy(client);

    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump(server, client, 50);
    const uint8_t reserved[] = {NET_MSG_HELLO, 0U};
    CHECK(net_ws_client_send(client, reserved, sizeof reserved));
    pump(server, client, 50);
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
    pump(rated, client, 50);
    CHECK(clog.opens == 1U);
    CHECK(net_ws_client_send(client, ping, sizeof ping));
    CHECK(net_ws_client_send(client, ping, sizeof ping));
    pump(rated, client, 50);
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
    pump(server, client, 50);
    bool refused = false;
    for (int index = 0; index < 50000 && !refused; ++index) {
        refused = !net_ws_server_send(server, slog.last_client, big, MAX_MESSAGE);
        if (index % 100 == 99) { net_ws_server_service(server, 1U); }
    }
    CHECK(refused);
    pump(server, client, 50);
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
        for (int round = 0; round < 100 && !upgraded; ++round) {
            net_ws_server_service(live, 5U);
            upgraded = raw_peer_upgraded(quiet, response, sizeof response, &got);
        }
        CHECK(upgraded && raw_peer_send_hello(quiet, VERSION));
        pump(live, NULL, 50);
        CHECK(llog.connects == 1U);
        /* Service calls return early on any event, so count time, not rounds. */
        const time_t started = time(NULL);
        while (llog.disconnects == 0U && time(NULL) - started < 6) { net_ws_server_service(live, 50U); }
        CHECK(llog.disconnects == 1U && llog.last_reason == NET_WS_CLOSE_PEER);
        raw_close(quiet);
        net_ws_server_destroy(live);
        /* A pong window of zero is refused, not silently widened. */
        lconfig.hangup_idle_s = 1U;
        CHECK(net_ws_server_create(&lconfig) == NULL);
    }

    /* Room capacity is enforced per connection. */
    client_log_t alog = {0};
    client_log_t blog = {0};
    client_log_t clog2 = {0};
    net_ws_client_t *a = connect_client(port, VERSION, &alog, 256U);
    net_ws_client_t *b = connect_client(port, VERSION, &blog, 256U);
    for (int round = 0; round < 50; ++round) {
        net_ws_server_service(server, 5U);
        net_ws_client_service(a, 5U);
        net_ws_client_service(b, 5U);
    }
    CHECK(net_ws_server_client_count(server) == 2U);
    net_ws_client_t *c = connect_client(port, VERSION, &clog2, 256U);
    for (int round = 0; round < 50; ++round) {
        net_ws_server_service(server, 5U);
        net_ws_client_service(a, 5U);
        net_ws_client_service(b, 5U);
        net_ws_client_service(c, 5U);
    }
    CHECK(clog2.closes == 1U && clog2.close_code == NET_CLOSE_FULL);
    CHECK(alog.closes == 0U && blog.closes == 0U);
    net_ws_client_destroy(c);
    net_ws_client_destroy(a);
    net_ws_client_destroy(b);
    pump(server, NULL, 20);
    CHECK(net_ws_server_client_count(server) == 0U);

    net_ws_server_destroy(server);
    printf("network-core roundtrip ok\n");
    return 0;
}
