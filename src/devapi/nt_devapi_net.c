#include "devapi/nt_devapi.h"

#if NT_DEVAPI_ENABLED && !defined(__EMSCRIPTEN__)

#include <stdio.h>
#include <string.h>

#include "log/nt_log.h"

#define GAME_DEVAPI_NET_RX_CAP 16384
#define GAME_DEVAPI_NET_TX_CAP 131072

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define GAME_BADSOCK INVALID_SOCKET
#define GAME_CLOSESOCK closesocket
static int sock_would_block(void) { return WSAGetLastError() == WSAEWOULDBLOCK; }
static void sock_set_nonblock(sock_t s) {
    u_long mode = 1;
    (void)ioctlsocket(s, (long)FIONBIO, &mode);
}
static int sock_recv(sock_t s, char *buf, int len) { return recv(s, buf, len, 0); }
static int sock_send(sock_t s, const char *buf, int len) { return send(s, buf, len, 0); }
#else
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define GAME_BADSOCK (-1)
#define GAME_CLOSESOCK close
static int sock_would_block(void) { return errno == EAGAIN || errno == EWOULDBLOCK; }
static void sock_set_nonblock(sock_t s) {
    int flags = fcntl(s, F_GETFL, 0);
    (void)fcntl(s, F_SETFL, flags | O_NONBLOCK);
}
static int sock_recv(sock_t s, char *buf, int len) { return (int)recv(s, buf, (size_t)len, 0); }
static int sock_send(sock_t s, const char *buf, int len) { return (int)send(s, buf, (size_t)len, 0); }
#endif

static sock_t s_listen = GAME_BADSOCK;
static sock_t s_client = GAME_BADSOCK;
static char s_rx[GAME_DEVAPI_NET_RX_CAP];
static int s_rx_len;
static bool s_rx_overflow;
static char s_tx[GAME_DEVAPI_NET_TX_CAP];
static int s_tx_pos;
static int s_tx_len;

static void reset_tx(void) {
    s_tx_pos = 0;
    s_tx_len = 0;
}

static void close_client(void) {
    if (s_client != GAME_BADSOCK) {
        GAME_CLOSESOCK(s_client);
        s_client = GAME_BADSOCK;
    }
    nt_devapi_cancel_pending();
    s_rx_len = 0;
    s_rx_overflow = false;
    reset_tx();
}

bool nt_devapi_net_start(uint16_t port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
#endif
    s_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen == GAME_BADSOCK) {
        return false;
    }

    int yes = 1;
    (void)setsockopt(s_listen, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(s_listen, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(s_listen, 1) != 0) {
        GAME_CLOSESOCK(s_listen);
        s_listen = GAME_BADSOCK;
        return false;
    }
    sock_set_nonblock(s_listen);
    nt_log_info("devapi: TCP listening on 127.0.0.1:%u", (unsigned)port);
    return true;
}

static bool queue_tx_bytes(const char *buf, int len) {
    if (len <= 0) {
        return true;
    }
    if (len > GAME_DEVAPI_NET_TX_CAP) {
        return false;
    }
    if (s_tx_pos > 0 && (s_tx_pos == s_tx_len || s_tx_len + len > GAME_DEVAPI_NET_TX_CAP)) {
        memmove(s_tx, s_tx + s_tx_pos, (size_t)(s_tx_len - s_tx_pos));
        s_tx_len -= s_tx_pos;
        s_tx_pos = 0;
    }
    if (s_tx_len + len > GAME_DEVAPI_NET_TX_CAP) {
        return false;
    }
    memcpy(s_tx + s_tx_len, buf, (size_t)len);
    s_tx_len += len;
    return true;
}

static void queue_protocol_error(const char *message) {
    char resp[256];
    int len = snprintf(resp, sizeof(resp), "{\"ok\":false,\"error\":\"%s\"}\n", message);
    if (len <= 0 || len >= (int)sizeof(resp) || !queue_tx_bytes(resp, len)) {
        close_client();
    }
}

static bool flush_tx(void) {
    while (s_client != GAME_BADSOCK && s_tx_pos < s_tx_len) {
        int sent = sock_send(s_client, s_tx + s_tx_pos, s_tx_len - s_tx_pos);
        if (sent > 0) {
            s_tx_pos += sent;
            continue;
        }
        if (sock_would_block()) {
            return true;
        }
        close_client();
        return false;
    }
    if (s_tx_pos >= s_tx_len) {
        reset_tx();
    }
    return true;
}

static void handle_line(char *line) {
    static char resp[1 << 16];
    int len = nt_devapi_submit(line, resp, (int)sizeof(resp) - 1);
    if (len <= 0) {
        return;
    }
    resp[len] = '\n';
    if (!queue_tx_bytes(resp, len + 1)) {
        close_client();
    }
}

static void flush_ready_responses(void) {
    static char resp[1 << 16];
    for (;;) {
        int len = nt_devapi_poll_response(resp, (int)sizeof(resp) - 1);
        if (len <= 0) {
            return;
        }
        resp[len] = '\n';
        if (!queue_tx_bytes(resp, len + 1)) {
            close_client();
            return;
        }
    }
}

void nt_devapi_net_poll(void) {
    if (s_listen == GAME_BADSOCK) {
        return;
    }
    if (s_client == GAME_BADSOCK) {
        sock_t client = accept(s_listen, NULL, NULL);
        if (client == GAME_BADSOCK) {
            return;
        }
        sock_set_nonblock(client);
        s_client = client;
        s_rx_len = 0;
        s_rx_overflow = false;
        reset_tx();
    }

    for (;;) {
        int space = (int)sizeof(s_rx) - 1 - s_rx_len;
        if (space <= 0) {
            s_rx_overflow = true;
            s_rx_len = 0;
            space = (int)sizeof(s_rx) - 1;
        }
        int len = sock_recv(s_client, s_rx + s_rx_len, space);
        if (len > 0) {
            s_rx_len += len;
        } else if (len == 0) {
            close_client();
            return;
        } else {
            if (!sock_would_block()) {
                close_client();
                return;
            }
            break;
        }
    }

    int start = 0;
    for (int i = 0; i < s_rx_len; i++) {
        if (s_rx[i] == '\n') {
            s_rx[i] = '\0';
            if (i > start && s_rx[i - 1] == '\r') {
                s_rx[i - 1] = '\0';
            }
            if (s_rx_overflow) {
                queue_protocol_error("request line too large");
                s_rx_overflow = false;
            } else {
                handle_line(s_rx + start);
            }
            if (s_client == GAME_BADSOCK) {
                return;
            }
            start = i + 1;
        }
    }
    if (start > 0) {
        memmove(s_rx, s_rx + start, (size_t)(s_rx_len - start));
        s_rx_len -= start;
    }
    flush_ready_responses();
    (void)flush_tx();
}

void nt_devapi_net_stop(void) {
    close_client();
    if (s_listen != GAME_BADSOCK) {
        GAME_CLOSESOCK(s_listen);
        s_listen = GAME_BADSOCK;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

#endif
