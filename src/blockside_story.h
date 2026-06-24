#ifndef BLOCKSIDE_STORY_H
#define BLOCKSIDE_STORY_H

#include "blockside_game_types.h"

const char *blockside_job_stage_name(JobStage stage);
void blockside_set_toast(GameRuntime *game, const char *text);
void blockside_reset_game(GameRuntime *game);
void blockside_try_pickup_package(GameRuntime *game);
void blockside_try_complete_job(GameRuntime *game);
void blockside_try_talk_rita(GameRuntime *game);
void blockside_try_start_repo_drive(GameRuntime *game);
void blockside_try_complete_repo_scout(GameRuntime *game);
void blockside_try_open_stash_lead(GameRuntime *game);
void blockside_try_follow_van_rumor(GameRuntime *game);
void blockside_try_start_market_watch(GameRuntime *game);
void blockside_try_spot_courier(GameRuntime *game);
void blockside_try_start_tail_route(GameRuntime *game);
void blockside_try_watch_tail_turn(GameRuntime *game);
void blockside_try_tail_pressure(GameRuntime *game);
void blockside_try_tail_stop(GameRuntime *game);
void blockside_try_target_handoff(GameRuntime *game);
void blockside_try_green_coupe_approach(GameRuntime *game);
void blockside_try_green_coupe_entry(GameRuntime *game);
void blockside_try_green_coupe_escape(GameRuntime *game);
void blockside_try_repo_dropoff_call(GameRuntime *game);
void blockside_try_repo_dropoff_garage(GameRuntime *game);
void blockside_try_repo_payout_meet(GameRuntime *game);
void blockside_try_repo_next_lead(GameRuntime *game);
void blockside_try_repo_heat_watch(GameRuntime *game);
void blockside_try_repo_meet_intercept(GameRuntime *game);
void blockside_try_repo_getaway_route(GameRuntime *game);
void blockside_try_repo_safehouse_drop(GameRuntime *game);
void blockside_try_repo_final_call(GameRuntime *game);
void blockside_try_repo_next_score_lead(GameRuntime *game);
void blockside_try_repo_crew_pickup(GameRuntime *game);
void blockside_try_repo_tool_cache(GameRuntime *game);
void blockside_try_repo_score_staging(GameRuntime *game);
void blockside_fire_weapon(GameRuntime *game);
void blockside_update_story(GameRuntime *game, float dt);

#endif /* BLOCKSIDE_STORY_H */
