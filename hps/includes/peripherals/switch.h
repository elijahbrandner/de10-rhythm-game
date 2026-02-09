#ifndef SWITCH_H
#define SWITCH_H

#include <stdint.h>
#include "../hal/hal-api.h"
#include "../game/game_types.h"

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

// Masks
#define SWITCH_ALL_MASK        0x3FFu      // SW[9:0]
#define SWITCH_FREESEL_MASK    (1u << 9)   // SW9
#define SWITCH_DIFFICULTY_MASK 0x3u        // SW1..SW0

typedef struct {
    hal_map_t *hal;
    volatile uint32_t *reg;
    int initialized;
} switch_handle_t;

// Core API
int switch_init(switch_handle_t *sw, hal_map_t *hal);
int switch_cleanup(switch_handle_t *sw);

int switch_read_all(const switch_handle_t *sw, uint32_t *switch_state);
int switch_read_bit(const switch_handle_t *sw, int bit_index, int *bit_state);

// Rhythm Game Helpers
int switch_read_select_mode(const switch_handle_t *sw, select_mode_t *mode_out);
int switch_read_game_mode(const switch_handle_t *sw, game_mode_t *mode_out);
int switch_all_on(const switch_handle_t *sw, int *all_on_out);

#endif // SWITCH_H
