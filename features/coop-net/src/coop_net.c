#include "coop_net.h"

#include "coop_transport.h"
#include "net_codec.h"

#include <stdlib.h>
#include <string.h>

/* Authoritative co-op rooms and their clients. The wire and the transport are contracts here;
   everything game-shaped lives in the payloads the callbacks carry. */

enum { COOP_MSG_INPUT = 1, COOP_MSG_WELCOME = 2, COOP_MSG_SNAPSHOT = 3, COOP_MSG_EVENT = 4 };

typedef struct CoopSeat {
    bool joined;
    uint32_t peer;
    uint32_t last_seq;
} CoopSeat;

struct coop_room {
    coop_server_transport_t transport;
    coop_room_callbacks_t callbacks;
    CoopSeat seats[COOP_NET_SEATS_MAX];
    uint32_t tick;
    uint32_t seat_count;
    uint32_t snapshot_bytes;
    uint8_t *frame;
    uint8_t *event_frame;
};

struct coop_client {
    coop_client_transport_t transport;
    coop_client_callbacks_t callbacks;
    int seat;
    bool ready;
    uint32_t seq;
    uint32_t ack[COOP_NET_SEATS_MAX];
};

static int seat_of_peer(const coop_room_t *room, uint32_t peer) {
    for (int i = 0; i < COOP_NET_SEATS_MAX; i++) {
        if (room->seats[i].joined && room->seats[i].peer == peer) return i;
    }
    return -1;
}

static void room_on_open(void *user, uint32_t peer) {
    coop_room_t *room = user;
    int seat = -1;
    for (int i = 0; i < COOP_NET_SEATS_MAX && seat < 0; i++) {
        if (!room->seats[i].joined) seat = i;
    }
    if (seat < 0) {
        // Full: the transport drops the peer; the game sees no join.
        return;
    }
    CoopSeat *slot = &room->seats[seat];
    *slot = (CoopSeat){.joined = true, .peer = peer};
    if ((uint32_t)seat + 1U > room->seat_count) room->seat_count = (uint32_t)seat + 1U;

    uint8_t frame[16];
    net_writer_t writer;
    net_writer_init(&writer, frame, sizeof frame);
    net_write_u8(&writer, COOP_MSG_WELCOME);
    net_write_u8(&writer, (uint8_t)seat);
    net_write_u32(&writer, room->tick);
    if (writer.ok) (void)room->transport.send(room->transport.state, peer, frame, writer.pos);
    if (room->callbacks.on_join != NULL) room->callbacks.on_join(room->callbacks.user, seat);
}

static void room_on_message(void *user, uint32_t peer, const uint8_t *data, size_t size) {
    coop_room_t *room = user;
    if (size < 5) return;
    net_reader_t reader;
    net_reader_init(&reader, data, size);
    if (net_read_u8(&reader) != COOP_MSG_INPUT) return;
    const uint32_t seq = net_read_u32(&reader);
    if (!reader.ok) return;
    const int seat = seat_of_peer(room, peer);
    if (seat < 0) return;
    room->seats[seat].last_seq = seq;
    if (room->callbacks.on_input != NULL) {
        room->callbacks.on_input(room->callbacks.user, seat, seq, data + 5, size - 5);
    }
}

static void room_on_close(void *user, uint32_t peer, uint16_t code) {
    (void)code;
    coop_room_t *room = user;
    const int seat = seat_of_peer(room, peer);
    if (seat < 0) return;
    room->seats[seat] = (CoopSeat){0};
    if (room->callbacks.on_leave != NULL) room->callbacks.on_leave(room->callbacks.user, seat);
}

coop_room_t *coop_room_create(const coop_room_config_t *given, const coop_room_callbacks_t *callbacks) {
    const coop_room_config_t config_in = given != NULL ? *given : (coop_room_config_t){0};
    coop_room_t *room = calloc(1, sizeof *room);
    if (room == NULL) return NULL;
    room->callbacks = *callbacks;
    room->snapshot_bytes = config_in.snapshot_bytes > 0 && config_in.snapshot_bytes <= COOP_NET_SNAPSHOT_MAX
                               ? config_in.snapshot_bytes
                               : COOP_NET_SNAPSHOT_MAX;
    room->frame = malloc(room->snapshot_bytes);
    room->event_frame = malloc(room->snapshot_bytes);
    if (room->frame == NULL || room->event_frame == NULL) {
        free(room->frame);
        free(room->event_frame);
        free(room);
        return NULL;
    }
    room->transport = config_in.transport != NULL ? *config_in.transport : coop_transport_ws_server();
    if (room->transport.state == NULL || room->transport.start == NULL) {
        free(room->frame);
        free(room->event_frame);
        free(room);
        return NULL;
    }

    coop_transport_config_t transport = {0};
    transport.port = config_in.port;
    transport.max_clients = config_in.max_clients > 0 && config_in.max_clients <= COOP_NET_SEATS_MAX
                                ? config_in.max_clients
                                : COOP_NET_SEATS_MAX;
    transport.max_message_bytes = config_in.max_message_bytes > 0 ? config_in.max_message_bytes : 256;
    transport.protocol_version =
        config_in.protocol_version > 0 ? config_in.protocol_version : COOP_NET_PROTOCOL_VERSION;
    transport.ping_idle_s = config_in.ping_idle_s;
    transport.hangup_idle_s = config_in.hangup_idle_s;
    const coop_link_events_t events = {
        .user = room,
        .on_open = room_on_open,
        .on_message = room_on_message,
        .on_close = room_on_close,
    };
    if (!room->transport.start(room->transport.state, &transport, &events)) {
        room->transport.stop(room->transport.state);
        free(room->frame);
        free(room->event_frame);
        free(room);
        return NULL;
    }
    return room;
}

coop_room_t *coop_room_start(uint16_t port, uint32_t max_clients, const coop_room_callbacks_t *callbacks) {
    const coop_room_config_t config = {.port = port, .max_clients = max_clients};
    return coop_room_create(&config, callbacks);
}

void coop_room_stop(coop_room_t *room) {
    if (room == NULL) return;
    room->transport.stop(room->transport.state);
    free(room->frame);
    free(room->event_frame);
    free(room);
}

uint16_t coop_room_port(const coop_room_t *room) {
    return room != NULL && room->transport.port != NULL ? room->transport.port(room->transport.state) : 0;
}

int coop_room_seats(const coop_room_t *room) {
    if (room == NULL) return 0;
    int count = 0;
    for (int i = 0; i < COOP_NET_SEATS_MAX; i++) {
        if (room->seats[i].joined) count++;
    }
    return count;
}

uint32_t coop_room_ack(const coop_room_t *room, int seat) {
    if (room == NULL || seat < 0 || seat >= COOP_NET_SEATS_MAX) return 0;
    return room->seats[seat].last_seq;
}

void coop_room_pump(coop_room_t *room, float dt) {
    room->transport.service(room->transport.state, 0);
    if (room->callbacks.on_tick != NULL) room->callbacks.on_tick(room->callbacks.user, dt);
    room->tick++;

    uint32_t acks[COOP_NET_SEATS_MAX];
    for (int i = 0; i < COOP_NET_SEATS_MAX; i++) acks[i] = room->seats[i].last_seq;

    net_writer_t writer;
    net_writer_init(&writer, room->frame, room->snapshot_bytes);
    net_write_u8(&writer, COOP_MSG_SNAPSHOT);
    net_write_u32(&writer, room->tick);
    net_write_u8(&writer, (uint8_t)room->seat_count);
    for (uint32_t i = 0; i < room->seat_count; i++) net_write_u32(&writer, acks[i]);
    size_t size = writer.ok ? writer.pos : 0;
    if (size > 0 && room->callbacks.write_snapshot != NULL) {
        const size_t payload = room->callbacks.write_snapshot(room->callbacks.user, acks, (int)room->seat_count,
                                                              room->frame + size, room->snapshot_bytes - size);
        if (!writer.ok || payload == 0) size = 0;
        else size += payload;
    }
    if (size == 0) return;
    for (int i = 0; i < COOP_NET_SEATS_MAX; i++) {
        if (!room->seats[i].joined) continue;
        // The newest snapshot replaces any the client has not read: a delayed one is history.
        (void)room->transport.send_latest(room->transport.state, room->seats[i].peer, room->frame, size);
    }
}

bool coop_room_broadcast(coop_room_t *room, const uint8_t *data, size_t size) {
    if (room == NULL || data == NULL || size == 0 || size + 1 > room->snapshot_bytes) return false;
    net_writer_t writer;
    net_writer_init(&writer, room->event_frame, room->snapshot_bytes);
    net_write_u8(&writer, COOP_MSG_EVENT);
    for (size_t i = 0; i < size; i++) net_write_u8(&writer, data[i]);
    if (!writer.ok) return false;
    bool delivered = true;
    for (int i = 0; i < COOP_NET_SEATS_MAX; i++) {
        if (!room->seats[i].joined) continue;
        // Reliable and ordered: a break or a hit must not be lost like a snapshot may be.
        if (!room->transport.send(room->transport.state, room->seats[i].peer, room->event_frame, writer.pos)) {
            delivered = false;
        }
    }
    return delivered;
}

static void client_on_message(void *user, uint32_t peer, const uint8_t *data, size_t size) {
    (void)peer;
    coop_client_t *client = user;
    if (size < 2) return;
    net_reader_t reader;
    net_reader_init(&reader, data, size);
    const uint8_t type = net_read_u8(&reader);
    if (type == COOP_MSG_WELCOME) {
        const uint8_t seat = net_read_u8(&reader);
        const uint32_t tick = net_read_u32(&reader);
        if (!reader.ok || seat >= COOP_NET_SEATS_MAX) return;
        client->seat = (int)seat;
        client->ready = true;
        if (client->callbacks.on_welcome != NULL) client->callbacks.on_welcome(client->callbacks.user, (int)seat, tick);
        return;
    }
    if (type == COOP_MSG_EVENT) {
        if (client->callbacks.on_event != NULL && size > 1) {
            client->callbacks.on_event(client->callbacks.user, data + 1, size - 1);
        }
        return;
    }
    if (type == COOP_MSG_SNAPSHOT) {
        const uint32_t tick = net_read_u32(&reader);
        const uint8_t count = net_read_u8(&reader);
        if (!reader.ok || count > COOP_NET_SEATS_MAX) return;
        for (int i = 0; i < (int)count; i++) client->ack[i] = net_read_u32(&reader);
        if (!reader.ok) return;
        if (client->callbacks.on_snapshot != NULL) {
            client->callbacks.on_snapshot(client->callbacks.user, tick, client->ack, (int)count, data + reader.pos,
                                          size - reader.pos);
        }
    }
}

static void client_on_close(void *user, uint32_t peer, uint16_t code) {
    (void)peer;
    coop_client_t *client = user;
    client->ready = false;
    if (client->callbacks.on_close != NULL) client->callbacks.on_close(client->callbacks.user, code);
}

coop_client_t *coop_client_create(const coop_client_config_t *given, const coop_client_callbacks_t *callbacks) {
    const coop_client_config_t config_in = given != NULL ? *given : (coop_client_config_t){0};
    coop_client_t *client = calloc(1, sizeof *client);
    if (client == NULL) return NULL;
    client->callbacks = *callbacks;
    client->seat = -1;
    client->transport = config_in.transport != NULL ? *config_in.transport : coop_transport_ws_client();
    if (client->transport.state == NULL || client->transport.start == NULL) {
        free(client);
        return NULL;
    }
    coop_transport_config_t transport = {0};
    transport.protocol_version =
        config_in.protocol_version > 0 ? config_in.protocol_version : COOP_NET_PROTOCOL_VERSION;
    transport.max_message_bytes = config_in.snapshot_bytes > 0 && config_in.snapshot_bytes <= COOP_NET_SNAPSHOT_MAX
                                      ? config_in.snapshot_bytes
                                      : COOP_NET_SNAPSHOT_MAX;
    const coop_link_events_t events = {
        .user = client,
        .on_open = NULL,
        .on_message = client_on_message,
        .on_close = client_on_close,
    };
    if (!client->transport.start(client->transport.state, config_in.url, &transport, &events)) {
        client->transport.stop(client->transport.state);
        free(client);
        return NULL;
    }
    return client;
}

coop_client_t *coop_client_connect(const char *url, const coop_client_callbacks_t *callbacks) {
    const coop_client_config_t config = {.url = url};
    return coop_client_create(&config, callbacks);
}

void coop_client_stop(coop_client_t *client) {
    if (client == NULL) return;
    client->transport.stop(client->transport.state);
    free(client);
}

void coop_client_service(coop_client_t *client) {
    if (client == NULL) return;
    client->transport.service(client->transport.state, 0);
}

bool coop_client_send_input(coop_client_t *client, const uint8_t *data, size_t size) {
    if (client == NULL || !client->ready || size + 5 > COOP_NET_SNAPSHOT_MAX) return false;
    uint8_t frame[COOP_NET_SNAPSHOT_MAX];
    net_writer_t writer;
    net_writer_init(&writer, frame, sizeof frame);
    net_write_u8(&writer, COOP_MSG_INPUT);
    net_write_u32(&writer, ++client->seq);
    for (size_t i = 0; i < size; i++) net_write_u8(&writer, data[i]);
    if (!writer.ok) return false;
    return client->transport.send(client->transport.state, frame, writer.pos);
}

uint32_t coop_client_seq(const coop_client_t *client) {
    return client != NULL ? client->seq : 0;
}

int coop_client_seat(const coop_client_t *client) {
    return client != NULL ? client->seat : -1;
}

bool coop_client_ready(const coop_client_t *client) {
    return client != NULL && client->ready;
}
