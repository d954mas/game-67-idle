#include "features/scenes/scene_manager.h"

#include "unity.h"

#include <stdio.h>
#include <string.h>

static scene_load_result_t load_ready(void *scene) {
    (void)scene;
    return SCENE_LOAD_READY;
}

static const scene_api_t k_api = {
    .load_step = load_ready,
};

void setUp(void) {}
void tearDown(void) {}

static void test_catalog_lookup_and_fixed_initial_state(void) {
    int root_instance = 1;
    int modal_instance = 2;
    const scene_descriptor_t catalog[] = {
        {
            .id = "root",
            .kind = SCENE_KIND_SCREEN,
            .keep_loaded = true,
            .instance = &root_instance,
            .api = &k_api,
        },
        {
            .id = "settings",
            .kind = SCENE_KIND_MODAL,
            .instance = &modal_instance,
            .api = &k_api,
        },
    };
    scene_manager_t manager;

    scene_manager_init(&manager, catalog, 2);

    TEST_ASSERT_TRUE(scene_manager_has_scene(&manager, "root"));
    TEST_ASSERT_TRUE(scene_manager_has_scene(&manager, "settings"));
    TEST_ASSERT_FALSE(scene_manager_has_scene(&manager, "missing"));
    TEST_ASSERT_EQUAL_PTR(&catalog[0], scene_manager_find_scene(&manager, "root"));
    TEST_ASSERT_NULL(scene_manager_find_scene(&manager, "missing"));
    TEST_ASSERT_EQUAL_UINT32(2, scene_manager_scene_count(&manager));
    TEST_ASSERT_EQUAL_UINT32(0, scene_manager_history_count(&manager));
    TEST_ASSERT_EQUAL(SCENE_RESIDENCY_UNLOADED,
                      scene_manager_scene_residency(&manager, &catalog[0]));
}

static void test_catalog_accepts_the_documented_maximum(void) {
    scene_descriptor_t catalog[SCENE_MANAGER_MAX_SCENES] = {0};
    char ids[SCENE_MANAGER_MAX_SCENES][16] = {{0}};
    int instances[SCENE_MANAGER_MAX_SCENES] = {0};
    scene_manager_t manager;

    for (size_t index = 0; index < SCENE_MANAGER_MAX_SCENES; ++index) {
        (void)snprintf(ids[index], sizeof ids[index], "scene_%02zu", index);
        instances[index] = (int)index;
        catalog[index] = (scene_descriptor_t){
            .id = ids[index],
            .kind = SCENE_KIND_SCREEN,
            .instance = &instances[index],
            .api = &k_api,
        };
    }

    scene_manager_init(
        &manager, catalog, SCENE_MANAGER_MAX_SCENES);

    TEST_ASSERT_EQUAL_UINT32(
        SCENE_MANAGER_MAX_SCENES,
        scene_manager_scene_count(&manager));
    TEST_ASSERT_EQUAL_PTR(
        &catalog[SCENE_MANAGER_MAX_SCENES - 1],
        scene_manager_find_scene(&manager, ids[SCENE_MANAGER_MAX_SCENES - 1]));

    scene_manager_shutdown(&manager);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_catalog_lookup_and_fixed_initial_state);
    RUN_TEST(test_catalog_accepts_the_documented_maximum);
    return UNITY_END();
}
