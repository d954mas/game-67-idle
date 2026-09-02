#ifndef SHADER_PROGRAMS_H
#define SHADER_PROGRAMS_H

#include "graphics/nt_gfx.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"

#include <stdbool.h>

// One linked program per (vertex, fragment) shader-resource pair, shared by
// every material created through here. A pair links once both stages have
// arrived and links again after a context loss; the materials borrow the
// handle and are re-pointed on every link, so callers only ever ask
// material_program_ready().

// desc->program is ignored: the pair's current link (or none yet) is used.
nt_material_t shader_program_material(nt_resource_t vs, nt_resource_t fs,
                                      const nt_material_create_desc_t *desc);

// Once per frame while the context is live, after nt_material_step().
void shader_programs_update(void);

// Before nt_gfx_shutdown: releases every owned program.
void shader_programs_shutdown(void);

static inline bool material_program_ready(nt_material_t mat) {
    const nt_material_info_t *info = nt_material_get_info(mat);
    return info != NULL && nt_gfx_program_ready(info->program);
}

#endif /* SHADER_PROGRAMS_H */
