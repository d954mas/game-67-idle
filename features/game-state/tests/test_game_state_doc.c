/* Instance documents: many profiles in one process, a v1 document migrated to
   v2, the seal, and the four solo-sync outcomes on sealed fixture documents. */
#include "unity.h"

#include "cJSON.h"
#include "game_save_seal.h"
#include "game_save_sync.h"
#include "game_state_doc.h"
#include "progress_state.h"
#include "wallet_state.h"

#include <stdio.h>
#include <string.h>

#define PROFILES 16
#define DOC_CAP 1024

/* The fixture's v1 -> v2 history: progress.experience became progress.xp (the
   old name is a tombstone in the schema), and the purse fragment became wallet. */
bool progress_migrate_experience_to_xp(cJSON *frag, char *err, int cap) {
    cJSON *experience = cJSON_DetachItemFromObjectCaseSensitive(frag, "experience");
    if (experience == NULL) {
        (void)snprintf(err, (size_t)cap, "progress v1 has no experience");
        return false;
    }
    return cJSON_AddItemToObject(frag, "xp", experience);
}

static bool rename_purse_to_wallet(cJSON *features, char *err, int cap) {
    (void)err;
    (void)cap;
    cJSON *purse = cJSON_DetachItemFromObjectCaseSensitive(features, "purse");
    return purse == NULL || cJSON_AddItemToObject(features, "wallet", purse);
}

static const game_state_doc_fragment_t *const k_fragments[] = {&progress_state_doc_fragment,
                                                                &wallet_state_doc_fragment};
static const GameSaveDocumentMigrateFn k_document_steps[] = {rename_purse_to_wallet};
static const game_state_doc_schema_t k_schema = {
    .fragments = k_fragments,
    .fragment_count = 2,
    .save_version = 2,
    .document_steps = k_document_steps,
};

static const uint8_t k_key[GAME_SAVE_SEAL_KEY_SIZE] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                                      11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                                      22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

typedef struct {
    ProgressState progress;
    WalletState wallet;
} profile_t;

void setUp(void) {}
void tearDown(void) {}

static size_t write_profile(const profile_t *profile, const game_state_doc_header_t *header, char *out, size_t cap) {
    const void *states[] = {&profile->progress, &profile->wallet};
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, out, cap);
    char error[128] = {0};
    TEST_ASSERT_TRUE_MESSAGE(game_state_doc_write(&k_schema, header, states, &writer, error, (int)sizeof error), error);
    return game_save_text_writer_size(&writer);
}

static void test_an_invalid_state_is_never_written(void) {
    ProgressState progress;
    WalletState wallet;
    progress_state_init_defaults(&progress);
    wallet_state_init_defaults(&wallet);
    wallet.nickname[0] = '\0';
    const void *states[] = {&progress, &wallet};
    const game_state_doc_header_t header = {.save_id = 1u};
    char out[DOC_CAP];
    char error[128] = {0};
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, out, sizeof out);
    TEST_ASSERT_FALSE(game_state_doc_write(&k_schema, &header, states, &writer, error, (int)sizeof error));
    TEST_ASSERT_EQUAL_size_t(0u, game_save_text_writer_size(&writer));
}

static bool read_profile(const char *text, size_t size, profile_t *profile, game_state_doc_header_t *header) {
    void *states[] = {&profile->progress, &profile->wallet};
    char error[128] = {0};
    return game_state_doc_read(&k_schema, text, size, header, states, error, (int)sizeof error);
}

static void test_sixteen_profiles_round_trip_in_one_process(void) {
    static profile_t profiles[PROFILES];
    static char docs[PROFILES][DOC_CAP];
    size_t sizes[PROFILES];
    for (int i = 0; i < PROFILES; i++) {
        progress_state_init_defaults(&profiles[i].progress);
        wallet_state_init_defaults(&profiles[i].wallet);
        profiles[i].progress.xp = 1000 * i + 7;
        profiles[i].progress.level = i + 1;
        profiles[i].wallet.coins = (uint32_t)(i * 3);
        (void)snprintf(profiles[i].wallet.nickname, sizeof profiles[i].wallet.nickname, "player %d", i);
        const game_state_doc_header_t header = {.save_id = 0x1000u + (uint64_t)i, .rev = i};
        sizes[i] = write_profile(&profiles[i], &header, docs[i], DOC_CAP);
    }
    for (int i = PROFILES - 1; i >= 0; i--) {
        profile_t loaded;
        game_state_doc_header_t header;
        TEST_ASSERT_TRUE(read_profile(docs[i], sizes[i], &loaded, &header));
        TEST_ASSERT_EQUAL_INT(2, header.save_version);
        TEST_ASSERT_EQUAL_UINT64(0x1000u + (uint64_t)i, header.save_id);
        TEST_ASSERT_EQUAL_INT64(i, header.rev);
        TEST_ASSERT_EQUAL_INT64(profiles[i].progress.xp, loaded.progress.xp);
        TEST_ASSERT_EQUAL_INT(profiles[i].progress.level, loaded.progress.level);
        TEST_ASSERT_EQUAL_UINT32(profiles[i].wallet.coins, loaded.wallet.coins);
        TEST_ASSERT_EQUAL_STRING(profiles[i].wallet.nickname, loaded.wallet.nickname);
        char again[DOC_CAP];
        TEST_ASSERT_EQUAL_size_t(sizes[i], write_profile(&loaded, &header, again, sizeof again));
        TEST_ASSERT_EQUAL_STRING(docs[i], again);
    }
}

static void test_a_missing_fragment_reads_as_defaults_and_an_unknown_one_fails(void) {
    const char *only_progress = "NTGS 1\nsave_version=2\nsave_id=\"0000000000000001\"\nrev=0\n\n[progress 2]\nxp=5\n";
    profile_t loaded;
    TEST_ASSERT_TRUE(read_profile(only_progress, strlen(only_progress), &loaded, NULL));
    TEST_ASSERT_EQUAL_INT64(5, loaded.progress.xp);
    TEST_ASSERT_EQUAL_INT(1, loaded.progress.level);
    TEST_ASSERT_EQUAL_UINT32(0u, loaded.wallet.coins);

    const char *unknown = "NTGS 1\nsave_version=2\nsave_id=\"0000000000000001\"\nrev=0\n\n[ghost 1]\nboo=1\n";
    TEST_ASSERT_FALSE(read_profile(unknown, strlen(unknown), &loaded, NULL));
    const char *no_rev = "NTGS 1\nsave_version=2\nsave_id=\"0000000000000001\"\n\n[progress 2]\nxp=5\n";
    TEST_ASSERT_FALSE(read_profile(no_rev, strlen(no_rev), &loaded, NULL));
}

static const char k_v1_document[] = "NTGS 1\n"
                                    "save_version=1\n"
                                    "save_id=\"00c0ffee00c0ffee\"\n"
                                    "rev=3\n"
                                    "\n"
                                    "[progress 1]\n"
                                    "experience=1200\n"
                                    "level=4\n"
                                    "\n"
                                    "[purse 1]\n"
                                    "coins=50\n"
                                    "nickname=\"Rex\"\n";

static void test_a_v1_fixture_migrates_to_v2(void) {
    profile_t loaded;
    TEST_ASSERT_FALSE(read_profile(k_v1_document, strlen(k_v1_document), &loaded, NULL));

    char migrated[DOC_CAP];
    char error[128] = {0};
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, migrated, sizeof migrated);
    TEST_ASSERT_TRUE_MESSAGE(
        game_state_doc_migrate(&k_schema, k_v1_document, strlen(k_v1_document), &writer, error, (int)sizeof error),
        error);
    game_state_doc_header_t header;
    TEST_ASSERT_TRUE(read_profile(migrated, game_save_text_writer_size(&writer), &loaded, &header));
    TEST_ASSERT_EQUAL_INT(2, header.save_version);
    TEST_ASSERT_EQUAL_UINT64(0x00c0ffee00c0ffeeULL, header.save_id);
    TEST_ASSERT_EQUAL_INT64(3, header.rev);
    TEST_ASSERT_EQUAL_INT64(1200, loaded.progress.xp);
    TEST_ASSERT_EQUAL_INT(4, loaded.progress.level);
    TEST_ASSERT_EQUAL_UINT32(50u, loaded.wallet.coins);
    TEST_ASSERT_EQUAL_STRING("Rex", loaded.wallet.nickname);

    char twice[DOC_CAP];
    game_save_text_writer_init(&writer, twice, sizeof twice);
    TEST_ASSERT_TRUE(game_state_doc_migrate(&k_schema, migrated, strlen(migrated), &writer, error, (int)sizeof error));
    TEST_ASSERT_EQUAL_STRING(migrated, twice);

    const char *newer = "NTGS 1\nsave_version=3\nsave_id=\"0000000000000001\"\nrev=0\n";
    game_save_text_writer_init(&writer, twice, sizeof twice);
    TEST_ASSERT_FALSE(game_state_doc_migrate(&k_schema, newer, strlen(newer), &writer, error, (int)sizeof error));
}

/* Computed by an independent implementation (Python hmac/hashlib and ChaCha20
   checked against RFC 8439 2.3.2): the format a server verifies against. */
static void test_the_seal_matches_its_reference_vector(void) {
    char plain[160];
    (void)snprintf(plain, sizeof plain, "%s%s",
                   "NTGS 1\nsave_version=2\nsave_id=\"00c0ffee00c0ffee\"\nrev=3\n",
                   "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    TEST_ASSERT_EQUAL_size_t(125u, strlen(plain));
    char sealed[512];
    size_t sealed_size = 0;
    TEST_ASSERT_TRUE(game_save_seal(k_key, plain, strlen(plain), sealed, sizeof sealed, &sealed_size));
    TEST_ASSERT_EQUAL_STRING("NTSEAL1:TIxbW8kkXNJBfv3W5Ol0tn9hf2jZQLJGI65h3UjksyngXnHROaBVPdFvLhUKcmme4fjgKGclr28MY-0jfSb"
                             "5SsYiyIVJpSg0NwtGiWGd2chgAofx8oqj243EP1jxp32nncdyY34bCnr7gZo7kOZLQBA34HtB0JwKtYkgghm0WqvMaGn"
                             "LnMqMSoPqm7h4_Wophdet8Vh7xe5pB2Xcm2HSRBdvy_ws990hIQ",
                             sealed);
    TEST_ASSERT_EQUAL_size_t(strlen(sealed), sealed_size);
    TEST_ASSERT_EQUAL_size_t(sealed_size + 1u, game_save_seal_capacity(strlen(plain)));
}

static void test_a_tampered_seal_is_rejected(void) {
    char doc[DOC_CAP];
    profile_t profile;
    progress_state_init_defaults(&profile.progress);
    wallet_state_init_defaults(&profile.wallet);
    profile.progress.xp = 99;
    const game_state_doc_header_t header = {.save_id = 42u, .rev = 1};
    const size_t size = write_profile(&profile, &header, doc, sizeof doc);

    char sealed[2048], again[2048], plain[DOC_CAP];
    size_t sealed_size = 0, plain_size = 0;
    TEST_ASSERT_TRUE(game_save_seal(k_key, doc, size, sealed, sizeof sealed, &sealed_size));
    TEST_ASSERT_TRUE(game_save_seal(k_key, doc, size, again, sizeof again, NULL));
    TEST_ASSERT_EQUAL_STRING(sealed, again); /* deterministic: sync compares bytes */
    TEST_ASSERT_TRUE(game_save_unseal(k_key, sealed, sealed_size, plain, sizeof plain, &plain_size, NULL, 0));
    TEST_ASSERT_EQUAL_size_t(size, plain_size);
    TEST_ASSERT_EQUAL_STRING(doc, plain);

    char error[128] = {0};
    for (size_t at = 8; at < sealed_size; at += 7) {
        memcpy(again, sealed, sealed_size + 1u);
        again[at] = again[at] == 'A' ? 'B' : 'A';
        TEST_ASSERT_FALSE(game_save_unseal(k_key, again, sealed_size, plain, sizeof plain, NULL, error,
                                           (int)sizeof error));
        TEST_ASSERT_EQUAL_CHAR('\0', plain[0]);
    }
    /* Non-canonical digits fail and leave no partial ciphertext behind. */
    const char invalid[] = {'=', '\n', ' ', '+', '/'};
    for (size_t k = 0; k < sizeof invalid; k++) {
        memcpy(again, sealed, sealed_size + 1u);
        again[sealed_size - 3u] = invalid[k];
        memset(plain, 'Z', sizeof plain);
        TEST_ASSERT_FALSE(game_save_unseal(k_key, again, sealed_size, plain, sizeof plain, NULL, NULL, 0));
        for (size_t i = 0; i < size; i++) TEST_ASSERT_EQUAL_CHAR('\0', plain[i]);
    }
    TEST_ASSERT_FALSE(game_save_unseal(k_key, sealed, sealed_size - 1u, plain, sizeof plain, NULL, NULL, 0));
    TEST_ASSERT_EQUAL_size_t(0u, game_save_seal_capacity(SIZE_MAX));
    TEST_ASSERT_EQUAL_size_t(0u, game_save_seal_capacity(SIZE_MAX - 100u));
    TEST_ASSERT_FALSE(game_save_seal(k_key, doc, SIZE_MAX, again, sizeof again, NULL));
    TEST_ASSERT_TRUE(game_save_unseal_capacity(SIZE_MAX) > SIZE_MAX / 2u);
    uint8_t wrong[GAME_SAVE_SEAL_KEY_SIZE];
    memcpy(wrong, k_key, sizeof wrong);
    wrong[31] ^= 1u;
    TEST_ASSERT_FALSE(game_save_unseal(wrong, sealed, sealed_size, plain, sizeof plain, NULL, NULL, 0));
    TEST_ASSERT_FALSE(game_save_unseal(k_key, doc, size, plain, sizeof plain, NULL, NULL, 0));
}

/* ---- Solo-sync rule on sealed fixture documents ----
   choose_by_progress is the reference policy a game copies: it compares only
   documents this build can read at the current version, and never lets an
   unreadable or newer document be overwritten without the player. */

typedef struct {
    char text[2048];
} sealed_doc_t;

static sealed_doc_t seal_text(const char *plain) {
    sealed_doc_t sealed;
    TEST_ASSERT_TRUE(game_save_seal(k_key, plain, strlen(plain), sealed.text, sizeof sealed.text, NULL));
    return sealed;
}

static sealed_doc_t make_doc(uint64_t save_id, int64_t rev, int64_t xp, int level) {
    profile_t profile;
    progress_state_init_defaults(&profile.progress);
    wallet_state_init_defaults(&profile.wallet);
    profile.progress.xp = xp;
    profile.progress.level = level;
    const game_state_doc_header_t header = {.save_id = save_id, .rev = rev};
    char doc[DOC_CAP];
    (void)write_profile(&profile, &header, doc, sizeof doc);
    return seal_text(doc);
}

/* Unseal, migrate to the current version, read. False for a bad seal, a
   document newer than this build, or one that does not parse. */
static bool open_doc(const char *sealed, profile_t *profile, game_state_doc_header_t *header) {
    char plain[DOC_CAP], current[DOC_CAP];
    size_t size = 0;
    if (!game_save_unseal(k_key, sealed, strlen(sealed), plain, sizeof plain, &size, NULL, 0)) return false;
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, current, sizeof current);
    return game_state_doc_migrate(&k_schema, plain, size, &writer, NULL, 0) &&
           read_profile(current, game_save_text_writer_size(&writer), profile, header);
}

static int s_choose_calls;

/* Another lineage: the cloud wins. Same lineage: the whole document with more
   progress (total XP, then level) wins, and a tie keeps the cloud. */
static game_save_choice_t choose_by_progress(const char *local, const char *remote, void *user) {
    (void)user;
    s_choose_calls++;
    profile_t l, r;
    game_state_doc_header_t lh, rh;
    if (!open_doc(local, &l, &lh) || !open_doc(remote, &r, &rh)) return GAME_SAVE_ASK;
    if (lh.save_id != rh.save_id) return GAME_SAVE_KEEP_REMOTE;
    if (l.progress.xp != r.progress.xp) {
        return l.progress.xp > r.progress.xp ? GAME_SAVE_KEEP_LOCAL : GAME_SAVE_KEEP_REMOTE;
    }
    return l.progress.level > r.progress.level ? GAME_SAVE_KEEP_LOCAL : GAME_SAVE_KEEP_REMOTE;
}

static game_save_sync_state_t sync_outcome(game_save_sync_t *sync, const sealed_doc_t *base,
                                           const sealed_doc_t *local, const sealed_doc_t *remote) {
    game_save_sync_init(sync);
    TEST_ASSERT_TRUE(game_save_sync_set_base(sync, base->text));
    TEST_ASSERT_TRUE(game_save_sync_set_local(sync, local->text, false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(sync, remote->text));
    return game_save_sync_decide(sync, choose_by_progress, NULL);
}

static void test_the_four_sync_outcomes(void) {
    const uint64_t id = 0xabcdefu;
    const sealed_doc_t base = make_doc(id, 5, 1000, 3);
    game_save_sync_t sync;

    /* 1. Cloud unchanged since base: keep local, upload it. */
    s_choose_calls = 0;
    const sealed_doc_t played = make_doc(id, 6, 1300, 4);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_UPLOAD_READY, sync_outcome(&sync, &base, &played, &base));
    TEST_ASSERT_EQUAL_STRING(played.text, game_save_sync_upload_document(&sync));
    TEST_ASSERT_EQUAL_INT(0, s_choose_calls);
    game_save_sync_destroy(&sync);

    /* 2. Local unchanged, cloud changed: adopt the cloud; nothing is displaced. */
    const sealed_doc_t elsewhere = make_doc(id, 6, 1100, 3);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_ADOPT_REMOTE, sync_outcome(&sync, &base, &base, &elsewhere));
    TEST_ASSERT_TRUE(game_save_sync_commit_remote_adoption(&sync));
    TEST_ASSERT_EQUAL_STRING(elsewhere.text, game_save_sync_base_document(&sync));
    TEST_ASSERT_NULL(game_save_sync_displaced_document(&sync));
    TEST_ASSERT_EQUAL_INT(0, s_choose_calls);
    game_save_sync_destroy(&sync);

    /* 3. Both changed: more progress wins the whole document; a tie keeps the cloud. */
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH, sync_outcome(&sync, &base, &played, &elsewhere));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, elsewhere.text));
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_UPLOAD_READY, game_save_sync_state(&sync));
    TEST_ASSERT_EQUAL_STRING(played.text, game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_ADOPT_REMOTE, sync_outcome(&sync, &base, &elsewhere, &played));
    game_save_sync_destroy(&sync);
    const sealed_doc_t tie_local = make_doc(id, 7, 1300, 4);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_ADOPT_REMOTE, sync_outcome(&sync, &base, &tie_local, &played));
    TEST_ASSERT_EQUAL_STRING(played.text, game_save_sync_remote_document_value(&sync));
    game_save_sync_destroy(&sync);

    /* 4. Another save_id: the cloud wins even against more local progress, and
       the local document it replaces is kept, not destroyed. */
    const sealed_doc_t other_account = make_doc(0x5151u, 1, 10, 1);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_ADOPT_REMOTE, sync_outcome(&sync, &base, &played, &other_account));
    TEST_ASSERT_EQUAL_STRING(other_account.text, game_save_sync_remote_document_value(&sync));
    TEST_ASSERT_EQUAL_INT(4, s_choose_calls);
    TEST_ASSERT_TRUE(game_save_sync_commit_remote_adoption(&sync));
    TEST_ASSERT_EQUAL_STRING(other_account.text, game_save_sync_local_document(&sync));
    TEST_ASSERT_EQUAL_STRING(played.text, game_save_sync_displaced_document(&sync));

    /* An unpersisted displaced copy blocks the next adoption that would displace. */
    const sealed_doc_t second_local = make_doc(0x5151u, 2, 20, 1);
    const sealed_doc_t second_remote = make_doc(0x5151u, 3, 30, 1);
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, second_local.text, false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, second_remote.text));
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_ADOPT_REMOTE, game_save_sync_decide(&sync, choose_by_progress, NULL));
    TEST_ASSERT_FALSE(game_save_sync_commit_remote_adoption(&sync));
    TEST_ASSERT_EQUAL_STRING(second_local.text, game_save_sync_local_document(&sync));
    TEST_ASSERT_EQUAL_STRING(played.text, game_save_sync_displaced_document(&sync));
    game_save_sync_clear_displaced(&sync);
    TEST_ASSERT_TRUE(game_save_sync_commit_remote_adoption(&sync));
    TEST_ASSERT_EQUAL_STRING(second_local.text, game_save_sync_displaced_document(&sync));
    game_save_sync_destroy(&sync);
}

static void test_the_rule_compares_migrated_documents_and_asks_about_the_rest(void) {
    const uint64_t id = 0x00c0ffee00c0ffeeULL;
    const sealed_doc_t base = make_doc(id, 2, 1000, 3);
    const sealed_doc_t remote = make_doc(id, 4, 1100, 3);
    game_save_sync_t sync;

    /* A local save from an older build is migrated, not discarded: its 1200 XP win. */
    const sealed_doc_t stale_local = seal_text(k_v1_document);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH, sync_outcome(&sync, &base, &stale_local, &remote));
    game_save_sync_destroy(&sync);

    /* Newer than this build, or unreadable: never overwritten automatically. */
    const sealed_doc_t newer_local =
        seal_text("NTGS 1\nsave_version=3\nsave_id=\"00c0ffee00c0ffee\"\nrev=9\n\n[progress 2]\nxp=1\n");
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_CONFLICT, sync_outcome(&sync, &base, &newer_local, &remote));
    game_save_sync_destroy(&sync);
    sealed_doc_t broken_remote;
    (void)snprintf(broken_remote.text, sizeof broken_remote.text, "%s", "NTSEAL1:AAAA");
    const sealed_doc_t played = make_doc(id, 3, 5000, 9);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_SYNC_CONFLICT, sync_outcome(&sync, &base, &played, &broken_remote));
    game_save_sync_destroy(&sync);
}

static bool migrate_text(const char *text, char *out, size_t cap, char *error, int error_cap) {
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, out, cap);
    return game_state_doc_migrate(&k_schema, text, strlen(text), &writer, error, error_cap);
}

static void test_migration_keeps_big_integers_exact_or_refuses(void) {
    char out[DOC_CAP];
    char error[160] = {0};
    /* progress is current and no step touches it, so it is copied as written. */
    const char *untouched = "NTGS 1\nsave_version=1\nsave_id=\"0000000000000001\"\nrev=0\n\n"
                            "[progress 2]\nxp=9007199254740993\nlevel=2\n\n[purse 1]\ncoins=1\n";
    TEST_ASSERT_TRUE_MESSAGE(migrate_text(untouched, out, sizeof out, error, (int)sizeof error), error);
    profile_t loaded;
    TEST_ASSERT_TRUE(read_profile(out, strlen(out), &loaded, NULL));
    TEST_ASSERT_EQUAL_INT64(9007199254740993LL, loaded.progress.xp);
    TEST_ASSERT_EQUAL_UINT32(1u, loaded.wallet.coins);

    /* A step does touch it: refused, naming the field, instead of an unreadable document. */
    const char *stepped = "NTGS 1\nsave_version=2\nsave_id=\"0000000000000001\"\nrev=0\n\n"
                          "[progress 1]\nexperience=9007199254740993\nlevel=2\n";
    error[0] = '\0';
    TEST_ASSERT_FALSE(migrate_text(stepped, out, sizeof out, error, (int)sizeof error));
    TEST_ASSERT_NOT_NULL(strstr(error, "2^53"));
    TEST_ASSERT_NOT_NULL(strstr(error, "xp"));

    char long_id[DOC_CAP];
    char name[140];
    memset(name, 'a', sizeof name - 1u);
    name[sizeof name - 1u] = '\0';
    (void)snprintf(long_id, sizeof long_id, "NTGS 1\nsave_version=1\nsave_id=\"0000000000000001\"\nrev=0\n\n[%s 1]\n",
                   name);
    error[0] = '\0';
    TEST_ASSERT_FALSE(migrate_text(long_id, out, sizeof out, error, (int)sizeof error));
    TEST_ASSERT_NOT_EQUAL(0, error[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sixteen_profiles_round_trip_in_one_process);
    RUN_TEST(test_a_missing_fragment_reads_as_defaults_and_an_unknown_one_fails);
    RUN_TEST(test_an_invalid_state_is_never_written);
    RUN_TEST(test_a_v1_fixture_migrates_to_v2);
    RUN_TEST(test_the_seal_matches_its_reference_vector);
    RUN_TEST(test_a_tampered_seal_is_rejected);
    RUN_TEST(test_the_four_sync_outcomes);
    RUN_TEST(test_the_rule_compares_migrated_documents_and_asks_about_the_rest);
    RUN_TEST(test_migration_keeps_big_integers_exact_or_refuses);
    return UNITY_END();
}
