/* Minimal second consumer of network-core: an echo server and a native client
   in one process, pumped alternately from one thread. Proves the handshake,
   echo, and every rejection path the transport promises. */
#include "net_codec.h"
#include "net_ws_client.h"
#include "net_ws_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) \
    do { if (!(condition)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

#define VERSION 7U
#define MAX_MESSAGE 64U

typedef struct server_log_t {
    net_ws_server_t *server;
    uint32_t connects;
    uint32_t disconnects;
    uint32_t last_client;
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
    net_ws_client_t *destroy_on_close; /* the natural game pattern: free in on_close */
} client_log_t;

static void server_connect(void *user, uint32_t client) {
    server_log_t *log = (server_log_t *)user;
    log->connects += 1U;
    log->last_client = client;
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

static void client_message(void *user, const uint8_t *data, size_t size) {
    client_log_t *log = (client_log_t *)user;
    log->messages += 1U;
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
    const net_ws_client_config_t config = {
        .url = url, .protocol_version = version, .max_message_bytes = max_message,
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

    uint8_t hello[NET_HELLO_SIZE];
    net_writer_init(&writer, hello, sizeof hello);
    net_hello_encode(&writer, 42U);
    uint32_t version = 0U;
    CHECK(writer.ok && net_hello_decode(hello, sizeof hello, &version) && version == 42U);
    CHECK(!net_hello_decode(hello, sizeof hello - 1U, &version));
    hello[1] ^= 0xFFU;
    CHECK(!net_hello_decode(hello, sizeof hello, &version));
}

int main(void) {
    test_codec();

    server_log_t slog = {0};
    const net_ws_server_config_t sconfig = {
        .bind_address = "127.0.0.1", .port = 0U, .max_clients = 2U, .handshake_timeout_ms = 1000U,
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
    const uint8_t ping[] = {NET_MSG_APP_FIRST, 1U, 2U, 3U};
    CHECK(net_ws_client_send(client, ping, sizeof ping));
    pump(server, client, 50);
    CHECK(slog.messages == 1U && clog.messages == 1U);
    CHECK(clog.last_size == sizeof ping && memcmp(clog.last, ping, sizeof ping) == 0);

    /* An application close delivers what was queued before it, then the
       application's own code. */
    const uint8_t farewell[] = {NET_MSG_APP_FIRST, 9U};
    CHECK(net_ws_server_send(server, slog.last_client, farewell, sizeof farewell));
    CHECK(net_ws_server_queued_bytes(server, slog.last_client) == sizeof farewell + 4U);
    net_ws_server_close(server, slog.last_client, 4123U);
    pump(server, client, 50);
    CHECK(clog.closes == 1U && clog.close_code == 4123U);
    CHECK(clog.messages == 2U && clog.last_size == sizeof farewell && clog.last[1] == 9U);
    CHECK(slog.disconnects == 1U && slog.last_reason == NET_WS_CLOSE_APP);
    CHECK(net_ws_server_client_count(server) == 0U);
    CHECK(!net_ws_server_send(server, slog.last_client, ping, 0U));
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
    for (int round = 0; round < 200 && clog.closes == 0U; ++round) { net_ws_client_service(client, 5U); }
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

    /* A client that cannot drain its queue is dropped as slow. */
    slog.echo = false;
    memset(&clog, 0, sizeof clog);
    client = connect_client(port, VERSION, &clog, 256U);
    pump(server, client, 50);
    bool overflowed = false;
    for (int index = 0; index < 100 && !overflowed; ++index) {
        overflowed = !net_ws_server_send(server, slog.last_client, big, MAX_MESSAGE);
    }
    CHECK(overflowed);
    pump(server, client, 50);
    CHECK(slog.last_reason == NET_WS_CLOSE_SLOW && clog.closes == 1U);
    CHECK(clog.close_code == NET_CLOSE_SLOW);
    net_ws_client_destroy(client);

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
