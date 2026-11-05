#include "features/ui_kit/ui_world_label.h"

#include <math.h>

bool ui_world_label_begin(const ui_metrics_t *m, const float view_proj[16], const float at[3], const ui_world_label_t *label) {
    ui_world_billboard_t b;
    if (!ui_world_billboard(view_proj, at, m->view_w, m->view_h, label->near_m, label->focus_m, label->margin, &b)) {
        return false;
    }
    nt_ui_transform_t t = nt_ui_transform_defaults();
    t.offset_x = b.x - m->view_w * 0.5F;
    t.offset_y = b.y - m->view_h * 0.5F;
    t.scale_x = t.scale_y = b.scale;
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .userData = (void *)nt_ui_make_element_data_xform(0U, NULL, &t, 1.0F),
        .floating = {.attachTo = CLAY_ATTACH_TO_ROOT, .offset = {m->view_w * 0.5F, m->view_h * 0.5F},
            .zIndex = (int16_t)(label->z_near - (int)fminf(b.distance * label->z_per_m, label->z_span)),
            .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH}});
    return true;
}

void ui_world_label_end(void) { Clay__CloseElement(); }

void *ui_world_depth_data(float ndc_depth, bool shown) {
    nt_ui_transform_t transform = nt_ui_transform_defaults();
    transform.offset_z = ui_world_depth_offset(ndc_depth);
    return (void *)nt_ui_make_element_data_xform(0U, NULL, &transform, shown ? 1.0F : 0.0F);
}
