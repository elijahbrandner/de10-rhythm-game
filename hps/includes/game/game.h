#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

#include "game_types.h"
#include "scoring.h"
#include "../fpga/fpga_if.h"

// ----------------------------
// Game states
// ----------------------------
typedef enum {
    ST_IDLE = 0,
    ST_WATCH,
    ST_PREVIEW,
    ST_GO,
    ST_PLAYBACK,
    ST_RESULTS,
    ST_EXIT
} game_state_t;

// ----------------------------
// Game inputs (provided by main)
// ----------------------------
typedef struct {
    uint32_t switches;     // SW[9:0] bitmask
    uint32_t buttons_raw;  // KEY[3:0] bitmask (pressed=1, not inverted)
    bool shake_detected;   // from accel
} game_inputs_t;

// ----------------------------
// Game outputs (consumed by UI layer later)
// ----------------------------
typedef struct {
    game_state_t state;
    game_mode_t mode;
    uint32_t bpm;
    uint32_t seq_len;

    const char *line1;
    const char *line2;

    uint32_t score_0_100;
    const char *rating_text;
} game_outputs_t;

// ----------------------------
// Game context
// ----------------------------
typedef struct {
    game_state_t state;

    // input edge tracking
    uint32_t prev_buttons;

    // mode selection
    select_mode_t select_mode;
    game_mode_t mode;

    // round parameters
    uint32_t bpm;
    uint32_t seq_len;
    uint32_t seed;

    // generated sequence
    fpga_step_t steps[64];

    // timing / state entry
    uint32_t state_enter_ms;

    // playback sync
    uint32_t last_beat_edge;
    uint32_t last_step_index;

    // scoring
    scoring_ctx_t scoring;
    bool step_scored[64];
    uint32_t score;
    bool completed;
    
    // LCD display
    uint32_t last_score;
    const char *last_rating_text;

    // control
    bool should_exit;

    // UI snapshot
    game_outputs_t out;

    
} game_t;

// ----------------------------
// API
// ----------------------------
void game_init(game_t *g);
void game_update(game_t *g, fpga_if_t *fpga, const game_inputs_t *in, uint32_t now_ms);

bool game_should_exit(const game_t *g);
const game_outputs_t *game_get_outputs(const game_t *g);

#endif // GAME_H