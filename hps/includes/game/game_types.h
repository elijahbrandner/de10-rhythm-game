#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>

typedef enum {
    GAME_MODE_EASY = 0,
    GAME_MODE_MEDIUM,
    GAME_MODE_HARD,
    GAME_MODE_EXPERT
} game_mode_t;

typedef enum {
    SELECT_LADDER = 0,   // SW9 = 0
    SELECT_FREE          // SW9 = 1
} select_mode_t;

#endif // GAME_TYPES_H
