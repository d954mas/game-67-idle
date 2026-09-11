#include "features/ui_kit/ui_metrics.h"

#include "features/ui_kit/ui_safe_area.h"
#include "features/ui_kit/ui_theme.h"

#include <stdbool.h>

/* The frame the consumer's UI runtime opened. Kept here rather than passed
   through every call because a screen asks for metrics from deep inside a
   widget tree, and threading a frame handle through Clay callbacks would buy
   nothing: there is exactly one UI frame in flight. */
static struct {
    UiScaleFit fit;
    float dpr;
    bool open;
} s_frame;

UiScaleFit ui_frame_begin(float fb_w, float fb_h, float dpr) {
    const ui_tokens_t *t = ui_theme_tokens();
    float insets[4];
    ui_safe_area_insets_css(insets);
    s_frame.fit = ui_scale_fit(fb_w, fb_h, dpr, t->ref_short,
                               insets[0] + insets[1], insets[2] + insets[3]);
    s_frame.dpr = dpr > 0.0F ? dpr : 1.0F;
    s_frame.open = true;
    return s_frame.fit;
}

float ui_css_unit(void) { return 1.0F; }

float ui_css(float css_px) { return css_px * ui_css_unit(); }

ui_metrics_t ui_metrics(void) {
    const ui_tokens_t *t = ui_theme_tokens();
    ui_metrics_t m = {0};
    float insets[4];

    /* Before the first frame the reference canvas is the honest answer, and it
       keeps callers free of divide-by-zero. */
    m.view_w = s_frame.open ? s_frame.fit.logical_w : t->ref_short * 16.0F / 9.0F;
    m.view_h = s_frame.open ? s_frame.fit.logical_h : t->ref_short;
    m.css = ui_css_unit();
    m.shortest = m.view_w < m.view_h ? m.view_w : m.view_h;

    m.t_display = t->t_display * m.css;
    m.t_title = t->t_title * m.css;
    m.t_body = t->t_body * m.css;
    m.t_num = t->t_num * m.css;
    m.t_badge = t->t_badge * m.css;

    m.rim = t->rim * m.css;
    m.lift = t->lift * m.css;
    m.gap = t->gap * m.css;
    m.pad = t->pad * m.css;
    m.margin = m.gap;
    m.hit = t->hit * m.css;

    /* The platform states its insets in CSS pixels, which is the one unit that
       means the same physical size on every device; everything else here is
       already in reference units. */
    ui_safe_area_insets_css(insets);
    const float safe_unit = ui_scale_css_unit(s_frame.fit.scale, s_frame.dpr);
    m.safe_l = insets[0] * safe_unit;
    m.safe_r = insets[1] * safe_unit;
    m.safe_t = insets[2] * safe_unit;
    m.safe_b = insets[3] * safe_unit;

    const float available_w = m.view_w - m.safe_l - m.safe_r;
    const float available_h = m.view_h - m.safe_t - m.safe_b;
    m.panel_w = available_w > 0.0F ? available_w * (available_w > available_h ? .68F : .90F) : 0.0F;
    return m;
}
