#include "features/ui_kit/ui_shape.h"

#include "core/nt_assert.h"
#include "memory/nt_mem_scratch.h"
#include "render/nt_render_defs.h"
#include "ui/nt_ui_image.h"

#include <string.h>

static const char *const k_attr_names[] = {"a_layout", "a_shape", "a_paint", "a_fx", NULL};

static nt_material_t s_uber;
static nt_resource_t s_atlas;

nt_material_create_desc_t ui_kit_uber_material_desc(nt_resource_t atlas_texture) {
    return (nt_material_create_desc_t){
        .textures = {{.name = "u_texture", .resource = atlas_texture}},
        .texture_count = 1,
        .blend = nt_blend_alpha_premultiplied(),
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .attr_map[0] = {.stream_name = k_attr_names[0], .location = 4},
        .attr_map[1] = {.stream_name = k_attr_names[1], .location = 5},
        .attr_map[2] = {.stream_name = k_attr_names[2], .location = 6},
        .attr_map[3] = {.stream_name = k_attr_names[3], .location = 7},
        .attr_map_count = 4,
        // Zero defaults are mode 0, so every base emit without its own block is a plain sprite.
        .has_attr_defaults = true,
        .label = "ui_uber",
    };
}

void ui_kit_shape_bind(nt_material_t uber, nt_resource_t atlas) {
    s_uber = uber;
    s_atlas = atlas;
}

static Clay_Color tint_color(uint32_t rgb, float alpha) {
    const uint32_t packed = ui_kit_shape_tint(rgb, alpha);
    return (Clay_Color){(float)(packed & 0xFFU), (float)((packed >> 8U) & 0xFFU), (float)((packed >> 16U) & 0xFFU),
        (float)(packed >> 24U)};
}

static void block_floats(const ui_kit_shape_block_t *b, float out[UI_KIT_UBER_BLOCK_FLOATS]) {
    memset(out, 0, 4U * sizeof(float)); // a_layout: the walker writes it
    memcpy(out + 4, b, sizeof *b);
}

static void leaf(nt_ui_context_t *ctx, const nt_ui_element_data_t *data, const ui_kit_shape_block_t *b, uint32_t rgb,
    float alpha, const Clay_ElementDeclaration *decl) {
    NT_ASSERT(s_uber.id != 0U && "ui_kit shapes: call ui_kit_shape_bind first");
    float blk[UI_KIT_UBER_BLOCK_FLOATS];
    block_floats(b, blk);
    nt_ui_image_custom(ctx, data,
        &(nt_ui_image_custom_t){.atlas = s_atlas, .material = s_uber, .custom_attrs = blk,
            .custom_bytes = (uint8_t)UI_KIT_UBER_BLOCK_BYTES, .geom_mode = NT_UI_IMAGE_GEOM_GEOMETRY,
            .slice9_scale = 1.0F, .color_packed = ui_kit_shape_tint(rgb, alpha), .attr_names = k_attr_names},
        decl);
}

// The engine has no container form of nt_ui_image_custom, so the element is
// opened here with the same payload it builds (nt_ui.h: scratch-allocated,
// alive until the walk). Children then draw over the shape in tree order,
// which a floating background would not once a clip splits the frame.
static void open(const nt_ui_element_data_t *data, const ui_kit_shape_block_t *b, uint32_t rgb, float alpha,
    const Clay_ElementDeclaration *decl) {
    NT_ASSERT(s_uber.id != 0U && "ui_kit shapes: call ui_kit_shape_bind first");
    nt_ui_image_custom_block_t *blk = NT_MEM_SCRATCH_ALLOC(nt_ui_image_custom_block_t);
    NT_ASSERT(blk != NULL && "ui_kit shapes: scratch alloc failed");
    *blk = (nt_ui_image_custom_block_t){.custom_bytes = (uint8_t)UI_KIT_UBER_BLOCK_BYTES,
        .geom_mode = NT_UI_IMAGE_GEOM_GEOMETRY};
    block_floats(b, blk->custom_attrs);
    nt_ui_image_payload_t *p = NT_MEM_SCRATCH_ALLOC(nt_ui_image_payload_t);
    NT_ASSERT(p != NULL && "ui_kit shapes: scratch alloc failed");
    *p = (nt_ui_image_payload_t){.atlas = s_atlas, .slice9_scale = 1.0F, .material = s_uber, .custom = blk};
    Clay_ElementDeclaration final = decl != NULL ? *decl : (Clay_ElementDeclaration){0};
    final.image = (Clay_ImageElementConfig){.imageData = p};
    final.backgroundColor = tint_color(rgb, alpha);
    final.userData = (void *)data;
    Clay__OpenElement();
    Clay__ConfigureOpenElement(final);
}

void ui_kit_panel(nt_ui_context_t *ctx, const nt_ui_element_data_t *data, const ui_kit_panel_style_t *style,
    const Clay_ElementDeclaration *decl) {
    ui_kit_shape_block_t b;
    ui_kit_pack_panel(style, &b);
    leaf(ctx, data, &b, style->top_rgb, style->alpha, decl);
}

void ui_kit_panel_begin_styled(nt_ui_context_t *ctx, const nt_ui_element_data_t *data,
    const ui_kit_panel_style_t *style, const Clay_ElementDeclaration *decl) {
    (void)ctx;
    ui_kit_shape_block_t b;
    ui_kit_pack_panel(style, &b);
    open(data, &b, style->top_rgb, style->alpha, decl);
}

void ui_kit_panel_styled_end(nt_ui_context_t *ctx) {
    (void)ctx;
    Clay__CloseElement();
}

void ui_kit_radial(nt_ui_context_t *ctx, const nt_ui_element_data_t *data, const ui_kit_radial_style_t *style,
    const Clay_ElementDeclaration *decl) {
    ui_kit_shape_block_t b;
    ui_kit_pack_radial(style, &b);
    leaf(ctx, data, &b, style->top_rgb, style->alpha, decl);
}

void ui_kit_radial_begin_styled(nt_ui_context_t *ctx, const nt_ui_element_data_t *data,
    const ui_kit_radial_style_t *style, const Clay_ElementDeclaration *decl) {
    (void)ctx;
    ui_kit_shape_block_t b;
    ui_kit_pack_radial(style, &b);
    open(data, &b, style->top_rgb, style->alpha, decl);
}

void ui_kit_radial_styled_end(nt_ui_context_t *ctx) {
    (void)ctx;
    Clay__CloseElement();
}
