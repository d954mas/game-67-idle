#ifndef BLOCKSIDE_GAME_TYPES_H
#define BLOCKSIDE_GAME_TYPES_H

#include "blockside_vehicle.h"

#include <stdbool.h>

typedef enum JobStage {
    JOB_STAGE_START = 0,
    JOB_STAGE_PACKAGE_COLLECTED,
    JOB_STAGE_COMPLETE,
    JOB_STAGE_SECOND_READY,
    JOB_STAGE_REPO_INTRO,
    JOB_STAGE_REPO_DRIVE,
    JOB_STAGE_REPO_SCOUT_COMPLETE,
    JOB_STAGE_STASH_LEAD,
    JOB_STAGE_VAN_RUMOR,
    JOB_STAGE_MARKET_WATCH,
    JOB_STAGE_COURIER_SPOTTED,
    JOB_STAGE_TAIL_ROUTE,
    JOB_STAGE_TAIL_TURN,
    JOB_STAGE_TAIL_PRESSURE,
    JOB_STAGE_TAIL_STOP,
    JOB_STAGE_TARGET_HANDOFF,
    JOB_STAGE_GREEN_COUPE_APPROACH,
    JOB_STAGE_GREEN_COUPE_CLAIMED,
    JOB_STAGE_GREEN_COUPE_ESCAPE,
    JOB_STAGE_REPO_DROPOFF_CALL,
    JOB_STAGE_REPO_DROPOFF_GARAGE,
    JOB_STAGE_REPO_PAYOUT_MEET,
    JOB_STAGE_REPO_NEXT_LEAD,
    JOB_STAGE_REPO_HEAT_WATCH,
    JOB_STAGE_REPO_MEET_INTERCEPT,
    JOB_STAGE_REPO_GETAWAY_ROUTE,
    JOB_STAGE_REPO_SAFEHOUSE_DROP,
    JOB_STAGE_REPO_FINAL_CALL,
    JOB_STAGE_REPO_NEXT_SCORE_LEAD,
    JOB_STAGE_REPO_CREW_PICKUP,
    JOB_STAGE_REPO_TOOL_CACHE,
    JOB_STAGE_REPO_SCORE_STAGING,
    JOB_STAGE_CAUGHT,
} JobStage;

typedef struct GameRuntime {
    float player_x;
    float player_z;
    float player_yaw;
    float car_x;
    float car_z;
    float car_yaw;
    BlocksideVehicleState vehicle;
    float pursuer_x;
    float pursuer_z;
    bool in_vehicle;
    bool package_collected;
    bool package_delivered;
    bool second_job_unlocked;
    bool repo_intro_active;
    bool repo_drive_active;
    bool repo_scout_complete;
    bool stash_lead_active;
    bool van_rumor_active;
    bool market_watch_active;
    bool courier_spotted;
    bool tail_route_active;
    bool tail_turn_watch;
    bool tail_pressure_active;
    bool tail_stop_resolved;
    bool target_handoff_active;
    bool green_coupe_approach;
    bool green_coupe_claimed;
    bool green_coupe_escaped;
    bool repo_dropoff_call;
    bool repo_dropoff_garage;
    bool repo_payout_meet;
    bool repo_next_lead;
    bool repo_heat_watch;
    bool repo_meet_intercept;
    bool repo_getaway_route;
    bool repo_safehouse_drop;
    bool repo_final_call;
    bool repo_next_score_lead;
    bool repo_crew_pickup;
    bool repo_tool_cache;
    bool repo_score_staging;
    bool pursuer_stunned;
    bool roadblock_active;
    bool roadblock_cleared;
    float pursuit_grace_timer;
    float stun_timer;
    float weapon_cooldown;
    int cash;
    int wanted_level;
    JobStage job_stage;
    char toast[96];
} GameRuntime;

#endif /* BLOCKSIDE_GAME_TYPES_H */
