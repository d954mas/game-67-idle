#ifndef BODY_SYNC_SYNC_PLAYBACK_H
#define BODY_SYNC_SYNC_PLAYBACK_H

/* The playback receiver: one timeline per owner seat, a buffer of snapshots per body, and what
   each copy does this tick. Pure C: records and ticks in; per body a pose at playback time and
   how to move there out. The Box3D shell performs it and decides nothing.

   Timeline. A record belongs to the timeline of the seat that simulates its body: on a guest
   the host's bodies play on the host's, another guest's (relayed, see sync_codec.h) on that
   guest's, so a character, what it carries and what it threw play back together. A record's
   sample tick is in its timeline's units: the sender's tick for a direct record, the host's
   tick at which the owner's state lay on the host's clock line for that owner, its earliest
   arrival (packet tick - age), so the first leg's jitter does not reach the second.

   Clock, per timeline, over the last SYNC_CLOCK_SAMPLES new samples (arrival pairs of sample
   tick s and local tick l):
     rate        the least-squares slope of s over l, clamped to [SYNC_RATE_LOW,
                 SYNC_RATE_HIGH]; 1 until the samples span SYNC_RATE_SPAN local ticks
     sender now  max over the samples of (s - rate * l), plus rate * l_now: the fastest path
     jitter      the 95th percentile of the samples' lag behind that line, in sample ticks
     delay       clamp(ceil(jitter) + SYNC_DELAY_MARGIN, delay_min, delay_max); delay_max until
                 SYNC_CLOCK_SETTLE samples have arrived (a new or restarted timeline)
     target      sender now - delay
   Each local tick playback += rate * pace, pace = clamp(1 + (target - playback - rate) / 20,
   SYNC_PACE_SLOW, SYNC_PACE_FAST); then playback = min(playback, newest sample): it waits at
   the newest sample, never extrapolates. A new timeline starts at min(target, newest).
   Behind the target by more than SYNC_JUMP_TICKS it jumps to min(target, newest) and reports
   the jump, so the shell hands the move to the render offset.
   Invariant: playback never decreases, never exceeds the newest sample seen, and grows by at
   most rate * SYNC_PACE_FAST per local tick except at a reported jump. Every time is counted
   from the timeline's base with wrap-aware differences, so the invariant holds across the
   32-bit wrap of the owner's ticks.
   A duplicate sample (one already seen) changes nothing, not even the jitter. A reordered
   sample counts for the jitter and is filed in its body's buffer if still ahead of playback;
   one behind playback keeps only rare groups newer than those already played.

   Buffer. Up to SYNC_PLAYBACK_SNAPSHOTS per body, by sample tick. A full buffer drops the oldest
   once the next one is at or before playback; otherwise it keeps its span and drops the inner
   snapshot whose neighbours lie closest (a longer delay plays on coarser snapshots, never on a
   cut path), its rare groups passed on to the next. At playback time t a body
   takes the snapshots around t: position by cubic Hermite when both carry velocities (sim
   records) kept inside the box of the two snapshots' positions, linearly otherwise;
   rotation by normalized lerp on the shorter arc; velocities (for copy records, which carry
   none, the line's between the two snapshots)
   linearly. Before its first snapshot it holds that one's pose; past its newest it holds that
   pose with that snapshot's velocities (for a dynamic hold; none once frozen); past it, waiting
   (the seam lets a flight go on its own motion), frozen once t is SYNC_FREEZE_TICKS past it, and is driven on when the
   next snapshot comes. A rest snapshot at or before t gives its pose exactly and stops it.
   Rare groups (flags, motion, authority, holder, data) are those of the newest snapshot at or
   before t that carried them, of its current timeline only: a new owner's first snapshot
   forgets the groups played of the old one. */

#include <stdbool.h>
#include <stdint.h>

#include "sync_codec.h"

enum {
    SYNC_PLAYBACK_SNAPSHOTS = 16, /* the default longest delay at one snapshot a tick */
    SYNC_CLOCK_SAMPLES = 64,     /* about a second at 60 packets a second */
    SYNC_CLOCK_SETTLE = 16,
    SYNC_RATE_SPAN = 32,         /* local ticks a rate is measured over at least */
    SYNC_DELAY_MIN = 4,          /* ticks: 67 ms at 60 a second */
    SYNC_DELAY_MAX = 15,
    SYNC_DELAY_MARGIN = 1,
    SYNC_JUMP_TICKS = 60,        /* further behind than a second, playback jumps */
    SYNC_FREEZE_TICKS = 8,
    SYNC_REBASE_TICKS = 1 << 20,
    /* A sample further ahead of its timeline's clock line than this disagrees with it: past the
       longest delay (15) and a jump (60), nothing an honest path makes in a steady state. */
    SYNC_FAR_TICKS = SYNC_DELAY_MAX + SYNC_JUMP_TICKS,
    /* Consecutive far samples agreeing with each other (within delay_max) that restart the clock:
       8 ticks, 133 ms of an owner whose time really moved (a latency drop, a skipped tick, the
       end of a silence), which a lone forged or corrupt tick cannot make. */
    SYNC_FAR_RUN = 8,
};
#define SYNC_PACE_SLOW 0.9
#define SYNC_PACE_FAST 1.25
#define SYNC_RATE_LOW 0.5
#define SYNC_RATE_HIGH 2.0

/* Render offset decay per tick, as the review sets it: 0.95 up to 25 cm, 0.85 from 1 m, by
   distance between; past SYNC_OFFSET_SNAP_M none at all (the copy is seen to jump), but for a
   lost take's (SYNC_OFFSET_LOST_M). */
#define SYNC_OFFSET_SLOW 0.95f
#define SYNC_OFFSET_FAST 0.85f
#define SYNC_OFFSET_NEAR_M 0.25f
#define SYNC_OFFSET_FAR_M 1.0f
#define SYNC_OFFSET_SNAP_M 2.0f
/* A take that lost while its claim waited keeps the drawn pose up to this far, closing 0.15 m a
   tick past SYNC_OFFSET_SNAP_M: the loser's picture glides to the winner's instead of jumping. A
   cap, not a derivation: how far two simulations of one contested body part over a round trip is
   chaos (the random three-participant sweep's farthest is 3.1 m), and a glide of 5 m, 20 ticks
   past 2 m, is the longest still read as one body moving; past it the pictures are of different
   places and a jump is the honest picture. */
#define SYNC_OFFSET_LOST_M 5.0f

/* A dynamic copy's velocity: the owner's plus the error closed over SYNC_CORRECT_TICKS, the
   correction no faster than SYNC_CORRECT_SPEED (m/s) and SYNC_CORRECT_SPIN (rad/s). */
#define SYNC_CORRECT_TICKS 3.0f
#define SYNC_CORRECT_SPEED 5.0f
#define SYNC_CORRECT_SPIN 10.0f

typedef struct sync_snapshot {
    uint32_t tick; /* sample tick in its timeline */
    bool sim;
    bool at_rest;
    float position[3];
    float rotation[4];
    float velocity[3];
    float angular[3];
    sync_rare_t rare; /* the groups this snapshot carried */
    bool played;      /* its rare groups were applied */
} sync_snapshot_t;

typedef struct sync_timeline {
    bool active;
    bool started;
    uint32_t base;   /* a sample tick every time of the timeline is counted from */
    double playback; /* sample ticks after `base` */
    float rate;
    float pace;
    uint32_t newest;
    uint64_t seen;      /* bit k: sample newest - k arrived */
    uint8_t delay;
    uint32_t jumped_at; /* local tick of the last jump, + 1 (0: never) */
    uint32_t jumps;
    uint32_t samples[SYNC_CLOCK_SAMPLES];
    uint32_t locals[SYNC_CLOCK_SAMPLES];
    uint32_t count, head, total;
    /* The far rule (sync_playback_admit). */
    uint8_t far_run;
    double far_offset;
    uint32_t far_dropped, restarts;
    bool verdict_valid, verdict;
    uint32_t verdict_sample;
} sync_timeline_t;

typedef struct sync_buffered {
    bool has;
    uint8_t timeline;
    uint8_t count;
    sync_snapshot_t snapshots[SYNC_PLAYBACK_SNAPSHOTS]; /* oldest first */
    uint8_t known;    /* rare groups played so far */
    sync_rare_t rare; /* their values at playback time */
    uint32_t rare_tick[SYNC_RARE_GROUPS];
    bool ended; /* its owner changed (sync_playback_end): past its newest snapshot it holds still */
} sync_buffered_t;

typedef struct sync_playback {
    float dt; /* seconds a sample tick lasts at its sender: the Hermite tangents' scale */
    uint8_t delay_min, delay_max;
    uint32_t local_tick;
    sync_timeline_t timelines[BODY_SYNC_SEATS_MAX];
    sync_buffered_t bodies[BODY_SYNC_BODIES_MAX];
} sync_playback_t;

/* Delay bounds in ticks, 0 for the defaults. */
void sync_playback_init(sync_playback_t *p, uint32_t tick_hz, uint32_t delay_min, uint32_t delay_max);
void sync_playback_forget_body(sync_playback_t *p, uint16_t slot);
/* The newest snapshot tick a body holds; false when none. */
bool sync_playback_newest(const sync_playback_t *p, uint16_t slot, uint32_t *tick);
/* The body's owner changed and the snapshots are the old owner's: it plays on to the first one
   after its playback time, drops the rest (a path its new owner never saw) and holds there with no
   motion, frozen, until a snapshot is filed again. */
void sync_playback_end(sync_playback_t *p, uint16_t slot);
/* Drops a body's snapshots at or before `tick` (its rare groups stay as played). */
void sync_playback_forget_until(sync_playback_t *p, uint16_t slot, uint32_t tick);
/* A join forgets that seat's timeline and every snapshot on it; a leave forgets the timeline
   (its clock starts over) and keeps the snapshots. */
void sync_playback_forget_seat(sync_playback_t *p, uint8_t seat, bool keep_states);

typedef enum sync_file {
    SYNC_FILE_TAKEN = 0,
    SYNC_FILE_DUPLICATE, /* that body already holds this sample tick */
    SYNC_FILE_LATE,      /* behind playback: only newer rare groups were kept */
} sync_file_t;

/* One packet's arrival on a timeline: feeds its clock once per packet, before its records. */
void sync_playback_arrival(sync_playback_t *p, uint8_t timeline, uint32_t sample_tick, uint32_t local_tick);
sync_file_t sync_playback_file(sync_playback_t *p, uint8_t timeline, uint32_t sample_tick, const sync_record_t *record);

/* Once per local tick, before apply: timelines move and played snapshots give their groups. */
void sync_playback_advance(sync_playback_t *p, uint32_t local_tick);

typedef struct sync_play {
    bool known;
    float position[3];
    float rotation[4];
    float velocity[3];
    float angular[3];
    bool at_rest;
    bool frozen;
    bool waiting; /* playback passed its newest snapshot, not frozen: its timeline went on without it */
    bool jumped;  /* its timeline jumped this tick */
} sync_play_t;

void sync_playback_sample(const sync_playback_t *p, uint16_t slot, sync_play_t *out);
/* The rare groups as of playback time; *known: which groups ever played. */
const sync_rare_t *sync_playback_rare(const sync_playback_t *p, uint16_t slot, uint8_t *known);
/* The owner tick the slot plays; false while it has nothing to play (sync_play_t.known): a
   timeline's tick is no picture of a body whose snapshots were all dropped. */
bool sync_playback_tick(const sync_playback_t *p, uint16_t slot, double *owner_tick);

/* False for a sample further ahead of the timeline's clock line than SYNC_FAR_TICKS, from the
   timeline's first sample on. */
bool sync_playback_plausible(const sync_playback_t *p, uint8_t timeline, uint32_t sample, uint32_t local);
/* The far rule, once per sample (later calls for the same sample repeat the verdict): a far
   sample alone is dropped and counted (false), since taking it would move every copy on the
   timeline; SYNC_FAR_RUN consecutive far samples that agree with each other restart the clock
   on them (true). arrival applies it too; the shell calls it first to drop the record. */
bool sync_playback_admit(sync_playback_t *p, uint8_t timeline, uint32_t sample, uint32_t local);

/* Host: the local tick at which `sample` lies on the timeline's clock line, its earliest
   arrival, so a relayed record's age leaves out the first leg's jitter. False before the
   timeline has heard anything. */
bool sync_playback_line_tick(const sync_playback_t *p, uint8_t timeline, uint32_t sample, uint32_t *local);

/* The rotation taking `from` to `to` on the shorter arc, as world axis times angle (radians). */
void sync_turn_between(const float from[4], const float to[4], float out[3]);

/* A kinematic copy: false when reaching the pose in one step needs more than max_speed or
   max_spin; the shell then places it there and hands the jump to the render offset. */
bool sync_drive_reachable(const float now_position[3], const float now_rotation[4], const sync_play_t *play, float dt, float max_speed,
                          float max_spin);
/* A dynamic copy's rotation on its locked angular axes (bits x y z), which a spin cannot turn:
   the part of the turn to `target` about those world axes, applied to `now`. False when that
   part is under SYNC_FACING_SET_RAD (setting a pose resets its contacts, so it is not done for
   a hair); the free axes are left to the spin. */
#define SYNC_FACING_SET_RAD 0.00872665f /* half a degree */
bool sync_drive_locked_turn(const float now[4], const float target[4], uint8_t locked_axes, float out[4]);

/* A dynamic copy's velocities for this step: the owner's plus the bounded correction. An owner
   standing still (its velocities under these) whose pose is within SYNC_CORRECT_REST_M and
   SYNC_CORRECT_REST_RAD gets zero: a correction of a hair it cannot close (pressed by what it
   stands among) would set a velocity every tick, and any velocity set wakes the island, so the
   bodies around it would never sleep. */
#define SYNC_CORRECT_REST_M 0.001f
#define SYNC_CORRECT_REST_RAD 0.0035f
#define SYNC_CORRECT_STILL_SPEED 0.001f
#define SYNC_CORRECT_STILL_SPIN 0.002f
void sync_drive_dynamic(const float now_position[3], const float now_rotation[4], const sync_play_t *play, float dt, float velocity[3],
                        float angular[3]);

typedef struct sync_offset {
    float position[3];
    float rotation[4];
} sync_offset_t;

void sync_offset_clear(sync_offset_t *o);
/* The physics pose jumped from `before` to `after`: the offset takes the difference so the drawn
   pose stays; past `reach` (SYNC_OFFSET_SNAP_M, or SYNC_OFFSET_LOST_M for a lost take) it is
   cleared and the copy is seen to jump. */
void sync_offset_absorb(sync_offset_t *o, const float before_position[3], const float before_rotation[4], const float after_position[3],
                        const float after_rotation[4], float reach);
/* Once per local tick. Physics never sees the offset; body_sync_render_offset reports it. */
void sync_offset_decay(sync_offset_t *o);

#endif
