#ifndef FPGA_IF_H
#define FPGA_IF_H

#include <stdint.h>
#include <stdbool.h>

#include "../hal/hal-api.h"
#include "../game/game_types.h"
#include "../../lib/address_map_arm.h"

// -----------------------------------------------------------------------------
// fpga_if.h
//
// Provides an interface between the HPS (ARM processor) and the custom
// FPGA rhythm game circuits.
//
// The FPGA is controlled using a single 32-bit control word written to
// the JP1 PIO register via memory-mapped I/O (through the HAL).
//
// This module abstracts:
// - lane selection (HEX display prompts)
// - LED animation modes
// - tempo selection (beat timing)
// - enable/reset control
//
// This allows the game logic (game.c) to interact with FPGA hardware
// without dealing with bit manipulation directly.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Control Word Layout (32-bit)
//
//   [1:0]   lane select (HEX prompt)
//   [3:2]   LED animation mode
//   [7:4]   tempo select (beat engine)
//   [8]     enable (turn system on/off)
//   [9]     reset (reset FPGA timing/state)
// -----------------------------------------------------------------------------

// Lane select for HEX prompt display.
typedef enum {
    FPGA_LANE_0 = 0,
    FPGA_LANE_1 = 1,
    FPGA_LANE_2 = 2,
    FPGA_LANE_3 = 3
} fpga_lane_t;

// LED controller modes from led_controller.v
typedef enum {
    FPGA_LED_OFF   = 0, // LEDs off
    FPGA_LED_CHASE = 1, // moving chase pattern
    FPGA_LED_PULSE = 2, // pulsing beat effect
    FPGA_LED_BLINK = 3  // blinking pattern
} fpga_led_mode_t;

// Tempo select from beat_engine.v
typedef enum {
    FPGA_TEMPO_45_BPM = 0,
    FPGA_TEMPO_60_BPM = 1,
    FPGA_TEMPO_75_BPM = 2
} fpga_tempo_t;

// FPGA interface handle
//
// THis structure maintains:
// - HAL mapping for MMIO access
// - a shadow copy of the control word
// - initialization status
typedef struct {
    hal_map_t *hal;       // HAL mapping used to access JP1 register
    uint32_t ctrl_word;   // software shadow copy of JP1 control word
    int initialized;      // Initialization flag
} fpga_if_t;

// -------------------- API --------------------

// -----------------------------------------------------------------------------
// Initialization / Cleanup
// -----------------------------------------------------------------------------

// Initialize FPGA interface and clear control word.
// Returns 0 on success, -1 on failure.
int  fpga_if_init(fpga_if_t *fpga, hal_map_t *hal);
int  fpga_if_cleanup(fpga_if_t *fpga);

// -----------------------------------------------------------------------------
// Low-Level Control
// -----------------------------------------------------------------------------

// Write the current control word to FPGA hardware.
void fpga_if_commit(fpga_if_t *fpga);

// Clear all fields in the control word and update FPGA
void fpga_if_clear(fpga_if_t *fpga);

// Generate a short reset pulse to re-sync FPGA timing.
void fpga_if_reset_pulse(fpga_if_t *fpga);

// -----------------------------------------------------------------------------
// Field Setters (modify control word only; require commit to apply)
// -----------------------------------------------------------------------------

// Set active lane for HEX display prompt.
void fpga_if_set_lane(fpga_if_t *fpga, fpga_lane_t lane);

// Set LED animation mode.
void fpga_if_set_led_mode(fpga_if_t *fpga, fpga_led_mode_t mode);

// Set tempo (beat timing).
void fpga_if_set_tempo(fpga_if_t *fpga, fpga_tempo_t tempo);

// Enable or disable FPGA output.
void fpga_if_set_enable(fpga_if_t *fpga, bool enable);

// Assert or deassert FPGA reset signal.
void fpga_if_set_reset(fpga_if_t *fpga, bool reset);

// -----------------------------------------------------------------------------
// Convenience Helpers
// -----------------------------------------------------------------------------

// Apply a full FPGA configuration in one call.
//
// This sets:
// - lane
// - LED mode
// - tempo (derived from game mode)
// - enable flag
//
// Then writes the result to hardware.
void fpga_if_apply(fpga_if_t *fpga,
                   game_mode_t game_mode,
                   uint8_t lane,
                   fpga_led_mode_t led_mode,
                   bool enable);

// Convert game difficulty mode to FPGA tempo setting.
fpga_tempo_t fpga_if_game_mode_to_tempo(game_mode_t mode);

#endif // FPGA_IF_H