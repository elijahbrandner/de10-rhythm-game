#ifndef SWITCH_H
#define SWITCH_H

#include <stdint.h>
#include "../hal/hal-api.h"
#include "../game/game_types.h"
// -----------------------------------------------------------------------------
// switch.h
//
// Provides an interface for reading hardware switches (SW[9:0]) on the
// DE10-Standard board using memory-mapped I/O via the HAL.
//
// This module supports:
// - reading raw switch states
// - extracting individual switch bits
// - interpreting switches for game mode selection
// - detecting special conditions (e.g., all switches ON)
//
// -----------------------------------------------------------------------------

// --------------------------------------------------------------------
// DE10-Standard Switch Map (Rhythm-Based Timing Game)
// --------------------------------------------------------------------
//
// SW9: Mode Select
//   0 = Ladder Mode (progress Easy->Medium->Hard->Expert)
//   1 = Free Select Mode (difficulty selected by SW1..SW0)
//
// SW1..SW0: Difficulty Select (only when SW9 = 1)
//   00 -> Easy
//   10 -> Medium
//   01 -> Hard
//   11 -> Expert
//
// SW8..SW2: Reserved (future use / debug / seed / etc.)
//
// Exit combo is handled in the game layer:
//   SW[9:0] = 1111111111 AND KEY0 pressed -> Exit
// --------------------------------------------------------------------

// Bit Masks
//
// Mask for all switches (SW[9:0])
#define SWITCH_ALL_MASK        0x3FFu      // SW[9:0]

// Mask for mode selection switch (SW9)
#define SWITCH_FREESEL_MASK    (1u << 9)   // SW9

// Mask for difficulty selection (SW1..SW0)
#define SWITCH_DIFFICULTY_MASK 0x3u        // SW1..SW0

// -----------------------------------------------------------------------------
// Switch Handle
// -----------------------------------------------------------------------------

// Structure representing the switch peripheral.
//
// Stores:
// - HAL mapping reference
// - pointer to switch register (SW_BASE)
// - initialization state
typedef struct {
    hal_map_t *hal;             // HAL mapping for MMIO access
    volatile uint32_t *reg;     // Pointer to SW register
    int initialized;            // Initialization flag
} switch_handle_t;


// -----------------------------------------------------------------------------
// Core API
// -----------------------------------------------------------------------------

// Initialize switch interface using HAL mapping.
// Returns 0 on success, -1 on failure.
int switch_init(switch_handle_t *sw, hal_map_t *hal);

// Clean up switch handle (does not unmap memory).
// Returns 0 on success, -1 on failure.
int switch_cleanup(switch_handle_t *sw);

// Read all switch states as a bitmask (SW[9:0]).
// Returns 0 on success, -1 on failure.
int switch_read_all(const switch_handle_t *sw, uint32_t *switch_state);

// Read a single switch bit (0–9).
// Outputs 1 if ON, 0 if OFF.
// Returns 0 on success, -1 on failure.
int switch_read_bit(const switch_handle_t *sw, int bit_index, int *bit_state);

// -----------------------------------------------------------------------------
// Rhythm Game Helper Functions
// -----------------------------------------------------------------------------

// Determine selection mode based on SW9.
// Outputs SELECT_LADDER or SELECT_FREE.
// Returns 0 on success, -1 on failure.
int switch_read_select_mode(const switch_handle_t *sw, select_mode_t *mode_out);

// Determine game difficulty based on switches.
//
// In ladder mode:
//   Always returns GAME_MODE_EASY (progression handled in game.c)
//
// In free select mode:
//   Uses SW1..SW0 mapping to determine difficulty.
//
// Returns 0 on success, -1 on failure.
int switch_read_game_mode(const switch_handle_t *sw, game_mode_t *mode_out);

// Check if all switches are ON (SW[9:0] == 1).
// Outputs 1 if all ON, else 0.
// Returns 0 on success, -1 on failure.
int switch_all_on(const switch_handle_t *sw, int *all_on_out);

#endif // SWITCH_H
