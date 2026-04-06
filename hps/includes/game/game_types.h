#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// game_types.h
//
// Defines core data types used across the rhythm game system.
//
// These types are shared between:
// - game logic (game.c)
// - sequence generation (sequence.c)
// - FPGA interface (fpga_if.c)
//
// This file acts as a central definition point for:
// - game difficulty modes
// - selection modes
// - sequence step structure
// -----------------------------------------------------------------------------

// Game difficulty modes.
//
// These correspond to increasing difficulty levels and are used to:
// - determine BPM (tempo)
// - determine sequence length
// - adjust gameplay complexity (e.g., shake notes in expert mode)
typedef enum {
    GAME_MODE_EASY = 0, // Slow tempo, short sequence
    GAME_MODE_MEDIUM,   // Moderate tempo and sequence length
    GAME_MODE_HARD,     // Faster tempo, longer sequence
    GAME_MODE_EXPERT    // Includes shake steps and highest difficulty
} game_mode_t;

// Mode selection type.
//
// Determines how the game selects difficulty:
// - Ladder mode: progression handled automatically by game logic
// - Free mode: difficulty selected directly via switches
typedef enum {
    SELECT_LADDER = 0,  // Start at Easy, progress automatically
    SELECT_FREE         // Use switches to select difficulty
} select_mode_t;

// Sequence step definition.
//
// Represents a single step in the generated rhythm sequence.
// This structure is used by:
// - sequence.c (generation)
// - game.c (playback and scoring)
// - FPGA interface (display/output)
//
// Each step defines what the player should do at a given beat.
typedef struct {
    uint8_t lane;       // Lane index (0-3) for button input / HEX display
    bool flash_all;     // If true, display uses all segments (used for special cues)
    bool shake;         // If true, this step requires a shake input instead of button
} fpga_step_t;

#endif // GAME_TYPES_H