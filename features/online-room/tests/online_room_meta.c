// The profile store and the status over the metaserver's HTTP, on a fake
// transport: the paths, the bearer and the bodies the contract names, and
// every answer mapped to the store's verdict (a 412 names this room life's
// writer only; a load's 404 is a failure to enter again, 413 a refusal).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "online_room/meta.h"
#include "net_codec.h"

void setUp(void) {}
void tearDown(void) {}

#define ROOM 7U
#define LIFE 0x1122334455ULL
#define NOW 1700000000U

typedef struct fake_call {
    char url[256];
    char bearer[ONLINE_TICKET_BEARER_CHARS + 1U];
    uint8_t body[4096];
    size_t size;
    bool answered;
    uint16_t status;
    uint8_t answer[4096];
    size_t answer_size;
    bool cancelled;
    bool heard;
} fake_call;

static fake_call g_calls[32];
static uint32_t g_count;

static void fake_pump(void *user) { (void)user; }

static uint32_t fake_begin(void *user, const char *url, const char *bearer, const void *body, size_t size, uint32_t timeout_ms) {
    (void)user;
    TEST_ASSERT_TRUE(timeout_ms >= 3000U);
    TEST_ASSERT_TRUE(g_count < 32U);
    fake_call *call = &g_calls[g_count];
    memset(call, 0, sizeof *call);
    snprintf(call->url, sizeof call->url, "%s", url);
    snprintf(call->bearer, sizeof call->bearer, "%s", bearer);
    // Only the head of a large body is kept; its size is recorded whole.
    if (size > 0U) { memcpy(call->body, body, size < sizeof call->body ? size : sizeof call->body); }
    call->size = size;
    g_count += 1U;
    return g_count;
}

static bool fake_poll(void *user, uint32_t handle, uint16_t *status, uint8_t **body, size_t *size) {
    (void)user;
    fake_call *call = &g_calls[handle - 1U];
    TEST_ASSERT_FALSE(call->heard);
    if (!call->answered) { return false; }
    call->heard = true;
    *status = call->status;
    *size = call->answer_size;
    *body = NULL;
    if (call->answer_size > 0U) {
        *body = malloc(call->answer_size);
        memcpy(*body, call->answer, call->answer_size);
    }
    return true;
}

static void fake_cancel(void *user, uint32_t handle) {
    (void)user;
    g_calls[handle - 1U].cancelled = true;
}

static const online_meta_transport k_fake = {.pump = fake_pump, .begin = fake_begin, .poll = fake_poll, .cancel = fake_cancel};

static uint32_t fixed_clock(void *user) {
    (void)user;
    return NOW;
}

static online_meta g_meta;
static online_profile_store g_store;

static void open_meta(void) {
    g_count = 0U;
    online_meta_config config = {.transport = &k_fake, .base_url = "http://meta.test:8080", .room_id = ROOM, .incarnation = LIFE,
        .clock = fixed_clock};
    for (uint32_t index = 0U; index < ONLINE_TICKET_KEY_SIZE; ++index) { config.key[index] = (uint8_t)(index * 3U); }
    TEST_ASSERT_TRUE(online_meta_init(&g_meta, &config));
    online_meta_store(&g_meta, &g_store);
}

static void answer(uint32_t call, uint16_t status, const void *body, size_t size) {
    g_calls[call].answered = true;
    g_calls[call].status = status;
    if (size > 0U) { memcpy(g_calls[call].answer, body, size); }
    g_calls[call].answer_size = size;
}

static online_store_status hear(uint32_t request, char *doc, size_t cap, online_store_answer *out) {
    return g_store.poll(g_store.user, request, doc, cap, out);
}


static void test_the_bearer_is_this_rooms_life(void) {
    open_meta();
    (void)g_store.load(g_store.user, 42U);
    uint8_t want[ONLINE_TICKET_SIZE];
    const online_ticket room = {.kind = ONLINE_TICKET_KIND_ROOM, .subject = LIFE, .room_id = ROOM, .expires_at = NOW + ONLINE_TICKET_BEARER_SECONDS};
    online_ticket_encode(&room, g_meta.config.key, want);
    char hex[ONLINE_TICKET_BEARER_CHARS + 1U];
    for (uint32_t index = 0U; index < ONLINE_TICKET_SIZE; ++index) { snprintf(hex + 2U * index, 3U, "%02x", want[index]); }
    TEST_ASSERT_EQUAL_STRING(hex, g_calls[0].bearer);
}

static void test_a_load_and_its_answers(void) {
    open_meta();
    char doc[64];
    online_store_answer out;
    uint32_t request = g_store.load(g_store.user, 42U);
    TEST_ASSERT_EQUAL_STRING("http://meta.test:8080/rooms/profiles/42/load", g_calls[0].url);
    TEST_ASSERT_EQUAL_size_t(0U, g_calls[0].size);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_PENDING, hear(request, doc, sizeof doc, &out));
    uint8_t body[32];
    net_writer_t writer;
    net_writer_init(&writer, body, 8U);
    net_write_u64(&writer, 5U);
    memcpy(body + 8U, "doc!", 4U);
    answer(0U, 200U, body, 12U);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_DONE, hear(request, doc, sizeof doc, &out));
    TEST_ASSERT_TRUE(out.rev == 5 && out.size == 4U);
    TEST_ASSERT_EQUAL_MEMORY("doc!", doc, 4U);
    // A first visit: rev 0, no document.
    request = g_store.load(g_store.user, 42U);
    answer(1U, 200U, body, 0U);
    net_writer_init(&writer, body, 8U);
    net_write_u64(&writer, 0U);
    answer(1U, 200U, body, 8U);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_DONE, hear(request, doc, sizeof doc, &out));
    TEST_ASSERT_TRUE(out.rev == 0 && out.size == 0U);
    // A document this build could not hold is refused, never cut.
    request = g_store.load(g_store.user, 42U);
    net_writer_init(&writer, body, 8U);
    net_write_u64(&writer, 2U);
    answer(2U, 200U, body, 12U);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_REFUSED, hear(request, doc, 2U, &out));
    static const struct {
        uint16_t status;
        online_store_status verdict;
    } k_cases[] = {{404U, ONLINE_STORE_FAILED}, {409U, ONLINE_STORE_SUPERSEDED}, {413U, ONLINE_STORE_REFUSED}, {400U, ONLINE_STORE_REFUSED},
        {500U, ONLINE_STORE_FAILED}, {0U, ONLINE_STORE_FAILED}, {401U, ONLINE_STORE_FAILED}};
    for (uint32_t index = 0U; index < sizeof k_cases / sizeof k_cases[0]; ++index) {
        request = g_store.load(g_store.user, 42U);
        answer(g_count - 1U, k_cases[index].status, "no", 2U);
        TEST_ASSERT_EQUAL_INT(k_cases[index].verdict, hear(request, doc, sizeof doc, &out));
    }
    TEST_ASSERT_EQUAL_UINT64(1U, g_meta.stats.unauthorized);
}

// A batch's answer of one entry: its status, its size, its body.
static void answer_one(uint32_t call, uint16_t status, const void *body, size_t size) {
    uint8_t framed[64];
    net_writer_t writer;
    net_writer_init(&writer, framed, 6U);
    net_write_u16(&writer, status);
    net_write_u32(&writer, (uint32_t)size);
    if (size > 0U) { memcpy(framed + 6U, body, size); }
    answer(call, 200U, framed, 6U + size);
}

// One save alone: the pending call goes on the store's send, and each of the
// metaserver's answers maps to the store's verdict.
static void test_a_save_and_its_answers(void) {
    open_meta();
    online_store_answer out;
    uint32_t request = g_store.save(g_store.user, 42U, 3, 0x0000000200000001ULL, "document", 8U);
    TEST_ASSERT_EQUAL_UINT32(0U, g_count);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_PENDING, hear(request, NULL, 0U, &out));
    g_store.send(g_store.user);
    TEST_ASSERT_EQUAL_STRING("http://meta.test:8080/rooms/profiles/save", g_calls[0].url);
    uint8_t want[36];
    net_writer_t writer;
    net_writer_init(&writer, want, 28U);
    net_write_u64(&writer, 42U);
    net_write_u64(&writer, 3U);
    net_write_u64(&writer, 0x0000000200000001ULL);
    net_write_u32(&writer, 8U);
    memcpy(want + 28U, "document", 8U);
    TEST_ASSERT_EQUAL_size_t(36U, g_calls[0].size);
    TEST_ASSERT_EQUAL_MEMORY(want, g_calls[0].body, 36U);
    uint8_t body[28];
    net_writer_init(&writer, body, 8U);
    net_write_u64(&writer, 4U);
    answer_one(0U, 200U, body, 8U);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_DONE, hear(request, NULL, 0U, &out));
    TEST_ASSERT_TRUE(out.rev == 4);
    // A send with nothing pending calls nothing.
    g_store.send(g_store.user);
    TEST_ASSERT_EQUAL_UINT32(1U, g_count);
    // A 412 names its writer: this room life's save id, else nobody.
    for (uint32_t ours = 0U; ours < 2U; ++ours) {
        request = g_store.save(g_store.user, 42U, 3, 9U, "document", 8U);
        g_store.send(g_store.user);
        net_writer_init(&writer, body, sizeof body);
        net_write_u64(&writer, 6U);
        net_write_u32(&writer, ROOM);
        net_write_u64(&writer, ours == 1U ? LIFE : LIFE - 1U);
        net_write_u64(&writer, 77U);
        answer_one(g_count - 1U, 412U, body, sizeof body);
        TEST_ASSERT_EQUAL_INT(ONLINE_STORE_CONFLICT, hear(request, NULL, 0U, &out));
        TEST_ASSERT_TRUE(out.rev == 6);
        TEST_ASSERT_TRUE(out.writer == (ours == 1U ? 77U : 0U));
    }
    // A stale answer that does not read (a rev past the store's range) is no
    // verdict: the save goes again, and only 413 and 400 are refusals.
    request = g_store.save(g_store.user, 42U, 3, 9U, "document", 8U);
    g_store.send(g_store.user);
    net_writer_init(&writer, body, sizeof body);
    net_write_u64(&writer, (uint64_t)INT64_MAX + 1U);
    net_write_u32(&writer, ROOM);
    net_write_u64(&writer, LIFE);
    net_write_u64(&writer, 9U);
    answer_one(g_count - 1U, 412U, body, sizeof body);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_FAILED, hear(request, NULL, 0U, &out));
    static const struct {
        uint16_t status;
        online_store_status verdict;
    } k_cases[] = {{409U, ONLINE_STORE_SUPERSEDED}, {413U, ONLINE_STORE_REFUSED}, {400U, ONLINE_STORE_REFUSED}, {500U, ONLINE_STORE_FAILED},
        {0U, ONLINE_STORE_FAILED}, {412U, ONLINE_STORE_FAILED}, {200U, ONLINE_STORE_FAILED}};
    for (uint32_t whole = 0U; whole < 2U; ++whole) {
        for (uint32_t index = 0U; index < sizeof k_cases / sizeof k_cases[0]; ++index) {
            request = g_store.save(g_store.user, 42U, 3, 9U, "document", 8U);
            g_store.send(g_store.user);
            // An entry's 412 whose body is cut is no verdict, a 200 whose body
            // is cut as unknown as no answer. A call not answered 200, or whose
            // entries do not read, is no verdict on any save in it.
            if (whole == 0U) {
                answer_one(g_count - 1U, k_cases[index].status, "no", 2U);
            } else {
                answer(g_count - 1U, k_cases[index].status, "no", 2U);
            }
            TEST_ASSERT_EQUAL_INT(whole == 0U ? k_cases[index].verdict : ONLINE_STORE_FAILED, hear(request, NULL, 0U, &out));
        }
    }
}

// Every save until the send goes in one call, in the order given; each hears
// its own entry, whichever is polled first. A save cancelled before the
// answer frees nothing the others need, and the call is dropped only once
// nobody waits for it.
static void test_saves_until_a_send_go_in_one_call(void) {
    open_meta();
    online_store_answer out;
    uint32_t requests[3];
    for (uint32_t index = 0U; index < 3U; ++index) {
        requests[index] = g_store.save(g_store.user, 40U + index, (int64_t)index, 100U + index, "doc", 3U);
        TEST_ASSERT_TRUE(requests[index] != 0U);
    }
    g_store.send(g_store.user);
    TEST_ASSERT_EQUAL_UINT32(1U, g_count);
    TEST_ASSERT_EQUAL_size_t(3U * 31U, g_calls[0].size);
    net_reader_t reader;
    net_reader_init(&reader, g_calls[0].body + 31U, 28U);
    TEST_ASSERT_EQUAL_UINT64(41U, net_read_u64(&reader));
    TEST_ASSERT_EQUAL_UINT64(1U, net_read_u64(&reader));
    TEST_ASSERT_EQUAL_UINT64(101U, net_read_u64(&reader));
    TEST_ASSERT_EQUAL_UINT32(3U, net_read_u32(&reader));
    // Answers: 200 rev 7, 409, 200 rev 9.
    uint8_t body[64];
    net_writer_t writer;
    net_writer_init(&writer, body, sizeof body);
    net_write_u16(&writer, 200U);
    net_write_u32(&writer, 8U);
    net_write_u64(&writer, 7U);
    net_write_u16(&writer, 409U);
    net_write_u32(&writer, 0U);
    net_write_u16(&writer, 200U);
    net_write_u32(&writer, 8U);
    net_write_u64(&writer, 9U);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_PENDING, hear(requests[2], NULL, 0U, &out));
    g_store.cancel(g_store.user, requests[1]);
    TEST_ASSERT_FALSE(g_calls[0].cancelled);
    answer(0U, 200U, body, 6U + 8U + 6U + 6U + 8U);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_DONE, hear(requests[2], NULL, 0U, &out));
    TEST_ASSERT_TRUE(out.rev == 9);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_DONE, hear(requests[0], NULL, 0U, &out));
    TEST_ASSERT_TRUE(out.rev == 7);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_FAILED, hear(requests[1], NULL, 0U, &out));

    // Both saves of a call cancelled before its answer: the call is dropped.
    requests[0] = g_store.save(g_store.user, 40U, 0, 200U, "doc", 3U);
    requests[1] = g_store.save(g_store.user, 41U, 0, 201U, "doc", 3U);
    g_store.send(g_store.user);
    g_store.cancel(g_store.user, requests[0]);
    TEST_ASSERT_FALSE(g_calls[1].cancelled);
    g_store.cancel(g_store.user, requests[1]);
    TEST_ASSERT_TRUE(g_calls[1].cancelled);
    // A save cancelled before the send is not sent alone.
    requests[0] = g_store.save(g_store.user, 40U, 0, 300U, "doc", 3U);
    g_store.cancel(g_store.user, requests[0]);
    g_store.send(g_store.user);
    TEST_ASSERT_EQUAL_UINT32(2U, g_count);
}

// Every seat and every last save of a full room at once: one call holds them all.
static void test_a_full_room_of_saves_is_one_call(void) {
    open_meta();
    static char doc[ONLINE_ROOM_PROFILE_TEXT_MAX];
    memset(doc, 'd', sizeof doc);
    for (uint32_t index = 0U; index < ONLINE_META_BATCH_MAX; ++index) {
        TEST_ASSERT_TRUE(g_store.save(g_store.user, 1000U + index, 0, 1U + index, doc, sizeof doc) != 0U);
    }
    TEST_ASSERT_EQUAL_UINT32(0U, g_count);
    g_store.send(g_store.user);
    TEST_ASSERT_EQUAL_UINT32(1U, g_count);
    TEST_ASSERT_EQUAL_size_t(ONLINE_META_BATCH_BYTES, g_calls[0].size);
}

static void test_a_cancelled_call_is_forgotten(void) {
    open_meta();
    online_store_answer out;
    const uint32_t request = g_store.load(g_store.user, 42U);
    g_store.cancel(g_store.user, request);
    TEST_ASSERT_TRUE(g_calls[0].cancelled);
    TEST_ASSERT_EQUAL_INT(ONLINE_STORE_FAILED, hear(request, NULL, 0U, &out));
}

static void test_one_status_at_a_time_and_its_round_trip(void) {
    open_meta();
    TEST_ASSERT_TRUE(online_meta_status_send(&g_meta, "{}", 2U, 1000U));
    TEST_ASSERT_EQUAL_STRING("http://meta.test:8080/rooms/status", g_calls[0].url);
    TEST_ASSERT_FALSE(online_meta_status_send(&g_meta, "{}", 2U, 1100U));
    online_meta_status_poll(&g_meta, 1200U);
    answer(0U, 200U, NULL, 0U);
    online_meta_status_poll(&g_meta, 1250U);
    TEST_ASSERT_EQUAL_UINT32(250U, g_meta.status_round_trip_ms);
    TEST_ASSERT_TRUE(online_meta_status_send(&g_meta, "{}", 2U, 3000U));
    answer(1U, 400U, NULL, 0U);
    online_meta_status_poll(&g_meta, 3010U);
    TEST_ASSERT_EQUAL_UINT64(1U, g_meta.stats.status_refused);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_the_bearer_is_this_rooms_life);
    RUN_TEST(test_a_load_and_its_answers);
    RUN_TEST(test_a_save_and_its_answers);
    RUN_TEST(test_saves_until_a_send_go_in_one_call);
    RUN_TEST(test_a_full_room_of_saves_is_one_call);
    RUN_TEST(test_a_cancelled_call_is_forgotten);
    RUN_TEST(test_one_status_at_a_time_and_its_round_trip);
    return UNITY_END();
}
