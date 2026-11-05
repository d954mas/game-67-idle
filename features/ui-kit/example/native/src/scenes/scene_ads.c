#include "scenes.h"

#include "features/ui_kit/ui_ad.h"
#include "features/ui_kit/ui_hint.h"
#include "features/ui_kit/ui_look.h"
#include "features/ui_kit/ui_metrics.h"
#include "features/ui_kit/ui_world_label.h"

#include "time/nt_time.h"

// The rewarded-ad rule, a plate standing in the world, the move hint and the
// bar, each from a game's data: a family, looks, art and copy. The world plate
// projects through a fixed camera, so it stays put on any canvas.

#define ADS_INK 0x231C26U
#define ADS_BLUE 0x1F7AF2U
#define ADS_GREEN 0x43BF1EU
#define ADS_H 52.0F

static const ui_look_family_t k_family = {.radius = 12.0F, .border = 4.0F, .gradient = true, .lift = 3.0F, .frame_r = 12.0F,
    .face_r = 8.0F, .ink = ADS_INK, .gloss_cap = 8.0F};

// Looking down -z with a 90 degree frustum: clip w is the distance.
static const float k_view_proj[16] = {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, -1.0F, -1.0F, 0.0F, 0.0F, -0.2F, 0.0F};

static ui_kit_button_look_t button_look(uint32_t rgb) {
    ui_kit_panel_style_t face = ui_look_face(&k_family, rgb, rgb, ADS_H, 0.0F, 0.0F);
    face.lip_h = k_family.lift;
    return (ui_kit_button_look_t){.idle = face};
}

static void ad_row(nt_ui_context_t *ctx, const char *id, float width, const ui_ad_look_t *ad, const ui_kit_button_look_t *close) {
    const ui_ad_choice_t row = {.ad = ad, .reward_icon = ui_kit_icon_ref(UI_ICON_STAR), .reward = "+50", .video = true,
        .standard_look = close, .standard = "Закрыть", .standard_style = ui_text_outlined(ADS_H * 0.4F, 0xFFFFFFU, ADS_INK, 0.15F),
        .standard_min_size = 12.0F, .pad = 8.0F, .fit_inset = 20.0F, .gap = 10.0F, .min_w = 150.0F};
    (void)ui_kit_ad_choice(ctx, id, width, ADS_H, &row);
}

void scene_ads_build(nt_ui_context_t *ctx) {
    scene_hud_build_world(ctx);
    const ui_metrics_t m = ui_metrics();
    const float width = fminf(m.view_w - m.safe_l - m.safe_r - 2.0F * m.margin, 340.0F);
    const nt_ui_label_style_t reward = ui_text_outlined(ADS_H * 0.42F, 0xFFFFFFU, ADS_INK, 0.15F);
    const nt_ui_label_style_t tag = ui_text_outlined(ADS_H * 0.26F, 0xD8F2FFU, ADS_INK, 0.15F);
    const ui_ad_look_t ad = {.button = button_look(ADS_BLUE), .video = ui_kit_icon_ref(UI_ICON_ARROW_RIGHT), .reward_style = &reward,
        .tag = "Видео", .tag_style = &tag, .video_size = ADS_H * 0.62F, .reward_icon_size = ADS_H * 0.5F, .gap = 6.0F};
    const ui_kit_button_look_t close = button_look(ADS_GREEN);
    CLAY({.id = CLAY_ID("ads/column"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT, .offset = {m.safe_l + m.margin, m.safe_t + m.margin}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIT(0)}, .childGap = 14, .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        ad_row(ctx, "ads/row", width, &ad, &close);
        ad_row(ctx, "ads/stacked", 240.0F, &ad, &close);
        const nt_ui_label_style_t text = ui_text_outlined(ADS_H * 0.42F, 0xFFFFFFU, ADS_INK, 0.15F);
        (void)ui_kit_text_button_styled(ctx, "ads/big", &close, CLAY_SIZING_FIXED(width), ADS_H * 1.3F, "Новая куча", &text, NULL, 16.0F, true);
        (void)ui_kit_ad_button(ctx, "ads/alone", width, ADS_H, &ad, NULL, "+150");
        const nt_ui_label_style_t caption = ui_text_outlined(14.0F, 0xFFFFFFU, ADS_INK, 0.15F);
        const ui_bar_t bar = {.family = &k_family, .w = width, .h = 20.0F, .ratio = 0.6F, .track = 0x14263DU, .top = 0xFFE07AU, .bottom = 0xE8A22CU,
            .radius = 7.0F, .outline = 1.0F, .alpha = 1.0F, .caption = "60 / 100", .caption_style = &caption};
        ui_kit_bar(ctx, "ads/bar", &bar);
    }
    const ui_world_label_t label = {.near_m = 1.0F, .focus_m = 10.0F, .margin = 200.0F, .z_near = 100, .z_per_m = 1.0F, .z_span = 50.0F};
    if (ui_world_label_begin(&m, k_view_proj, (const float[3]){4.0F, m.portrait ? -2.0F : 3.0F, -10.0F}, &label)) {
        ui_kit_panel_style_t plate = ui_look_face(&k_family, ADS_INK, ADS_INK, 34.0F, 17.0F, 1.0F);
        plate.alpha = 0.7F;
        ui_kit_lifted_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(34.0F)}, .padding = {12, 12, 0, 0},
            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}, &plate);
        const nt_ui_label_style_t sign = ui_text_outlined(18.0F, 0xFFFFFFU, ADS_INK, 0.15F);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), "В мире", &sign);
        ui_kit_lifted_end(ctx);
        ui_world_label_end();
    }
    ui_hint_t hint = ui_hint_defaults();
    hint.clock = (float)nt_time_now();
    hint.desktop = true;
    hint.label_family = &k_family;
    hint.key_family = &k_family;
    hint.ink = ADS_INK;
    static const char *const keys[4] = {"W", "A", "S", "D"};
    ui_hint_move_keys(ctx, &hint, &m, m.portrait ? m.view_w * 0.62F : m.view_w + 100.0F, m.view_h * 0.55F, "Идти", keys);
}
