#ifndef FPGA_IF_H
#define FPGA_IF_H

#include <stdint.h>
#include <stdbool.h>
#include "../hal/hal-api.h"
#include "../game/game_types.h"

// ------------------------------------------------------------
// FPGA Rhythm Game Avalon-MM Interface (LW Bridge offsets)
// ------------------------------------------------------------
// NOTE: RHYTHM_IF_BASE is the base offset you will assign in
// Platform Designer (Qsys) for your custom FPGA peripheral.
// Keep the register map stable to avoid rework.
// ------------------------------------------------------------

#ifndef RHYTHM_IF_BASE
#define RHYTHM_IF_BASE 0x00004000u   // placeholder until Qsys assigns it
#endif

typedef struct {
  uint8_t lane;        // 0..3
  bool flash_all;      // show "all segments" flash cue
  bool shake;          // shake step (expert only)
} fpga_step_t;

typedef struct {
  bool preview_active;
  bool playback_active;
  bool done;

  uint32_t step_index;           // current step index
  uint32_t beat_edge;            // increments/toggles each beat

  bool window_active;            // button scoring window active
  bool shake_window_active;      // shake scoring window active
} fpga_status_t;

typedef struct {
  hal_map_t *hal;                // your HAL mapping (LW bridge)
  game_mode_t mode;
  uint32_t bpm;
  uint32_t seq_len;
} fpga_if_t;

// -------------------- API --------------------

int  fpga_if_init(fpga_if_t *fpga, hal_map_t *hal);

void fpga_if_reset(fpga_if_t *fpga);

void fpga_if_configure(fpga_if_t *fpga, game_mode_t mode, uint32_t bpm, uint32_t seq_len);

int  fpga_if_load_sequence(fpga_if_t *fpga, const fpga_step_t *steps, uint32_t count);

void fpga_if_start_preview(fpga_if_t *fpga);
void fpga_if_start_playback(fpga_if_t *fpga);
void fpga_if_stop(fpga_if_t *fpga);

fpga_status_t fpga_if_read_status(fpga_if_t *fpga);

// Utility: pack a step into a 32-bit register word
uint32_t fpga_if_pack_step(fpga_step_t s);

#endif // FPGA_IF_H
