#ifndef GAME_SHELL_H
#define GAME_SHELL_H

/* The shell has four phases: platform, boot, tick and present. Boot runs once
   before the loop and once at teardown; every frame then runs these calls,
   in this order and no other:

     shell_platform_frame_begin -> shell_present_upload -> shell_tick
       -> shell_present -> shell_tick_after_draw -> shell_platform_frame_end

   Platform owns argv, the DevAPI transport and the OS/browser edge (window,
   input, swap, page markers). Boot owns startup and teardown of engine
   subsystems, the pack and the render stack. Tick owns the session: the start
   barrier, load, cloud adoption, rules and saves. Present owns drawing and GPU
   loss/recovery, and only reads the World. main.c sequences the calls and owns
   only the storage they share: the World, the input frame and the options. */

// Every phase and main.c gate their web edge on NT_PLATFORM_WEB; without this header it is silently undefined.
#include "core/nt_platform.h"
#include "font/nt_font.h"
#include "game_input.h"
#include "hash/nt_hash.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"
#include "world/world.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct shell_options_t {
    char capture_path[260];
    bool capture;
    bool open_settings_on_start;
    bool fresh_state;
    bool disable_autosave;
    int window_width;
    int window_height;
    /* Bot-only switches, parsed in DevAPI builds; devapi/README.md explains each. */
    bool no_vsync;
    bool time_manual;
    bool devapi_requested;
    uint16_t devapi_port;
} shell_options_t;

typedef struct shell_tick_status_t {
    bool ready;    /* the start barrier passed: the session and features run */
    bool playable; /* ready, and the render stack can show it */
} shell_tick_status_t;

/* What the present phase put on screen; the platform phase acts on it. */
typedef struct shell_shown_t {
    bool presented; /* a frame was ended that must be swapped to the screen */
    bool complete;  /* a whole frame, not a GPU rebuild: post-draw work runs */
    bool recovered; /* the first playable frame after the GPU objects were rebuilt */
} shell_shown_t;

typedef struct shell_text_t {
    nt_material_t material;
    nt_resource_t font_resource;
    nt_font_t font;
} shell_text_t;

/* Returns 0 to continue, otherwise the process exit code. The options must
   outlive the run: the platform keeps a pointer for the DevAPI start. */
int shell_platform_parse_args(int argc, char **argv, shell_options_t *options);
bool shell_platform_start(World *world);
void shell_platform_frame_begin(game_input_frame_t *input);
float shell_platform_pack_loading_progress(nt_hash32_t pack_id);
void shell_platform_frame_end(shell_shown_t shown, bool playable);
void shell_platform_shutdown(void);

/* Runs after nt_engine_init and the window title, both of which main.c owns. */
bool shell_boot_init(const shell_options_t *options, World *world);
nt_hash32_t shell_boot_pack_id(void);
const shell_text_t *shell_boot_text(void);
void shell_boot_shutdown(void);

void shell_tick_init(const shell_options_t *options);
shell_tick_status_t shell_tick(World *world, const game_input_frame_t *input);
void shell_tick_after_draw(World *world);
void shell_tick_shutdown(World *world);

void shell_present_init(const shell_options_t *options);
void shell_present_upload(void);
shell_shown_t shell_present(World *world, const game_input_frame_t *input, shell_tick_status_t tick);

#endif
