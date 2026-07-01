#include "devapi/game_state_devapi.h"

#if NT_DEVAPI_ENABLED
#include "app/nt_app.h"
#include "core/nt_assert.h"
#include "devapi/nt_devapi.h"
#include "systems/sys_settings.h"
#include "window/nt_window.h"

static cJSON *add_object(cJSON *parent, const char *key) {
    cJSON *object = cJSON_AddObjectToObject(parent, key);
    NT_ASSERT(object != NULL);
    return object;
}

static void add_string(cJSON *parent, const char *key, const char *value) {
    cJSON *item = cJSON_AddStringToObject(parent, key, value);
    NT_ASSERT(item != NULL);
    (void)item;
}

static void add_number(cJSON *parent, const char *key, double value) {
    cJSON *item = cJSON_AddNumberToObject(parent, key, value);
    NT_ASSERT(item != NULL);
    (void)item;
}

static void add_bool(cJSON *parent, const char *key, bool value) {
    cJSON *item = cJSON_AddBoolToObject(parent, key, value);
    NT_ASSERT(item != NULL);
    (void)item;
}

static bool cmd_game_state(const cJSON *params, cJSON *result, nt_devapi_error *err, void *user_data) {
    (void)params;

    World *world = (World *)user_data;
    if (world == NULL) {
        err->code = "internal";
        err->message = "game.state was registered without a World pointer";
        return false;
    }

    add_string(result, "schema", "template.game_state.snapshot.v1");
    add_string(result, "schema_document", "game_seed.state");
    add_string(result, "document", "game");
    add_number(result, "version", 1);

    cJSON *frame = add_object(result, "frame");
    add_number(frame, "current", (double)g_nt_app.frame);
    add_number(frame, "time_seconds", g_nt_app.time);
    add_number(frame, "dt", (double)g_nt_app.dt);
    add_bool(frame, "paused", g_nt_app.paused);
    add_bool(frame, "render_enabled", nt_app_render_enabled());

    cJSON *window = add_object(result, "window");
    add_number(window, "width", (double)g_nt_window.width);
    add_number(window, "height", (double)g_nt_window.height);
    add_number(window, "fb_width", (double)g_nt_window.fb_width);
    add_number(window, "fb_height", (double)g_nt_window.fb_height);
    add_number(window, "dpr", (double)g_nt_window.dpr);

    cJSON *state = add_object(result, "state");
    add_number(state, "time_seconds", (double)world->time_seconds);

    cJSON *player = add_object(state, "player");
    add_number(player, "x", (double)world->player_x);
    add_number(player, "z", (double)world->player_z);
    add_number(player, "yaw", (double)world->player_yaw);
    add_bool(player, "spawned", world->player_spawned);
    add_number(player, "entity_id", (double)world->player_entity.id);

    cJSON *prop = add_object(state, "prop");
    add_bool(prop, "spawned", world->prop_spawned);
    add_number(prop, "entity_id", (double)world->prop_entity.id);

    cJSON *settings = add_object(state, "settings");
    add_bool(settings, "open", sys_settings_is_open());
    add_number(settings, "master_volume", (double)sys_settings_master());
    add_number(settings, "music_volume", (double)sys_settings_music());
    add_number(settings, "sfx_volume", (double)sys_settings_sfx());

    return true;
}

static const nt_devapi_command_desc k_game_state_command = {
    .method = "game.state",
    .group = "game",
    .summary = "Return a read-only snapshot of the template game state.",
    .params_shape = "{}",
    .result_shape =
        "{schema:string,schema_document:string,document:string,version:number,"
        "frame:{current:number,time_seconds:number,dt:number,paused:bool,render_enabled:bool},"
        "window:{width:number,height:number,fb_width:number,fb_height:number,dpr:number},"
        "state:{time_seconds:number,player:{x:number,z:number,yaw:number,spawned:bool,entity_id:number},"
        "prop:{spawned:bool,entity_id:number},"
        "settings:{open:bool,master_volume:number,music_volume:number,sfx_volume:number}}}",
    .frame_behavior = "immediate",
    .side_effects = "none",
};
#endif

void game_state_devapi_register(World *world) {
#if NT_DEVAPI_ENABLED
    nt_result_t result = nt_devapi_register(&k_game_state_command, cmd_game_state, world);
    NT_ASSERT(result == NT_OK);
#else
    (void)world;
#endif
}
