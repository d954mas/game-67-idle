#include "blockside_hud.h"

#include "math/nt_math.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_text_renderer.h"
#include "window/nt_window.h"

#include <stdio.h>
#include <string.h>

static float dist2(float ax, float az, float bx, float bz) {
    const float dx = ax - bx;
    const float dz = az - bz;
    return dx * dx + dz * dz;
}

static void hud_uniforms(float view[16], float proj[16], nt_frame_uniforms_t *uniforms) {
    float vp[16];
    glm_mat4_mul((vec4 *)proj, (vec4 *)view, (vec4 *)vp);
    memset(uniforms, 0, sizeof(*uniforms));
    memcpy(uniforms->view_proj, vp, 64);
    memcpy(uniforms->view, view, 64);
    memcpy(uniforms->proj, proj, 64);
    uniforms->camera_pos[2] = 1.0F;
}

static void hud_text(const char *text, float x, float y, float size, const float color[4]) {
    float model[16];
    glm_mat4_identity((vec4 *)model);
    glm_translate((vec4 *)model, (vec3){x, y, 0.0F});
    nt_text_renderer_draw(text, model, size, color, 0.0F, 0.0F);
}

static const char *hud_job_title(const GameRuntime *game) {
    if (game->repo_score_staging) { return "JOB: SCORE STAGED"; }
    if (game->repo_tool_cache) { return "JOB: TOOL CACHE"; }
    if (game->repo_crew_pickup) { return "JOB: CREW PICKUP"; }
    if (game->repo_next_score_lead) { return "JOB: BIG SCORE"; }
    if (game->repo_final_call) { return "JOB: RITA CALL"; }
    if (game->repo_safehouse_drop) { return "JOB: SAFEHOUSE"; }
    if (game->repo_getaway_route) { return "JOB: GETAWAY"; }
    if (game->repo_meet_intercept) { return "JOB: MEET HIT"; }
    if (game->repo_heat_watch) { return "JOB: HEAT WATCH"; }
    if (game->repo_next_lead) { return "JOB: NEW LEAD"; }
    if (game->repo_payout_meet) { return "JOB: PAYOUT DONE"; }
    if (game->repo_dropoff_garage) { return "JOB: GARAGE DONE"; }
    if (game->repo_dropoff_call) { return "JOB: DROPOFF CALL"; }
    if (game->green_coupe_escaped) { return "JOB: HEAT LOST"; }
    if (game->green_coupe_claimed) { return "JOB: COUPE CLAIMED"; }
    if (game->green_coupe_approach) { return "JOB: GREEN COUPE"; }
    if (game->target_handoff_active) { return "JOB: TARGET HANDOFF"; }
    if (game->tail_stop_resolved) { return "JOB: COURIER STOPPED"; }
    if (game->tail_pressure_active) { return "JOB: TAIL PRESSURE"; }
    if (game->tail_turn_watch) { return "JOB: TURN WATCH"; }
    if (game->tail_route_active) { return "JOB: SAFE DISTANCE"; }
    if (game->courier_spotted) { return "JOB: TAIL ROUTE"; }
    if (game->market_watch_active) { return "JOB: COURIER WATCH"; }
    if (game->van_rumor_active) { return "JOB: MARKET WATCH"; }
    if (game->stash_lead_active) { return "JOB: VAN RUMOR"; }
    if (game->repo_scout_complete) { return "JOB: STASH SPOTTED"; }
    if (game->repo_drive_active) { return "JOB: CURB SCOUT"; }
    if (game->repo_intro_active) { return "JOB: RED COMPACT"; }
    if (game->second_job_unlocked) { return "JOB: RITA REPO"; }
    return "JOB: PICKUP";
}

static const char *hud_job_subtitle(const GameRuntime *game) {
    if (game->repo_score_staging) { return "Score staged. Next: pick target."; }
    if (game->repo_tool_cache) { return "Cache found. Next: score staging."; }
    if (game->repo_crew_pickup) { return "Crew onboard. Next: tool cache."; }
    if (game->repo_next_score_lead) { return "Lead found. Next: crew pickup."; }
    if (game->repo_final_call) { return "Rita called. Next: bigger score."; }
    if (game->repo_safehouse_drop) { return "Drop done. Next: Rita call."; }
    if (game->repo_getaway_route) { return "Route found. Next: safehouse."; }
    if (game->repo_meet_intercept) { return "Meet hit. Next: getaway route."; }
    if (game->repo_heat_watch) { return "Watch started. Next: intercept."; }
    if (game->repo_next_lead) { return "Lead found. Next: watch the meet."; }
    if (game->repo_payout_meet) { return "Rita paid. Next: new heat lead."; }
    if (game->repo_dropoff_garage) { return "Garage done. Meet Rita for payout."; }
    if (game->repo_dropoff_call) { return "North garage next. Keep it clean."; }
    if (game->green_coupe_escaped) { return "Heat lost. Next: call Rita."; }
    if (game->green_coupe_claimed) { return "Target claimed. Next: lose heat."; }
    if (game->green_coupe_approach) { return "Target car found. Next: get in."; }
    if (game->target_handoff_active) { return "Green coupe by depot. Next: repo it."; }
    if (game->tail_stop_resolved) { return "Target ID found. Get handoff."; }
    if (game->tail_pressure_active) { return "Pressure up. Force a stop."; }
    if (game->tail_turn_watch) { return "Turn watched. Next: close in."; }
    if (game->tail_route_active) { return "Hold distance. Follow the turn."; }
    if (game->courier_spotted) { return "Keep distance on the tail."; }
    if (game->market_watch_active) { return "Watch for the courier next."; }
    if (game->van_rumor_active) { return "Stake out the market next."; }
    if (game->stash_lead_active) { return "Follow the rumor east."; }
    if (game->repo_scout_complete) { return "New lead near the purple block."; }
    if (game->repo_drive_active) { return "Drive to the orange curb marker."; }
    if (game->repo_intro_active) { return "Get in the car and scout the curb."; }
    if (game->second_job_unlocked) { return "Meet Rita by the blue jacket."; }
    if (game->package_collected) { return "Deliver the package."; }
    return "Grab the alley package.";
}

void blockside_draw_hud(const GameRuntime *game,
                        nt_material_t text_material,
                        nt_resource_t font_resource,
                        nt_font_t font,
                        nt_buffer_t frame_ubo) {
    if (!nt_material_get_info(text_material) || !nt_resource_is_ready(font_resource)) {
        return;
    }

    float view[16];
    float proj[16];
    glm_mat4_identity((vec4 *)view);
    glm_ortho(0.0F, (float)g_nt_window.fb_width, 0.0F, (float)g_nt_window.fb_height, -1.0F, 1.0F, (vec4 *)proj);

    nt_frame_uniforms_t uniforms;
    hud_uniforms(view, proj, &uniforms);
    nt_gfx_update_buffer(frame_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(frame_ubo, 0);

    nt_font_step();
    nt_text_renderer_set_material(text_material);
    nt_text_renderer_set_font(font);

    char line[128];
    const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    const float yellow[4] = {1.0F, 0.86F, 0.25F, 1.0F};
    const float red[4] = {1.0F, 0.25F, 0.18F, 1.0F};
    const float green[4] = {0.25F, 1.0F, 0.45F, 1.0F};
    const float cyan[4] = {0.35F, 0.82F, 1.0F, 1.0F};
    const float h = (float)g_nt_window.fb_height;

    hud_text(hud_job_title(game), 30.0F, h - 48.0F, 25.0F, yellow);
    hud_text(hud_job_subtitle(game), 30.0F, h - 82.0F, 18.0F, white);
    (void)snprintf(line, sizeof(line), "CASH $%d", game->cash);
    hud_text(line, (float)g_nt_window.fb_width - 210.0F, h - 48.0F, 24.0F, green);
    (void)snprintf(line, sizeof(line), "WANTED %d", game->wanted_level);
    hud_text(line, (float)g_nt_window.fb_width - 210.0F, h - 86.0F, 24.0F, game->wanted_level > 0 ? red : white);
    hud_text(game->in_vehicle ? "W/S GAS-BRAKE   A/D STEER   E EXIT" : "WASD MOVE   E ACTION   SPACE TOY BLASTER", 30.0F, 38.0F, 17.0F, cyan);
    if (dist2(game->player_x, game->player_z, game->car_x, game->car_z) < 2.25F || game->in_vehicle) {
        hud_text(game->in_vehicle ? "E EXIT CAR" : "E ENTER CAR", 490.0F, 86.0F, 22.0F, white);
    }
    hud_text(game->toast, 30.0F, 76.0F, 18.0F, white);
    nt_text_renderer_flush();
}
