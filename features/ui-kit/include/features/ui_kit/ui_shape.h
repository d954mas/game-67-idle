#ifndef FEATURES_UI_KIT_UI_SHAPE_H
#define FEATURES_UI_KIT_UI_SHAPE_H

// SDF panels and radials drawn by the kit's uber material (shaders/ui_uber.*),
// which also draws every plain sprite the same as sprite.frag. A game that
// makes it the context's base sprite material gets icons, Clay rectangles,
// panels and radials into one draw per text-free run:
//
//   nt_material_t uber = <create from ui_kit_uber_material_desc(atlas page)>;
//   nt_ui_set_sprite_material(ctx, uber);
//   ui_kit_shape_bind(uber, atlas);
//
// Plain emits carry no block of their own; they bake the material's attr
// defaults, which are all zero: mode 0, a sprite.
//
// Every shape is a GEOMETRY-mode quad against the context's white region: it
// needs no art in the atlas. Styles and packing: ui_shape_pack.h.

#include "clay.h"
#include "features/ui_kit/ui_shape_pack.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui.h"

// a_layout, a_shape, a_paint, a_fx: one vec4 each.
#define UI_KIT_UBER_BLOCK_FLOATS 16U
#define UI_KIT_UBER_BLOCK_BYTES 64U

// Blend, depth and attribute map of the uber material over the UI atlas page;
// the game creates it with its own shader resources (ui_uber.vert/.frag).
nt_material_create_desc_t ui_kit_uber_material_desc(nt_resource_t atlas_texture);

// The material and atlas every shape below draws with.
void ui_kit_shape_bind(nt_material_t uber, nt_resource_t atlas);

// A panel as a leaf, or as a container whose children draw over it. The
// container owns decl's image, backgroundColor and userData; decl may carry an id.
void ui_kit_panel(nt_ui_context_t *ctx, const nt_ui_element_data_t *data, const ui_kit_panel_style_t *style,
    const Clay_ElementDeclaration *decl);
void ui_kit_panel_begin_styled(nt_ui_context_t *ctx, const nt_ui_element_data_t *data,
    const ui_kit_panel_style_t *style, const Clay_ElementDeclaration *decl);
void ui_kit_panel_styled_end(nt_ui_context_t *ctx);

void ui_kit_radial(nt_ui_context_t *ctx, const nt_ui_element_data_t *data, const ui_kit_radial_style_t *style,
    const Clay_ElementDeclaration *decl);
void ui_kit_radial_begin_styled(nt_ui_context_t *ctx, const nt_ui_element_data_t *data,
    const ui_kit_radial_style_t *style, const Clay_ElementDeclaration *decl);
void ui_kit_radial_styled_end(nt_ui_context_t *ctx);

#endif
