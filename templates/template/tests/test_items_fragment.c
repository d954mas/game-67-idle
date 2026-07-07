/* System headers before Unity to avoid noreturn / __declspec conflict on MSVC
   (unity_internals.h pulls in <stdnoreturn.h>; ср. test_game_save.c). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

/* Unity is built with UNITY_EXCLUDE_FLOAT (engine deps/unity CMakeLists, see
   test_game_state_json.c) -- no TEST_ASSERT_EQUAL_FLOAT; compare with fabsf + epsilon. */
#define ITEMS_TEST_FLOAT_EPS 0.0001f

#include "features/items/items.h"
#include "game_events.h"
#include "items_state.h"
#include "items_state_events.gen.h"

/* И2a skeleton -> И2b full (§6.6c/§6.8/§7.4). Links items_state.c + items_state_
   events.gen.c (R2: not empty, unlike settings) + items_bootstrap.c (real Р9
   bodies, §7.3) + items_containers.c (И2b ownership) + items_catalog.c/
   items_catalog.gen.c (item_core, reconcile/containers need it) + game_events.c;
   game_save.c is NOT linked -- items_containers.c/items_bootstrap.c only need
   game_save_mark_dirty(), stubbed below (precedent: test_game_state_roundtrip,
   code-review #11). */
void game_save_mark_dirty(void) {}

void setUp(void) { game_event_frame_reset(); /* per-test event-log isolation */ }
void tearDown(void) {}

/* reset() -> owned{} empty (Р9: reset is the NEUTRAL default, distinct from
   on_new_game). */
void test_reset_owned_empty(void) {
    items_state_fragment.reset();
    cJSON *json = items_state_to_json(&items_state);
    TEST_ASSERT_NOT_NULL(json);
    const cJSON *owned = cJSON_GetObjectItemCaseSensitive(json, "owned");
    TEST_ASSERT_NOT_NULL(owned);
    TEST_ASSERT_TRUE(cJSON_IsObject(owned));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(owned));
    cJSON_Delete(json);
}

/* init_defaults -> to_json -> from_json (other instance) -> to_json: equal (G4 groundwork). */
void test_round_trip_byte_stable(void) {
    ItemsState a;
    items_state_init_defaults(&a);
    cJSON *ja = items_state_to_json(&a);
    TEST_ASSERT_NOT_NULL(ja);
    char *sa = cJSON_PrintUnformatted(ja);
    TEST_ASSERT_NOT_NULL(sa);

    ItemsState b;
    char err[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&b, ja, err, (int)sizeof(err)));
    cJSON *jb = items_state_to_json(&b);
    char *sb = cJSON_PrintUnformatted(jb);
    TEST_ASSERT_NOT_NULL(sb);

    TEST_ASSERT_EQUAL_STRING(sa, sb);

    cJSON_free(sa);
    cJSON_free(sb);
    cJSON_Delete(ja);
    cJSON_Delete(jb);
}

void test_schema_json_contains_owned(void) {
    cJSON *schema = items_state_schema_json();
    TEST_ASSERT_NOT_NULL(schema);
    char *text = cJSON_PrintUnformatted(schema);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "owned"));
    cJSON_free(text);
    cJSON_Delete(schema);
}

/* fixtures/items_v1.json (§6.8/§9): v1-payload migration anchor -- tolerant
   from_json loads it. WORKING_DIRECTORY for this test is tests/ (CMakeLists.txt),
   so the relative path below resolves regardless of ctest's own cwd. */
void test_fixture_v1_loads(void) {
    FILE *f = fopen("fixtures/items_v1.json", "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)size + 1);
    TEST_ASSERT_NOT_NULL(buf);
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    TEST_ASSERT_NOT_NULL(json);

    ItemsState state;
    char err[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&state, json, err, (int)sizeof(err)));
    cJSON_Delete(json);
}

/* ---- И2b: ownership / containers / purse (§7.1/§7.4) ---- */

/* Стаки/count: add 3x -> sums; remove beyond remainder -> false, remainder
   intact; remove to 0 -> slot freed. */
void test_add_accumulate_and_remove_stack(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 10, "loot:test"));
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 10, "loot:test"));
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 10, "loot:test"));
    TEST_ASSERT_EQUAL_INT64(30, items_count("purse", "tmpl.gold"));

    TEST_ASSERT_FALSE(items_remove("purse", "tmpl.gold", 999, "sell:test"));
    TEST_ASSERT_EQUAL_INT64(30, items_count("purse", "tmpl.gold"));

    TEST_ASSERT_TRUE(items_remove("purse", "tmpl.gold", 30, "sell:test"));
    TEST_ASSERT_EQUAL_INT64(0, items_count("purse", "tmpl.gold"));

    bool slot_freed = true;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].key, "purse/tmpl.gold") == 0) {
            slot_freed = false;
        }
    }
    TEST_ASSERT_TRUE(slot_freed);
}

/* Контейнеры/accept-policy: add currency without explicit container -> purse;
   purse (currency_only) rejects a non-currency add. */
void test_add_default_container_and_currency_only_policy(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add(NULL, "tmpl.gold", 5, "loot:test"));
    TEST_ASSERT_EQUAL_INT64(5, items_count("purse", "tmpl.gold"));

    TEST_ASSERT_FALSE(items_add("purse", "tmpl.wood", 1, "loot:test"));
    TEST_ASSERT_EQUAL_INT64(0, items_count("purse", "tmpl.wood"));
}

/* T10: a non-currency def with no explicit container also defaults correctly --
   to backpack, not purse (purse is currency_only and would reject it). */
void test_add_default_container_noncurrency_lands_in_backpack(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add(NULL, "tmpl.wood", 4, "loot:test"));
    TEST_ASSERT_EQUAL_INT64(4, items_count("backpack", "tmpl.wood"));
}

/* capacity REJECT (M1): backpack capacity=20 -- 20 distinct records fill it; a
   21st NEW def is rejected, but growing one of the 20 existing records is not
   capacity-limited. The first 19 "slots" are fabricated directly (white-box) so
   the test does not depend on the demo catalog having 20 real items. */
void test_capacity_reject_new_record_grow_existing_ok(void) {
    items_state_fragment.reset();
    for (int i = 0; i < 19; ++i) {
        ItemsItemOwned *slot = &items_state.owned[i];
        slot->used = true;
        (void)snprintf(slot->key, sizeof slot->key, "backpack/fake_%d", i);
        (void)snprintf(slot->def_id, sizeof slot->def_id, "fake_%d", i);
        (void)snprintf(slot->container, sizeof slot->container, "backpack");
        slot->count = 1;
    }

    TEST_ASSERT_TRUE(items_add("backpack", "tmpl.wood", 5, "loot:test")); /* 20th distinct record */
    TEST_ASSERT_EQUAL_INT64(5, items_count("backpack", "tmpl.wood"));

    TEST_ASSERT_TRUE(items_add("backpack", "tmpl.wood", 3, "loot:test")); /* growth, not new -> OK */
    TEST_ASSERT_EQUAL_INT64(8, items_count("backpack", "tmpl.wood"));

    TEST_ASSERT_FALSE(items_add("backpack", "tmpl.potion", 1, "loot:test")); /* 21st NEW def -> REJECT */
    TEST_ASSERT_EQUAL_INT64(0, items_count("backpack", "tmpl.potion"));
}

/* currency.cap CLAMP (M3): add sums past cap clamp to cap, do not reject. */
void test_currency_cap_clamp(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.energy", 150, "loot:test"));
    TEST_ASSERT_EQUAL_INT64(100, items_count("purse", "tmpl.energy"));
}

/* key-authority (M2, G7): stack .container field == prefix of its .key; move
   re-keys, invariant holds after. */
void test_stack_key_authority(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 10, "loot:test"));

    bool found_before = false;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].key, "purse/tmpl.gold") == 0) {
            TEST_ASSERT_EQUAL_STRING("purse", items_state.owned[i].container);
            found_before = true;
        }
    }
    TEST_ASSERT_TRUE(found_before);

    TEST_ASSERT_TRUE(items_move("purse", "backpack", "tmpl.gold", 10, "loot:test"));

    bool old_present = false;
    bool new_present = false;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (!items_state.owned[i].used) {
            continue;
        }
        if (strcmp(items_state.owned[i].key, "purse/tmpl.gold") == 0) {
            old_present = true;
        }
        if (strcmp(items_state.owned[i].key, "backpack/tmpl.gold") == 0) {
            TEST_ASSERT_EQUAL_STRING("backpack", items_state.owned[i].container);
            new_present = true;
        }
    }
    TEST_ASSERT_FALSE(old_present);
    TEST_ASSERT_TRUE(new_present);
    TEST_ASSERT_EQUAL_INT64(0, items_count("purse", "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(10, items_count("backpack", "tmpl.gold"));
}

/* T7 (M2): moving within the SAME container is a no-op -- count unchanged, no
   txn emitted (nothing about ownership changed). */
void test_self_move_is_noop(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 30, "loot:test_txn"));
    game_event_frame_reset();

    TEST_ASSERT_TRUE(items_move("purse", "purse", "tmpl.gold", 30, "loot:test_txn"));
    TEST_ASSERT_EQUAL_INT64(30, items_count("purse", "tmpl.gold"));

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t txn_type = items_ev_txn_type();
    for (int i = 0; i < n; ++i) {
        TEST_ASSERT_FALSE(log[i].type.value == txn_type.value);
    }
}

/* T8 (M3): moving a currency stack into a container where it hits its cap
   conserves the TOTAL sum -- the untransferable remainder stays in the source
   instead of being silently destroyed. */
void test_move_capped_currency_conserves_total_sum(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.energy", 80, "loot:test_txn"));    /* under cap=100 */
    TEST_ASSERT_TRUE(items_add("backpack", "tmpl.energy", 50, "loot:test_txn")); /* separate record, own cap room */

    int64_t before_sum = items_count("purse", "tmpl.energy") + items_count("backpack", "tmpl.energy");
    TEST_ASSERT_EQUAL_INT64(130, before_sum);

    TEST_ASSERT_TRUE(items_move("backpack", "purse", "tmpl.energy", 50, "loot:test_txn"));

    TEST_ASSERT_EQUAL_INT64(100, items_count("purse", "tmpl.energy"));   /* clamped to cap */
    TEST_ASSERT_EQUAL_INT64(30, items_count("backpack", "tmpl.energy")); /* the 20 that didn't fit stayed put */

    int64_t after_sum = items_count("purse", "tmpl.energy") + items_count("backpack", "tmpl.energy");
    TEST_ASSERT_EQUAL_INT64(before_sum, after_sum);
}

/* T1: unique route re-parents via the FIELD, never the key (§2.3) -- use a
   currency def so both backpack (any) and purse (currency_only) accept it, to
   isolate the field-authority behavior from accept-policy. Then M4: filling the
   destination to its capacity REJECTs a unique move in exactly the same way it
   would reject allocating a new stack record there. */
void test_unique_move_field_authority_and_capacity_reject(void) {
    items_state_fragment.reset();

    const char *iid = items_instance_create("backpack", "tmpl.gold", "loot:test");
    TEST_ASSERT_NOT_NULL(iid);
    char iid_copy[ITEMS_STATE_STRING_MAX];
    (void)snprintf(iid_copy, sizeof iid_copy, "%s", iid); /* alloc_owned_slot -> index 0, first alloc after reset */

    TEST_ASSERT_TRUE(items_move("backpack", "purse", iid_copy, 1, "loot:test"));

    ItemsItemOwned *rec = NULL;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].key, iid_copy) == 0) {
            rec = &items_state.owned[i];
        }
    }
    TEST_ASSERT_NOT_NULL(rec);
    TEST_ASSERT_EQUAL_STRING("purse", rec->container); /* field changed */
    TEST_ASSERT_EQUAL_STRING(iid_copy, rec->key);       /* key untouched -- field authority for uniques */

    /* Fill backpack to its capacity=20 with fabricated distinct records (slots
       1..20, away from the gold record's slot 0) -- then moving the unique INTO
       backpack must be REJECTed the same way a brand-new stack record would be. */
    for (int i = 0; i < 20; ++i) {
        ItemsItemOwned *slot = &items_state.owned[i + 1];
        slot->used = true;
        (void)snprintf(slot->key, sizeof slot->key, "backpack/fake_%d", i);
        (void)snprintf(slot->def_id, sizeof slot->def_id, "fake_%d", i);
        (void)snprintf(slot->container, sizeof slot->container, "backpack");
        slot->count = 1;
    }

    TEST_ASSERT_FALSE(items_move("purse", "backpack", iid_copy, 1, "loot:test"));
    TEST_ASSERT_EQUAL_STRING("purse", rec->container); /* rejected -> stayed put */
}

/* T2 (H1): reconcile must reseed the unique-instance sequence above every
   "<def_id>#<seq>" key already present after a load -- otherwise a freshly
   created unique can collide with one already in the save. Uses an
   implausibly high fabricated seq so this is robust regardless of how many
   uniques earlier tests in this binary happened to create. */
void test_seq_reseed_after_reconcile(void) {
    items_state_fragment.reset();

    ItemsItemOwned *rec = &items_state.owned[0];
    rec->used = true;
    (void)snprintf(rec->key, sizeof rec->key, "tmpl.sword#5000000");
    (void)snprintf(rec->def_id, sizeof rec->def_id, "tmpl.sword");
    (void)snprintf(rec->container, sizeof rec->container, "backpack");
    rec->count = 1;
    rec->level = 1;
    rec->durability = 1.0F;
    rec->quarantined = false;

    items_state_fragment.reconcile();

    const char *iid = items_instance_create("backpack", "tmpl.sword", "loot:test");
    TEST_ASSERT_NOT_NULL(iid);
    TEST_ASSERT_TRUE(strcmp(iid, "tmpl.sword#5000000") != 0);

    const char *hash = strrchr(iid, '#');
    TEST_ASSERT_NOT_NULL(hash);
    long long new_seq = atoll(hash + 1);
    TEST_ASSERT_TRUE(new_seq > 5000000);
}

/* Инвариант «ровно один контейнер»: after a move, exactly one owned record
   carries the def -- sum conserved, never present in two containers at once. */
void test_move_invariant_single_container(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 25, "loot:test"));
    TEST_ASSERT_TRUE(items_move("purse", "backpack", "tmpl.gold", 25, "loot:test"));

    TEST_ASSERT_EQUAL_INT64(0, items_count("purse", "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(25, items_count("backpack", "tmpl.gold"));

    int occurrences = 0;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].def_id, "tmpl.gold") == 0) {
            ++occurrences;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, occurrences);
}

/* Уник-пул: instance_create -> non-empty id, count=1, per-copy defaults; destroy -> gone. */
void test_instance_pool_create_destroy(void) {
    items_state_fragment.reset();
    const char *iid = items_instance_create("backpack", "tmpl.sword", "loot:test");
    TEST_ASSERT_NOT_NULL(iid);
    TEST_ASSERT_TRUE(iid[0] != '\0');

    ItemsItemOwned *rec = NULL;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].key, iid) == 0) {
            rec = &items_state.owned[i];
        }
    }
    TEST_ASSERT_NOT_NULL(rec);
    TEST_ASSERT_EQUAL_INT64(1, rec->count);
    TEST_ASSERT_EQUAL_INT(ITEMS_STATE_ITEM_OWNED_LEVEL_DEFAULT, rec->level);
    TEST_ASSERT_TRUE(fabsf(rec->durability - ITEMS_STATE_ITEM_OWNED_DURABILITY_DEFAULT) < ITEMS_TEST_FLOAT_EPS);
    TEST_ASSERT_EQUAL_STRING("backpack", rec->container);

    char iid_copy[ITEMS_STATE_STRING_MAX];
    (void)snprintf(iid_copy, sizeof iid_copy, "%s", iid);
    TEST_ASSERT_TRUE(items_instance_destroy(iid_copy, "loot:test"));

    bool still_present = false;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].key, iid_copy) == 0) {
            still_present = true;
        }
    }
    TEST_ASSERT_FALSE(still_present);
}

void test_purse_helper(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 7, "loot:test"));
    TEST_ASSERT_EQUAL_INT64(7, items_purse("tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(items_count("purse", "tmpl.gold"), items_purse("tmpl.gold"));
}

/* on_new_game (Р9/G5): reset -> empty; on_new_game -> 50 gold in purse + 1
   potion in backpack; reset AFTER -> empty again (proves reset != on_new_game). */
void test_on_new_game_bootstrap(void) {
    items_state_fragment.reset();
    cJSON *json_empty = items_state_to_json(&items_state);
    const cJSON *owned_empty = cJSON_GetObjectItemCaseSensitive(json_empty, "owned");
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(owned_empty));
    cJSON_Delete(json_empty);

    items_state_fragment.on_new_game();
    TEST_ASSERT_EQUAL_INT64(50, items_count("purse", "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(1, items_count("backpack", "tmpl.potion"));

    items_state_fragment.reset();
    cJSON *json_after = items_state_to_json(&items_state);
    const cJSON *owned_after = cJSON_GetObjectItemCaseSensitive(json_after, "owned");
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(owned_after));
    cJSON_Delete(json_after);
}

/* Карантин round-trip (G6): unknown def_id -> reconcile quarantines; excluded
   from live count; survives to_json/from_json; catalog restoration ->
   un-quarantined by a subsequent reconcile. */
void test_quarantine_round_trip(void) {
    items_state_fragment.reset();
    ItemsItemOwned *rec = &items_state.owned[0];
    rec->used = true;
    (void)snprintf(rec->key, sizeof rec->key, "backpack/ghost");
    (void)snprintf(rec->def_id, sizeof rec->def_id, "ghost");
    (void)snprintf(rec->container, sizeof rec->container, "backpack");
    rec->count = 3;
    rec->level = 1;
    rec->durability = 1.0F;
    rec->quarantined = false;

    items_state_fragment.reconcile();
    TEST_ASSERT_TRUE(items_state.owned[0].quarantined);
    TEST_ASSERT_EQUAL_INT64(0, items_count("backpack", "ghost"));

    cJSON *json = items_state_to_json(&items_state);
    TEST_ASSERT_NOT_NULL(json);
    ItemsState reloaded;
    char err[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&reloaded, json, err, (int)sizeof(err)));
    cJSON_Delete(json);

    bool found = false;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (reloaded.owned[i].used && strcmp(reloaded.owned[i].def_id, "ghost") == 0) {
            TEST_ASSERT_TRUE(reloaded.owned[i].quarantined);
            TEST_ASSERT_EQUAL_STRING("backpack", reloaded.owned[i].container);
            TEST_ASSERT_EQUAL_INT64(3, reloaded.owned[i].count); /* T9: count survives quarantine round-trip too */
            found = true;
        }
    }
    TEST_ASSERT_TRUE(found);

    /* Simulate catalog restoration: relabel the ghost record to a real def_id,
       then reconcile again -> un-quarantined. */
    items_state = reloaded;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].def_id, "ghost") == 0) {
            (void)snprintf(items_state.owned[i].def_id, sizeof items_state.owned[i].def_id, "tmpl.wood");
        }
    }
    items_state_fragment.reconcile();
    bool restored = false;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].def_id, "tmpl.wood") == 0) {
            TEST_ASSERT_FALSE(items_state.owned[i].quarantined);
            restored = true;
        }
    }
    TEST_ASSERT_TRUE(restored);
}

/* byte-stable (G4): on_new_game -> to_json A -> from_json -> to_json B -> A==B. */
void test_byte_stable_after_on_new_game(void) {
    items_state_fragment.reset();
    items_state_fragment.on_new_game();

    cJSON *ja = items_state_to_json(&items_state);
    TEST_ASSERT_NOT_NULL(ja);
    char *sa = cJSON_PrintUnformatted(ja);
    TEST_ASSERT_NOT_NULL(sa);

    ItemsState reloaded;
    char err[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&reloaded, ja, err, (int)sizeof(err)));
    cJSON *jb = items_state_to_json(&reloaded);
    char *sb = cJSON_PrintUnformatted(jb);
    TEST_ASSERT_NOT_NULL(sb);

    TEST_ASSERT_EQUAL_STRING(sa, sb);

    cJSON_free(sa);
    cJSON_free(sb);
    cJSON_Delete(ja);
    cJSON_Delete(jb);
}

/* items.txn (§10): add emits items.txn{def_id,delta,reason} in the shared event
   log (canon idiom, game_features.c:30-47); move does NOT emit (ownership of
   the def does not change, only its container). */
void test_items_txn_event_emitted_on_add(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 42, "loot:test_txn"));

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t txn_type = items_ev_txn_type();
    bool found = false;
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value == txn_type.value) {
            const ItemsEvTxn *e = (const ItemsEvTxn *)log[i].payload;
            if (strcmp(items_ev_txn_def_id(e), "tmpl.gold") == 0 && e->delta == 42 &&
                strcmp(items_ev_txn_reason(e), "loot:test_txn") == 0) {
                found = true;
            }
        }
    }
    TEST_ASSERT_TRUE(found);
}

void test_items_move_does_not_emit_txn(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 5, "loot:test_txn"));
    game_event_frame_reset(); /* isolate the move -- clear the add's own txn */
    TEST_ASSERT_TRUE(items_move("purse", "backpack", "tmpl.gold", 5, "loot:test_txn"));

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t txn_type = items_ev_txn_type();
    for (int i = 0; i < n; ++i) {
        TEST_ASSERT_FALSE(log[i].type.value == txn_type.value);
    }
}

/* Shared scan helpers for the txn-log assertions below (T3-T6). */
static bool txn_event_exists(const char *def_id, int64_t delta, const char *reason) {
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t txn_type = items_ev_txn_type();
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value == txn_type.value) {
            const ItemsEvTxn *e = (const ItemsEvTxn *)log[i].payload;
            if (strcmp(items_ev_txn_def_id(e), def_id) == 0 && e->delta == delta &&
                strcmp(items_ev_txn_reason(e), reason) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool any_txn_event_exists(void) {
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t txn_type = items_ev_txn_type();
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value == txn_type.value) {
            return true;
        }
    }
    return false;
}

/* T3: txn.delta must equal the CLAMPED amount actually applied (spec §7.4), not
   the requested amount -- add 150 into a cap=100 stack must emit delta==100. */
void test_txn_delta_equals_clamped_amount(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.energy", 150, "loot:test_txn"));
    TEST_ASSERT_EQUAL_INT64(100, items_count("purse", "tmpl.energy"));
    TEST_ASSERT_TRUE(txn_event_exists("tmpl.energy", 100, "loot:test_txn"));
}

/* T4: remove/instance_create/instance_destroy all emit items.txn with the
   correct signed delta (remove = -count, create = +1, destroy = -count). */
void test_txn_emitted_on_remove_and_instance_lifecycle(void) {
    items_state_fragment.reset();

    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 20, "loot:test_txn"));
    game_event_frame_reset();
    TEST_ASSERT_TRUE(items_remove("purse", "tmpl.gold", 5, "sell:test_txn"));
    TEST_ASSERT_TRUE(txn_event_exists("tmpl.gold", -5, "sell:test_txn"));

    game_event_frame_reset();
    const char *iid = items_instance_create("backpack", "tmpl.sword", "loot:test_txn");
    TEST_ASSERT_NOT_NULL(iid);
    char iid_copy[ITEMS_STATE_STRING_MAX];
    (void)snprintf(iid_copy, sizeof iid_copy, "%s", iid);
    TEST_ASSERT_TRUE(txn_event_exists("tmpl.sword", 1, "loot:test_txn"));

    game_event_frame_reset();
    TEST_ASSERT_TRUE(items_instance_destroy(iid_copy, "sell:test_txn"));
    TEST_ASSERT_TRUE(txn_event_exists("tmpl.sword", -1, "sell:test_txn"));
}

/* T5: a REJECTed add (capacity full) must not emit a txn -- failed mutations
   are silent, not partial events. */
void test_capacity_reject_emits_no_txn(void) {
    items_state_fragment.reset();
    for (int i = 0; i < 20; ++i) {
        ItemsItemOwned *slot = &items_state.owned[i];
        slot->used = true;
        (void)snprintf(slot->key, sizeof slot->key, "backpack/fake_%d", i);
        (void)snprintf(slot->def_id, sizeof slot->def_id, "fake_%d", i);
        (void)snprintf(slot->container, sizeof slot->container, "backpack");
        slot->count = 1;
    }
    game_event_frame_reset();
    TEST_ASSERT_FALSE(items_add("backpack", "tmpl.wood", 1, "loot:test_txn"));
    TEST_ASSERT_FALSE(any_txn_event_exists());
}

/* T6 (M1): adding near the schema's int64 max clamps to
   ITEMS_STATE_ITEM_OWNED_COUNT_MAX instead of overflowing; txn.delta reports
   only what actually fit. */
void test_add_overflow_clamps_to_schema_max(void) {
    items_state_fragment.reset();
    int64_t near_max = ITEMS_STATE_ITEM_OWNED_COUNT_MAX - 10;
    TEST_ASSERT_TRUE(items_add("backpack", "tmpl.wood", near_max, "loot:test_txn"));
    TEST_ASSERT_EQUAL_INT64(near_max, items_count("backpack", "tmpl.wood"));

    game_event_frame_reset();
    TEST_ASSERT_TRUE(items_add("backpack", "tmpl.wood", 1000, "loot:test_txn")); /* would overflow past MAX */
    TEST_ASSERT_EQUAL_INT64(ITEMS_STATE_ITEM_OWNED_COUNT_MAX, items_count("backpack", "tmpl.wood"));
    TEST_ASSERT_TRUE(txn_event_exists("tmpl.wood", 10, "loot:test_txn")); /* honest: only 10 fit before MAX */
}

int main(void) {
    game_events_init();
    UNITY_BEGIN();
    RUN_TEST(test_reset_owned_empty);
    RUN_TEST(test_round_trip_byte_stable);
    RUN_TEST(test_schema_json_contains_owned);
    RUN_TEST(test_fixture_v1_loads);
    RUN_TEST(test_add_accumulate_and_remove_stack);
    RUN_TEST(test_add_default_container_and_currency_only_policy);
    RUN_TEST(test_add_default_container_noncurrency_lands_in_backpack);
    RUN_TEST(test_capacity_reject_new_record_grow_existing_ok);
    RUN_TEST(test_currency_cap_clamp);
    RUN_TEST(test_stack_key_authority);
    RUN_TEST(test_self_move_is_noop);
    RUN_TEST(test_move_capped_currency_conserves_total_sum);
    RUN_TEST(test_unique_move_field_authority_and_capacity_reject);
    RUN_TEST(test_seq_reseed_after_reconcile);
    RUN_TEST(test_move_invariant_single_container);
    RUN_TEST(test_instance_pool_create_destroy);
    RUN_TEST(test_purse_helper);
    RUN_TEST(test_on_new_game_bootstrap);
    RUN_TEST(test_quarantine_round_trip);
    RUN_TEST(test_byte_stable_after_on_new_game);
    RUN_TEST(test_items_txn_event_emitted_on_add);
    RUN_TEST(test_items_move_does_not_emit_txn);
    RUN_TEST(test_txn_delta_equals_clamped_amount);
    RUN_TEST(test_txn_emitted_on_remove_and_instance_lifecycle);
    RUN_TEST(test_capacity_reject_emits_no_txn);
    RUN_TEST(test_add_overflow_clamps_to_schema_max);
    int result = UNITY_END();
    game_events_shutdown();
    return result;
}
