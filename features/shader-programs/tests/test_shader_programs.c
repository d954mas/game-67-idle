#include "unity.h"
#include "features/shader_programs/shader_programs.h"

#include "core/nt_assert.h"

#include <setjmp.h>
#include <string.h>

// Link-time fakes of the engine calls the cache makes. Handles carry a
// generation like the engine pools: a destroyed id stays invalid, and a slot
// reused by a new material gets a new id.
#define FAKE_RESOURCES 8
#define FAKE_PROGRAMS 1024
#define FAKE_MATERIALS 300

static uint32_t s_resource_shader[FAKE_RESOURCES];
static bool s_shader_ready[FAKE_RESOURCES];
static bool s_link_fails;
static bool s_programs_pending;
static bool s_material_pool_full;

static bool s_program_alive[FAKE_PROGRAMS];
static uint32_t s_next_program;
static int s_links;
static int s_destroys;
static const char *s_last_label;

static struct {
    bool used;
    uint32_t generation;
    nt_material_info_t info;
} s_slots[FAKE_MATERIALS];

uint32_t nt_resource_get(nt_resource_t handle) { return handle.id < FAKE_RESOURCES ? s_resource_shader[handle.id] : 0U; }

bool nt_gfx_shader_ready(nt_shader_t shd) {
    for (uint32_t r = 0; r < FAKE_RESOURCES; ++r) {
        if (s_resource_shader[r] == shd.id) { return s_shader_ready[r]; }
    }
    return false;
}

nt_program_t nt_gfx_make_program(nt_shader_t vs, nt_shader_t fs, const char *label) {
    TEST_ASSERT_TRUE(vs.id != 0U && fs.id != 0U);
    if (s_link_fails) { return NT_PROGRAM_INVALID; }
    s_links++;
    s_last_label = label;
    s_program_alive[++s_next_program] = true;
    return (nt_program_t){s_next_program};
}

void nt_gfx_destroy_program(nt_program_t prog) {
    if (prog.id == 0U) { return; }
    TEST_ASSERT_TRUE_MESSAGE(s_program_alive[prog.id], "program destroyed twice");
    s_program_alive[prog.id] = false;
    s_destroys++;
}

bool nt_gfx_program_ready(nt_program_t prog) { return prog.id != 0U && s_program_alive[prog.id] && !s_programs_pending; }

static int slot_of(nt_material_t mat) { return (int)(mat.id & 0xFFFFU) - 1; }

nt_material_t nt_material_create(const nt_material_create_desc_t *desc) {
    if (s_material_pool_full) { return NT_MATERIAL_INVALID; }
    for (int i = 0; i < FAKE_MATERIALS; ++i) {
        if (!s_slots[i].used) {
            s_slots[i].used = true;
            s_slots[i].generation++;
            memset(&s_slots[i].info, 0, sizeof s_slots[i].info);
            s_slots[i].info.program = desc->program;
            s_slots[i].info.label = desc->label;
            return (nt_material_t){(s_slots[i].generation << 16) | (uint32_t)(i + 1)};
        }
    }
    return NT_MATERIAL_INVALID;
}

bool nt_material_valid(nt_material_t mat) {
    const int slot = slot_of(mat);
    return slot >= 0 && slot < FAKE_MATERIALS && s_slots[slot].used && s_slots[slot].generation == mat.id >> 16;
}

void nt_material_set_program(nt_material_t mat, nt_program_t program) {
    TEST_ASSERT_TRUE(nt_material_valid(mat));
    s_slots[slot_of(mat)].info.program = program;
}

const nt_material_info_t *nt_material_get_info(nt_material_t mat) {
    return nt_material_valid(mat) ? &s_slots[slot_of(mat)].info : NULL;
}

static void destroy_material(nt_material_t mat) {
    TEST_ASSERT_TRUE(nt_material_valid(mat));
    s_slots[slot_of(mat)].used = false;
}

static nt_program_t program_of(nt_material_t mat) { return nt_material_get_info(mat)->program; }

static const nt_resource_t VS = {1}, FS = {2}, VS2 = {3}, FS2 = {4};

static nt_material_t create(nt_resource_t vs, nt_resource_t fs) {
    const nt_material_create_desc_t desc = {.label = "test_material"};
    return shader_program_material(vs, fs, &desc);
}

void setUp(void) {
    shader_programs_shutdown();
    memset(s_slots, 0, sizeof s_slots);
    memset(s_program_alive, 0, sizeof s_program_alive);
    s_next_program = 0;
    s_links = s_destroys = 0;
    s_link_fails = s_programs_pending = s_material_pool_full = false;
    s_last_label = NULL;
    for (uint32_t r = 1; r < FAKE_RESOURCES; ++r) {
        s_resource_shader[r] = 100U + r;
        s_shader_ready[r] = true;
    }
}

void tearDown(void) {}

static void test_one_link_per_pair(void) {
    const nt_material_t a = create(VS, FS);
    const nt_material_t b = create(VS, FS);
    const nt_material_t c = create(VS, FS2);
    shader_programs_update();
    shader_programs_update();
    TEST_ASSERT_EQUAL_INT(2, s_links);
    TEST_ASSERT_EQUAL_UINT32(program_of(a).id, program_of(b).id);
    TEST_ASSERT_NOT_EQUAL(program_of(a).id, program_of(c).id);
    TEST_ASSERT_TRUE(shader_program_ready(a) && shader_program_ready(c));
    TEST_ASSERT_EQUAL_STRING("test_material", s_last_label);
}

static void test_unready_stages_hold_the_link(void) {
    s_shader_ready[FS.id] = false;
    const nt_material_t a = create(VS, FS);
    shader_programs_update();
    TEST_ASSERT_EQUAL_INT(0, s_links);
    TEST_ASSERT_EQUAL_UINT32(0U, program_of(a).id);
    TEST_ASSERT_FALSE(shader_program_ready(a));

    s_shader_ready[FS.id] = true;
    s_programs_pending = true;
    shader_programs_update();
    TEST_ASSERT_EQUAL_INT(1, s_links);
    TEST_ASSERT_NOT_EQUAL(0U, program_of(a).id);
    TEST_ASSERT_FALSE(shader_program_ready(a));
    s_programs_pending = false;
    TEST_ASSERT_TRUE(shader_program_ready(a));
    // A material created after the link starts with the pair's program.
    TEST_ASSERT_EQUAL_UINT32(program_of(a).id, program_of(create(VS, FS)).id);
}

static void test_failed_link_retries(void) {
    const nt_material_t a = create(VS, FS);
    s_link_fails = true;
    shader_programs_update();
    TEST_ASSERT_EQUAL_UINT32(0U, program_of(a).id);
    s_link_fails = false;
    shader_programs_update();
    TEST_ASSERT_EQUAL_INT(1, s_links);
    TEST_ASSERT_TRUE(shader_program_ready(a));
}

static void test_restore_relinks_and_repoints(void) {
    const nt_material_t a = create(VS, FS);
    const nt_material_t b = create(VS2, FS2);
    shader_programs_update();
    const nt_program_t lost = program_of(a);
    shader_programs_restore_gpu();
    TEST_ASSERT_EQUAL_INT(2, s_destroys);
    TEST_ASSERT_FALSE(shader_program_ready(a));
    shader_programs_update();
    TEST_ASSERT_EQUAL_INT(4, s_links);
    TEST_ASSERT_TRUE(shader_program_ready(a) && shader_program_ready(b));
    TEST_ASSERT_NOT_EQUAL(lost.id, program_of(a).id);
}

static void test_dead_row_goes_to_the_material_in_its_slot(void) {
    const nt_material_t first = create(VS, FS);
    const nt_material_t second = create(VS, FS);
    for (int i = 2; i < 256; ++i) { create(VS, FS); }
    shader_programs_update();
    destroy_material(first);
    // The table is full; the new material takes the destroyed one's pool slot and row.
    const nt_material_t other = create(VS2, FS2);
    TEST_ASSERT_EQUAL_INT(slot_of(first), slot_of(other));
    shader_programs_restore_gpu();
    shader_programs_update();
    TEST_ASSERT_TRUE(shader_program_ready(other) && shader_program_ready(second));
    TEST_ASSERT_NOT_EQUAL(program_of(second).id, program_of(other).id);
}

static void test_create_destroy_cycles_reuse_rows(void) {
    for (int i = 0; i < 1000; ++i) {
        destroy_material(create(VS, FS));
    }
    const nt_material_t a = create(VS, FS);
    shader_programs_update();
    TEST_ASSERT_TRUE(shader_program_ready(a));
}

static void test_shutdown_and_restore_never_destroy_twice(void) {
    create(VS, FS);
    create(VS2, FS2);
    shader_programs_update();
    shader_programs_restore_gpu();
    shader_programs_restore_gpu();
    shader_programs_shutdown();
    shader_programs_shutdown();
    TEST_ASSERT_EQUAL_INT(2, s_destroys);

    const nt_material_t again = create(VS, FS);
    shader_programs_update();
    TEST_ASSERT_EQUAL_INT(3, s_links);
    TEST_ASSERT_TRUE(shader_program_ready(again));
    shader_programs_shutdown();
    TEST_ASSERT_EQUAL_INT(3, s_destroys);
}

// The consumer builds this test with NT_ASSERT_MODE=2, so the handler runs before the trap.
static jmp_buf s_assert_jmp;

static void trap_assert(const char *expr, const char *file, int line) {
    (void)expr;
    (void)file;
    (void)line;
    longjmp(s_assert_jmp, 1);
}

static void test_material_create_failure_asserts(void) {
    s_material_pool_full = true;
    const nt_assert_handler_t previous = nt_assert_handler;
    nt_assert_handler = trap_assert;
    if (setjmp(s_assert_jmp) == 0) {
        create(VS, FS);
        nt_assert_handler = previous;
        TEST_FAIL_MESSAGE("Expected NT_ASSERT to fire");
    }
    nt_assert_handler = previous;
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_one_link_per_pair);
    RUN_TEST(test_unready_stages_hold_the_link);
    RUN_TEST(test_failed_link_retries);
    RUN_TEST(test_restore_relinks_and_repoints);
    RUN_TEST(test_dead_row_goes_to_the_material_in_its_slot);
    RUN_TEST(test_create_destroy_cycles_reuse_rows);
    RUN_TEST(test_shutdown_and_restore_never_destroy_twice);
    RUN_TEST(test_material_create_failure_asserts);
    return UNITY_END();
}
