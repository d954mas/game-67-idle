#include "features/ui_kit/ui_ad.h"

#include "core/nt_assert.h"
#include "features/ui_kit/ui_look.h"

#include <string.h>

static void ad_face(nt_ui_context_t *ctx, const ui_ad_look_t *look, nt_atlas_region_ref_t *reward_icon, const char *reward) {
    CLAY({.layout = {.childGap = (uint16_t)look->gap, .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        ui_kit_icon(ctx, look->video, look->video_size);
        if (reward_icon != NULL) { ui_kit_icon(ctx, reward_icon, look->reward_icon_size); }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), reward != NULL ? reward : "", look->reward_style);
        if (look->tag != NULL) { nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), look->tag, look->tag_style); }
    }
}

static bool ad_button(nt_ui_context_t *ctx, Clay_ElementId element, uint32_t plate, float width, float height,
    const ui_ad_look_t *look, nt_atlas_region_ref_t *reward_icon, const char *reward) {
    NT_ASSERT(ui_ad_rgb_is_blue(look->button.idle.bottom_rgb) && "ui_kit_ad: the ad button is blue");
    bool clicked = false;
    CLAY({.id = element, .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(height)}}}) {
        ui_kit_button_begin_styled(ctx, plate, &look->button, true, NULL);
        ad_face(ctx, look, reward_icon, reward);
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}

bool ui_kit_ad_button(nt_ui_context_t *ctx, const char *id, float width, float height, const ui_ad_look_t *look,
    nt_atlas_region_ref_t *reward_icon, const char *reward) {
    const Clay_ElementId element = Clay_GetElementId((Clay_String){.length = (int32_t)strlen(id), .chars = id});
    return ad_button(ctx, element, nt_ui_child_id(element.id, "button"), width, height, look, reward_icon, reward);
}

static bool standard_button(nt_ui_context_t *ctx, uint32_t element, float width, float height, const ui_ad_choice_t *c) {
    nt_ui_label_style_t style = c->standard_style;
    ui_text_fit_width(ctx, c->standard, &style, width - c->fit_inset, c->standard_min_size);
    const ui_kit_button_look_t *look = c->standard_look;
    const float lip = look->idle.lip_inside ? 0.0F : look->idle.lip_h;
    bool clicked = false;
    CLAY({.id = {.id = element}, .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(height + lip)}}}) {
        ui_kit_button_begin_styled(ctx, nt_ui_child_id(element, "plate"), look, true, NULL);
        CLAY({.layout = {.padding = {(uint16_t)c->pad, (uint16_t)c->pad, 0, 0}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), c->standard != NULL ? c->standard : "", &style);
        }
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}

ui_ad_pick_t ui_kit_ad_choice(nt_ui_context_t *ctx, const char *id, float width, float height, const ui_ad_choice_t *choice) {
    NT_ASSERT(!ui_ad_rgb_is_blue(choice->standard_look->idle.bottom_rgb) && "ui_kit_ad: the standard button is never blue");
    const Clay_ElementId row_id = Clay_GetElementId((Clay_String){.length = (int32_t)strlen(id), .chars = id});
    const uint32_t standard_id = nt_ui_child_id(row_id.id, "standard");
    ui_ad_pick_t pick = UI_AD_NONE;
    const ui_ad_split_t split = choice->video ? ui_ad_split(width, choice->gap, choice->min_w) : (ui_ad_split_t){true, width};
    CLAY({.id = row_id, .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIT(0)}, .childGap = (uint16_t)choice->gap,
        .layoutDirection = split.row ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM}}) {
        if (standard_button(ctx, standard_id, split.each, height, choice)) { pick = UI_AD_STANDARD; }
        if (choice->video) {
            const uint32_t ad_id = nt_ui_child_id(row_id.id, "ad");
            if (ad_button(ctx, (Clay_ElementId){.id = ad_id}, nt_ui_child_id(ad_id, "button"), split.each, height, choice->ad,
                    choice->reward_icon, choice->reward)) { pick = UI_AD_VIDEO; }
        }
    }
    return pick;
}
