#ifndef FPGA_IF_H
#define FPGA_IF_H

#include <stdint.h>
#include <stdbool.h>

#include "../hal/hal-api.h"
#include "../game/game_types.h"
#include "../../lib/address_map_arm.h"

// ------------------------------------------------------------
// Rhythm FPGA Interface
// ------------------------------------------------------------
// The custom rhythm_fpga_circuits module is driven by a single
// 32-bit control word written by the HPS through the existing
// JP1 PIO exported by the DE10 Standard Computer base project.
//
// ctrl bit fields:
//   [1:0]  lane select
//   [3:2]  LED animation mode
//   [7:4]  tempo select
//   [8]    enable
//   [9]    reset
// ------------------------------------------------------------

// Lane select for HEX prompt
typedef enum {
    FPGA_LANE_0 = 0,
    FPGA_LANE_1 = 1,
    FPGA_LANE_2 = 2,
    FPGA_LANE_3 = 3
} fpga_lane_t;

// LED controller modes from led_controller.v
typedef enum {
    FPGA_LED_OFF   = 0, // mode 0
    FPGA_LED_CHASE = 1, // mode 1
    FPGA_LED_PULSE = 2, // mode 2
    FPGA_LED_BLINK = 3  // mode 3
} fpga_led_mode_t;

// Tempo select from beat_engine.v
typedef enum {
    FPGA_TEMPO_45_BPM = 0,
    FPGA_TEMPO_60_BPM = 1,
    FPGA_TEMPO_75_BPM = 2
} fpga_tempo_t;

typedef struct {
    hal_map_t *hal;
    uint32_t ctrl_word;   // software shadow copy of JP1 control word
    int initialized;
} fpga_if_t;

// -------------------- API --------------------

int  fpga_if_init(fpga_if_t *fpga, hal_map_t *hal);
int  fpga_if_cleanup(fpga_if_t *fpga);

// low-level write of current shadow control word
void fpga_if_commit(fpga_if_t *fpga);

// whole-word helpers
void fpga_if_clear(fpga_if_t *fpga);
void fpga_if_reset_pulse(fpga_if_t *fpga);

// field setters
void fpga_if_set_lane(fpga_if_t *fpga, fpga_lane_t lane);
void fpga_if_set_led_mode(fpga_if_t *fpga, fpga_led_mode_t mode);
void fpga_if_set_tempo(fpga_if_t *fpga, fpga_tempo_t tempo);
void fpga_if_set_enable(fpga_if_t *fpga, bool enable);
void fpga_if_set_reset(fpga_if_t *fpga, bool reset);

// convenience helpers
void fpga_if_apply(fpga_if_t *fpga, game_mode_t game_mode, uint8_t lane, fpga_led_mode_t led_mode, bool enable);
fpga_tempo_t fpga_if_game_mode_to_tempo(game_mode_t mode);

#endif // FPGA_IF_H