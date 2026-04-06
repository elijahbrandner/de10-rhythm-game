#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

#include "game_types.h"
#include "scoring.h"
#include "../fpga/fpga_if.h"

// -----------------------------------------------------------------------------
// game.h
//
// Core game logic interface for the rhythm game.
//
// This module manages:
// - game state transitions (idle → playback → results, etc.)
// - sequence playback and timing
// - input processing (buttons, switches, accelerometer)
// - scoring and results
// - UI output generation
//
// The main loop provides inputs and time, while this module updates
// game state and produces outputs for display and FPGA control.
// -----------------------------------------------------------------------------

// ----------------------------
// Game states
// ----------------------------

// Represents the current state of the game state machine
typedef enum { 
    ST_IDLE = 0,        // Waiting for player to start
    ST_WATCH,           // Optional pre-round observation period
    ST_PREVIEW,         // SHow sequence before gameplay
    ST_GO,              // Transition state ("Get Ready")
    ST_COUNTDOWN,       // Countdown before gameplay starts
    ST_PLAYBACK,        // Active gameplay (player input required)
    ST_RESULTS,         // Show score and rating
    ST_EXIT             // Exit state (terminate program)
} game_state_t;

// ----------------------------
// Game inputs (provided by main)
// ----------------------------

// Encapsulates all external outputs to the game for a single update cycle
typedef struct {
    uint32_t switches;     // SW[9:0] bitmask
    uint32_t buttons_raw;  // KEY[3:0] bitmask (pressed=1, not inverted)
    bool shake_detected;   // from accelerometer input (true if shake detected)
} game_inputs_t;

// ----------------------------
// Game outputs (provided by main.c, consumed by UI layer later)
// ----------------------------

// Snapshot of the current game state for display and UI purposes
typedef struct {
    game_state_t state;     // Current game state
    game_mode_t mode;       // Current difficulty mode

    uint32_t bpm;           // Current tempo (beats per minute)
    uint32_t seq_len;       // Sequence length for this round

    const char *line1;      // LCD display line 1
    const char *line2;      // LCD dispaly line 2

    uint32_t score_0_100;   // Final score (0-100)
    const char *rating_text;    // Rating string ("Great", "Excellent")
} game_outputs_t;

// ----------------------------
// Game context (internal state)
// ----------------------------

// Main game state structure
//
// This holds all persistent data required to run the game across frames
typedef struct {
    game_state_t state;     // Current state of game

    // input edge tracking
    //
    // Previous buton state (for edge detection)
    uint32_t prev_buttons;

    // mode selection
    select_mode_t select_mode;  // Ladder vs Free select mode
    game_mode_t mode;           // Current difficulty

    // round parameters
    uint32_t bpm;       // Tempo for current round
    uint32_t seq_len;   // Number of steps in sequence
    uint32_t seed;      // RNG seed for sequence generation

    // generated sequence
    fpga_step_t steps[64];  // Generated sequence (max 64 steps)

    // timing / state entry
    uint32_t state_enter_ms;    // Timestamp when current state was entered

    // playback sync
    uint32_t last_beat_edge;    // Last detected beat timing reference
    uint32_t last_step_index;   // Last processed step index

    // scoring
    scoring_ctx_t scoring;      // Scoring system context
    bool step_scored[64];       // Tracks which steps have been scored
    uint32_t score;             // Final computed score
    bool completed;             // Whether sequence has finished
    
    // LCD display
    uint32_t last_score;            // Last round score 
    const char *last_rating_text;   // Last rating string

    // control
    bool should_exit;       // Set when exit coniditon is triggered

    // UI snapshot
    game_outputs_t out;     // Cached output for UI layer

    
} game_t;

// ----------------------------
// API
// ----------------------------
//
// Initialize the game state and default values
void game_init(game_t *g);

// Update game logic for one frame.
//
// Parameters:
// - g: game context
// - fpga: FPGA interface for hardware control
// - in: current input snapshot
// - now_ms: current time in milliseconds
void game_update(game_t *g, fpga_if_t *fpga, const game_inputs_t *in, uint32_t now_ms);

// Check whether the game has requested exit
bool game_should_exit(const game_t *g);

// Get current ouptut snapshot for UI/Display
const game_outputs_t *game_get_outputs(const game_t *g);

#endif // GAME_H