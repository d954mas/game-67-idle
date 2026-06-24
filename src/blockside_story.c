#include "blockside_story.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float story_dist2(float ax, float az, float bx, float bz) {
    const float dx = ax - bx;
    const float dz = az - bz;
    return dx * dx + dz * dz;
}

const char *blockside_job_stage_name(JobStage stage) {
    switch (stage) {
    case JOB_STAGE_PACKAGE_COLLECTED:
        return "package_collected";
    case JOB_STAGE_COMPLETE:
        return "complete";
    case JOB_STAGE_SECOND_READY:
        return "second_ready";
    case JOB_STAGE_REPO_INTRO:
        return "repo_intro";
    case JOB_STAGE_REPO_DRIVE:
        return "repo_drive";
    case JOB_STAGE_REPO_SCOUT_COMPLETE:
        return "repo_scout_complete";
    case JOB_STAGE_STASH_LEAD:
        return "stash_lead";
    case JOB_STAGE_VAN_RUMOR:
        return "van_rumor";
    case JOB_STAGE_MARKET_WATCH:
        return "market_watch";
    case JOB_STAGE_COURIER_SPOTTED:
        return "courier_spotted";
    case JOB_STAGE_TAIL_ROUTE:
        return "tail_route";
    case JOB_STAGE_TAIL_TURN:
        return "tail_turn";
    case JOB_STAGE_TAIL_PRESSURE:
        return "tail_pressure";
    case JOB_STAGE_TAIL_STOP:
        return "tail_stop";
    case JOB_STAGE_TARGET_HANDOFF:
        return "target_handoff";
    case JOB_STAGE_GREEN_COUPE_APPROACH:
        return "green_coupe_approach";
    case JOB_STAGE_GREEN_COUPE_CLAIMED:
        return "green_coupe_claimed";
    case JOB_STAGE_GREEN_COUPE_ESCAPE:
        return "green_coupe_escape";
    case JOB_STAGE_REPO_DROPOFF_CALL:
        return "repo_dropoff_call";
    case JOB_STAGE_REPO_DROPOFF_GARAGE:
        return "repo_dropoff_garage";
    case JOB_STAGE_REPO_PAYOUT_MEET:
        return "repo_payout_meet";
    case JOB_STAGE_REPO_NEXT_LEAD:
        return "repo_next_lead";
    case JOB_STAGE_REPO_HEAT_WATCH:
        return "repo_heat_watch";
    case JOB_STAGE_REPO_MEET_INTERCEPT:
        return "repo_meet_intercept";
    case JOB_STAGE_REPO_GETAWAY_ROUTE:
        return "repo_getaway_route";
    case JOB_STAGE_REPO_SAFEHOUSE_DROP:
        return "repo_safehouse_drop";
    case JOB_STAGE_REPO_FINAL_CALL:
        return "repo_final_call";
    case JOB_STAGE_REPO_NEXT_SCORE_LEAD:
        return "repo_next_score_lead";
    case JOB_STAGE_REPO_CREW_PICKUP:
        return "repo_crew_pickup";
    case JOB_STAGE_REPO_TOOL_CACHE:
        return "repo_tool_cache";
    case JOB_STAGE_REPO_SCORE_STAGING:
        return "repo_score_staging";
    case JOB_STAGE_CAUGHT:
        return "caught";
    case JOB_STAGE_START:
    default:
        return "start";
    }
}

void blockside_set_toast(GameRuntime *game, const char *text) {
    (void)snprintf(game->toast, sizeof(game->toast), "%s", text);
}

void blockside_reset_game(GameRuntime *game) {
    memset(game, 0, sizeof(*game));
    game->player_x = -2.8F;
    game->player_z = -1.5F;
    game->player_yaw = 0.25F;
    game->car_x = -1.0F;
    game->car_z = -0.8F;
    game->car_yaw = 0.15F;
    blockside_vehicle_reset(&game->vehicle);
    game->pursuer_x = 4.2F;
    game->pursuer_z = 1.8F;
    game->job_stage = JOB_STAGE_START;
    blockside_set_toast(game, "Find the package. Enter car with E.");
}

void blockside_try_pickup_package(GameRuntime *game) {
    if (game->package_collected || story_dist2(game->player_x, game->player_z, 3.2F, 0.8F) > 1.6F) {
        return;
    }
    game->package_collected = true;
    game->job_stage = JOB_STAGE_PACKAGE_COLLECTED;
    game->wanted_level = 1;
    game->roadblock_active = true;
    game->roadblock_cleared = false;
    game->pursuit_grace_timer = 1.2F;
    blockside_set_toast(game, "Guard raised a roadblock. Stun him or drive around.");
}

void blockside_try_complete_job(GameRuntime *game) {
    if (!game->package_collected || game->package_delivered || story_dist2(game->player_x, game->player_z, -4.0F, -3.4F) > 1.8F) {
        return;
    }
    game->package_delivered = true;
    game->second_job_unlocked = true;
    game->job_stage = JOB_STAGE_SECOND_READY;
    game->cash += 75;
    game->wanted_level = 0;
    game->roadblock_active = false;
    blockside_set_toast(game, "Rita has a repo tip. Meet the blue jacket.");
}

void blockside_try_talk_rita(GameRuntime *game) {
    if (!game->second_job_unlocked || game->repo_intro_active || story_dist2(game->player_x, game->player_z, 1.8F, -2.4F) > 1.8F) {
        return;
    }
    game->repo_intro_active = true;
    game->job_stage = JOB_STAGE_REPO_INTRO;
    blockside_set_toast(game, "Rita: repo the red compact. Get in and scout the curb.");
}

void blockside_try_start_repo_drive(GameRuntime *game) {
    if (!game->repo_intro_active || game->repo_drive_active || !game->in_vehicle) {
        return;
    }
    game->repo_drive_active = true;
    game->job_stage = JOB_STAGE_REPO_DRIVE;
    blockside_set_toast(game, "Repo scout started. Drive to the orange curb.");
}

void blockside_try_complete_repo_scout(GameRuntime *game) {
    if (!game->repo_drive_active || game->repo_scout_complete || !game->in_vehicle ||
        story_dist2(game->car_x, game->car_z, 4.6F, -2.9F) > 1.35F) {
        return;
    }
    game->repo_scout_complete = true;
    game->job_stage = JOB_STAGE_REPO_SCOUT_COMPLETE;
    game->cash += 25;
    blockside_set_toast(game, "Scout complete. Stash van spotted near purple block.");
}

void blockside_try_open_stash_lead(GameRuntime *game) {
    if (!game->repo_scout_complete || game->stash_lead_active ||
        story_dist2(game->player_x, game->player_z, 6.4F, -4.8F) > 2.2F) {
        return;
    }
    game->stash_lead_active = true;
    game->job_stage = JOB_STAGE_STASH_LEAD;
    blockside_set_toast(game, "Stash lead found. Van rumor points east.");
}

void blockside_try_follow_van_rumor(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->stash_lead_active || game->van_rumor_active || story_dist2(x, z, 7.2F, -1.8F) > 1.7F) {
        return;
    }
    game->van_rumor_active = true;
    game->job_stage = JOB_STAGE_VAN_RUMOR;
    game->cash += 15;
    blockside_set_toast(game, "Van rumor confirmed. Market watch is next.");
}

void blockside_try_start_market_watch(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->van_rumor_active || game->market_watch_active || story_dist2(x, z, 5.8F, 3.2F) > 1.8F) {
        return;
    }
    game->market_watch_active = true;
    game->job_stage = JOB_STAGE_MARKET_WATCH;
    game->wanted_level = 1;
    blockside_set_toast(game, "Market watch started. Spot the courier next.");
}

void blockside_try_spot_courier(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->market_watch_active || game->courier_spotted || story_dist2(x, z, -3.6F, 1.6F) > 1.9F) {
        return;
    }
    game->courier_spotted = true;
    game->job_stage = JOB_STAGE_COURIER_SPOTTED;
    blockside_set_toast(game, "Courier spotted. Tail route is next.");
}

void blockside_try_start_tail_route(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->courier_spotted || game->tail_route_active || story_dist2(x, z, -0.8F, 3.8F) > 1.9F) {
        return;
    }
    game->tail_route_active = true;
    game->job_stage = JOB_STAGE_TAIL_ROUTE;
    blockside_set_toast(game, "Tail route started. Keep two car lengths.");
}

void blockside_try_watch_tail_turn(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->tail_route_active || game->tail_turn_watch || story_dist2(x, z, 2.4F, 4.8F) > 1.7F) {
        return;
    }
    game->tail_turn_watch = true;
    game->job_stage = JOB_STAGE_TAIL_TURN;
    game->cash += 20;
    blockside_set_toast(game, "Courier turned right. Next: close in.");
}

void blockside_try_tail_pressure(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->tail_turn_watch || game->tail_pressure_active || story_dist2(x, z, 4.8F, 2.2F) > 1.7F) {
        return;
    }
    game->tail_pressure_active = true;
    game->job_stage = JOB_STAGE_TAIL_PRESSURE;
    game->wanted_level = 2;
    game->pursuer_x = 3.8F;
    game->pursuer_z = 3.1F;
    blockside_set_toast(game, "Courier is nervous. Stay close, not too close.");
}

void blockside_try_tail_stop(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->tail_pressure_active || game->tail_stop_resolved || story_dist2(x, z, 6.6F, 0.6F) > 1.7F) {
        return;
    }
    game->tail_stop_resolved = true;
    game->job_stage = JOB_STAGE_TAIL_STOP;
    game->wanted_level = 1;
    game->cash += 40;
    blockside_set_toast(game, "Courier stopped. Repo target identified.");
}

void blockside_try_target_handoff(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->tail_stop_resolved || game->target_handoff_active || story_dist2(x, z, 5.2F, -1.1F) > 1.8F) {
        return;
    }
    game->target_handoff_active = true;
    game->job_stage = JOB_STAGE_TARGET_HANDOFF;
    game->cash += 10;
    blockside_set_toast(game, "Target handoff: green coupe by the depot.");
}

void blockside_try_green_coupe_approach(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->target_handoff_active || game->green_coupe_approach || story_dist2(x, z, 7.4F, -3.6F) > 1.7F) {
        return;
    }
    game->green_coupe_approach = true;
    game->job_stage = JOB_STAGE_GREEN_COUPE_APPROACH;
    blockside_set_toast(game, "Green coupe found. Get close and take it.");
}

void blockside_try_green_coupe_entry(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->green_coupe_approach || game->green_coupe_claimed || story_dist2(x, z, 8.3F, -4.25F) > 2.0F) {
        return;
    }
    game->green_coupe_claimed = true;
    game->job_stage = JOB_STAGE_GREEN_COUPE_CLAIMED;
    game->cash += 35;
    blockside_set_toast(game, "Green coupe claimed. Lose the heat next.");
}

void blockside_try_green_coupe_escape(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->green_coupe_claimed || game->green_coupe_escaped || story_dist2(x, z, -2.4F, -5.2F) > 1.8F) {
        return;
    }
    game->green_coupe_escaped = true;
    game->job_stage = JOB_STAGE_GREEN_COUPE_ESCAPE;
    game->wanted_level = 0;
    game->cash += 30;
    blockside_set_toast(game, "Heat lost. Rita has the drop-off next.");
}

void blockside_try_repo_dropoff_call(GameRuntime *game) {
    if (!game->green_coupe_escaped || game->repo_dropoff_call) {
        return;
    }
    game->repo_dropoff_call = true;
    game->job_stage = JOB_STAGE_REPO_DROPOFF_CALL;
    blockside_set_toast(game, "Rita: north garage, no scratches.");
}

void blockside_try_repo_dropoff_garage(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_dropoff_call || game->repo_dropoff_garage || story_dist2(x, z, -6.8F, 4.2F) > 2.0F) {
        return;
    }
    game->repo_dropoff_garage = true;
    game->job_stage = JOB_STAGE_REPO_DROPOFF_GARAGE;
    game->cash += 90;
    blockside_set_toast(game, "Garage drop-off done. Meet Rita for payout.");
}

void blockside_try_repo_payout_meet(GameRuntime *game) {
    if (!game->repo_dropoff_garage || game->repo_payout_meet || story_dist2(game->player_x, game->player_z, 1.8F, -2.4F) > 1.8F) {
        return;
    }
    game->repo_payout_meet = true;
    game->job_stage = JOB_STAGE_REPO_PAYOUT_MEET;
    game->cash += 60;
    blockside_set_toast(game, "Rita paid out. New heat lead opens next.");
}

void blockside_try_repo_next_lead(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_payout_meet || game->repo_next_lead || story_dist2(x, z, -1.2F, 3.4F) > 1.9F) {
        return;
    }
    game->repo_next_lead = true;
    game->job_stage = JOB_STAGE_REPO_NEXT_LEAD;
    game->wanted_level = 1;
    blockside_set_toast(game, "New heat lead found. Watch the meet next.");
}

void blockside_try_repo_heat_watch(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_next_lead || game->repo_heat_watch || story_dist2(x, z, 3.7F, 3.6F) > 1.8F) {
        return;
    }
    game->repo_heat_watch = true;
    game->job_stage = JOB_STAGE_REPO_HEAT_WATCH;
    game->wanted_level = 2;
    blockside_set_toast(game, "Heat watch started. Intercept the meet next.");
}

void blockside_try_repo_meet_intercept(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_heat_watch || game->repo_meet_intercept || story_dist2(x, z, -4.6F, 2.8F) > 1.8F) {
        return;
    }
    game->repo_meet_intercept = true;
    game->job_stage = JOB_STAGE_REPO_MEET_INTERCEPT;
    game->cash += 30;
    game->wanted_level = 2;
    blockside_set_toast(game, "Meet intercepted. Get the getaway route next.");
}

void blockside_try_repo_getaway_route(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_meet_intercept || game->repo_getaway_route || story_dist2(x, z, -7.2F, -0.6F) > 1.9F) {
        return;
    }
    game->repo_getaway_route = true;
    game->job_stage = JOB_STAGE_REPO_GETAWAY_ROUTE;
    game->cash += 20;
    game->wanted_level = 1;
    blockside_set_toast(game, "Getaway route found. Safehouse drop is next.");
}

void blockside_try_repo_safehouse_drop(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_getaway_route || game->repo_safehouse_drop || story_dist2(x, z, 2.6F, -5.4F) > 2.0F) {
        return;
    }
    game->repo_safehouse_drop = true;
    game->job_stage = JOB_STAGE_REPO_SAFEHOUSE_DROP;
    game->cash += 35;
    game->wanted_level = 0;
    blockside_set_toast(game, "Safehouse drop done. Rita's final call is next.");
}

void blockside_try_repo_final_call(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_safehouse_drop || game->repo_final_call || story_dist2(x, z, -1.6F, -5.8F) > 1.8F) {
        return;
    }
    game->repo_final_call = true;
    game->job_stage = JOB_STAGE_REPO_FINAL_CALL;
    game->cash += 15;
    game->wanted_level = 0;
    blockside_set_toast(game, "Rita called. Next: pick a bigger score.");
}

void blockside_try_repo_next_score_lead(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_final_call || game->repo_next_score_lead || story_dist2(x, z, -4.8F, -5.6F) > 1.9F) {
        return;
    }
    game->repo_next_score_lead = true;
    game->job_stage = JOB_STAGE_REPO_NEXT_SCORE_LEAD;
    game->cash += 10;
    game->wanted_level = 0;
    blockside_set_toast(game, "Big score lead found. Next: find a crew pickup.");
}

void blockside_try_repo_crew_pickup(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_next_score_lead || game->repo_crew_pickup || story_dist2(x, z, -6.2F, -3.8F) > 1.9F) {
        return;
    }
    game->repo_crew_pickup = true;
    game->job_stage = JOB_STAGE_REPO_CREW_PICKUP;
    game->cash += 10;
    game->wanted_level = 0;
    blockside_set_toast(game, "Crew pickup done. Next: hit the tool cache.");
}

void blockside_try_repo_tool_cache(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_crew_pickup || game->repo_tool_cache || story_dist2(x, z, -7.4F, -2.2F) > 1.9F) {
        return;
    }
    game->repo_tool_cache = true;
    game->job_stage = JOB_STAGE_REPO_TOOL_CACHE;
    game->cash += 10;
    game->wanted_level = 0;
    blockside_set_toast(game, "Tool cache found. Next: stage the score.");
}

void blockside_try_repo_score_staging(GameRuntime *game) {
    const float x = game->in_vehicle ? game->car_x : game->player_x;
    const float z = game->in_vehicle ? game->car_z : game->player_z;
    if (!game->repo_tool_cache || game->repo_score_staging || story_dist2(x, z, -8.2F, 1.4F) > 1.9F) {
        return;
    }
    game->repo_score_staging = true;
    game->job_stage = JOB_STAGE_REPO_SCORE_STAGING;
    game->cash += 20;
    game->wanted_level = 0;
    blockside_set_toast(game, "Score staged. Next: pick the target.");
}

void blockside_fire_weapon(GameRuntime *game) {
    if (game->weapon_cooldown > 0.0F) {
        return;
    }
    game->weapon_cooldown = 0.6F;
    if (game->package_collected && !game->package_delivered && story_dist2(game->player_x, game->player_z, game->pursuer_x, game->pursuer_z) < 8.0F) {
        game->pursuer_stunned = true;
        game->stun_timer = 1.5F;
        game->roadblock_active = false;
        game->roadblock_cleared = true;
        blockside_set_toast(game, "Toy blaster cleared the roadblock.");
    } else {
        blockside_set_toast(game, "Toy blaster pop.");
    }
}

void blockside_update_story(GameRuntime *game, float dt) {
    if (game->weapon_cooldown > 0.0F) {
        game->weapon_cooldown -= dt;
        if (game->weapon_cooldown < 0.0F) {
            game->weapon_cooldown = 0.0F;
        }
    }
    if (game->stun_timer > 0.0F) {
        game->stun_timer -= dt;
        if (game->stun_timer <= 0.0F) {
            game->stun_timer = 0.0F;
            game->pursuer_stunned = false;
        }
    }
    if (game->pursuit_grace_timer > 0.0F) {
        game->pursuit_grace_timer -= dt;
        if (game->pursuit_grace_timer < 0.0F) {
            game->pursuit_grace_timer = 0.0F;
        }
    }

    if (game->package_collected && !game->package_delivered && !game->pursuer_stunned && game->pursuit_grace_timer <= 0.0F) {
        const float dx = game->player_x - game->pursuer_x;
        const float dz = game->player_z - game->pursuer_z;
        const float d = sqrtf(dx * dx + dz * dz);
        if (d > 0.01F) {
            const float chase = (game->roadblock_active ? 3.15F : 2.35F) * dt;
            game->pursuer_x += dx / d * chase;
            game->pursuer_z += dz / d * chase;
        }
        if (d < 0.75F) {
            game->job_stage = JOB_STAGE_CAUGHT;
            game->package_collected = false;
            game->in_vehicle = false;
            game->player_x = 1.2F;
            game->player_z = 0.4F;
            game->car_x = -1.0F;
            game->car_z = -0.8F;
            blockside_vehicle_reset(&game->vehicle);
            game->wanted_level = 0;
            game->roadblock_active = false;
            blockside_set_toast(game, "Caught. Retry the package run.");
        }
    }

    blockside_try_pickup_package(game);
    blockside_try_complete_job(game);
    blockside_try_complete_repo_scout(game);
    blockside_try_follow_van_rumor(game);
    blockside_try_start_market_watch(game);
    blockside_try_spot_courier(game);
    blockside_try_start_tail_route(game);
    blockside_try_watch_tail_turn(game);
    blockside_try_tail_pressure(game);
    blockside_try_tail_stop(game);
    blockside_try_target_handoff(game);
    blockside_try_green_coupe_approach(game);
    blockside_try_green_coupe_escape(game);
    blockside_try_repo_dropoff_garage(game);
    blockside_try_repo_payout_meet(game);
    blockside_try_repo_next_lead(game);
    blockside_try_repo_heat_watch(game);
    blockside_try_repo_meet_intercept(game);
    blockside_try_repo_getaway_route(game);
    blockside_try_repo_safehouse_drop(game);
    blockside_try_repo_final_call(game);
    blockside_try_repo_next_score_lead(game);
    blockside_try_repo_crew_pickup(game);
    blockside_try_repo_tool_cache(game);
    blockside_try_repo_score_staging(game);
}
