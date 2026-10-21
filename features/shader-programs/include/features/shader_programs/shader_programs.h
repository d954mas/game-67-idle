#ifndef FEATURES_SHADER_PROGRAMS_H
#define FEATURES_SHADER_PROGRAMS_H

#include "graphics/nt_gfx.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"

#include <stdbool.h>

// One linked program per (vertex, fragment) shader-resource pair, shared by
// every material created through here. A pair links once both stages are ready
// and links again after a context loss; the materials borrow the handle and are
// re-pointed on every link, so callers only ever ask shader_program_ready().

// desc->program is ignored: the pair's current link (or none yet) is used. The
// first material of a pair names its program with desc->label, which must have
// static storage. The material stays caller-owned; destroying it frees its row.
nt_material_t shader_program_material(nt_resource_t vs, nt_resource_t fs, const nt_material_create_desc_t *desc);

// Every frame while the context is live, after nt_resource_step().
void shader_programs_update(void);

// Context restore order: invalidate NT_ASSET_SHADER_CODE, call this, draw nothing
// that frame; the next update links every pair again.
void shader_programs_restore_gpu(void);

// Before nt_gfx_shutdown: destroys the cached programs, not the materials.
// Repeating restore or shutdown never destroys a program twice.
void shader_programs_shutdown(void);

static inline bool shader_program_ready(nt_material_t mat) {
    const nt_material_info_t *info = nt_material_get_info(mat);
    return info != NULL && nt_gfx_program_ready(info->program);
}

#endif /* FEATURES_SHADER_PROGRAMS_H */
