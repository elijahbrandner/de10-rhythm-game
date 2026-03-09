#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GAME_MODE_EASY = 0,
    GAME_MODE_MEDIUM,
    GAME_MODE_HARD,
    GAME_MODE_EXPERT
} game_mode_t;

typedef enum {
    SELECT_LADDER = 0,
    SELECT_FREE
} select_mode_t;

// Sequence step used by game + sequence generation
typedef struct {
    uint8_t lane;       // 0..3
    bool flash_all;     // optional cue flag
    bool shake;         // expert shake step
} fpga_step_t;

#endif // GAME_TYPES_H