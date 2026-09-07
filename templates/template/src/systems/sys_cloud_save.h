#ifndef SYS_CLOUD_SAVE_H
#define SYS_CLOUD_SAVE_H

#include <stdbool.h>

/* The portal carries the run between a player's devices; local storage still
   owns every save the game makes. The cloud copy is a mirror with one job at
   startup: if the account's copy is newer than this browser's, it replaces it
   before anything is loaded. */

/* Asks the portal for the account's copy. Called once, as early as the SDK
   exists, so the answer is usually waiting by the time the pack is ready. */
void cloud_save_begin(void);

/* The startup barrier's question: the portal answered, said it has nothing,
   cannot answer at all, or took longer than a player will stare at a loading
   screen. Loading local state before this is true risks swapping the save out
   from under a run that already started. */
bool cloud_save_settled(void);

/* True when the local slot now holds the portal's copy and the caller must
   load state again. Keeps the newer of the two stamps, so a session on another
   device is never overwritten by an older local save.

   `local_is_fresh` says the load found no save here and started a new game.
   That new game stamps itself with the current time, which would out-rank any
   copy the account holds, so a fresh device takes the account's run whatever
   the stamps say — that IS the case cloud saves exist for. */
bool cloud_save_adopt(bool local_is_fresh);

/* Mirrors the local save up whenever the game has written a newer one. */
void cloud_save_tick(void);

#endif /* SYS_CLOUD_SAVE_H */
