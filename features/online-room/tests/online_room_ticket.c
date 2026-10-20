// Version 2 tickets as the metaserver mints them: the contract's test vector
// encodes byte for byte and verifies; each refusal comes in the contract's
// order; an accepted ticket shown again is refused until it expires; the
// room's bearer is a room ticket for this room and life that lives 60 s.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "online_room/ticket.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t g_key[ONLINE_TICKET_KEY_SIZE];

static void hex(const char *text, uint8_t *out, size_t size) {
    for (size_t index = 0U; index < size; ++index) {
        unsigned value = 0U;
        TEST_ASSERT_EQUAL_INT(1, sscanf(text + 2U * index, "%2x", &value));
        out[index] = (uint8_t)value;
    }
}

static void key(void) {
    for (uint32_t index = 0U; index < ONLINE_TICKET_KEY_SIZE; ++index) { g_key[index] = (uint8_t)index; }
}

static void ticket(uint8_t kind, uint64_t subject, uint32_t room, uint32_t expires, uint8_t out[ONLINE_TICKET_SIZE]) {
    const online_ticket t = {.kind = kind, .subject = subject, .room_id = room, .expires_at = expires};
    online_ticket_encode(&t, g_key, out);
}

static void test_the_contracts_vector_encodes_and_verifies(void) {
    key();
    uint8_t want[ONLINE_TICKET_SIZE];
    hex("02540807060504030201070000001ef1536528d7b8b8d3fa973c90b19ce275f8c85c", want, sizeof want);
    uint8_t got[ONLINE_TICKET_SIZE];
    ticket(ONLINE_TICKET_KIND_PLAYER, 0x0102030405060708ULL, 7U, 1700000030U, got);
    TEST_ASSERT_EQUAL_MEMORY(want, got, ONLINE_TICKET_SIZE);
    online_ticket out;
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_verify(g_key, 7U, want, sizeof want, 1700000000U, &out));
    TEST_ASSERT_TRUE(out.subject == 0x0102030405060708ULL);
    TEST_ASSERT_EQUAL_UINT32(1700000030U, out.expires_at);
}

static void test_each_refusal(void) {
    key();
    uint8_t good[ONLINE_TICKET_SIZE];
    ticket(ONLINE_TICKET_KIND_PLAYER, 42U, 7U, 1000U, good);
    uint8_t bad[ONLINE_TICKET_SIZE];
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_MALFORMED, online_ticket_verify(g_key, 7U, good, sizeof good - 1U, 900U, NULL));
    memcpy(bad, good, sizeof bad);
    bad[0] = 1U;
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_MALFORMED, online_ticket_verify(g_key, 7U, bad, sizeof bad, 900U, NULL));
    ticket(ONLINE_TICKET_KIND_ROOM, 42U, 7U, 1000U, bad);
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_WRONG_KIND, online_ticket_verify(g_key, 7U, bad, sizeof bad, 900U, NULL));
    memcpy(bad, good, sizeof bad);
    bad[ONLINE_TICKET_SIZE - 1U] ^= 1U;
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_FORGED, online_ticket_verify(g_key, 7U, bad, sizeof bad, 900U, NULL));
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_WRONG_ROOM, online_ticket_verify(g_key, 8U, good, sizeof good, 900U, NULL));
    // Five seconds of skew either way.
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_verify(g_key, 7U, good, sizeof good, 1005U, NULL));
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_EXPIRED, online_ticket_verify(g_key, 7U, good, sizeof good, 1006U, NULL));
    ticket(ONLINE_TICKET_KIND_PLAYER, 0U, 7U, 1000U, bad);
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_NO_SUBJECT, online_ticket_verify(g_key, 7U, bad, sizeof bad, 900U, NULL));
}

static void test_page_entry_kind_is_signed_and_player_scoped(void) {
    key();
    uint8_t page[ONLINE_TICKET_SIZE];
    ticket(ONLINE_TICKET_KIND_PAGE_ENTRY, 42U, 7U, 1000U, page);
    online_ticket verified = {0};
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_verify(g_key, 7U, page, sizeof page, 900U, &verified));
    TEST_ASSERT_EQUAL_UINT8(ONLINE_TICKET_KIND_PAGE_ENTRY, verified.kind);
    uint8_t tampered[ONLINE_TICKET_SIZE];
    memcpy(tampered, page, sizeof tampered);
    tampered[1] = ONLINE_TICKET_KIND_PLAYER;
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_FORGED, online_ticket_verify(g_key, 7U, tampered, sizeof tampered, 900U, NULL));
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_WRONG_ROOM, online_ticket_verify(g_key, 8U, page, sizeof page, 900U, NULL));
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_EXPIRED, online_ticket_verify(g_key, 7U, page, sizeof page, 1006U, NULL));
    online_ticket_seen seen = {0};
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_admit(&seen, g_key, 7U, page, sizeof page, 900U, NULL));
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_REPEATED, online_ticket_admit(&seen, g_key, 7U, page, sizeof page, 901U, NULL));
}

static void test_a_ticket_seats_once(void) {
    key();
    static online_ticket_seen seen;
    memset(&seen, 0, sizeof seen);
    uint8_t one[ONLINE_TICKET_SIZE];
    uint8_t two[ONLINE_TICKET_SIZE];
    ticket(ONLINE_TICKET_KIND_PLAYER, 42U, 7U, 1000U, one);
    ticket(ONLINE_TICKET_KIND_PLAYER, 42U, 7U, 1001U, two);
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_admit(&seen, g_key, 7U, one, sizeof one, 990U, NULL));
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_REPEATED, online_ticket_admit(&seen, g_key, 7U, one, sizeof one, 991U, NULL));
    // Another ticket of the same account is its own.
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_admit(&seen, g_key, 7U, two, sizeof two, 991U, NULL));
    // Past its expiry the ticket is refused as expired, and its memory is let go.
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_EXPIRED, online_ticket_admit(&seen, g_key, 7U, one, sizeof one, 1006U, NULL));
    uint8_t three[ONLINE_TICKET_SIZE];
    ticket(ONLINE_TICKET_KIND_PLAYER, 42U, 7U, 2000U, three);
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_admit(&seen, g_key, 7U, three, sizeof three, 1006U, NULL));
    uint32_t held = 0U;
    for (uint32_t index = 0U; index < ONLINE_TICKET_SEEN_MAX; ++index) { held += seen.until[index] != 0U ? 1U : 0U; }
    TEST_ASSERT_EQUAL_UINT32(2U, held);
}

static void test_a_full_memory_refuses_rather_than_forgets(void) {
    key();
    static online_ticket_seen seen;
    memset(&seen, 0, sizeof seen);
    uint8_t one[ONLINE_TICKET_SIZE];
    for (uint32_t index = 0U; index < ONLINE_TICKET_SEEN_MAX; ++index) {
        ticket(ONLINE_TICKET_KIND_PLAYER, 1U + index, 7U, 1000U, one);
        TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_admit(&seen, g_key, 7U, one, sizeof one, 990U, NULL));
    }
    ticket(ONLINE_TICKET_KIND_PLAYER, 99999U, 7U, 1000U, one);
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_FULL, online_ticket_admit(&seen, g_key, 7U, one, sizeof one, 990U, NULL));
    TEST_ASSERT_EQUAL_UINT64(1U, seen.full);
}

// One account's tickets cannot fill the memory for everybody else: past its
// cap only its own new tickets are refused.
static void test_one_account_is_capped_and_nobody_else(void) {
    key();
    static online_ticket_seen seen;
    memset(&seen, 0, sizeof seen);
    uint8_t one[ONLINE_TICKET_SIZE];
    for (uint32_t index = 0U; index < ONLINE_TICKET_PER_ACCOUNT; ++index) {
        ticket(ONLINE_TICKET_KIND_PLAYER, 42U, 7U, 1000U + index, one);
        TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_admit(&seen, g_key, 7U, one, sizeof one, 990U, NULL));
    }
    ticket(ONLINE_TICKET_KIND_PLAYER, 42U, 7U, 1100U, one);
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_CAPPED, online_ticket_admit(&seen, g_key, 7U, one, sizeof one, 990U, NULL));
    ticket(ONLINE_TICKET_KIND_PLAYER, 43U, 7U, 1100U, one);
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_admit(&seen, g_key, 7U, one, sizeof one, 990U, NULL));
}

// A ticket checked but not remembered (its seat was not taken) seats later.
static void test_a_checked_ticket_is_not_used_until_remembered(void) {
    key();
    static online_ticket_seen seen;
    memset(&seen, 0, sizeof seen);
    uint8_t one[ONLINE_TICKET_SIZE];
    ticket(ONLINE_TICKET_KIND_PLAYER, 42U, 7U, 1000U, one);
    online_ticket out;
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_check(&seen, g_key, 7U, one, sizeof one, 990U, &out));
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_OK, online_ticket_check(&seen, g_key, 7U, one, sizeof one, 990U, &out));
    online_ticket_remember(&seen, one, &out, 990U);
    TEST_ASSERT_EQUAL_INT(ONLINE_TICKET_REPEATED, online_ticket_check(&seen, g_key, 7U, one, sizeof one, 990U, &out));
}

static void test_the_bearer_is_a_room_ticket_of_this_life(void) {
    key();
    char bearer[ONLINE_TICKET_BEARER_CHARS + 1U];
    online_ticket_bearer(g_key, 7U, 0xABCDEF0123ULL, 5000U, bearer);
    TEST_ASSERT_EQUAL_size_t(ONLINE_TICKET_BEARER_CHARS, strlen(bearer));
    uint8_t bytes[ONLINE_TICKET_SIZE];
    hex(bearer, bytes, sizeof bytes);
    uint8_t want[ONLINE_TICKET_SIZE];
    ticket(ONLINE_TICKET_KIND_ROOM, 0xABCDEF0123ULL, 7U, 5000U + ONLINE_TICKET_BEARER_SECONDS, want);
    TEST_ASSERT_EQUAL_MEMORY(want, bytes, ONLINE_TICKET_SIZE);
    TEST_ASSERT_TRUE(ONLINE_TICKET_BEARER_SECONDS <= 300U);
}

static void test_a_key_file_is_64_hex_digits(void) {
    uint8_t parsed[ONLINE_TICKET_KEY_SIZE];
    TEST_ASSERT_TRUE(online_ticket_key_parse("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\n", parsed));
    key();
    TEST_ASSERT_EQUAL_MEMORY(g_key, parsed, ONLINE_TICKET_KEY_SIZE);
    TEST_ASSERT_FALSE(online_ticket_key_parse("0001", parsed));
    TEST_ASSERT_FALSE(online_ticket_key_parse("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f00", parsed));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_the_contracts_vector_encodes_and_verifies);
    RUN_TEST(test_each_refusal);
    RUN_TEST(test_page_entry_kind_is_signed_and_player_scoped);
    RUN_TEST(test_a_ticket_seats_once);
    RUN_TEST(test_a_full_memory_refuses_rather_than_forgets);
    RUN_TEST(test_one_account_is_capped_and_nobody_else);
    RUN_TEST(test_a_checked_ticket_is_not_used_until_remembered);
    RUN_TEST(test_the_bearer_is_a_room_ticket_of_this_life);
    RUN_TEST(test_a_key_file_is_64_hex_digits);
    return UNITY_END();
}
