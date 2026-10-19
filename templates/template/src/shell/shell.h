#ifndef GAME_SHELL_H
#define GAME_SHELL_H

/* The shell has four phases: platform, boot, tick and present. Boot runs once
   before the loop and once at teardown; every frame then runs, in this order
   and no other:

     shell_platform_frame_begin -> shell_tick -> shell_present -> shell_platform_frame_end

   Platform owns argv, the DevAPI transport and the OS/browser edge (window,
   input, swap, page markers). Boot owns startup and teardown of engine
   subsystems. Tick owns the session: load, rules and saves. Present owns
   drawing and GPU loss/recovery. Rules run before presentation, and present
   only reads the World. main.c sequences the calls and owns only the storage
   they share: the World and the input frame. */

// Every phase and main.c gate their web edge on NT_PLATFORM_WEB; without this header it is silently undefined.
#include "core/nt_platform.h"
#include "game_input.h"
#include "world/world.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct shell_options_t {
    bool devapi_requested;
    uint16_t devapi_port;
} shell_options_t;

/* What the present phase put on screen; the platform phase acts on it. */
typedef struct shell_shown_t {
    bool presented; /* a frame was ended that must be swapped to the screen */
    bool complete;  /* a whole frame, not a GPU rebuild: post-present work runs */
    bool recovered; /* the first whole frame after the GPU objects were rebuilt */
} shell_shown_t;

/* Returns 0 to continue, otherwise the process exit code. */
int shell_platform_parse_args(int argc, char **argv, shell_options_t *options);
bool shell_platform_start(const shell_options_t *options);
void shell_platform_frame_begin(game_input_frame_t *input);
void shell_platform_frame_end(shell_shown_t shown);
void shell_platform_shutdown(void);

void shell_boot_init(void);
void shell_boot_shutdown(void);

void shell_tick_init(World *world);
void shell_tick(World *world, const game_input_frame_t *input);
void shell_tick_shutdown(World *world);

shell_shown_t shell_present(World *world, const game_input_frame_t *input);

#endif
