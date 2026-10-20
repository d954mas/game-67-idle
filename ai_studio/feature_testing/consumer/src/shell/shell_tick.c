#include "shell/shell.h"

#include "app/nt_app.h"
#include "features/game_features.h"
#include "features/platform_sdk/platform_sdk.h"
#include "features/settings/settings.h"
#include "game_events.h"
#include "game_items.h"
#include "game_save.h"
#include "game_scenes.h"
#include "game_storage.h"
#include "platform_lifecycle.h"
#include "render/render_mesh.h"
#include "systems/sys_cloud_save.h"
#include "systems/sys_platform_hooks.h"
#include "ui/ui_runtime.h"

#include <stdio.h>

static bool s_fresh_state;
static bool s_disable_autosave;
static bool s_open_settings_on_start;
static uint64_t s_scene_frame_index;
static bool s_game_runtime_initialized;
static bool s_game_runtime_ready;
static bool s_game_runtime_failed;
/* A fresh local run is distinguishable so confirmed account state can be
   adopted before gameplay starts instead of being treated as a divergence. */
static bool s_local_save_was_fresh;

void shell_tick_init(const shell_options_t *options) {
    s_fresh_state = options->fresh_state;
    s_disable_autosave = options->disable_autosave;
    s_open_settings_on_start = options->open_settings_on_start;
}

static void game_runtime_fail(const char *message, int detail) {
    fprintf(stderr, "%s (%d)\n", message, detail);
    s_game_runtime_failed = true;
    nt_app_quit();
}

static void game_runtime_reset_fragments_without_grants(void) {
    const int fragment_count = game_save_fragment_count();
    for (int i = 0; i < fragment_count; ++i) {
        const GameSaveFragment *fragment = game_save_fragment_at(i);
        if (fragment != NULL && fragment->reset != NULL) {
            fragment->reset();
        }
    }
    game_items_create_defaults(false);
}

static void game_runtime_reset_world_transient_state(World *world) {
    world->time_seconds = 0.0F;
    world->player_x = 0.0F;
    world->player_z = 0.0F;
    world->player_yaw = 0.0F;
}

static bool game_runtime_apply_pending_new_game(World *world) {
    const game_save_transition_result_t transition = game_save_apply_pending_new_game();
    if (!transition.state_changed) {
        return false;
    }
    if (!transition.persisted) {
        fprintf(stderr, "new game started, but save will be retried\n");
    }
    game_runtime_reset_world_transient_state(world);
    return true;
}

static void game_runtime_load_state(void) {
    /* Fresh means no local slot survived: a --fresh-state run over a real save
       must not let the account document replace that save unkept. */
    s_local_save_was_fresh = s_fresh_state && !game_storage_exists(GAME_SAVE_AUTOSAVE_SLOT);
    if (!s_fresh_state) {
        game_save_load_result_t load_result;
        game_save_load(&load_result);
        s_local_save_was_fresh = load_result.status == GAME_SAVE_LOAD_FRESH ||
                                 load_result.status == GAME_SAVE_LOAD_CORRUPT_RESET;
        if (load_result.status == GAME_SAVE_LOAD_CORRUPT_RESET) {
            /* load already did reset()+quarantine but NOT on_new_game; this is
               the single on_new_game call on the corrupt-save path. */
            char save_err[128];
            (void)game_save_new_game(save_err, (int)sizeof save_err);
        }
        /* Deliberately NOT reached for NEWER or BLOCKED: those loads leave a
           real session in memory, so redoing the reset would gut it and the
           next autosave would persist the gutted version. */
        return;
    }

    /* --fresh-state skips load and grants, but still creates required game-owned
       containers so every runtime consumer has valid explicit ownership. */
    game_runtime_reset_fragments_without_grants();
}

static void game_runtime_reload_cloud_state(void) {
    /* The prior import is transactional; this storage reload rebinds consumers
       through the normal LOADED path without running new-game grants. */
    const bool fresh_state = s_fresh_state;
    s_fresh_state = false;
    game_runtime_load_state();
    s_fresh_state = fresh_state;
}

static void game_runtime_apply_pending_cloud_save(World *world) {
    const game_save_choice_t choice = settings_take_save_conflict_choice();
    if (choice != GAME_SAVE_ASK && game_save_cloud_resolve(choice)) {
        settings_close();
    }
    /* UI readers have finished; game-owned consumers can now bind the new state. */
    if (game_save_cloud_apply_remote_at_safe_point()) {
        game_runtime_reload_cloud_state();
        settings_apply_language();
        /* Another device's save carries that device's guess for every unpicked value. */
        settings_adopt_device_graphics();
        game_runtime_reset_world_transient_state(world);
        settings_close();
    }
}

static void game_runtime_try_start(World *world) {
    if (s_game_runtime_ready || s_game_runtime_failed) {
        return;
    }

    const nt_pack_state_t pack_state = nt_resource_pack_state(shell_boot_pack_id());
    if (pack_state == NT_PACK_STATE_FAILED) {
        game_runtime_fail("game asset pack failed to load", (int)pack_state);
        return;
    }
    if (pack_state != NT_PACK_STATE_READY) {
        return;
    }
    /* Held here rather than after the load: swapping the save under a run that
       has already started is worse than a second on the loading screen. */
    if (!game_save_cloud_boot_settled()) {
        return;
    }

    char seed_error[128] = {0};
    if (!game_items_validate_default_seed(seed_error, (int)sizeof seed_error)) {
        game_runtime_fail(seed_error[0] ? seed_error : "items seed contract is incompatible", 0);
        return;
    }

    /* A ready pack is the startup barrier: save reconciliation and every
       feature that reads content run only after this point. */
    game_runtime_load_state();
    if (game_save_cloud_start(s_local_save_was_fresh)) {
        game_runtime_reload_cloud_state();
    }
    /* The persisted language reaches the string table only here: both load
       paths above leave settings_state populated, and every accessor before
       this point renders the corpus fallback. The portal's language wins on
       every launch until the player picks one in the settings. */
    settings_adopt_platform_language();
    settings_apply_language();
    /* After the save: a value the player picked is kept, the rest follow the device. */
    settings_adopt_device_graphics();
#ifdef NT_PLATFORM_WEB
    /* Do not expose pagehide/visibility flush until live fragments contain
       the loaded save. Fresh-state runs are intentionally non-persistent. */
    if (!s_fresh_state) {
        game_save_install_web_flush();
    }
#endif
    game_features_init(world);
    game_scenes_init(world);
    platform_lifecycle_init();
    platform_hooks_init();
    s_game_runtime_initialized = true;

    if (!shell_platform_start(world)) {
        game_runtime_fail("failed to start game runtime DevAPI", 0);
        return;
    }
    if (s_open_settings_on_start) {
        settings_open();
    }
    s_game_runtime_ready = true;
}

static void game_runtime_update(World *world, const game_input_frame_t *input) {
    if (!s_game_runtime_ready) {
        game_event_frame_reset();
        return;
    }

    world->time_seconds += g_nt_app.dt;
    /* Apply deferred new-game requests before feature updates and rendering. */
    (void)game_runtime_apply_pending_new_game(world);

    game_scenes_update(g_nt_app.dt, input);
    game_features_update(world, g_nt_app.dt);
    game_events_react_begin();
    do {
        game_features_react(world);
    } while (game_events_react_progressed());
    game_events_set_phase(GAME_EVENT_PHASE_RECORD);
    game_features_record(world);
    if (!s_disable_autosave) {
        game_save_tick();
        game_save_cloud_tick();
    }
    settings_set_save_conflict_visible(
        game_save_cloud_state() == GAME_SAVE_SYNC_CONFLICT,
        game_save_cloud_conflict_remote_document() != NULL);
    game_event_frame_reset();
}

shell_tick_status_t shell_tick(World *world, const game_input_frame_t *input) {
    game_runtime_try_start(world);
    /* An ad on screen, a portal pause and a hidden tab are the same thing to
       the world: time must not pass behind them. Frames keep being presented,
       so the page never looks frozen while the SDK owns the screen. */
    const bool platform_break = platform_sdk_break_active() || !platform_hooks_gameplay_allowed();
    if (s_game_runtime_ready) {
        game_scenes_step(++s_scene_frame_index, platform_break ? 0.0F : g_nt_app.dt);
    }
    const bool playable_shell_ready =
        s_game_runtime_ready && render_mesh_ready(world) && ui_runtime_ready();
    (void)platform_sdk_game_loading_progress(shell_platform_pack_loading_progress(shell_boot_pack_id()));
    /* Freezing the clock comes first so the lifecycle reports the stop in the
       same frame the simulation actually stopped. */
    platform_hooks_update(s_game_runtime_ready);
    platform_lifecycle_update(
        playable_shell_ready,
        !platform_break && game_scenes_can_process_game_input());
    game_save_update_playtime(
        playable_shell_ready && !g_nt_app.paused &&
        platform_sdk_gameplay_active() && !platform_sdk_break_active());
    game_runtime_update(world, input);
    return (shell_tick_status_t){.ready = s_game_runtime_ready, .playable = playable_shell_ready};
}

void shell_tick_after_draw(World *world) {
    /* UI may request New Game during draw. Drain it before pagehide can flush
       the previous state; this callback is still one synchronous frame. */
    game_runtime_apply_pending_cloud_save(world);
    (void)game_runtime_apply_pending_new_game(world);
}

/* Native only: the web runtime never returns from nt_app_run. */
void shell_tick_shutdown(World *world) {
    game_save_cloud_shutdown();
    if (s_game_runtime_initialized) {
        game_save_update_playtime(false);
        if (!s_disable_autosave) (void)game_save_flush(NULL, 0);
        game_scenes_shutdown();
        nt_resource_step();
        game_features_shutdown(world);
        platform_lifecycle_shutdown();
    }
}
