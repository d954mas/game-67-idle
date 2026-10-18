#ifndef GAME_WORLD_H
#define GAME_WORLD_H

/* World holds game state that systems share. Add domain-owned fields here only
   after a system needs them; presentation reads it but never becomes its owner. */
typedef struct World {
    float elapsed_seconds;
} World;

#endif
