#include "render/shader_programs.h"

#include "core/nt_assert.h"
#include "material/nt_program_ref.h"

#define SHADER_PAIR_MAX 64
#define SHADER_MATERIAL_MAX 256

static nt_program_ref_t s_pairs[SHADER_PAIR_MAX];
static int s_pair_count;

static struct {
    nt_material_t material;
    int pair;
} s_materials[SHADER_MATERIAL_MAX];
static int s_material_count;

static int pair_index(nt_resource_t vs, nt_resource_t fs) {
    for (int i = 0; i < s_pair_count; ++i) {
        if (s_pairs[i].vs.id == vs.id && s_pairs[i].fs.id == fs.id) {
            return i;
        }
    }
    NT_ASSERT(s_pair_count < SHADER_PAIR_MAX && "shader_programs: raise SHADER_PAIR_MAX");
    s_pairs[s_pair_count] = (nt_program_ref_t){.vs = vs, .fs = fs, .program = NT_PROGRAM_INVALID};
    return s_pair_count++;
}

nt_material_t shader_program_material(nt_resource_t vs, nt_resource_t fs,
                                      const nt_material_create_desc_t *desc) {
    NT_ASSERT(desc != NULL && vs.id != 0U && fs.id != 0U &&
              "shader_program_material: both stage resources are required");
    const int pair = pair_index(vs, fs);
    nt_material_create_desc_t bound = *desc;
    bound.program = s_pairs[pair].program;
    const nt_material_t material = nt_material_create(&bound);
    if (material.id == 0U) {
        return material;
    }
    NT_ASSERT(s_material_count < SHADER_MATERIAL_MAX && "shader_programs: raise SHADER_MATERIAL_MAX");
    s_materials[s_material_count].material = material;
    s_materials[s_material_count].pair = pair;
    s_material_count++;
    return material;
}

void shader_programs_update(void) {
    for (int pair = 0; pair < s_pair_count; ++pair) {
        if (!nt_program_ref_update(&s_pairs[pair])) {
            continue;
        }
        for (int i = 0; i < s_material_count; ++i) {
            if (s_materials[i].pair == pair && nt_material_valid(s_materials[i].material)) {
                nt_material_set_program(s_materials[i].material, s_pairs[pair].program);
            }
        }
    }
}

void shader_programs_shutdown(void) {
    for (int pair = 0; pair < s_pair_count; ++pair) {
        nt_program_ref_drop(&s_pairs[pair]);
    }
    s_pair_count = 0;
    s_material_count = 0;
}
